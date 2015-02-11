/*
 ============================================================================
 Name        : scanner.c
 Author      : Martin
 Version     :
 Copyright   : (c) 2013 Martin Lane-Smith
 Description : Receives FOCUS messags from the overmind and directs the scanner accordingly.
 	 	 	   Receives proximity detector messages and publishes a consolidated PROX-REP
 ============================================================================
 */

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <stropts.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <errno.h>

#include "c_pwm.h"
#include "event_gpio.h"
#include "RobotTypes.h"
#include "broker.h"
#include "Blackboard.h"
#include "SysLog.h"

int focusHeading = 0;

//convenience
#define PROX_SECTOR(servoangle)  (servoangle>=0 ? \
		((servoangle+SECTOR_WIDTH/2)/SECTOR_WIDTH) : \
		((servoangle-SECTOR_WIDTH/2)/SECTOR_WIDTH) + 15 )

static const float proxSectorBearings[PROX_SECTORS] =
{0.0, 45.0, 90.0, 135.0, 180.0, -135, -90.0, -45.0};

//Obstacle hypotheses
float probObstacle[PROX_SECTORS][RANGE_BUCKETS];

struct SectorData {
	bool scanNext;				//scan in upcoming pass
	bool report;				//send report
	time_t lastScanned;			//wall clock time in seconds
	psProxStatus_enum status;	//whether obstacle thought present
	float confidence;			//0 - 1
	int rangeBucket;			//0 to RANGE_BUCKETS-1
	float weight;
} sectors[PROX_SECTORS];

void Update(int servoAngle, int rangeReported, enum SensorType T);
void Normalize(int sector);
void calcProximity(int sector);

//I2C Sonar interfacing
char I2CdevName[80];   //sonar I2C filename
int StartRanging();
int ReadRange();    //read sonar

void *ScannerThread(void *arg);

int ScannerInit()
{
	int r, s;

	//prepare sonar gpio
	gpio_export(SONAR_GPIO);
	gpio_set_direction(SONAR_GPIO, 1);		//input

	//create scanner thread
	pthread_t thread;
	int result = pthread_create(&thread, NULL, ScannerThread, NULL);
	if (result != 0)
	{
		LogError("No thread");
		return -1;
	}

	//start PWM
	if (pwm_start(SERVO_PWM_KEY, DUTY_CYCLE(0), SERVO_FREQUENCY, 1) != 1)
	{
		LogError("pwm_start failed");
		return -1;
	}

	//sonar I2C device name
	snprintf(I2CdevName, sizeof(I2CdevName), "/dev/i2c-%d", SONAR_I2C);

	//initialize Prox array
	for (s=0; s<PROX_SECTORS; s++)
	{
		for (r=0; r<RANGE_BUCKETS; r++)
		{
			probObstacle[s][r] = (float)1.0/RANGE_BUCKETS;
		}
		sectors[s].status = PROX_UNKNOWN;
		sectors[s].confidence = 1.0 - (float)1.0/RANGE_BUCKETS;
	}
	return 0;
}

int proximityFactor[PROX_STATUS_COUNT] = {
								2,	//PROX_UNKNOWN
								0, 	//PROX_CLEAR
								1,	//PROX_FAR
								2,	//PROX_NEAR
								3	//PROX_APPROACHING
};

void *ScannerThread(void *arg)
{
	int servoCurrent = 0;
	int servoNext;
	int servoDirection = 1;

	struct timespec request, remain;
	int reply;
	int a, r, i;

	while(1)
	{
		//update sector priority
		for (a=LEFT_SCAN_LIMIT; a<=RIGHT_SCAN_LIMIT; a+=SECTOR_WIDTH)
		{
			int s = PROX_SECTOR(a);

			//factors

			//what report have we already?
			int proximity = proximityFactor[sectors[s].status];

			//how close to our direction of focus?
			int nearFocus = 10 - abs(s - PROX_SECTOR(focusHeading));
			if (nearFocus < 0) nearFocus = 0;

			//how near the front?
			int forwardFacing = 10 - abs(s);
			if (forwardFacing < 0) forwardFacing = 0;

			//how long since we last scanned?
			int sampleAge = time(NULL) - sectors[s].lastScanned;

			sectors[s].weight = proximity * proxWeight +
								nearFocus * focusWeight +
								forwardFacing * fwdWeight +
								sampleAge * ageWeight;

			sectors[s].scanNext = false;
		}
		//find the top priorities
		int sectorsToScan = 0;
		while (sectorsToScan < SECTORS_PER_PASS)
		{
			float maxWeight = 0;
			int sector = -1;
			for (a=LEFT_SCAN_LIMIT; a<=RIGHT_SCAN_LIMIT; a+=SECTOR_WIDTH)
			{
				int s = PROX_SECTOR(a);
				if (sectors[s].weight > maxWeight)
				{
					maxWeight = sectors[s].weight;
					sector = s;
				}
				if (s > 0)
				{
					sectors[s].scanNext = true;
					sectorsToScan++;
				}
				else
				{
					break;
				}
			}
		}

		//now do the scan
		if (servoDirection > 0)
		{
			servoNext = LEFT_SCAN_LIMIT;
		}
		else
		{
			servoNext = RIGHT_SCAN_LIMIT;
		}

		while (servoNext > LEFT_SCAN_LIMIT && servoNext < RIGHT_SCAN_LIMIT)
		{
			int sector = PROX_SECTOR(servoNext);

			if (sectors[sector].scanNext)
			{
				//move servo
				pwm_set_duty_cycle(SERVO_PWM_KEY, DUTY_CYCLE(servoNext));

				int delay = MIN_MOVE_MS + MS_PER_STEP * abs(servoNext - servoCurrent); //millisecs

				//sleep while the servo moves
				request.tv_sec = 0;
				request.tv_nsec = delay * 1000000;
				reply = nanosleep(&request, &remain);
				servoCurrent = servoNext;

				//take range reading
				StartRanging();
				int range = ReadRange();

				//process range data
				Update(servoCurrent, range, SCANNER_SONAR);
			}
			servoNext = servoNext + servoDirection * SCAN_STEP;
		}
		servoDirection = -servoDirection;	//switch direction

		//calculate proximity and report
		for (i=0; i<PROX_SECTORS; i++)
		{
			calcProximity(i);
		}
	}
	return 0;
}

