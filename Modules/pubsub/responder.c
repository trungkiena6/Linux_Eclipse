/*
 * responder.c
 *
 * Responds to ping messages from the APP
 *
 *  Created on: Jul 27, 2014
 *      Author: martin
 */

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "PubSubData.h"
#include "Helpers.h"
#include "pubsub/pubsub.h"
#include "blackboard/blackboard.h"
#include "behavior/behavior.h"
#include "syslog/syslog.h"

#include "brokerQ.h"
#include "broker_debug.h"

BrokerQueue_t responderQueue = BROKER_Q_INITIALIZER;

void *ResponderMessageThread(void *arg);
void sendOptionConfig(char *name, int var, int minV, int maxV);
void sendSettingConfig(char *name, float var, float minV, float maxV);

int configCount;
int startFlag = 1;

pthread_t ResponderInit()
{
	pthread_t thread;
	int s = pthread_create(&thread, NULL, ResponderMessageThread, NULL);
	if (s != 0)
	{
		ERRORPRINT("ResponderMessageThread create - %i\n", s);
		return -1;
	}
	return thread;
}

void ResponderProcessMessage(psMessage_t *msg)
{
	CopyMessageToQ(&responderQueue, msg);
}

//thread to receive messages and respond
void *ResponderMessageThread(void *arg)
{
	DEBUGPRINT("Responder message thread started\n");

	while (1)
	{
		psMessage_t *msg = GetNextMessage(&responderQueue);

		//check message for response requirement
		switch (msg->header.messageType)
		{
		case CONFIG:
			if (msg->bytePayload.value == OVERMIND)
			{
				DEBUGPRINT("Send Config msg\n");
				configCount = 0;
#define optionmacro(name, var, minV, maxV, def) sendOptionConfig(name, var, minV, maxV);
#include "options.h"
#undef optionmacro
				sleep(1);
#define settingmacro(name, var, minV, maxV, def) sendSettingConfig(name, var, minV, maxV);
#include "settings.h"
#undef settingmacro
				sleep(1);
				configCount += ReportAvailableScripts();

				{
					psMessage_t msg;
					psInitPublish(msg, CONFIG_DONE);
					msg.bytePayload.value = configCount;
					RouteMessage(&msg);
				}
				DEBUGPRINT("Config Done\n");
			}
			break;

		case PING_MSG:
		{
			DEBUGPRINT("Ping msg\n");
			psMessage_t msg2;
			psInitPublish(msg2, PING_RESPONSE);
			strcpy(msg2.pingResponsePayload.subsystem, "OVM");

			msg2.pingResponsePayload.startup = startFlag;
			msg2.pingResponsePayload.name[0] = '\0';;
			startFlag = 0;
			RouteMessage(&msg2);
		}
		break;
		default:
			//ignore anything else
			break;
		}

		DoneWithMessage(msg);
	}
	return 0;
}

void sendOptionConfig(char *name, int var, int minV, int maxV) {
	struct timespec request, remain;
	psMessage_t msg;
	psInitPublish(msg, OPTION);
	strncpy(msg.name3IntPayload.name, name, PS_NAME_LENGTH);
	msg.name3IntPayload.value = var;
	msg.name3IntPayload.min = minV;
	msg.name3IntPayload.max = maxV;
	RouteMessage(&msg);
	configCount++;
}

void sendSettingConfig(char *name, float var, float minV, float maxV) {
	struct timespec request, remain;
	psMessage_t msg;
	psInitPublish(msg, SETTING);
	strncpy(msg.name3FloatPayload.name, name, PS_NAME_LENGTH);
	msg.name3FloatPayload.value = var;
	msg.name3FloatPayload.min = minV;
	msg.name3FloatPayload.max = maxV;
	RouteMessage(&msg);
	configCount++;
}
