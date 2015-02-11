/*
 * File:   serialbroker.c
 * Author: martin
 *
 * Created on November 22, 2013
 */

//Forwards PubSub messages via UART to PIC

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "c_uart.h"
#include "common.h"
#include "RobotTypes.h"
#include "PubSubParser.h"
#include "broker.h"
#include "Blackboard.h"
#include "SysLog.h"

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

int SerialBrokerInit()
{
	struct termios settings;

	if (!load_device_tree(PS_UART_OVERLAY))
	{
		LogError("load dt overlay %s failed: %s",PS_UART_OVERLAY, strerror(errno));
		return errno;
	}
	else
	{
		LogRoutine("dt overlay %s loaded", PS_UART_OVERLAY);
	}

	//initialize UART
	picUartFD = open(PS_UART_DEVICE, O_RDWR | O_NOCTTY);

	if (picUartFD < 0) {
		LogError("open of %s failed: %s",PS_UART_DEVICE, strerror(errno));
		return errno;
	}
	else
	{
		LogRoutine("%s opened", PS_UART_DEVICE);
	}

	if (tcgetattr(picUartFD, &settings) != 0) {
		LogError("tcgetattr failed: %s", strerror(errno));
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
		LogError("tcsetattr failed: %s", strerror(errno));
		return -1;
	}

	LogRoutine("uart configured");

	//start RX & TX threads
	pthread_t thread;

	int s = pthread_create(&thread, NULL, RxThread, NULL);
	if (s != 0)
	{
		LogError("Rx: pthread_create failed. %s\n", strerror(s));
		return s;
	}
	s = pthread_create(&thread, NULL, TxThread, NULL);
	if (s != 0)
	{
		LogError("Tx: pthread_create failed. %s\n", strerror(s));
		return s;
	}

	routineCount = 0;
	infoCount = 0;
	warningCount = 0;
	errorCount = 0;

	return 0;
}

//called by the broker to see whether a message should be queued for the TX Thread

void SerialBrokerProcessMessage(psMessage_t *msg)
{
	if (msg->header.source != SOURCE_BBB) return;

//	DEBUG_PRINT("Serial: %s\n", psLongMsgNames[msg->header.messageType]);

	//check for messages to send to PIC
	switch(psDefaultTopics[msg->header.messageType])
	{
	case LOG_TOPIC:              //SysLog
	{
		//decide whether to report this log message
		int severity = msg->logPayload.severity;
		switch (severity)
		{
		case SYSLOG_ROUTINE:
			if (!logRoutine) return;
			if (routineCount++ >= MAX_ROUTINE) return;
			break;
		case SYSLOG_INFO:
			if (!logInfo && !logRoutine) return;
			if (infoCount++ >= MAX_INFO) return;
			break;
		case SYSLOG_WARNING:
			if (!logWarning && !logInfo && !logRoutine) return;
			if (warningCount++ >= MAX_WARNING) return;
			break;
		default:
			if (errorCount++ >= MAX_ERROR) return;
			break;
		}
	}
	AddMessage(&uartTxQueue, msg);
	break;
	default:
		//add to transmit queue
		AddMessage(&uartTxQueue, msg);
//		DEBUG_PRINT("uart: Queuing for send: %s\n", psLongMsgNames[msg->header.messageType]);
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

	LogRoutine("TX ready");

	for (;;) {

		//wait for a message
		psMessage_t *msg = GetMessage(&uartTxQueue);

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
//			DEBUG_PRINT("uart TX: %s\n", psLongMsgNames[msg->header.messageType]);
		} else {
			printf("uart TX: Failed to write to uart. %s\n", strerror(errno));
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

	LogRoutine("RX ready");

	for (;;) {
		do {
			messageComplete = 0;

			int count = (int) read(picUartFD, &c, 1);

			if (count == 1)
			{
				messageComplete = ParseNextCharacter(c, &msg, &parseStatus);
			}
		} while (messageComplete == 0);

		if (msg.header.source != SOURCE_BBB) {
//			DEBUG_PRINT("uart RX: %s\n", psLongMsgNames[msg.header.messageType]);
			//route the message
			NewBrokerMessage(&msg);
		}
	}
	return 0;
}

