/*
 * autopilot.h
 *
 *  Created on: 2015
 *      Author: martin
 */

#ifndef AUTOPILOT_H_
#define AUTOPILOT_H_

//autopilot task
pthread_t AutopilotInit();

void AutopilotProcessMessage(psMessage_t *msg);


#endif
