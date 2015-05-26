//
//  Blackboard.h
//  Hex
//
//  Created by Martin Lane-Smith on 7/2/14.
//  Copyright (c) 2014 Martin Lane-Smith. All rights reserved.
//

#ifndef Blackboard_h
#define Blackboard_h

#include <sys/time.h>
#include "PubSubData.h"
#include "pubsub/pubsub.h"

//options
#define optionmacro(name, var, min, max, def) extern int var;
#include "Options.h"
#undef optionmacro

//Settings
#define settingmacro(name, var, min, max, def) extern float var;
#include "Settings.h"
#undef settingmacro

extern char *batteryStateNames[];
extern char *eventNames[NOTIFICATION_COUNT];
extern char *stateCommandNames[COMMAND_COUNT];
extern char *powerStateNames[POWER_STATE_COUNT];
extern char *ovmCommandNames[OVERMIND_ACTION_COUNT];
extern char *arbStateNames[ARB_STATE_COUNT];

//blackboard
pthread_t BlackboardInit();

void BlackboardProcessMessage(psMessage_t *msg);

//get a pointer to a message - current or 'relativeTime' seconds in the past
psMessage_t *bbGetMessage(psMessageType_enum messageType, time_t relativeTime);

//get timestamp
time_t bbGetTimeStamp(psMessage_t *msg);

//release the message for GC
void bbDoneWithData(psMessage_t *msg);

//notifications
NotificationMask_t bbGetActiveNotifications();
bool bbIsNotificationActive(Notification_enum e);


#endif
