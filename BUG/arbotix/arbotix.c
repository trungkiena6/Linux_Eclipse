/*
 ============================================================================
 Name        : arbotix.c
 Author      : Martin
 Version     :
 Copyright   : (c) 2013 Martin Lane-Smith
 Description : Exchanges messages with the ArbotixM subsystem
 	 	 	   Receives MOVE messages, and publishes ODOMETRY messages
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

#include "RobotTypes.h"
#include "hex_msg.h"
#include "broker.h"
#include "Blackboard.h"
#include "common.h"
#include "SysLog.h"

//Process_enum process_id = PROCESS_ARBOTIX;

message_t lastStatusMessage, lastFailMessage, lastVoltsMessage;
message_t lastWalkMessage;

int stepsRemaining;
struct timeval lastWalkMessageTime;

void *ArbRxThread(void *a);					//receives msg from ArbotixM
void *ArbTimeoutThread(void *a);			//checks for timeout conditions

void SendGetStatus();						//asks for status
void SendActionCommand(msgType_enum m);		//sends a command
void SendGetPose(int leg);
int SendArbMessage(message_t *arbMsg);		//send message to Arbotix
void CheckBugState();						//compares Arbotix and system states
void ReportArbStatus(ArbotixState_enum s);	//report Arbotix status to system
void RequestBugState(BugState_enum s);		//request change of system status

int arbUartFD;								//Arbotix uart file descriptor

ArbotixState_enum 	arbState = ARB_STATE_UNKNOWN;	//latest reported from Arbotix state machine

time_t actionTimeout, statusTimeout;

//names for log messages
static char *servoNames[] = SERVO_NAMES;
static char *msgTypes_L[] = DEBUG_MSG_L;

int lowVoltageError = false;

int ArbotixInit() {
	//set up serial port
	struct termios settings;

	if (!load_device_tree(ARB_UART_OVERLAY)) {
		LogError("Load dt %s : %s", ARB_UART_OVERLAY, strerror(errno));
		return errno;
	} else {
		LogRoutine("dt overlay %s loaded", ARB_UART_OVERLAY);
	}

	//open UART
	arbUartFD = open(ARB_UART_DEVICE, O_RDWR | O_NOCTTY);

	if (arbUartFD < 0) {
		LogError("open %s : %s", ARB_UART_DEVICE, strerror(errno));
		return errno;
	} else {
		LogRoutine("%s opened", ARB_UART_DEVICE);
	}

	//set raw & speed attributes
	if (tcgetattr(arbUartFD, &settings) != 0) {
		LogError("tcgetattr %s", strerror(errno));
		return errno;
	}
	cfmakeraw(&settings);
	settings.c_cc[VTIME] = 1;
	settings.c_cc[VMIN] = 1;
	settings.c_cflag |= CLOCAL | CREAD | CS8;        //no modem, 8-bits

	//baudrate
	cfsetospeed(&settings, ARB_UART_BAUDRATE);
	cfsetispeed(&settings, ARB_UART_BAUDRATE);

	if (tcsetattr(arbUartFD, TCSANOW, &settings) != 0) {
		LogError("tcsetattr %s", strerror(errno));
		return errno;
	}

	statusTimeout = time(NULL) + 2;

	//create thread to receive Arbotix messages
	pthread_t thread;
	int s = pthread_create(&thread, NULL, ArbRxThread, NULL);
	if (s != 0) {
		LogError("Rx: pthread_create %i %s", s, strerror(errno));
		return errno;
	}
	//create thread for timeouts
	s = pthread_create(&thread, NULL, ArbTimeoutThread, NULL);
	if (s != 0) {
		LogError("T/O: pthread_create %i %s", s, strerror(errno));
		return errno;
	}

	statusTimeout = actionTimeout = -1;
	return 0;
}

//thread to listen for messages
void ArbotixProcessMessage(psMessage_t *msg) {

//	DEBUG_PRINT("Arbotix: %s\n", psLongMsgNames[msg->header.messageType]);

	switch (msg->header.messageType)
	{
	case MOVE:					//from Overmind via Navigator, or from iOS App
		if (arbDebug)
			printf("MOVE msg\n");

		if ((msg->header.source == SOURCE_APP && rcControl)
				|| (msg->header.source == SOURCE_BBB && !rcControl))
		{
			if (bugSystemState.state == BUG_ACTIVE)
			{
				switch (arbState)
				{
				case ARB_READY:
				case ARB_WALKING:
					lastWalkMessage.msgType = MSG_TYPE_WALK;
					lastWalkMessage.xSpeed = (int8_t) msg->threeFloatPayload.xSpeed;
					lastWalkMessage.ySpeed = (int8_t) msg->threeFloatPayload.ySpeed;
					lastWalkMessage.zRotateSpeed = (int8_t) msg->threeFloatPayload.zRotateSpeed;
					SendArbMessage(&lastWalkMessage);
					break;
				case ARB_STOPPING:
					break;
				default:
					LogWarning("MOVE in %s", arbStateNames[arbState]);
					break;
				}
			}
			else
			{
				LogWarning("MOVE not in BUG_ACTIVE");
			}
		}
		else
		{
			printf("source=%s, rc=%s\n", subsystemNames[msg->header.source], (rcControl?"ON":"OFF"));

			LogWarning("RC Option overrides MOVE");
		}
		break;
	case TICK_1S:


		if (arbDebug)
			printf("Tick (%s)\n", bugStateNames[bugSystemState.state]);

		CheckBugState();		//check for action needed
		break;
	}
}

//thread to listen for messages from ArbotixM
void *ArbRxThread(void *a) {
	message_t arbMsg;

	unsigned char c;
	int count;
	int csum;
	int i;

	unsigned char *msgChars = (unsigned char *) &arbMsg.intValues;
	int msgLen = MSG_INTS * 2;

	LogRoutine("Rx thread ready");
	SendGetStatus();

	while (1) {
		//read next message from Arbotix
		for (i = 0; i < MSG_LEN; i++) {
			msgChars[i] = 0;
		}
		//scan for '@'
		c = 0;
		while (c != MSG_START)
		{
			count = read(arbUartFD, &c, 1);
		}
		if (arbDebug)
			printf("arbotix: message start\n");

		//read message type
		count = 0;
		while (count == 0)
		{
			count = read(arbUartFD, &arbMsg.msgType, 1);
		}
		//read rest
		count = 0;
		while (count < msgLen)
		{
			count += read(arbUartFD, msgChars + count , msgLen - count);
		}
		if (count == msgLen) {
			//valid length
			csum = arbMsg.msgType;
			//calculate checksum
			for (i = 0; i < msgLen; i++) {
				csum += msgChars[i];
			}
			count = read(arbUartFD, &c, 1);	//read checksum
			if (count == 1 && c == (csum & 0xff)) {
				//valid checksum
				if (arbMsg.msgType < MSG_TYPE_COUNT)
				{
//					if (arbDebug)
						printf("Arbotix Rx: %s\n", msgTypes_L[arbMsg.msgType]);
				}
				else
				{
					LogError("bad msgType: %i -", arbMsg.msgType);
					for (i = 0; i < MSG_INTS; i++) {
						printf(" %i", arbMsg.intValues[i]);
					}
					printf("\n");
				}
				switch (arbMsg.msgType) {
				case MSG_TYPE_STATUS:    //Status report payload
					if (arbMsg.state < AX_STATE_COUNT)
						LogRoutine("arbotix: status message: %s", arbStateNames[arbMsg.state])
						else
							LogRoutine("arbotix: status message: %u", arbMsg.state);
					lastStatusMessage = arbMsg;
					statusTimeout = time(NULL) + arbTimeout;
					ReportArbStatus(arbMsg.state);
					CheckBugState();

					switch(arbState)
					{
					case ARB_STOPPING:		 //in process of stopping
					case ARB_SITTING:        //in process of sitting down
					case ARB_STANDING:       //in process of standing
						actionTimeout = time(NULL) + arbTimeout;
						break;
					default:
						actionTimeout = -1;
						break;
					}
					break;
					case MSG_TYPE_POSE:      //leg position payload
						LogInfo("Leg %i: X=%i Y=%i Z=%i", arbMsg.legNumber, arbMsg.x, arbMsg.y, arbMsg.z );
						break;
					case MSG_TYPE_SERVOS:    //Servo payload
						LogInfo("Leg %i: Coxa=%i Femur=%i Tibia=%i", arbMsg.legNumber, arbMsg.coxa, arbMsg.femur, arbMsg.tibia );
						break;
					case MSG_TYPE_MSG:       //Text message
						LogInfo("arbotix: %4s", arbMsg.text);
						break;
					case MSG_TYPE_ODOMETRY: {
						psMessage_t msg;
						if (stepsRemaining > 0)
							stepsRemaining--;
						psInitPublish(msg, ODOMETRY);
						msg.odometryPayload.xMovement = arbMsg.xMovement;
						msg.odometryPayload.yMovement = arbMsg.yMovement;
						msg.odometryPayload.zRotation = arbMsg.zRotation;

						NewBrokerMessage(&msg);
					}
					break;
					case MSG_TYPE_VOLTS:     //Text message
						lastVoltsMessage = arbMsg;
						LogInfo("arbotix: batt: %0.1fV",
								(float) (arbMsg.volts / 10));
						break;
					case MSG_TYPE_FAIL:
						LogWarning("arb: Fail: %s = %i", servoNames[arbMsg.servo], arbMsg.angle);
						break;
					case MSG_TYPE_ERROR:
					{
						if (arbMsg.errorCode >= 0 && arbMsg.errorCode < MSG_TYPE_COUNT)
						{
							if (arbMsg.errorCode < MSG_TYPE_UNKNOWN)
							{
								LogInfo("Debug: %s", msgTypes_L[arbMsg.errorCode]);
							}
							else
							{
								LogInfo("Error: %s", msgTypes_L[arbMsg.errorCode]);
								switch(arbMsg.errorCode)
								{
								case LOW_VOLTAGE:
									lowVoltageError = true;
									CheckBugState();
									break;
								default:
									break;
								}
							}
						}
						else
						{
							LogInfo("debug: %i?", arbMsg.errorCode);
						}
					}
					break;
					default:
						break;
				}
			}
			else
			{
				LogWarning("Bad checksum:\n");
				for (i = 0; i < MSG_LEN; i++) {
					printf(" %i", msgChars[i]);
				}
				printf(". sum=%x.\nExpected %02x, got %02x\n", csum, (csum & 0xff), c);
			}
		}
		else
		{
			if (count < 0)
			{
				LogWarning("Read error %s", strerror(errno));
			}
			else LogWarning("Read %i bytes", count);
		}
	}
	return NULL;
}

//thread to check for ArbotixM command timeouts
void *ArbTimeoutThread(void *a)
{
	while (1)
	{
		//		printf("Timeout thread\n");

		//check for timeouts and errors
		switch(arbState)
		{
		case ARB_STOPPING:		 //in process of stopping
		case ARB_SITTING:        //in process of sitting down
		case ARB_STANDING:       //in process of standing
			if (actionTimeout > 0 && actionTimeout < time(NULL))
			{
				LogError("Timeout");
				ReportArbStatus(ARB_TIMEOUT);
				SendGetStatus();
			}
			break;
			//error states
		case ARB_STATE_UNKNOWN:	//start up
		case ARB_TIMEOUT:		//no response to command
		case ARB_ERROR:			//error reported
			if (statusTimeout < time(NULL))
			{
//				if (arbDebug)
					printf("Timeout GetStatus\n");
				SendGetStatus();
			}
			break;
		default:
			break;
		}
		sleep(1);
	}
	return 0;
}

//review Arbotix state versus system state and take action
pthread_mutex_t	bugStateMtx = PTHREAD_MUTEX_INITIALIZER;
void CheckBugState()
{
//	if (arbDebug)
		printf("bugSystemState= %s, arbState= %s\n", bugStateNames[bugSystemState.state],  arbStateNames[arbState]);

	//critical section
	int s = pthread_mutex_lock(&bugStateMtx);
	if (s != 0)
	{
		LogError("bugStateMtx lock %i", s);
	}
	switch (arbState) {
	case ARB_WALKING:         	//walking
		if (bugSystemState.state < BUG_ACTIVE || lowVoltageError) {
			//shouldn't be...
			SendActionCommand(MSG_TYPE_HALT);
		}
		break;
	case ARB_READY:       		//standing up and ready
		if (bugSystemState.state < BUG_STANDBY || lowVoltageError) {
			//shouldn't be...
			SendActionCommand(MSG_TYPE_SIT);
		}
		break;
	case ARB_TORQUED:           //in transient sitting position, torque on
		if (bugSystemState.state >= BUG_STANDBY && !lowVoltageError) {
			//shouldn't be...
			SendActionCommand(MSG_TYPE_STAND);
		} else if (bugSystemState.state < BUG_STANDBY || lowVoltageError) {
			//shouldn't be...
			SendActionCommand(MSG_TYPE_RELAX);
		}
		break;
	case ARB_RELAXED:        	//torque off
		if (bugSystemState.state >= BUG_STANDBY && !lowVoltageError) {
			//shouldn't be...
			SendActionCommand(MSG_TYPE_TORQUE);
		}
		break;
	default:
		break;
	}

	s = pthread_mutex_unlock(&bugStateMtx);
	if (s != 0)
	{
		LogError("bugStateMtx unlock %i", s);
	}
	//end critical section
}

//send message to ArbotixM
pthread_mutex_t	arbMsgMtx = PTHREAD_MUTEX_INITIALIZER;
int SendArbMessage(message_t *arbMsg) {
	int count;
	int result = 0;
	int csum = 0;
	int i;

	int dataLen = MSG_INTS * 2;		//payload only
	int msgLen  = dataLen + 3;		//plus @, msgType, csum
	unsigned char msgChars[msgLen];
	msgChars[0] = MSG_START;
	msgChars[1] = arbMsg->msgType;

	memcpy(&msgChars[2], (void*) &arbMsg->intValues, dataLen);

	//calculate checksum
	for (i = 1; i < dataLen+2; i++) {
		csum += msgChars[i];
	}
	msgChars[msgLen-1] = csum & 0xff;

//	if (arbDebug)
		printf("Arb Tx: %s\n", msgTypes_L[arbMsg->msgType]);

	//critical section
	int m = pthread_mutex_lock(&arbMsgMtx);
	if (m != 0)
	{
		LogError("arbMsgMtx lock %i", m);
	}
	count = MSG_LEN+2;
	while (count > 0)
	{
		int written = write(arbUartFD, msgChars + MSG_LEN + 2 - count, count);
		count -= written;
	}
	m = pthread_mutex_unlock(&arbMsgMtx);
	if (m != 0)
	{
		LogError("arbMsgMtx unlock %i", m);
	}
	//end critical section
	return result;
}

#define ZERO_MSG(X) {int i;for(i=0;i<MSG_INTS;i++) X.intValues[i]=0;}

//functions to send various messages to the ArbotixM
void SendGetStatus() {
	message_t arbMsg;
	ZERO_MSG(arbMsg);
	arbMsg.msgType = MSG_TYPE_GETSTATUS;
	if (SendArbMessage(&arbMsg) == 0)
	{
		statusTimeout = time(NULL) + 2;		//give it 2 seconds to answer
	}
}
//send command only
void SendActionCommand(msgType_enum m) {
	message_t arbMsg;
	ZERO_MSG(arbMsg);
	arbMsg.msgType = m;
	if (SendArbMessage(&arbMsg) == 0)
	{
		actionTimeout = time(NULL) + arbTimeout;		//give it 5 seconds or so
	}
}
//send GET_POSE command plus legNumber
void SendGetPose(int leg) {
	message_t arbMsg;
	ZERO_MSG(arbMsg);
	arbMsg.legNumber = leg;
	arbMsg.msgType = MSG_TYPE_GET_POSE;
	SendArbMessage(&arbMsg);
}
//report Arbotix Status
pthread_mutex_t	arbStateMtx = PTHREAD_MUTEX_INITIALIZER;
void ReportArbStatus(ArbotixState_enum s) {
	if (arbState != s)
	{
		psMessage_t msg;
		//critical section
		int m = pthread_mutex_lock(&arbStateMtx);
		if (m != 0)
		{
			LogError("arbStateMtx lock %i", m);
		}

		arbState = s;
		if (s != arbSystemState.state) {
			psInitPublish(msg, ARB_STATE);
			msg.bytePayload.value = s;
			NewBrokerMessage(&msg);
		}

		LogRoutine("State %s", arbStateNames[s]);

		m = pthread_mutex_unlock(&arbStateMtx);
		if (m != 0)
		{
			LogError("arbStateMtx unlock %i", m);
		}
		//end critical section
	}
}
//request change of Bug system state
void RequestBugState(BugState_enum s)
{
	LogRoutine("arb: bug state %s", bugStateNames[s]);
	psMessage_t msg;
	psInitPublish(msg, BUG_STATE);
	msg.bytePayload.value = s;
	NewBrokerMessage(&msg);
}
