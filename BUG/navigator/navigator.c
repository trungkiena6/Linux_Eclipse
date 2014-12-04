/*
 ============================================================================
 Name        : navigator.c
 Author      : Martin
 Version     :
 Copyright   : (c) 2013 Martin Lane-Smith
 Description : Receives IMU (compass) updates & odometry messages from the robot and consolidates a POSE report.
 	 	 	   Receives MOVEMENT requests from the overmind and derives MOVE commands to the robot.
 ============================================================================
 */

#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "RobotTypes.h"
#include "Blackboard.h"
#include "broker.h"
#include "Kalman.h"
#include "SysLog.h"

Kalman_t kalmanX;		//roll
Kalman_t kalmanY;		//pitch
Kalman_t kalmanZ;		//heading

#define RAD_TO_DEG (180.0 / M_PI)
#define NORMALIZE_HEADING(x) (x + 360)%360

//reporting criteria
#define REPORT_BEARING_CHANGE 2			//degrees
#define REPORT_CONFIDENCE_CHANGE 0.2f	//probability
#define REPORT_MAX_INTERVAL	5			//seconds
#define REPORT_MIN_CONFIDENCE	0.5f
#define REPORT_LOCATION_CHANGE 5

pthread_mutex_t	navdatamtx = PTHREAD_MUTEX_INITIALIZER;

//thread to forward move commands to arbotix process
void *NavArbotixThread(void *arg);

//last IMU message
uint32_t lastImuTimestamp;
bool timestampValid = false;

//latest pose reports
psMessage_t poseMsg;
psPosePayload_t lastPoseMessage;
time_t lastReportTime;

//latest commands
psOrientPayload_t lastOrientPayload;
struct timeval lastOrientTime = {0,0};
int16_t desiredCompassHeading = 0;

psMovePayload_t lastMovePayload;
struct timeval lastMoveTime = {0,0};
int16_t desiredNorthing = 0;
int16_t desiredEasting = 0;

