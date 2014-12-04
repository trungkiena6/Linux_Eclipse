/*
 ============================================================================
 Name        : PrintLogLinux.c
 Author      : Martin
 Version     :
 Copyright   : (c) 2013 Martin Lane-Smith
 Description : Print log messages to stdout/file
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "RobotTypes.h"
#include "SysLog.h"

//print to an open stream - all processes
void PrintLogMessage(FILE *f, psMessage_t *msg)
{
	uint8_t severityCode;
	char *logText;
	char *severity;
	char *source;

	const time_t now = time(NULL);
	struct tm *timestruct = localtime(&now);

	switch(msg->header.messageType)
	{
	case SYSLOG_MSG:
		severityCode = msg->logPayload.severity;
		logText = msg->logPayload.text;
		msg->logPayload.text[PS_MAX_LOG_TEXT-1] = '\0';
		switch (msg->header.source) {
		case SOURCE_UNKNOWN:
		default:
			source = "???";
			break;
		case SOURCE_PIC:
			source = "PIC";
			break;
		case SOURCE_BBB:
			source = "bbb";
			break;
		case SOURCE_APP:
			source = "APP";
			break;
		}
		break;
	case BBBLOG_MSG:
		severityCode = msg->bbbLogPayload.severity;
		logText = msg->bbbLogPayload.text;
		msg->bbbLogPayload.text[BBB_MAX_LOG_TEXT-1] = '\0';
		source = msg->bbbLogPayload.file;
		break;
	}

	switch (severityCode) {
	case SYSLOG_ROUTINE:
	default:
		severity = "R";
		break;
	case SYSLOG_INFO:
		severity = "I";
		break;
	case SYSLOG_WARNING:
		severity = "W";
		break;
	case SYSLOG_ERROR:
		severity = "E";
		break;
	case SYSLOG_FAILURE:
		severity = "F";
		break;
	}

	fprintf(f, "%02i:%02i:%02i %s@%s: %s\n",
			timestruct->tm_hour, timestruct->tm_min, timestruct->tm_sec,
			severity, source, logText);
}
