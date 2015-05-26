/*
 ============================================================================
gd Name        : Broker.c
 Author      : Martin
 Version     :
 Copyright   : (c) 2013 Martin Lane-Smith
 Description : Linux PubSub broker
 ============================================================================
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "PubSubData.h"
#include "Helpers.h"
#include "pubsub/pubsub.h"
#include "blackboard/blackboard.h"
#include "behavior/behavior.h"
#include "navigator/navigator.h"
#include "scanner/scanner.h"
#include "autopilot/autopilot.h"
#include "syslog/syslog.h"
#include "SoftwareProfile.h"

#include "brokerQ.h"
#include "broker_debug.h"

FILE *psDebugFile;

bool PubSubBrokerReady = false;

//broker structures
//input queue
BrokerQueue_t brokerQueue = BROKER_Q_INITIALIZER;

void *BrokerInputThread(void *args);

pthread_t PubSubInit()
{
	int reply;
	pthread_t inputThread;

	psDebugFile = fopen("/root/logfiles/broker.log", "w");

	//create thread to receive messages from the broker pipe/queue
	int s = pthread_create(&inputThread, NULL, BrokerInputThread, NULL);
	if (s != 0)
	{
		ERRORPRINT("input pthread_create\n");
		return s;
	}
	return s;
}

//processes messages from the broker queue
void *BrokerInputThread(void *args)
{
	PubSubBrokerReady = true;

	while(1)
	{
		//process messages off the broker queue
		psMessage_t *msg = GetNextMessage(&brokerQueue);

		AdjustMessageLength(msg);

		if (msg->header.messageType != SYSLOG_MSG &&
				msg->header.messageType != BBBLOG_MSG)
			DEBUGPRINT("Broker: %s\n", psLongMsgNames[msg->header.messageType]);

		RouteMessage(msg);

		DoneWithMessage(msg);
	}
}

//pass message to appropriate modules
void RouteMessage(psMessage_t *msg)
{
	AdjustMessageLength(msg);

	switch(psDefaultTopics[msg->header.messageType])
	{
	default:
		break;
	case LOG_TOPIC:              //SysLog
		LogProcessMessage(msg);
		SerialBrokerProcessMessage(msg);
		break;
	case OVM_LOG_TOPIC:
		LogProcessMessage(msg);
		break;
	case ANNOUNCEMENTS_TOPIC:    //Common channel
		AutopilotProcessMessage(msg);
		BlackboardProcessMessage(msg);
		ResponderProcessMessage(msg);
		BehaviorProcessMessage(msg);
		SerialBrokerProcessMessage(msg);
		break;
	case CONFIG_TOPIC:           //To send config data to APP/BBB
	case STATS_TOPIC:            //To send stats to APP
	case MOT_ACTION_TOPIC:
		SerialBrokerProcessMessage(msg);
		break;
	case NAV_REPORT_TOPIC:       //Fused Odometry & Prox Reports
		AutopilotProcessMessage(msg);
		BehaviorProcessMessage(msg);
		BlackboardProcessMessage(msg);
		break;
	case SYS_REPORT_TOPIC:       //Reports to APP
		BlackboardProcessMessage(msg);
		SerialBrokerProcessMessage(msg);
		break;
	case RAW_NAV_TOPIC:          //Raw compass & GPS
	case NAV_ACTION_TOPIC:       //Navigator action
		NavigatorProcessMessage(msg);
		BlackboardProcessMessage(msg);
		break;
	case RAW_ODO_TOPIC:          //Raw Odometry
		AutopilotProcessMessage(msg);
		NavigatorProcessMessage(msg);
		BlackboardProcessMessage(msg);
		break;
	case RAW_PROX_TOPIC:	    //Raw Prox reports
#ifdef SCANNER
		ScannerProcessMessage(msg);
#endif
		BlackboardProcessMessage(msg);
		break;
	case ACTION_TOPIC:
		AutopilotProcessMessage(msg);
#ifdef SCANNER
		ScannerProcessMessage(msg);
#endif
		BlackboardProcessMessage(msg);
		break;
	case TICK_1S_TOPIC:          //1 second ticks
		AutopilotProcessMessage(msg);
		NavigatorProcessMessage(msg);
		BlackboardProcessMessage(msg);
		BehaviorProcessMessage(msg);
		break;
	}
}

//notifications
void Notify(Notification_enum e)
{
	if (e < 0 || e >= NOTIFICATION_COUNT) return;

	//if (isNotificationActive(e)) return;

	DEBUGPRINT("Notify: %s\n", psNotificationNames[e] );

	BrokerQueueEntry_t *qe = GetFreeEntry();
	if (qe)
	{
		qe->msg.header.messageType = NOTIFICATION;
		qe->msg.header.source = OVERMIND;
		qe->msg.intPayload.value = e;
		AdjustMessageLength(&qe->msg);
		AppendQueueEntry(&brokerQueue, qe);
	}
	else
	{
		ERRORPRINT("Notify: no memory\n");
	}
}

void CancelNotification(Notification_enum e)
{
	if (e < 0 || e >= NOTIFICATION_COUNT) return;

	//if (!isNotificationActive(e)) return;

	DEBUGPRINT("Cancel: %s\n", psNotificationNames[e] );

	BrokerQueueEntry_t *qe = GetFreeEntry();
	if (qe)
	{
		qe->msg.header.messageType = NOTIFICATION;
		qe->msg.header.source = OVERMIND;
		qe->msg.intPayload.value = -e;
		AdjustMessageLength(&qe->msg);
		AppendQueueEntry(&brokerQueue, qe);
	}
	else
	{
		ERRORPRINT("Notify: no memory\n");
	}
}
