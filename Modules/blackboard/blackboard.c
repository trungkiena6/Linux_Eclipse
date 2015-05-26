//
//  Blackboard.c
//  Hex
//
//  Created by Martin Lane-Smith on 7/2/14.
//  Copyright (c) 2014 Martin Lane-Smith. All rights reserved.
//

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "SoftwareProfile.h"
#include "PubSubData.h"
#include "pubsub/pubsub.h"
#include "syslog/syslog.h"

#include "blackboard.h"
#include "blackboardData.h"

FILE *bbDebugFile;

#ifdef BLACKBOARD_DEBUG
#define DEBUGPRINT(...) fprintf(stdout, __VA_ARGS__);fprintf(bbDebugFile, __VA_ARGS__);fflush(bbDebugFile);
#else
#define DEBUGPRINT(...) fprintf(bbDebugFile, __VA_ARGS__);fflush(bbDebugFile);
#endif

#define ERRORPRINT(...) fprintf(stdout, __VA_ARGS__);fprintf(bbDebugFile, __VA_ARGS__);fflush(bbDebugFile);


char *eventNames[] = NOTIFICATION_NAMES;
char *stateCommandNames[] = USER_COMMAND_NAMES;
char *powerStateNames[] = POWER_STATE_NAMES;
char *ovmCommandNames[] = OVERMIND_COMMAND_NAMES;
char *arbStateNames[]	= ARB_STATE_NAMES;

//---------------------------------------Blackboard Data Implementation--------------------------------

RawBlackboardData_t *rawBlackboardData[PS_MSG_COUNT];

//options
#define optionmacro(name, var, min, max, def) int var = def;
#include "options.h"
#undef optionmacro

//Settings
#define settingmacro(name, var, min, max, def) float var = def;
#include "settings.h"
#undef settingmacro

//-----------------------------------------Blackboard Memory Management--------------

RawBlackboardData_t *bbFreelist = NULL;					//common free list
pthread_mutex_t	bbFreeMtx = PTHREAD_MUTEX_INITIALIZER;	//freelist mutex
BrokerQueue_t blackboardQueue = BROKER_Q_INITIALIZER;

//private
int bbAddToMsgList(RawBlackboardData_t *e);
RawBlackboardData_t *bbNewEntry();
void bbAddToFreelist(RawBlackboardData_t *e);
void _bbAddToFreelist(RawBlackboardData_t *e);			//called from critical section
RawBlackboardData_t *bbGetFreeEntry();

#define BB_MEMORY_PRELOAD 	(30 * 60)
//---------------------------------------------------
//threads to manage blackboard
void *BlackboardThread(void *arg);
void *BlackboardMessageThread(void *arg);

pthread_t BlackboardInit()
{
	bbDebugFile = fopen("/root/logfiles/blackboard.log", "w");

	int i;
	//initialize the message store
	for (i=0; i<PS_MSG_COUNT; i++)
	{
		rawBlackboardData[i] = (RawBlackboardData_t *) 0;
	}
	//preload the freelist
	for (i=0; i<BB_MEMORY_PRELOAD; i++)
	{
		RawBlackboardData_t *entry = bbNewEntry();
		if (entry == NULL) return -1;
		_bbAddToFreelist(entry);
	}

	//create blackboard thread
	pthread_t thread;
	int result = pthread_create(&thread, NULL, BlackboardThread, NULL);
	if (result != 0)
	{
		ERRORPRINT("No Blackboard thread\n");
		return -1;
	}

	result = pthread_create(&thread, NULL, BlackboardMessageThread, NULL);
	if (result != 0)
	{
		ERRORPRINT("No BlackboardMessageThread thread\n");
		return -1;
	}

	return thread;
}

