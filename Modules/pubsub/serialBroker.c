/*
 * File:   serialbroker.c
 * Author: martin
 *
 * Created on November 22, 2013
 */

//Forwards PubSub messages via UART to PIC

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "uart.h"
#include "common.h"
#include "PubSubData.h"
#include "PubSubParser.h"
#include "blackboard/blackboard.h"
#include "syslog/syslog.h"
#include "SoftwareProfile.h"
#include "brokerQ.h"
#include "broker_debug.h"


//Serial Tx queue
BrokerQueue_t uartTxQueue = {NULL, NULL,
		PTHREAD_MUTEX_INITIALIZER,
		PTHREAD_COND_INITIALIZER
};

//RX and TX UART threads
void *RxThread(void *a);
void *TxThread(void *a);

#define MAX_UART_MESSAGE (sizeof(psMessage_t) + 10)

int picUartFD;

int routineCount;
int infoCount;
int warningCount;
int errorCount;

pthread_t SerialBrokerInit()
{
	struct termios settings;

	if (uart_setup(PS_TX_PIN, PS_RX_PIN) < 0)
	{
		return -1;
	}

	//initialize UART
	picUartFD = open(PS_UART_DEVICE, O_RDWR | O_NOCTTY);

	if (picUartFD < 0) {
		ERRORPRINT("open of %s failed: %s",PS_UART_DEVICE, strerror(errno));
		return errno;
	}
	else
	{
		DEBUGPRINT("%s opened", PS_UART_DEVICE);
	}

	if (tcgetattr(picUartFD, &settings) != 0) {
		ERRORPRINT("tcgetattr failed: %s\n", strerror(errno));
		return -1;
	}

	//no processing
	settings.c_iflag = 0;
	settings.c_oflag = 0;
	settings.c_lflag = 0;
	settings.c_cflag = CLOCAL | CREAD | CS8;        //no modem, 8-bits

	//baudrate
	cfsetospeed(&settings, PS_UART_BAUDRATE);
	cfsetispeed(&settings, PS_UART_BAUDRATE);

	if (tcsetattr(picUartFD, TCSANOW, &settings) != 0) {
		ERRORPRINT("tcsetattr failed: %s\n", strerror(errno));
		return -1;
	}

	DEBUGPRINT("uart configured\n");

	routineCount = 0;
	infoCount = 0;
	warningCount = 0;
	errorCount = 0;

	//start RX & TX threads
	pthread_t thread;

	int s = pthread_create(&thread, NULL, RxThread, NULL);
	if (s != 0)
	{
		ERRORPRINT("Rx: pthread_create failed. %s\n", strerror(s));
		return s;
	}
	s = pthread_create(&thread, NULL, TxThread, NULL);
	if (s != 0)
	{
		ERRORPRINT("Tx: pthread_create failed. %s\n", strerror(s));
		return s;
	}

	return s;
}

//called by the broker to see whether a message should be queued for the TX Thread

void SerialBrokerProcessMessage(psMessage_t *msg)
{
	if (msg->header.source != OVERMIND) return;

	DEBUGPRINT("Serial: %s\n", psLongMsgNames[msg->header.messageType]);

	//check for messages to send to MCP
	switch(psDefaultTopics[msg->header.messageType])
	{
	case LOG_TOPIC:              //SysLog
	{
		//decide whether to report this log message
		int severity = msg->logPayload.severity;
		switch (severity)
		{
        case SYSLOG_ERROR:
        case SYSLOG_FAILURE:
			if (errorCount++ >= MAX_ERROR) return;
			break;
        default:
        	return;
			break;
		}
	}
	CopyMessageToQ(&uartTxQueue, msg);
	break;
	default:
		//add to transmit queue
		CopyMessageToQ(&uartTxQueue, msg);
		DEBUGPRINT("uart: Queuing for send: %s\n", psLongMsgNames[msg->header.messageType]);
		break;

	}
}

//Tx thread - sends messages from the Tx Q
void *TxThread(void *a) {
	int i;
	int length;
	char messageBuffer[MAX_UART_MESSAGE];
	char *current;
	long written;
	int checksum;
	unsigned char sequenceNumber = 0;

	DEBUGPRINT("TX ready\n");

	for (;;) {

		//wait for a message
		psMessage_t *msg = GetNextMessage(&uartTxQueue);

		length =  msg->header.length + 7;   //stx, header(5), payload, checksum

		current = messageBuffer;

		*current++ = STX_CHAR;
		*current++ = msg->header.length;
		*current++ = ~msg->header.length;
		*current++ = sequenceNumber;
		sequenceNumber = (sequenceNumber + 1) & 0xff;
		*current++ = msg->header.source;
		*current++ = msg->header.messageType;

		memcpy(current, msg->packet, msg->header.length);

		checksum = 0;
		for (i = 1; i < length-1; i++) {
			checksum += messageBuffer[i];
		}
		messageBuffer[length-1] = (checksum & 0xff);

		written = write(picUartFD, messageBuffer, length);

		if (written == length)
		{
			DEBUGPRINT("uart TX: %s\n", psLongMsgNames[msg->header.messageType]);
		} else {
			ERRORPRINT("uart TX: Failed to write to uart. %s\n", strerror(errno));
		}

		if (psDefaultTopics[msg->header.messageType] == LOG_TOPIC)
		{
			switch (msg->logPayload.severity)
			{
			case SYSLOG_ROUTINE:
				routineCount--;
				break;
			case SYSLOG_INFO:
				infoCount--;
				break;
			case SYSLOG_WARNING:
				warningCount--;
				break;
			default:
				errorCount--;
				break;
			}
		}
		DoneWithMessage(msg);
	}
	return 0;
}

//receives messages and passes to broker
void *RxThread(void *a) {
	int messageComplete;
	psMessage_t msg;
	uint8_t c;

	status_t parseStatus;

	parseStatus.noCRC 		= 0;	///< Do not expect a CRC, if > 0
	parseStatus.noSeq		= 0;	///< Do not check seq #s, if > 0
	parseStatus.noLength2	= 0;	///< Do not check for duplicate length, if > 0
	parseStatus.noTopic		= 1;	///< Do not check for topic ID, if > 0
	ResetParseStatus(&parseStatus);

	DEBUGPRINT("RX ready\n");

	for (;;) {
		do {
			messageComplete = 0;

			int count = (int) read(picUartFD, &c, 1);

			if (count == 1)
			{
				messageComplete = ParseNextCharacter(c, &msg, &parseStatus);
			}
		} while (messageComplete == 0);

		if (msg.header.source != OVERMIND) {
			DEBUGPRINT("uart RX: %s\n", psLongMsgNames[msg.header.messageType]);
			//route the message
			RouteMessage(&msg);
		}
	}
	return 0;
}

