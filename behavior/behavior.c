//
//  behavior.c
//
//  Created by Martin Lane-Smith on 6/14/14.
//  Copyright (c) 2014 Martin Lane-Smith. All rights reserved.
//
// Root threads of the scripted behavior subsystem

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "lua.h"

#include "RobotTypes.h"
#include "broker.h"
#include "Blackboard.h"
#include "fidobrain.h"
#include "SysLog.h"

pthread_mutex_t	luaMtx = PTHREAD_MUTEX_INITIALIZER;

void *ScriptThread(void *arg);

int BehaviorInit()
{
	pthread_t thread;

	int result = InitScriptingSystem();
	if (result == 0)
	{
		LogRoutine("Script system initialized");
	}
	else
	{
		return -1;
	}

	int s = pthread_create(&thread, NULL, ScriptThread, NULL);
	if (s != 0)
	{
		LogError("ScriptThread create - %i", s);
		return -1;
	}
	return 0;
}

//thread to receive messages and update lua globals
void BehaviorProcessMessage(psMessage_t *msg)
{
	int reply;

//	DEBUG_PRINT("Behavior: %s\n", psLongMsgNames[msg->header.messageType]);

	//critical section
	int s = pthread_mutex_lock(&luaMtx);
	if (s != 0)
	{
		LogError("overmind: lua mutex lock %i", s);
	}

	ScriptProcessMessage(msg);

	s = pthread_mutex_unlock(&luaMtx);
	if (s != 0)
	{
		LogError("overmind: lua mutex unlock %i", s);
	}
	//end critical section
}

//thread to run scripts periodically
void *ScriptThread(void *arg)
{
	struct timespec requested_time = {0,0};
	struct timespec remaining;

	LogRoutine("Script thread started");

	while (1)
	{
		//critical section
		int s = pthread_mutex_lock(&luaMtx);
		if (s != 0)
		{
			LogError("overmind: lua mutex lock %i", s);
		}

		InvokeScript("main");

		s = pthread_mutex_unlock(&luaMtx);
		if (s != 0)
		{
			LogError("overmind: lua mutex unlock %i", s);
		}
		//end critical section

		//delay
		requested_time.tv_nsec = behLoopDelay * 1000000;
		nanosleep(&requested_time, &remaining);
	}
	return 0;
}

