//
//  Blackboard.c
//  Hex
//
//  Created by Martin Lane-Smith on 7/2/14.
//  Copyright (c) 2014 Martin Lane-Smith. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h> 		/* Defines O_* constants */
#include <sys/stat.h> 	/* Defines mode constants */
#include <sys/mman.h>

#include "RobotTypes.h"
#include "Blackboard.h"
#include "SysLog.h"

//---------------------------------------Blackboard Data Implementation--------------------------------
struct BatteryStruct battery;

//(Goal) System State set by e.g. iOS App
struct SystemStateStruct setSystemState;

//Current Fido State, responding to above
struct FidoStateStruct fidoSystemState;

//Fused Proximity analysis - from scanner
//16 entries for all directions
//ProxDirection_enum 16 x 22.5 degree sectors relative to heading, clockwise
struct ProximityStruct  proximity[PROX_SECTORS];

//Fused Navigation analysis from navigator
psPosePayload_t pose;

//options
#define optionmacro(name, var, min, max, def) int var = def;
#include "options.h"
#undef optionmacro

//Settings
#define settingmacro(name, var, min, max, def) float var = def;
#include "settings.h"
#undef settingmacro

//process a message
void BlackboardProcessMessage(psMessage_t *msg)
//process received message to update blackboard
{
//	DEBUG_PRINT("Blackboard: %s\n", psLongMsgNames[msg->header.messageType]);

	switch (msg->header.messageType)
	{
	case BATTERY:
		battery.status  = msg->batteryPayload.status;
		battery.volts   = msg->batteryPayload.volts;
		gettimeofday(&battery.atTime, NULL);
		break;
	case TICK_1S:
		setSystemState.state = msg->tickPayload.sysState;
		fidoSystemState.state = msg->tickPayload.fidoState;
		gettimeofday(&setSystemState.atTime, NULL);
		break;
	case NEW_SETTING:
#define settingmacro(n, var, min, max, def) if (strncmp(n,msg->nameFloatPayload.name,PS_NAME_LENGTH) == 0)\
		var = msg->nameFloatPayload.value;
#include "settings.h"
#undef settingmacro
break;
	case SET_OPTION:
#define optionmacro(n, var, min, max, def) if (strncmp(n,msg->nameIntPayload.name,PS_NAME_LENGTH) == 0)\
		var = msg->nameIntPayload.value;
#include "options.h"
#undef optionmacro
		break;
	default:
		//ignores other messages
		break;
	}
}

void BlackboardInit()
{
	//initialize
	battery.status = BATT_UNKNOWN;
	gettimeofday(&battery.atTime, NULL);

	setSystemState.state = STATE_UNSPECIFIED;
	gettimeofday(&setSystemState.atTime, NULL);

	fidoSystemState.state = FIDO_STATE_UNKNOWN;
	gettimeofday(&fidoSystemState.atTime, NULL);
}

