/*
 * PubSubLinux.c
 *
 *	PubSub Services on Linux
 *
 *  Created on: Jan 19, 2014
 *      Author: martin
 */

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include "RobotTypes.h"
#include "SysLog.h"
#include "broker.h"

//#define PSDEBUG(x) printf("%s: %s\n", processNames[process_id], x)
#define PSDEBUG(x)
char *subsystemNames[SOURCE_COUNT] = MESSAGE_SOURCE_NAMES;
char *psTopicNames[PS_TOPIC_COUNT] = PS_TOPIC_NAMES;

//#define messagemacro(enum, short name, qos, topic, payload, long name)
#define messagemacro(m,s,q,t,f,l) f,
int psMsgFormats[PS_MSG_COUNT] = {
#include "MessageList.h"
#include "BugMessageList.h"
#include "BBBMessageList.h"
};
#undef messagemacro
//#define messagemacro(m,s,q,t,f,l) s,
//char *psMsgNames[PS_MSG_COUNT] = {
//#include "MessageList.h"
//#include "BugMessageList.h"
//#include "BBBMessageList.h"
//};
//#undef messagemacro
#define messagemacro(m,s,q,t,f,l) t,
int psDefaultTopics[PS_MSG_COUNT] = {
#include "MessageList.h"
#include "BugMessageList.h"
#include "BBBMessageList.h"
};
#undef messagemacro
#define messagemacro(m,s,q,t,f,l) l,
char *psLongMsgNames[PS_MSG_COUNT] = {
#include "MessageList.h"
#include "BugMessageList.h"
#include "BBBMessageList.h"
};
#undef messagemacro
//#define messagemacro(m,s,q,t,f,l) q,
//psQOS_enum psQOS[PS_MSG_COUNT] = {
//#include "MessageList.h"
//#include "BugMessageList.h"
//#include "BBBMessageList.h"
//};
//#undef messagemacro
#define formatmacro(e,t,v,s) s,
int psMessageFormatLengths[PS_FORMAT_COUNT] = {
#include "MsgFormatList.h"
#include "BugMsgFormatList.h"
#include "BBBMsgFormatList.h"
};
#undef formatmacro
