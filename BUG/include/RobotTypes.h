/* 
 * File:   RobotTypes.h
 * Author: martin
 *
 * Created on April 19, 2014, 11:09 AM
 */

#ifndef ROBOTTYPES_H
#define	ROBOTTYPES_H

//robot dependent header file
//HEX BUG version

#include <stdint.h>
#include <unistd.h>

#include "SoftwareProfile.h"

#include "MessageFormats.h"
#include "BugMessageFormats.h"
#include "BBBMessageFormats.h"

typedef int bool;
#define true 1
#define false 0

//------------------------PubSub sources

typedef enum {
    SOURCE_UNKNOWN,
    SOURCE_PIC,
    SOURCE_BBB,
    SOURCE_APP,     //iOS App
    SOURCE_COUNT
} MessageSource_enum;

#define MESSAGE_SOURCE_NAMES {"???","PIC","BBB","APP"}
extern char *subsystemNames[SOURCE_COUNT];

#include "BugTopics.h"

//----------------------QOS

typedef enum {
    PS_QOS1,    //critical
    PS_QOS2,    //important
    PS_QOS3     //discardable
} psQOS_enum;

//---------------------Messages

//Enum of Message codes
#define messagemacro(m,s,q,t,f,l) m,

typedef enum {
#include "MessageList.h"
#include "BugMessageList.h"
#include "BBBMessageList.h"
    PS_MSG_COUNT
} psMessageType_enum;
#undef messagemacro

//extern char *psMsgNames[PS_MSG_COUNT];
extern int psDefaultTopics[PS_MSG_COUNT];
extern int psMsgFormats[PS_MSG_COUNT];
extern char *psLongMsgNames[PS_MSG_COUNT];
//psQOS_enum psQOS[PS_MSG_COUNT];

//---------------------Message Formats

//enum of message formats

#define formatmacro(e,t,p,s) e,

typedef enum {
#include "MsgFormatList.h"
#include "BugMsgFormatList.h"
#include "BBBMsgFormatList.h"
    PS_FORMAT_COUNT
} psMsgFormat_enum;
#undef formatmacro

extern int psMessageFormatLengths[PS_FORMAT_COUNT];

//-------------------------------------------------------------

//Complete message
//Generic struct for all messages

#define formatmacro(e,t,p,s) t p;
typedef struct {
    psMessageHeader_t header;
    //Union option for each payload type
    union {
        uint8_t packet[1];
#include "MsgFormatList.h"
#include "BugMsgFormatList.h"
#include "BBBMsgFormatList.h"
    };
} psMessage_t;
#undef formatmacro

#define PI 3.1415927f

#endif	/* ROBOTTYPES_H */

