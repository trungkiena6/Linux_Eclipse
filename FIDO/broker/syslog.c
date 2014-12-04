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

#include "RobotTypes.h"
#include "broker.h"
#include "Blackboard.h"
#include "SysLog.h"
#include "fidobrain.h"

//Sys Log queue
BrokerQueue_t logQueue = {NULL, NULL,
		PTHREAD_MUTEX_INITIALIZER,
		PTHREAD_COND_INITIALIZER
};

void *LoggingThread(void *arg);
//print to an open stream
void PrintLogMessage(FILE *f, psMessage_t *msg);

FILE *logFile;

//open logging file
int SysLogInit()
{
#define LOGFILENAME "/root/logfiles/overmind.log"
		logFile = fopen(LOGFILENAME, "w");
		if (logFile == NULL)
		{
			logFile = stderr;
            printf("syslog: Failed to open %s\n", LOGFILENAME);
		}
        else {
            printf("syslog: Logfile opened on %s\n", LOGFILENAME);
        }

    //start log print threads
    pthread_t thread;

    int s = pthread_create(&thread, NULL, LoggingThread, NULL);
    if (s != 0)
    {
        LogError("pthread_create");
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
		psMessage_t *msg = GetMessage(&logQueue);
		uint8_t severity;

		if (msg->header.messageType == BBBLOG_MSG)
		{
			severity = msg->bbbLogPayload.severity;
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
	int forward = 0;

	switch(msg->header.messageType)
	{
	case SYSLOG_MSG:		//message from other parts of the system (eg PIC)
		if (msg->header.source != SOURCE_BBB)
			AddMessage(&logQueue, msg);
		break;
	case BBBLOG_MSG:		//BBB originated messages
		AddMessage(&logQueue, msg);
		//check whether we need to forward a copy to PIC/APP
	    switch (msg->bbbLogPayload.severity) {
	        case SYSLOG_ROUTINE:
	            if (logRoutine) forward = 1;
	            break;
	        case SYSLOG_INFO:
	            if (logInfo) forward = 1;
	            break;
	        case SYSLOG_WARNING:
	            if (logWarning) forward = 1;
	            break;
//	        case SYSLOG_ERROR:
//	        case SYSLOG_FAILURE:
	        default:
	        	forward = 1;
	            break;
	    }
	    if (forward)
	    {
	    	//make a SYSLOG_MSG
	    	psMessage_t newMsg;
	    	psInitPublish(newMsg, SYSLOG_MSG);
	    	newMsg.header.source = SOURCE_BBB;
	    	newMsg.logPayload.severity = msg->bbbLogPayload.severity;
	    	snprintf(newMsg.logPayload.text, PS_MAX_LOG_TEXT-1, "%s: %s",
	    			msg->bbbLogPayload.file, msg->bbbLogPayload.text);
	    	newMsg.logPayload.text[PS_MAX_LOG_TEXT-1] = '\0';
	    	newMsg.header.length = strlen(newMsg.logPayload.text) + 1;
	    	SerialBrokerProcessMessage(&newMsg);
	    }
		break;
	}
}

//create new log message. called by macros in syslog.h
void _LogMessage(SysLogSeverity_enum _severity, const char *_message, const char *_file)
{
    const char *filecomponent = (strrchr(_file, '/') + 1);
    if (!filecomponent) filecomponent = _file;

    psMessage_t msg;
    psInitPublish(msg, BBBLOG_MSG);
    msg.bbbLogPayload.severity = _severity;
    strncpy(msg.bbbLogPayload.file, filecomponent, 4);
    msg.bbbLogPayload.file[4] = 0;
    strncpy(msg.bbbLogPayload.text, _message, BBB_MAX_LOG_TEXT);
    msg.bbbLogPayload.text[BBB_MAX_LOG_TEXT-1] = 0;

    int _textLen = strlen(msg.bbbLogPayload.text);
    msg.header.length = _textLen - psMessageFormatLengths[psMsgFormats[BBBLOG_MSG]];;

    //publish a copy
    if (SYSLOG_LEVEL <= _severity || LOG_TO_SERIAL <= _severity) {
    	NewBrokerMessage(&msg);
    }
}