//--------------------------------------------------Blackboard thread
void *BlackboardThread(void *arg)
{
	int messageType = 0;
	RawBlackboardData_t *d, *parent;
	struct tm current, last;
	time_t now;
	bool discard;
	while (1)
	{
		sleep (1);
		parent = NULL;
		now = time(NULL);

		//garbage collect one message type
		//critical section
		int s = pthread_mutex_lock(&bbFreeMtx);
		if (s != 0)
		{
			ERRORPRINT("BB Thread:  mutex lock %i\n", s);
		}

		d = rawBlackboardData[messageType];
		if (d)
		{
			localtime_r(&d->timeStamp, &last);
		}
		while (d)
		{
			discard = false;
			localtime_r(&d->timeStamp, &current);

			int age = now - d->timeStamp;
			if (age < 10)
			{
				//keep all
			}
			else if (age < 60)
			{
				//keep 1 per second, for a minute
				if (last.tm_sec == current.tm_sec && last.tm_min == current.tm_min && last.tm_hour == current.tm_hour)
				{
					discard = true;
				}
			}
			else if (age < 3600)
			{
				//keep 1 per minute, for an hour
				if (last.tm_min == current.tm_min && last.tm_hour == current.tm_hour)
				{
					discard = true;
				}
			}
			else if (age < 3600 * 24)
			{
				//keep one per hour, for a day
				if (last.tm_hour == current.tm_hour)
				{
					discard = true;
				}
			}
			else
			{
				//discard all over 24 hours
				discard = true;
			}

			if (discard & d->accessCount == 0)
			{
				if (parent)
				{
					parent->next = d->next;
				}
				else
				{
					rawBlackboardData[messageType] = d->next;
				}
				_bbAddToFreelist(d);
			}
			else
			{
				parent = d;
				localtime_r(&d->timeStamp, &last);
			}
			d = (RawBlackboardData_t *) d->next;
		}

		s = pthread_mutex_unlock(&bbFreeMtx);
		if (s != 0)
		{
			ERRORPRINT("BB Thread:  mutex unlock %i\n", s);
		}
		//end critical section
	}
}

//--------------------------------------------------update Blackboard

void BlackboardProcessMessage(psMessage_t *msg)
{
	CopyMessageToQ(&blackboardQueue, msg);
}

void *BlackboardMessageThread(void *arg)
{
	DEBUGPRINT("Blackboard message thread started\n");

	while (1)
	{
		bool saveMessage = false;

		psMessage_t *msg = GetNextMessage(&blackboardQueue);
		switch (msg->header.messageType)
		{
		case TICK_1S:
		{
			RawBlackboardData_t *e = rawBlackboardData[TICK_1S];
			if (e)
			{
				//update existing tick message
				memcpy(&e->message, msg, sizeof(psMessage_t));
				e->timeStamp = time(NULL);
			}
			else
			{
				saveMessage = true;
			}
		}
		break;
		case PING_RESPONSE:
		case PING_MSG:
		case GEN_STATS:
			break;
		case BATTERY:
		case ENVIRONMENT:
			saveMessage = true;
			break;
		case NOTIFICATION:
			if (msg->nameIntPayload.value > 0)
			{
				DEBUGPRINT("Blackboard: Notified: %s\n", msg->nameIntPayload.name);
			}
			else
			{
				DEBUGPRINT("Blackboard: Cancelled: %s\n", msg->nameIntPayload.name);
			}
			saveMessage = true;
			break;
		case NEW_SETTING:
			DEBUGPRINT("Blackboard: New Setting: %s\n", msg->nameFloatPayload.name);
#define settingmacro(n, var, min, max, def) if (strncmp(n,msg->nameFloatPayload.name,PS_NAME_LENGTH) == 0)\
		var = msg->nameFloatPayload.value;
#include "settings.h"
#undef settingmacro
			break;

		case SET_OPTION:
			DEBUGPRINT("Blackboard: Set Option: %s\n", msg->nameIntPayload.name);
#define optionmacro(n, var, min, max, def) if (strncmp(n,msg->nameIntPayload.name,PS_NAME_LENGTH) == 0)\
		var = msg->nameIntPayload.value;
#include "options.h"
#undef optionmacro
			break;

		default:
			DEBUGPRINT("Blackboard: %s\n", psLongMsgNames[msg->header.messageType]);
			break;
		}

		if (saveMessage)
		{
			RawBlackboardData_t *e = bbGetFreeEntry();

			if (!e)
			{
				ERRORPRINT("Blackboard: No memory\n");
			}
			else
			{
				memcpy(&e->message, msg, sizeof(psMessage_t));

				e->timeStamp = time(NULL);
				e->accessCount = 0;

				if (bbAddToMsgList(e) < 0)
				{
					//failed
					ERRORPRINT("Blackboard: Add failed\n");
					bbAddToFreelist(e);
				}
			}
		}
		DoneWithMessage(msg);
	}
}
//-------------------------------------Access to data
//get a pointer to a message - current or 'relativeTime' seconds in the past
psMessage_t *bbGetMessage(psMessageType_enum messageType, time_t timespec)
{
	RawBlackboardData_t *d ;
	time_t timeNow, timeRequired;

	timeNow = time(NULL);

	if (timespec > 0)
	{
		//absolute time
		timeRequired = timespec;
	} else if (timespec <= 0 && timeNow > 0)
	{
		//relative time
		timeRequired = time(NULL) + timespec;
	}
	else
	{
		//fallback
		timeRequired = 0;
	}

	if (messageType >= PS_MSG_COUNT || messageType <= 0)
	{
		ERRORPRINT("Blackboard: bad message type: %i\n", messageType);
		return NULL;
	}

	//critical section
	int s = pthread_mutex_lock(&bbFreeMtx);
	if (s != 0)
	{
		ERRORPRINT("Blackboard:  mutex lock %i\n", s);
	}

	d = rawBlackboardData[messageType];

	while (d && d->timeStamp > timeRequired)
	{
		d = (RawBlackboardData_t *) d->next;
	}
	if (d)
	{
		d->accessCount++;
	}
	s = pthread_mutex_unlock(&bbFreeMtx);
	if (s != 0)
	{
		ERRORPRINT("Blackboard:  mutex unlock %i\n", s);
	}
	//end critical section

	return (psMessage_t*) d;
}

