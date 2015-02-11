/*
 * responder.c
 *
 * Responds to ping messages from the APP
 *
 *  Created on: Jul 27, 2014
 *      Author: martin
 */

#include <string.h>
#include "RobotTypes.h"
#include "broker.h"
#include "Blackboard.h"
#include "SysLog.h"
#include "fidobrain.h"

void sendOptionConfig(char *name, int var, int minV, int maxV);
void sendSettingConfig(char *name, float var, float minV, float maxV);

int configCount;
int startFlag = 1;

void ResponderProcessMessage(psMessage_t *msg)
{
//	DEBUG_PRINT("Responder: %s\n", psLongMsgNames[msg->header.messageType]);

	//check message for response requirement
	switch (msg->header.messageType)
	{
	case CONFIG:
		if (msg->bytePayload.value == SOURCE_BBB)
		{
			LogRoutine("Send Config msg");
			configCount = 0;
#define optionmacro(name, var, minV, maxV, def) sendOptionConfig(name, var, minV, maxV);
#include "options.h"
#undef optionmacro

#define settingmacro(name, var, minV, maxV, def) sendSettingConfig(name, var, minV, maxV);
#include "settings.h"
#undef settingmacro
			{
				psMessage_t msg;
				psInitPublish(msg, CONFIG_DONE);
				msg.bytePayload.value = configCount;
				SerialBrokerProcessMessage(&msg);
			}
			LogRoutine("Config Done");
		}
		break;

    case PING_MSG:
    {
    	LogRoutine("Ping msg");
        psMessage_t msg2;
        psInitPublish(msg2, SS_ONLINE);
        strcpy(msg2.onlinePayload.name, "BBB");

        msg2.onlinePayload.startup = startFlag;
        msg2.onlinePayload.sysState = STATE_UNSPECIFIED;
        startFlag = 0;
        AdjustMessageLength(&msg2);
        SerialBrokerProcessMessage(&msg2);
    }
        break;
//    case TICK_1S:
//    case SET_STATE: //change power state
//    {
//    	//just to reset the startFlag
//        startFlag = 0;
//    }
    default:
        //ignore anything else
        break;

	}
}

void sendOptionConfig(char *name, int var, int minV, int maxV) {
	struct timespec request, remain;
    psMessage_t msg;
    psInitPublish(msg, OPTION);
    strncpy(msg.name3IntPayload.name, name, PS_NAME_LENGTH);
    msg.name3IntPayload.value = var;
    msg.name3IntPayload.min = minV;
    msg.name3IntPayload.max = maxV;
    AdjustMessageLength(&msg);
    SerialBrokerProcessMessage(&msg);
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
    AdjustMessageLength(&msg);
    SerialBrokerProcessMessage(&msg);
    configCount++;
}
