/*
 ============================================================================
gd Name        : Broker.c
 Author      : Martin
 Version     :
 Copyright   : (c) 2013 Martin Lane-Smith
 Description : Linux PubSub broker
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "fidobrain.h"
#include "RobotTypes.h"
#include "Blackboard.h"
#include "broker.h"
#include "SysLog.h"

//broker structures
//input queue
BrokerQueue_t brokerQueue = {NULL, NULL,
		PTHREAD_MUTEX_INITIALIZER,
		PTHREAD_COND_INITIALIZER
};

void *BrokerInputThread(void *args);

int BrokerInit()
{
	int reply;
	pthread_t inputThread;

	//initialize the broker queues
	BrokerQueueInit(100);

	//create thread to receive messages from the broker pipe/queue
	int s = pthread_create(&inputThread, NULL, BrokerInputThread, NULL);
	if (s != 0)
	{
		LogError("input pthread_create");
		return s;
	}
	return 0;
}

//processes messages from the broker queue
void *BrokerInputThread(void *args)
{
	while(1)
	{
		//process messages off the broker queue
		psMessage_t *msg = GetMessage(&brokerQueue);

		AdjustMessageLength(msg);

		if (msg->header.messageType != SYSLOG_MSG &&
				msg->header.messageType != BBBLOG_MSG
//				&& blackboard->brokerDebug
				)
//			LogRoutine("%s", psLongMsgNames[msg->header.messageType]);
			DEBUG_PRINT("Broker: %s\n", psLongMsgNames[msg->header.messageType]);

		switch(psDefaultTopics[msg->header.messageType])
		{
		default:
			break;
		case LOG_TOPIC:              //SysLog
			LogProcessMessage(msg);
			SerialBrokerProcessMessage(msg);
			break;
		case BBB_LOG_TOPIC:
			LogProcessMessage(msg);
			break;
		case ANNOUNCEMENTS_TOPIC:    //Common channel
			BlackboardProcessMessage(msg);
			SerialBrokerProcessMessage(msg);
			ResponderProcessMessage(msg);
			BehaviorProcessMessage(msg);
			break;
		case CONFIG_TOPIC:           //To send config data to APP/BBB
		case STATS_TOPIC:            //To send stats to APP
//		case PIC_ACTION_TOPIC:       //commands to PIC
			SerialBrokerProcessMessage(msg);
			break;
		case MOT_ACTION_TOPIC:       //Move commands to BBB
			BehaviorProcessMessage(msg);
			break;
		case NAV_REPORT_TOPIC:       //Fused Odometry & Prox Reports
		case SYS_REPORT_TOPIC:       //Reports to APP
			BlackboardProcessMessage(msg);
			SerialBrokerProcessMessage(msg);
			BehaviorProcessMessage(msg);
			break;
		case NAV_ACTION_TOPIC:       //Navigator action
		case RAW_ODO_TOPIC:          //Raw Odometry
		case RAW_NAV_TOPIC:          //Raw compass PIC->BBB
			NavigatorProcessMessage(msg);
			break;
		case RAW_PROX_TOPIC:	    //Raw Prox reports PIC->BBB
		case SCAN_ACTION_TOPIC:
//			ScannerProcessMessage(msg);
			break;
		case TICK_1S_TOPIC:          //1 second ticks
			BlackboardProcessMessage(msg);
			BehaviorProcessMessage(msg);
			break;
		}
		DoneWithMessage(msg);
	}
	return 0;
}

