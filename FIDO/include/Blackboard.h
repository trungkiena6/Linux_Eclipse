//
//  Blackboard.h
//  Hex
//
//  Created by Martin Lane-Smith on 7/2/14.
//  Copyright (c) 2014 Martin Lane-Smith. All rights reserved.
//

#ifndef Hex_Blackboard_h
#define Hex_Blackboard_h

#include <sys/time.h>
#include "RobotTypes.h"
#include "broker.h"

extern struct BatteryStruct {
    float volts;
    psBatteryStatus_enum status;
    struct timeval atTime;
} battery;

//(Goal) System State set by e.g. iOS App
extern struct SystemStateStruct {
    SysState_enum state;			//in MessageFormats.h
    struct timeval atTime;
} setSystemState;

//Current Fido State, responding to above
extern struct FidoStateStruct {
    FidoState_enum state;			//in FidoMessageFormats.h
    struct timeval atTime;
} fidoSystemState;

//Fused Proximity analysis - from scanner
//16 entries for all directions
//ProxDirection_enum 16 x 22.5 degree sectors relative to heading, clockwise
extern struct ProximityStruct {
    uint8_t status;			//psProxStatus_enum in MessageFormats.h
    uint8_t confidence;		//0 - 100%
    uint8_t range;			//0 - 200 cm
    struct timeval atTime;
} proximity[PROX_SECTORS];

//Fused Navigation analysis from navigator
extern psPosePayload_t pose;

//options
#define optionmacro(name, var, min, max, def) extern int var;
#include "Options.h"
#undef optionmacro

//Settings
#define settingmacro(name, var, min, max, def) extern float var;
#include "Settings.h"
#undef settingmacro

extern char *batteryStateNames[];
extern char *systemStateNames[];
extern char *bugStateNames[];
extern char *arbStateNames[];

#endif