int NavigatorInit()
{
	int i;

	memset((void*) &poseMsg, 0, sizeof(psMessage_t));
	memset((void*) &lastOrientPayload, 0, sizeof(psOrientPayload_t));
	memset((void*) &lastMovePayload, 0, sizeof(psMovePayload_t));

	psInitPublish(poseMsg, POSE);

	initKalman(&kalmanX);
	initKalman(&kalmanY);
	initKalman(&kalmanZ);

	kalmanX.angle = 180;
	kalmanY.angle = 180;
	kalmanZ.angle = 0;

	//create arbotix control thread
	pthread_t thread;
	int s = pthread_create(&thread, NULL, NavArbotixThread, NULL);
	if (s != 0)
	{
		LogError("ArbThread create failed. %i", s);
		return -1;
	}

	return 0;
}
//func to receive system messages
void NavigatorProcessMessage(psMessage_t *msg)
{
//	DEBUG_PRINT("Navigator: %s\n", psLongMsgNames[msg->header.messageType]);

	switch (msg->header.messageType)
	{
	case IMU:
	{
		//update heading belief
		double timeInterval;
		if (timestampValid)
		{
			timeInterval = (double)(poseMsg.imuPayload.timestamp - lastImuTimestamp);
		}
		else
		{
			timeInterval = 0;
			timestampValid = true;
		}
		double gyroXrate = (double) poseMsg.imuPayload.gyroX;
		double gyroYrate = (double) poseMsg.imuPayload.gyroY;
		double gyroZrate = (double) poseMsg.imuPayload.gyroZ;

		double accXangle = (atan2(
				(double) poseMsg.imuPayload.accelX,
				(double) poseMsg.imuPayload.accelZ)+PI)*RAD_TO_DEG;
		double accYangle = (atan2(
				(double) poseMsg.imuPayload.accelY,
				(double) poseMsg.imuPayload.accelZ)+PI)*RAD_TO_DEG;
		double magZangle = (atan2(
				(double) poseMsg.imuPayload.magY,
				(double) poseMsg.imuPayload.magX))*RAD_TO_DEG;

		poseMsg.posePayload.roll.degrees = (int8_t) getAngle(&kalmanX, accXangle, gyroXrate, timeInterval); // calculate the angle using a Kalman filter
		poseMsg.posePayload.pitch.degrees = (int8_t) getAngle(&kalmanY, accYangle, gyroYrate, timeInterval);
		poseMsg.posePayload.heading.degrees = (int8_t) getAngle(&kalmanZ, magZangle, gyroZrate, timeInterval);
		poseMsg.posePayload.roll.variance = (int8_t) kalmanX.S;
		poseMsg.posePayload.pitch.variance = (int8_t) kalmanY.S;
		poseMsg.posePayload.heading.variance = (int8_t) kalmanZ.S;

		lastImuTimestamp = poseMsg.imuPayload.timestamp;
	}
	break;
	case ODOMETRY:
	{
		double dx_local = (double) msg->odometryPayload.xMovement;
		double dy_local = (double) msg->odometryPayload.yMovement;

		//update location belief
		//transform to global coordinates
		double b = M_PI * poseMsg.posePayload.heading.degrees / 180.0;
		double dx = dx_local * cosf(b) + dy_local * sinf(b);
		double dy = dy_local * cosf(b) + dx_local * sinf(b);

		poseMsg.posePayload.northing.cm += dx;
		poseMsg.posePayload.easting.cm += dy;
	}
	break;

	case ORIENT:
		if (turnOK)
		{
			lastOrientPayload = msg->orientPayload;
			switch (msg->orientPayload.orientType)
			{
			case ORIENT_RELATIVE:
				//convert to absolute
				desiredCompassHeading = msg->orientPayload.heading + poseMsg.posePayload.heading.degrees;
				break;
			case ORIENT_COMPASS:
				desiredCompassHeading = msg->orientPayload.heading;
				break;
			default:
				break;
			}
			gettimeofday(&lastOrientTime, NULL);
		}
		break;

	case MOVEMENT:
		if (moveOK)
		{
			lastMovePayload = msg->movePayload;
			switch(msg->movePayload.moveType)
			{
			case MOVE_LOCAL:
			{
				//transform into compass frame
				float headingOffset = (float) poseMsg.posePayload.heading.degrees;
				float cosHO = cosf(headingOffset);
				float sinHO = sinf(headingOffset);
				float X	= msg->movePayload.x;
				float Y	= msg->movePayload.y;

				float northing = X * cosHO - Y * sinHO;
				float easting  = X * sinHO + Y * cosHO;

				desiredNorthing = northing + poseMsg.posePayload.northing.cm;
				desiredEasting = easting + poseMsg.posePayload.easting.cm;
			}
			break;
			case MOVE_COMPASS:
				desiredNorthing = lastMovePayload.northing + poseMsg.posePayload.northing.cm;
				desiredEasting = lastMovePayload.easting + poseMsg.posePayload.easting.cm;
				break;
			case MOVE_ABSOLUTE:
				desiredNorthing = lastMovePayload.northing;
				desiredEasting = lastMovePayload.easting;
				break;
			default:
				break;
			}
			gettimeofday(&lastMoveTime, NULL);
		}
		break;

	default:
		break;
	}

	int s = pthread_mutex_unlock(&navdatamtx);
	if (s != 0)
	{
		LogError("mutex unlock %i", s);
	}
	//end critical section

	//whether to report
#define max(a,b) (a>b?a:b)
	if ((abs(lastPoseMessage.northing.cm - poseMsg.posePayload.northing.cm) > REPORT_LOCATION_CHANGE) ||
			(abs(lastPoseMessage.easting.cm - poseMsg.posePayload.easting.cm) > REPORT_LOCATION_CHANGE) ||
			(abs(NORMALIZE_HEADING(lastPoseMessage.heading.degrees - poseMsg.posePayload.heading.degrees)) > REPORT_BEARING_CHANGE) ||
			(lastReportTime + REPORT_MAX_INTERVAL < time(NULL)))
	{
		lastPoseMessage = poseMsg.posePayload;
		NewBrokerMessage(&poseMsg);
		lastReportTime = time(NULL);
	}

}
//thread to send updates to the arbotix process
void *NavArbotixThread(void *arg)
{
	psMessage_t msg;
	struct timespec waitTime = {0,0};
	psInitPublish(msg, MOVE);

	LogRoutine("Arb thread ready");

	//loop
	while(1)
	{
		if  (bugSystemState.state == BUG_ACTIVE
				&& (arbSystemState.state == ARB_READY
				|| arbSystemState.state == ARB_WALKING))
		{
			//ready to go
			//critical section
			int s = pthread_mutex_lock(&navdatamtx);
			if (s != 0)
			{
				LogError("mutex lock %i", s);
			}

			//orientation
			float zRotateSpeed = (float) (desiredCompassHeading - poseMsg.posePayload.heading.degrees)
																		* rotationP;
			//movement
			float N = desiredNorthing - poseMsg.posePayload.northing.cm;
			float E = desiredEasting - poseMsg.posePayload.easting.cm;
			float normalization = sqrtf(N * N + E * E);
			if (normalization > 10)
			{
				N = N * 10 / normalization;
				E = E * 10 / normalization;
			}

			//transform into hexabot frame
			float headingOffset = (float) poseMsg.posePayload.heading.degrees;
			float cosHO = cosf(headingOffset);
			float sinHO = sinf(headingOffset);

			float xSpeed = (float) movementP * (N * cosHO + E * sinHO);
			float ySpeed = (float) movementP * (E * cosHO - N * sinHO);

			s = pthread_mutex_unlock(&navdatamtx);
			if (s != 0)
			{
				LogError("mutex unlock %i", s);
			}
			//end critical section
			float maxSpeed = maxSpeed;
			float totalSpeed2 = xSpeed * xSpeed + ySpeed * ySpeed;
			if (totalSpeed2 > maxSpeed * maxSpeed)
			{
				float factor = maxSpeed / sqrt(totalSpeed2);
				xSpeed *= factor;
				ySpeed *= factor;
			}
			if (zRotateSpeed > maxRotateSpeed)
			{
				zRotateSpeed = maxRotateSpeed;
			}
			if (moveOK)
			{
				msg.threeFloatPayload.xSpeed		= xSpeed;
				msg.threeFloatPayload.ySpeed		= ySpeed;
			}
			else
			{
				msg.threeFloatPayload.xSpeed		= 0;
				msg.threeFloatPayload.ySpeed		= 0;
			}
			if (turnOK)
			{
				msg.threeFloatPayload.zRotateSpeed	= zRotateSpeed;
			}
			else
			{
				msg.threeFloatPayload.zRotateSpeed	= 0;
			}
			if (moveOK || turnOK)
			{
				NewBrokerMessage(&msg);
			}
		}

		waitTime.tv_nsec = navLoopDelay * 1000000;
		nanosleep(&waitTime, NULL);
	}
	return 0;
}