int StartRanging()
{
	int file;
	unsigned char rangeCommand = TAKE_RANGE_READING;

	struct timespec request, remain;
	request.tv_sec = 0;
	request.tv_nsec = SONAR_GPIO_DELAY * 1000000;

	//wait for sonar ready
	unsigned int pinValue;
	do
	{
		gpio_get_value(SONAR_GPIO, &pinValue);
		if (pinValue) nanosleep(&request, &remain);
	} while (pinValue);

	//send start command
	if ((file = open(I2CdevName, O_RDWR)) < 0){
		LogError("Open %s - %s", I2CdevName, strerror(errno));
		return -1;
	}
	if (ioctl(file, I2C_SLAVE, SONAR_I2C_ADDRESS) < 0){
		LogError("I2C Addr %2x failed",SONAR_I2C_ADDRESS);
		close(file);
		return -1;
	}

	int bytesWritten = write(file, &rangeCommand, 1);
	if (bytesWritten == -1){
		LogError("Write Range Command");
		close(file);
		return -1;
	}

	close(file);

	//wait for complete
	do
	{
		nanosleep(&request, &remain);
		gpio_get_value(SONAR_GPIO, &pinValue);
	} while (pinValue);

	return 0;
}

//read range from sonar
int ReadRange()
{
	int file;
	unsigned char rangeData[2];

	if ((file = open(I2CdevName, O_RDWR)) < 0){
		LogError("Open %s - %s", I2CdevName, strerror(errno));
		return(-1);
	}
	if (ioctl(file, I2C_SLAVE, SONAR_I2C_ADDRESS) < 0){
		LogError("I2C Addr %2x",SONAR_I2C_ADDRESS);
		close(file);
		return(-2);
	}

	int bytesRead = read(file, rangeData, 2);
	if (bytesRead == -1){
		LogError("Read Byte Stream");
		close(file);
		return -4;
	}

	close(file);

	return (rangeData[0] << 8) | rangeData[1];
}

