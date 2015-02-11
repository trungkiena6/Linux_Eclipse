/*
 * broker.h
 *
 * Header for broker internal message queues
 *
 *  Created on: Jul 8, 2014
 *      Author: martin
 */

#ifndef BROKER_H_
#define BROKER_H_

#include <stdio.h>
#include "pthread.h"

//#define DEBUG_PRINT(...) if (blackboard->options.brokerDebug) printf(__VA_ARGS__);
#define DEBUG_PRINT(...) printf(__VA_ARGS__);

//convenience macro to initialize the message struct
#define psInitPublish(msg, msgType) {\
    msg.header.messageType=msgType;\
    msg.header.source=SOURCE_BBB;\
        msg.header.length=psMessageFormatLengths[psMsgFormats[msgType]];}

//queue item struct
//just a message and a next pointer
typedef struct {
	psMessage_t msg;
	void *next;
} BrokerQueueEntry_t;

//queue struct - allocated and kept by the owning subsystem
//list head and tail, plus mutex and condition variable for signalling
typedef struct {
	BrokerQueueEntry_t *qHead, *qTail;
	pthread_mutex_t mtx;	//access control
	pthread_cond_t cond;	//signals item in previously empty queue
} BrokerQueue_t;

int BrokerQueueInit(int pre);	//one init to pre-allocate shared pool of queue entries
int AddMessage(BrokerQueue_t *q, psMessage_t *msg);	//appends to queue
psMessage_t *GetMessage(BrokerQueue_t *q);			//waits if empty
void DoneWithMessage(psMessage_t *msg);				//when done with message Q entry

//main queue for the publishing broker
extern BrokerQueue_t brokerQueue;
#define NewBrokerMessage(msg) AddMessage(&brokerQueue, msg) //;printf("%s\n",__BASE_FILE__);


void AdjustMessageLength (psMessage_t *_msg);

#endif /* BROKER_H_ */
