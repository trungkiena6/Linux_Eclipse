/* 
 * File:   SysLog.h
 * Author: martin
 *
 * Created on August 7, 2013, 8:19 PM
 */

#ifndef SYSLOG_H
#define	SYSLOG_H

#include <stdio.h>
#include "PubSubData.h"
#include "pubsub/pubsub.h"

enum {
	LOG_ALL, LOG_INFO_PLUS, LOG_WARN_PLUS, LOG_NONE
};

void _LogMessage(SysLogSeverity_enum _severity, const char *_message, const char *_file);

#define SysLog( s, ...) {char tmp[BBB_MAX_LOG_TEXT];\
    snprintf(tmp,BBB_MAX_LOG_TEXT,__VA_ARGS__);\
    tmp[BBB_MAX_LOG_TEXT-1] = 0;\
    _LogMessage(s, tmp, __BASE_FILE__);}


#if (SYSLOG_LEVEL == LOG_ALL) || (LOG_TO_SERIAL == LOG_ALL)
#define LogRoutine( ...) {SysLog( SYSLOG_ROUTINE, __VA_ARGS__);}
#else
    #define LogRoutine( m, ...)
#endif

    #if (SYSLOG_LEVEL <= LOG_INFO_PLUS) || (LOG_TO_SERIAL <= LOG_INFO_PLUS)
#define LogInfo( ...) {SysLog( SYSLOG_INFO, __VA_ARGS__);}
#else
    #define LogInfo( m, ...)
#endif

#if ((SYSLOG_LEVEL <= LOG_WARN_PLUS) || (LOG_TO_SERIAL <= LOG_WARN_PLUS))
#define LogWarning( ...) {SysLog( SYSLOG_WARNING, __VA_ARGS__);}
#else
    #define LogWarning( m, ...)
#endif

#define LogError(...) {SysLog(SYSLOG_ERROR, __VA_ARGS__);}

//syslog
extern FILE *logFile;

pthread_t SysLogInit();

void LogProcessMessage(psMessage_t *msg);				//process for logging

void PrintLogMessage(FILE *f, psMessage_t *msg);

#endif	/* SYSLOG_H */
