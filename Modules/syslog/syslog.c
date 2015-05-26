/*
 * SysLog.c
 *
 *  Created on: Sep 10, 2014
 *      Author: martin
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "PubSubData.h"
#include "pubsub/pubsub.h"
#include "blackboard/blackboard.h"
#include "syslog/syslog.h"
#include "behavior/behavior.h"

//Sys Log queue
BrokerQueue_t logQueue = BROKER_Q_INITIALIZER;

void *LoggingThread(void *arg);

//print to an open stream
void PrintLogMessage(FILE *f, psMessage_t *msg);

FILE *logFile;

//open logging file
pthread_t SysLogInit()
{
#define LOGFILENAME "/root/logfiles/syslog.log"
		logFile = fopen(LOGFILENAME, "w");
		if (logFile == NULL)
		{
			logFile = stderr;
            fprintf(stderr, "syslog: fopen(%s) fail (%s)\n", LOGFILENAME, strerror(errno));
		}
        else {
        	fprintf(stderr, "syslog: Logfile opened on %s\n", LOGFILENAME);
        }

    //start log print threads
    pthread_t thread;

    int s = pthread_create(&thread, NULL, LoggingThread, NULL);
    if (s != 0)
    {
    	fprintf(stderr, "syslog: pthread_create fail (%s)\n", strerror(errno));
        return s;
    }
	return 0;
}
//writes log messages to the logfile
void *LoggingThread(void *arg)
{
    {
    	psMessage_t msg;
    	psInitPublish(msg, BBBLOG_MSG);
    	msg.bbbLogPayload.severity = SYSLOG_ROUTINE;
    	strcpy(msg.bbbLogPayload.file, "log");
    	strcpy(msg.bbbLogPayload.text, "Logfile started");

    	PrintLogMessage(logFile, &msg);
    }
	while (1)
	{
		psMessage_t *msg = GetNextMessage(&logQueue);
		uint8_t severity;

		if (msg->header.messageType == BBBLOG_MSG)
		{
			severity = msg->bbbLogPayload.severity;

			//check whether we need to forward a copy to PIC/APP
		    switch (msg->bbbLogPayload.severity) {
		        case SYSLOG_ERROR:
		        case SYSLOG_FAILURE:
			    {
			    	//make a SYSLOG_MSG
			    	psMessage_t newMsg;
			    	psInitPublish(newMsg, SYSLOG_MSG);
			    	newMsg.header.source = OVERMIND;
			    	newMsg.logPayload.severity = msg->bbbLogPayload.severity;
			    	snprintf(newMsg.logPayload.text, PS_MAX_LOG_TEXT-1, "%s: %s",
			    			msg->bbbLogPayload.file, msg->bbbLogPayload.text);
//			    	newMsg.logPayload.text[PS_MAX_LOG_TEXT-1] = '\0';
//			    	newMsg.header.length = strlen(newMsg.logPayload.text) + 1;
			    	RouteMessage(&newMsg);
			    }
			    break;
		        default:
		            break;
		    }

		}
		else
		{
			severity = msg->logPayload.severity;
		}

		//print to logfile
		if (SYSLOG_LEVEL <= severity) {
			PrintLogMessage(logFile, msg);
			fflush(logFile);
		}
        //print a copy to stderr
        if (LOG_TO_SERIAL <= severity) {
        	PrintLogMessage(stderr, msg);
        	fflush(stderr);
        }
        DoneWithMessage(msg);
	}
	return 0;
}
//accept a message from the broker for logging
void LogProcessMessage(psMessage_t *msg)
{
	switch(msg->header.messageType)
	{
	case SYSLOG_MSG:		//message from other parts of the system (eg PIC)
		if (msg->header.source != OVERMIND)	//skip forwarded copies
			CopyMessageToQ(&logQueue, msg);
		break;
	case BBBLOG_MSG:		//BBB originated messages
		CopyMessageToQ(&logQueue, msg);
		break;
	}
}

//create new log message. called by macros in syslog.h
void _LogMessage(SysLogSeverity_enum _severity, const char *_message, const char *_file)
{
	if (SYSLOG_LEVEL <= _severity ) {
		const char *filecomponent = (strrchr(_file, '/') + 1);
		if (!filecomponent) filecomponent = _file;
		if (!filecomponent) filecomponent = "****";

		psMessage_t msg;
		psInitPublish(msg, BBBLOG_MSG);
		msg.bbbLogPayload.severity = _severity;
		strncpy(msg.bbbLogPayload.file, filecomponent, 4);
		msg.bbbLogPayload.file[4] = 0;
		strncpy(msg.bbbLogPayload.text, _message, BBB_MAX_LOG_TEXT);

		//publish a copy
		RouteMessage(&msg);
	}
}