//get timestamp
time_t bbGetTimeStamp(psMessage_t *msg)
{
	RawBlackboardData_t *d = (RawBlackboardData_t*) msg;
	return d->timeStamp;
}

//release the message for GC
void bbDoneWithData(psMessage_t *msg)
{
	RawBlackboardData_t *d = (RawBlackboardData_t*) msg;
	d->accessCount--;
}

//notifications
NotificationMask_t bbGetActiveNotifications()
{
	RawBlackboardData_t *latestTick = rawBlackboardData[TICK_1S];

	if (latestTick) return latestTick->message.tickPayload.activeNotifications;
	else return 0;
}

bool bbIsNotificationActive(Notification_enum e)
{
	NotificationMask_t mask = (NotificationMask_t) 0x1 << e;
	return (mask & bbGetActiveNotifications());
}

//-------------------------------------Memory Management
//allocate a data entry
RawBlackboardData_t *bbNewEntry()
{
	RawBlackboardData_t *e = calloc(1, sizeof(RawBlackboardData_t));
	if (e == NULL)
	{
		ERRORPRINT("Blackboard: no memory\n");
	}
	return e;
}
//add to the front of a data list
int bbAddToMsgList(RawBlackboardData_t *e)
{
	int msgType = e->message.header.messageType;

	if (msgType >= PS_MSG_COUNT)
	{
		ERRORPRINT("Blackboard: bad message type: %i\n", msgType);
		return -1;
	}

	//critical section
	int s = pthread_mutex_lock(&bbFreeMtx);
	if (s != 0)
	{
		ERRORPRINT("Blackboard:  mutex lock %i\n", s);
	}

	e->next = rawBlackboardData[msgType];
	rawBlackboardData[msgType] = e;

	s = pthread_mutex_unlock(&bbFreeMtx);
	if (s != 0)
	{
		ERRORPRINT("Blackboard:  mutex unlock %i\n", s);
	}
	//end critical section
	return 0;
}

//free a used entry
void bbAddToFreelist(RawBlackboardData_t *e)
{
	//critical section
	int s = pthread_mutex_lock(&bbFreeMtx);
	if (s != 0)
	{
		ERRORPRINT("Blackboard:  mutex lock %i\n", s);
	}

	_bbAddToFreelist(e);

	s = pthread_mutex_unlock(&bbFreeMtx);
	if (s != 0)
	{
		ERRORPRINT("Blackboard:  mutex unlock %i\n", s);
	}
	//end critical section
}
void _bbAddToFreelist(RawBlackboardData_t *e)
{
	e->next = bbFreelist;
	bbFreelist = e;
}
//get next free entry
RawBlackboardData_t *bbGetFreeEntry()
{
	RawBlackboardData_t *e;

	//critical section
	int s = pthread_mutex_lock(&bbFreeMtx);
	if (s != 0)
	{
		ERRORPRINT("Blackboard:  mutex lock %i\n", s);
	}

	if (bbFreelist)
	{
		e = bbFreelist;
		bbFreelist = e->next;
	}
	else
	{
		e = bbNewEntry();
	}

	s = pthread_mutex_unlock(&bbFreeMtx);
	if (s != 0)
	{
		ERRORPRINT("Blackboard:  mutex unlock %i\n", s);
	}
	//end critical section

	return e;
}


