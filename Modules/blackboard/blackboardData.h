/*
 * blackboardData.h
 *
 *  Created on: Mar 30, 2015
 *      Author: martin
 */

#ifndef BLACKBOARDDATA_H_
#define BLACKBOARDDATA_H_

#include "blackboard.h"

//latest reports

typedef struct {
	psMessage_t message;
	time_t timeStamp;
	void *next;
	int accessCount;
} RawBlackboardData_t;

//define a pointer for each message type
extern RawBlackboardData_t *rawBlackboardData[PS_MSG_COUNT];

#endif /* BLACKBOARDDATA_H_ */
