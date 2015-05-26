//
//  behavior.c
//
//  Created by Martin Lane-Smith on 6/14/14.
//  Copyright (c) 2014 Martin Lane-Smith. All rights reserved.
//
// Root threads of the scripted behavior subsystem

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include <time.h>
#include <string.h>
#include <pthread.h>

#include "lua.h"

#include "PubSubData.h"
#include "pubsub/pubsub.h"
#include "blackboard/blackboard.h"
#include "behavior/behavior.h"
#include "behavior/behaviorDebug.h"
#include "syslog/syslog.h"

FILE *behDebugFile;

pthread_mutex_t	luaMtx = PTHREAD_MUTEX_INITIALIZER;

BrokerQueue_t behaviorQueue = BROKER_Q_INITIALIZER;

void *ScriptThread(void *arg);
void *BehaviorMessageThread(void *arg);

pthread_t BehaviorInit()
{
	pthread_t thread;

	behDebugFile = fopen("/root/logfiles/behavior.log", "w");

	int result = InitScriptingSystem();
	if (result == 0)
	{
		DEBUGPRINT("Script system initialized\n");
	}
	else
	{
		ERRORPRINT("Script system init fail: %i\n", result);
		return -1;
	}

	int s = pthread_create(&thread, NULL, ScriptThread, NULL);
	if (s != 0)
	{
		ERRORPRINT("ScriptThread create - %i\n", s);
		return -1;
	}

	s = pthread_create(&thread, NULL, BehaviorMessageThread, NULL);
	if (s != 0)
	{
		ERRORPRINT("BehaviorMessageThread create - %i\n", s);
		return -1;
	}

	return s;
}

void BehaviorProcessMessage(psMessage_t *msg)
{
	CopyMessageToQ(&behaviorQueue, msg);
}
//thread to receive messages and update lua globals
void *BehaviorMessageThread(void *arg)
{
	DEBUGPRINT("Behavior message thread started\n");

	while (1)
	{
		psMessage_t *msg = GetNextMessage(&behaviorQueue);

		//	DEBUGPRINT("BT msg: %s\n", psLongMsgNames[msg->header.messageType]);

		//critical section
		int s = pthread_mutex_lock(&luaMtx);
		if (s != 0)
		{
			ERRORPRINT("BT: lua mutex lock %i\n", s);
		}

		ScriptProcessMessage(msg);	//update lua globals as necessary

		s = pthread_mutex_unlock(&luaMtx);
		if (s != 0)
		{
			ERRORPRINT("BT: lua mutex unlock %i\n", s);
		}
		//end critical section

		DoneWithMessage(msg);
	}
	return 0;
}

//thread to run scripts periodically
void *ScriptThread(void *arg)
{
	struct timespec requested_time = {0,0};
	struct timespec remaining;

	DEBUGPRINT("Script thread started\n");

	while (1)
	{
		//critical section
		int s = pthread_mutex_lock(&luaMtx);
		if (s != 0)
		{
			ERRORPRINT("overmind: lua mutex lock %i\n", s);
		}

		InvokeScript("update");

		s = pthread_mutex_unlock(&luaMtx);
		if (s != 0)
		{
			ERRORPRINT("overmind: lua mutex unlock %i\n", s);
		}
		//end critical section

		//delay
		requested_time.tv_nsec = behLoopDelay * 1000000;
		nanosleep(&requested_time, &remaining);
	}
}

//report available scripts
int ReportAvailableScripts()
{
	//critical section
	int s = pthread_mutex_lock(&luaMtx);
	if (s != 0)
	{
		ERRORPRINT("overmind: lua mutex lock %i\n", s);
	}

	int messageCount = AvailableScripts();

	s = pthread_mutex_unlock(&luaMtx);
	if (s != 0)
	{
		ERRORPRINT("overmind: lua mutex unlock %i\n", s);
	}
	//end critical section
	return messageCount;
}
