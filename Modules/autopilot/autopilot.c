/*
 ============================================================================
 Name        : autopilot.c
 Author      : Martin
 Version     :
 Copyright   : (c) 2015 Martin Lane-Smith
 Description : Receives MOVE commands and issues commands to the motors, comparing
 navigation results against the goal
 ============================================================================
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stropts.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <errno.h>

#include "SoftwareProfile.h"
#include "PubSubData.h"
#include "pubsub/pubsub.h"
#include "blackboard/blackboard.h"
#include "syslog/syslog.h"

#include "navigator/navigator.h"
#include "autopilot.h"

FILE *pilotDebugFile;

#ifdef AUTOPILOT_DEBUG
#define DEBUGPRINT(...) fprintf(stdout, __VA_ARGS__);fprintf(pilotDebugFile, __VA_ARGS__);fflush(pilotDebugFile);
#else
#define DEBUGPRINT(...) fprintf(pilotDebugFile, __VA_ARGS__);fflush(pilotDebugFile);
#endif

#define ERRORPRINT(...) fprintf(stdout, __VA_ARGS__);fprintf(pilotDebugFile, __VA_ARGS__);fflush(pilotDebugFile);

static char *moveTypeNames[] = MOVE_TYPE_NAMES;
static char *orientTypeNames[] = ORIENT_TYPE_NAMES;

BrokerQueue_t autopilotQueue = BROKER_Q_INITIALIZER;

//latest pose report
psPosePayload_t posePayload;
struct timeval latestPoseTime = { 0, 0 };
bool locationValid = false;
bool orientationValid = false;

psMessage_t pilotMotorMessage;
struct timeval latestMotorTime = { 0, 0 };

//latest command packets
psOrientPayload_t orientPayload;
psMovePayload_t movePayload;
struct timeval latestCommandTime = { 0, 0 };

//latest odometry
psOdometryPayload_t odometryPayload;
struct timeval latestOdoTime = { 0, 0 };

int desiredCompassHeading = 0;
int desiredNorthing = 0;
int desiredEasting = 0;

bool sendMovementMessage = false;
//Autopilot thread
void *AutopilotThread(void *arg);

bool StartOrient();
bool StartAlignment();
bool StartMovement();
bool ProcessOrientCommand();
bool ProcessMovementCommand();

enum {
	PILOT_STATE_IDLE,		//ready for a command
	PILOT_STATE_FORWARD,		//short distance using encoders only
	PILOT_STATE_BACKWARD,		//ditto
	PILOT_STATE_ORIENTING,		//monitor motion using compass
	PILOT_STATE_ALIGNING,
	PILOT_STATE_MOVING,			//monitor move using pose msg
	PILOT_STATE_DONE,		//move complete
	PILOT_STATE_FAILED,			//move failed
	PILOT_STATE_INACTIVE		//motors disabled
} pilotState = PILOT_STATE_INACTIVE;


enum {
	MOTORS_STATE_IDLE,
	MOTORS_STATE_COMMANDED,		//comand sent
	MOTORS_STATE_RUNNING,		//running confirmed
	MOTORS_STATE_INHIBIT,
	MOTORS_STATE_ERRORS
} motorsState = MOTORS_STATE_INHIBIT;

pthread_t AutopilotInit() {
	pilotDebugFile = fopen("/root/logfiles/pilot.log", "w");

	//create autopilot thread
	pthread_t thread;
	int s = pthread_create(&thread, NULL, AutopilotThread, NULL);
	if (s != 0) {
		ERRORPRINT("Autopilot create failed. %i\n", s);
		return -1;
	}

	return thread;
}

void AutopilotProcessMessage(psMessage_t *msg) {
	switch (msg->header.messageType) {
	case MOVEMENT:
	case ORIENT:
	case TICK_1S:
	case POSE:
		break;
	case ODOMETRY:
		break;
	case NOTIFICATION:
		switch (msg->intPayload.value) {
		case MOTORS_INHIBIT:
			motorsState = MOTORS_STATE_INHIBIT;
			break;
		case -MOTORS_INHIBIT:
		case MOTORS_DONE:
			motorsState = MOTORS_STATE_IDLE;
			break;
		case MOTORS_STARTING:
			motorsState = MOTORS_STATE_RUNNING;
			break;
		case MOTORS_ERRORS:
			motorsState = MOTORS_STATE_ERRORS;
			break;
		default:
			return;	//ignore other notifications
			break;
		}
		break;
		default:
			return;		//ignore other messages
			break;
	}
	CopyMessageToQ(&autopilotQueue, msg);
}

//thread to send updates to the motor processor
void *AutopilotThread(void *arg) {

	int priorPilotState;	//used to cancel notifications

	PowerState_enum powerState = POWER_STATE_UNKNOWN;

	psInitPublish(pilotMotorMessage, MOVE);	//prepare message to motors

	DEBUGPRINT("Pilot thread ready\n");

	//loop
	while (1) {
		psMessage_t *rxMessage = GetNextMessage(&autopilotQueue);

		//general processing
		switch (rxMessage->header.messageType) {
		case TICK_1S:
			powerState = rxMessage->tickPayload.systemPowerState;
			break;
		case POSE:
			posePayload = rxMessage->posePayload;
			locationValid = posePayload.location.valid;
			orientationValid = posePayload.orientation.valid;
			gettimeofday(&latestPoseTime, NULL);
			break;
		case ORIENT:
			orientPayload = rxMessage->orientPayload;
			DEBUGPRINT("ORIENT: %s\n", orientTypeNames[orientPayload.orientType]);
			gettimeofday(&latestCommandTime, NULL);
			break;
		case MOVEMENT:
			movePayload = rxMessage->movePayload;
			DEBUGPRINT("MOVE: %s\n",moveTypeNames[movePayload.moveType]);
			gettimeofday(&latestCommandTime, NULL);
			break;
		case ODOMETRY:
		{
			odometryPayload = rxMessage->odometryPayload;
			gettimeofday(&latestOdoTime, NULL);
			struct timeval result;
			if ((timeval_subtract (&result, &latestOdoTime, &latestMotorTime) == 0) && (result.tv_sec > 1))
			{
				//long enough for round trip
				if (motorsState == MOTORS_STATE_COMMANDED && odometryPayload.motorsRunning)
				{
					motorsState == MOTORS_STATE_RUNNING;
				}
			}
		}
		break;
		case NOTIFICATION:
			switch (rxMessage->intPayload.value) {
			case MOTORS_INHIBIT:
				break;
			case -MOTORS_INHIBIT:
				break;
			case MOTORS_DONE:
			case MOTORS_STARTING:
			case MOTORS_ERRORS:
				break;
			default:
				break;
			}
			default:
				break;
		}

		//Initialize motor message
		pilotMotorMessage.threeFloatPayload.zRotateSpeed =
				pilotMotorMessage.threeFloatPayload.xSpeed =
						pilotMotorMessage.threeFloatPayload.ySpeed = 0;
		sendMovementMessage = false;

		priorPilotState = pilotState;	//track state for Notifications

		switch (pilotState)
		{
		case PILOT_STATE_DONE:
		case PILOT_STATE_FAILED:
		case PILOT_STATE_IDLE:
			//first check whether moving OK
			if (powerState != POWER_ACTIVE || !moveOK || (motorsState == MOTORS_STATE_INHIBIT)) {
				pilotState = PILOT_STATE_INACTIVE;
				DEBUGPRINT("Pilot INACTIVE\n");
			} else
				//look for a command
			{
				switch (rxMessage->header.messageType)  {
				case ORIENT:
					ProcessOrientCommand();
					break;
				case MOVEMENT:
					ProcessMovementCommand();
					break;
				}
			}
			break;

		//Orienting is just a turn to a specific compass direction
		case PILOT_STATE_ORIENTING:
		{
			switch (rxMessage->header.messageType) {
			case MOVEMENT:
				ProcessMovementCommand();
				break;
			case ORIENT:
				ProcessOrientCommand();
				break;
			case TICK_1S:
				//TODO: timeout?
				break;
			case POSE:
			case ODOMETRY:
				break;
			case NOTIFICATION:
				switch (rxMessage->intPayload.value) {
				case MOTORS_INHIBIT:
				case MOTORS_ERRORS:
					pilotState = PILOT_STATE_FAILED;
					break;
				case MOTORS_DONE:
					if (StartOrient())	//returns true if done
					{
						pilotState = PILOT_STATE_DONE;
					}
					else
						{
							pilotState = PILOT_STATE_ORIENTING;
							DEBUGPRINT("Orient Again\n");
						}
					break;
				case MOTORS_STARTING:
					break;
				default:

					break;
				}
				break;
				default:
					break;
			}
		}
		break;

		//aligning is a turn to face a destination, prior to moving forward
		case PILOT_STATE_ALIGNING:
		{
			switch (rxMessage->header.messageType) {
			case MOVEMENT:
				ProcessMovementCommand();
				break;
			case ORIENT:
				ProcessOrientCommand();
				break;
			case TICK_1S:
				//TODO: timeout?
				break;
			case POSE:
			case ODOMETRY:
				break;
			case NOTIFICATION:
				switch (rxMessage->intPayload.value) {
				case MOTORS_INHIBIT:
				case MOTORS_ERRORS:
					pilotState = PILOT_STATE_FAILED;
					break;
				case MOTORS_DONE:
					if (StartAlignment()) //true if aligned
					{
						if (StartMovement())	//true if arrived
							{
								pilotState = PILOT_STATE_DONE;
							}
						else
							{
							pilotState = PILOT_STATE_MOVING;
							}
					}
					else
						{
						pilotState = PILOT_STATE_ALIGNING;
						DEBUGPRINT("Align Again\n");
						}

					break;
				case MOTORS_STARTING:
					break;
				default:
					break;
				}
				break;
				default:
					break;
			}
		}
		break;

		//once aligned, the pilot drives to the destination
		case PILOT_STATE_MOVING: {
			switch (rxMessage->header.messageType) {
			case MOVEMENT:
				ProcessMovementCommand();
				break;
			case ORIENT:
				ProcessOrientCommand();
				break;
			case TICK_1S:
				//TODO: timeout?
				break;
			case POSE:
			case ODOMETRY:
				break;
			case NOTIFICATION:
				switch (rxMessage->intPayload.value) {
				case MOTORS_INHIBIT:
				case MOTORS_ERRORS:
					pilotState = PILOT_STATE_FAILED;
					break;
				case MOTORS_DONE:
					if (StartAlignment())	//true if aligned
					{
						if (StartMovement()) //true if arrived
						{
							pilotState = PILOT_STATE_DONE;
						}
						else
						{
							pilotState = PILOT_STATE_MOVING;
						}
					}
					else pilotState = PILOT_STATE_ALIGNING;
					break;
				case MOTORS_STARTING:
					break;
				default:
					break;
				}
				break;
				default:
					break;
			}
		}
		break;
		case PILOT_STATE_FORWARD:
		case PILOT_STATE_BACKWARD: {

			switch (rxMessage->header.messageType) {
			case MOVEMENT:
				ProcessMovementCommand();
				break;
			case ORIENT:
				ProcessOrientCommand();
				break;
			case TICK_1S:
				//TODO: timeout?
				break;
			case POSE:
			case ODOMETRY:
				break;
			case NOTIFICATION:
				switch (rxMessage->intPayload.value) {
				case MOTORS_INHIBIT:
				case MOTORS_ERRORS:
					pilotState = PILOT_STATE_FAILED;
					break;
				case MOTORS_DONE:
					pilotState = PILOT_STATE_DONE;
					DEBUGPRINT("Move Done\n");
					break;
				case MOTORS_STARTING:
					break;
				default:
					break;
				}
				break;
				default:
					break;
			}
		}
		break;

		case PILOT_STATE_INACTIVE:
			if (powerState == POWER_ACTIVE && moveOK && (motorsState != MOTORS_STATE_INHIBIT)) {
				pilotState = PILOT_STATE_IDLE;
				DEBUGPRINT("Pilot IDLE\n");
			}
			break;
		}

		DoneWithMessage(rxMessage);

		if (sendMovementMessage) {
			RouteMessage(&pilotMotorMessage);
			gettimeofday(&latestMotorTime, NULL);
			motorsState = MOTORS_STATE_COMMANDED;

			DEBUGPRINT("Motors P: %i, S: %i\n", pilotMotorMessage.motorPayload.portMotors, pilotMotorMessage.motorPayload.starboardMotors)
		}

		if (priorPilotState != pilotState) {
			switch (priorPilotState) {
			case PILOT_STATE_IDLE:
			case PILOT_STATE_INACTIVE:
				break;
			case PILOT_STATE_ORIENTING:
			case PILOT_STATE_ALIGNING:
			case PILOT_STATE_MOVING:
				CancelNotification(PILOT_ENGAGED);
				break;
			case PILOT_STATE_DONE:
				CancelNotification(PILOT_DONE);
				break;
			case PILOT_STATE_FAILED:
				CancelNotification(PILOT_FAILED);
				break;
			}
			switch (pilotState) {
			case PILOT_STATE_IDLE:
			case PILOT_STATE_INACTIVE:
				break;
			case PILOT_STATE_ORIENTING:
			case PILOT_STATE_ALIGNING:
			case PILOT_STATE_MOVING:
				Notify(PILOT_ENGAGED);
				break;
			case PILOT_STATE_DONE:
				Notify(PILOT_DONE);
				break;
			case PILOT_STATE_FAILED:
				Notify(PILOT_FAILED);
				break;
			}
		}
	}
}

bool StartOrient()
{
	//initiate turn to 'desiredCompassHeading'
	//calculate angle error
	int angleError = (360 + desiredCompassHeading
			- posePayload.orientation.heading) % 360;
	if (angleError > 180)
		angleError -= 360;

	DEBUGPRINT("Pilot: angle error %i\n", angleError);

	//if close, report done
	if (abs(angleError) < arrivalHeading) {
		DEBUGPRINT("Pilot: Orient done\n");
		return true;;
	} else {
		//send turn command
		int range = abs(DEGREESTORADIANS(angleError)) * FIDO_RADIUS;
		//send turn command
		if (angleError > 0) {
			pilotMotorMessage.motorPayload.portMotors = range;
			pilotMotorMessage.motorPayload.starboardMotors = -range;
		} else {
			pilotMotorMessage.motorPayload.portMotors = -range;
			pilotMotorMessage.motorPayload.starboardMotors = range;
		}
		pilotMotorMessage.motorPayload.speed = defTurnRate;
		pilotMotorMessage.motorPayload.flags = 0;
		sendMovementMessage = true;
		return false;
	}
}

bool StartAlignment()
{
	//initiate turn towards a destination
	//start required orientation
	desiredCompassHeading = (int) RADIANSTODEGREES(
			atan2(desiredEasting - posePayload.location.easting,
					desiredNorthing - posePayload.location.northing));
	return StartOrient();
}

bool StartMovement()
{
	//initiate movement assuming we're aligned
	//check range
	int northingDifference = (desiredNorthing
			- posePayload.location.northing);
	int eastingDifference = (desiredEasting
			- posePayload.location.easting);

	int range = sqrt(
			northingDifference * northingDifference
			+ eastingDifference * eastingDifference);

	DEBUGPRINT("Pilot: Range = %.0f\n", range);

	//if close, change to PILOT_STATE_DONE
	if (range < arrivalRange) {
		DEBUGPRINT("Pilot: Move done\n");
		return true;;
	} else {

		//send move command
		pilotMotorMessage.motorPayload.portMotors =
				pilotMotorMessage.motorPayload.starboardMotors =
						(range > 100.0 ? 100.0 : range);
		pilotMotorMessage.motorPayload.speed = defSpeed;
		pilotMotorMessage.motorPayload.flags = 0;
		sendMovementMessage = true;

		//TODO: Adjust port & starboard to correct residual heading error

		return false;
	}
}
bool VerifyCompass()
{
	if (!orientationValid) {
		DEBUGPRINT("Pilot: No compass\n");
		pilotState = PILOT_STATE_FAILED;
		return false;
	}
	return true;
}
bool VerifyLocation()
{
	if (!locationValid) {
		DEBUGPRINT("Pilot: No GPS\n");
		pilotState = PILOT_STATE_FAILED;
		return false;
	}
	return true;
}
bool ProcessOrientCommand()
{
	if (VerifyCompass())
	{
		switch (orientPayload.orientType) {
		case ORIENT_RELATIVE:
			//convert to absolute
			desiredCompassHeading = orientPayload.heading + posePayload.orientation.heading;
			DEBUGPRINT("Pilot: Orient Relative: %i\n", desiredCompassHeading);
			if (StartOrient()) pilotState = PILOT_STATE_DONE;
			else pilotState = PILOT_STATE_ORIENTING;
			break;
		case ORIENT_COMPASS:
			desiredCompassHeading = orientPayload.heading;
			DEBUGPRINT("Pilot: Orient Compass: %i\n", orientPayload.heading);
			if (StartOrient()) pilotState = PILOT_STATE_DONE;
			else pilotState = PILOT_STATE_ORIENTING;
			break;
		case ORIENT_NONE:
			DEBUGPRINT("Pilot: Orient NONE\n");
			pilotState = PILOT_STATE_DONE;
			break;
		default:
			ERRORPRINT("Pilot: Unknown Orient: %i\n", orientPayload.orientType);
			pilotState = PILOT_STATE_FAILED;
			break;
		}
	}
}

bool ProcessMovementCommand()
{
	switch (movePayload.moveType) {
	case MOVE_FORWARD:
		pilotMotorMessage.motorPayload.portMotors = pilotMotorMessage.motorPayload.starboardMotors = movePayload.x;
		pilotMotorMessage.motorPayload.speed = defSpeed;
		pilotMotorMessage.motorPayload.flags = 0;
		sendMovementMessage = true;
		DEBUGPRINT("Pilot: Move forward: %i\n", movePayload.x);
		pilotState = PILOT_STATE_FORWARD;
		break;
	case MOVE_BACKWARD:
		pilotMotorMessage.motorPayload.portMotors = pilotMotorMessage.motorPayload.starboardMotors = -movePayload.x;
		pilotMotorMessage.motorPayload.speed = defSpeed;
		pilotMotorMessage.motorPayload.flags = 0;
		sendMovementMessage = true;
		DEBUGPRINT("Pilot: Move backward: %i\n", movePayload.x);
		pilotState = PILOT_STATE_BACKWARD;
		break;
	case MOVE_RELATIVE:
		if (VerifyCompass() && VerifyLocation())
		{
			desiredNorthing = movePayload.northing + posePayload.location.northing;
			desiredEasting = movePayload.easting + posePayload.location.easting;
			DEBUGPRINT("Pilot: Move Relative: %iN, %iE\n", desiredNorthing, desiredEasting);
			if (StartAlignment())
			{
				if (StartMovement()) pilotState = PILOT_STATE_DONE;
				else pilotState = PILOT_STATE_MOVING;
			}
			else pilotState = PILOT_STATE_ALIGNING;
		}
		break;
	case MOVE_ABSOLUTE:
		if (VerifyCompass() && VerifyLocation())
		{
			desiredNorthing = movePayload.northing;
			desiredEasting = movePayload.easting;
			DEBUGPRINT("Pilot: Move Absolute: %iN, %iE\n", movePayload.easting, movePayload.northing);
			if (StartAlignment())
			{
				if (StartMovement()) pilotState = PILOT_STATE_DONE;
				else pilotState = PILOT_STATE_MOVING;
			}
			else pilotState = PILOT_STATE_ALIGNING;
		}
		break;
	case MOVE_NONE:
		DEBUGPRINT("Pilot: Move NONE\n", desiredCompassHeading);
		pilotState = PILOT_STATE_DONE;
		break;
	default:
		ERRORPRINT("Pilot: Unknown Move type: %i\n", movePayload.moveType);
		pilotState = PILOT_STATE_FAILED;
		break;
	}
}
