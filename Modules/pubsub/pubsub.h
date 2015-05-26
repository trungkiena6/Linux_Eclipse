/*
 * pubsub.h
 *
 *  message handling
 *
 *      Author: martin
 */

#ifndef PUBSUB_H
#define PUBSUB_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "PubSubData.h"
#include "pubsub/brokerQ.h"

extern bool PubSubBrokerReady;

//#define DEBUG_PRINT(...) if (blackboard->options.brokerDebug) printf(__VA_ARGS__);
//#define DEBUG_PRINT(...) printf(__VA_ARGS__);

//convenience macro to initialize the message struct
#define psInitPublish(msg, msgType) {\
    msg.header.messageType=msgType;\
    msg.header.source=OVERMIND;\
        msg.header.length=psMessageFormatLengths[psMsgFormats[msgType]];}

//main queue for the publishing broker
extern BrokerQueue_t brokerQueue;

//send a message to the broker
#define NewBrokerMessage(msg) CopyMessageToQ(&brokerQueue, msg)

//route message directly
void RouteMessage(psMessage_t *msg);

//NBotifications
void Notify(Notification_enum e);
void CancelNotification(Notification_enum e);

//broker init
pthread_t PubSubInit();
pthread_t ResponderInit();

//serial broker init
pthread_t SerialBrokerInit();

void SerialBrokerProcessMessage(psMessage_t *msg);	//consider candidate msg to send over the uart

//responder
void ResponderProcessMessage(psMessage_t *msg);

#endif