//Bayes code
void Update(int servoAngle, int rangeReported, enum SensorType T)
{
	float halfBeamWidth, falsePositive, falseNegative20, falseNegative200;

	switch(T)
	{
	case MB1242:
		halfBeamWidth 		= MB1242_HALF_BEAM_WIDTH;
		falsePositive 		= MB1242_FALSE_POSITIVE;
		falseNegative20 	= MB1242_FALSE_NEGATIVE_20;
		falseNegative200 	= MB1242_FALSE_NEGATIVE_200;
		break;
	case MB1000:
		halfBeamWidth 		= MB1000_HALF_BEAM_WIDTH;
		falsePositive 		= MB1000_FALSE_POSITIVE;
		falseNegative20 	= MB1000_FALSE_NEGATIVE_20;
		falseNegative200 	= MB1000_FALSE_NEGATIVE_200;
		break;
	case ANALOG_IR:
		halfBeamWidth 		= ANALOG_IR_HALF_BEAM_WIDTH;
		falsePositive 		= ANALOG_IR_FALSE_POSITIVE;
		falseNegative20 	= ANALOG_IR_FALSE_NEGATIVE_20;
		falseNegative200 	= ANALOG_IR_FALSE_NEGATIVE_200;
		break;
	}

	float contrib[3] = {0.0,0.0,0.0};	//contribution to left, center & right sectors

	float fSector = PROX_SECTOR(servoAngle);
	//calculate contributions to sectors
	int primarySector = (int) fSector;
	float remainder = fSector - primarySector;
	//how much beam overlaps into adjacent sectors
	float leftOverlap = (float)(halfBeamWidth - (remainder * SECTOR_WIDTH));
	if (leftOverlap > 0.0)
		contrib[0] = (leftOverlap * leftOverlap) / (2 * halfBeamWidth * halfBeamWidth);

	float rightOverlap = (float)(halfBeamWidth - ((1-remainder) * SECTOR_WIDTH));
	if (rightOverlap > 0.0)
		contrib[2] = (rightOverlap * rightOverlap) / (2 * halfBeamWidth * halfBeamWidth);

	contrib[1] = 1.0 - contrib[0] - contrib[2];

	//calculate likelihood
	int r,s;
	float range = (float) rangeReported;
	float rangeFactor = (float)(rangeReported - MIN_PROX_RANGE)/(MAX_PROX_RANGE - MIN_PROX_RANGE);

	float totalP = 0.0;		//Cumulative probability assigned

	for (r = 0; r<RANGE_BUCKETS; r++)
	{
		float p;
		float bucketCenter = (float) (r + 0.5) * RANGE_BUCKET_SIZE + MIN_PROX_RANGE;
		float falseNegative = (r / RANGE_BUCKETS) * (falseNegative200 - falseNegative20) + falseNegative20;

		if (r == 0 && rangeReported <= MIN_PROX_RANGE)
		{
			//minimum range case
			p = (1 - falsePositive);
		}
		else if (r == RANGE_BUCKETS-1 && rangeReported >= bucketCenter)
		{
			//maximum range case
			p = (1 - falsePositive);
		}
		else
		{

			if (fabs(bucketCenter-range) < RANGE_BUCKET_SIZE/2)
			{
				//hit
				p = (1 - falsePositive);
			}
			else
			{
				if (bucketCenter < range)
				{
					//nearer buckets - false negatives?
					p = falseNegative / primarySector;
				}
				else
				{
					//further buckets - shadowed by false positive?
					p = falsePositive / (RANGE_BUCKETS - primarySector);
				}
			}

		}
		totalP += p;

		//allocate the likelihood across the affected sectors
		for (s = primarySector-1; s<primarySector+2; s++)
		{
			if (s < 0) s += PROX_SECTORS;
			if (s >= PROX_SECTORS) s -= PROX_SECTORS;

			probObstacle[s][r] *= contrib[s] * p;
		}
	}
}

void Normalize(int sector)
{
	int r;
	float total = 0.0;
	for (r = 0; r<RANGE_BUCKETS; r++)
	{
		total += probObstacle[sector][r];
	}
	for (r = 0; r<RANGE_BUCKETS; r++)
	{
		probObstacle[sector][r] /= total;
	}
}
void calcProximity(int sector)
{
	//determine obstacle presence and confidence for a sector
	int r;
	float maxProb = 0.0;
	int rangeBucket = -1;
	psProxStatus_enum oldStatus = sectors[sector].status;
	float oldConfidence = sectors[sector].confidence;
	Normalize(sector);

	//find highest probability
	for (r = 0; r<RANGE_BUCKETS-1; r++)
	{
		if ( probObstacle[sector][r] > maxProb)
		{
			maxProb = probObstacle[sector][r];
			rangeBucket = r;
		}
	}
	//enough for a hit?
	if (maxProb > MIN_CONFIDENCE)
	{
		sectors[sector].confidence = maxProb;
		sectors[sector].rangeBucket = rangeBucket;
		if (rangeBucket < NEAR_PROXIMITY)
		{
			sectors[sector].status = PROX_NEAR;
		}
		else
		{
			sectors[sector].status = PROX_FAR;
		}
	}
	else
	{
		sectors[sector].confidence = 1 - maxProb;
		sectors[sector].rangeBucket = RANGE_BUCKETS-1;
		sectors[sector].status = PROX_CLEAR;
	}
	if (sectors[sector].status != oldStatus
			|| fabs(sectors[sector].confidence - oldConfidence) > CONFIDENCE_CHANGE)
	{
		psMessage_t msg;
		psInitPublish(msg, PROXREP);
		msg.proxReportPayload.direction 	= sector;
		msg.proxReportPayload.confidence 	= (sectors[sector].confidence * 100);
		msg.proxReportPayload.range			= (int)((float)(sectors[sector].rangeBucket + 0.5) * RANGE_BUCKET_SIZE + MIN_PROX_RANGE);
		msg.proxReportPayload.status		= sectors[sector].status;
		NewBrokerMessage(&msg);
	}
}
//thread created to monitor messages
void ScannerProcessMessage(psMessage_t *msg)
{
	DEBUG_PRINT("Scanner: %s\n", psLongMsgNames[msg->header.messageType]);

	switch (msg->header.messageType)
	{
	case PICPROXREP:
	{
		int direction = msg->proxReportPayload.direction;

		if (direction < 4 || direction > 12)
		{
			//sonar
			Update(proxSectorBearings[direction], msg->proxReportPayload.range, MB1000);
		}
		else
		{
			//IR
			Update(proxSectorBearings[direction], msg->proxReportPayload.range, ANALOG_IR);
		}
		calcProximity(direction);

	}
	break;
	case FOCUS:
		//set focus of interest
		switch(msg->orientPayload.orientType)
		{
		case ORIENT_RELATIVE:
			focusHeading = msg->orientPayload.heading;
			break;
		case ORIENT_COMPASS:
			LogWarning("Compass orienting not supported");
			focusHeading = 0;
			break;
		}
		break;
	}
}
