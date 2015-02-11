//
//  SoftwareProfile.h
//  Hex
//
//  Created by Martin Lane-Smith on 6/17/14.
//  Copyright (c) 2014 Martin Lane-Smith. All rights reserved.
//

#ifndef SoftwareProfile_h
#define SoftwareProfile_h

#define INIT_SCRIPT_PATH "/root/lua/init"
#define BEHAVIOR_SCRIPT_PATH "/root/lua/behaviors"
#define HOOK_SCRIPT_PATH "/root/lua/hooks"
#define OTHER_SCRIPT_PATH "/root/lua/scripts"

//UART broker
//Rx P9_22
//Tx P9_21
#define PS_UART_DEVICE "/dev/ttyO2"
#define PS_UART_OVERLAY "BB-UART2"
#define PS_UART_BAUDRATE B57600

//Servo
#define SERVO_PWM_KEY   "P9_14"

//I2C Lidar
#define SONAR_GPIO          61      //P8_26 GPIO1_29
#define SONAR_I2C           2       //P9_19 (SCL), P9_20 (SDA)
#define SONAR_I2C_ADDRESS   0x70
#define TAKE_RANGE_READING  0x51
#define SONAR_GPIO_DELAY 	10		//ms

//range processing parameters
#define MIN_PROX_RANGE	20				//cm - no data closer
#define MAX_PROX_RANGE 200				//cm - ignore obstacles further
#define RANGE_BUCKETS	20				//[<=20cm | <18 x 10cm> | >200cm]
#define RANGE_BUCKET_SIZE ((MAX_PROX_RANGE-MIN_PROX_RANGE)/(RANGE_BUCKETS-2))	//10cm
#define PRIOR_PROBABILITY		0.1f	//10% chance of an existing obstacle, in absence of data
#define MIN_CONFIDENCE			0.5f	//require >50% to declare an obstacle present
#define NEAR_PROXIMITY			3		//first 3 buckets = near (<~40cm)
#define CONFIDENCE_CHANGE		0.2f	//confidence change to be reported

//sensor characteristics
enum SensorType 						{MB1242, MB1000, ANALOG_IR};
#define SCANNER_SONAR 					MB1242
#define MB1242_HALF_BEAM_WIDTH			20		//degrees
#define MB1242_FALSE_POSITIVE			0.1f 	//10% chance of false positive
#define MB1242_FALSE_NEGATIVE_20		0.01f	//1% chance of false negative at 20cm
#define MB1242_FALSE_NEGATIVE_200		1.0f	//100% chance of false negative at 200cm
#define MB1000_HALF_BEAM_WIDTH			30		//degrees
#define MB1000_FALSE_POSITIVE			0.1f 	//10% chance of false positive
#define MB1000_FALSE_NEGATIVE_20		0.01f	//1% chance of false negative at 20cm
#define MB1000_FALSE_NEGATIVE_200		1.0f	//100% chance of false negative at 200cm
#define ANALOG_IR_HALF_BEAM_WIDTH		20		//degrees
#define ANALOG_IR_FALSE_POSITIVE		0.1f 	//10% chance of false positive
#define ANALOG_IR_FALSE_NEGATIVE_20		0.05f	//5% chance of false negative at 20cm
#define ANALOG_IR_FALSE_NEGATIVE_200	1.0f	//100% chance of false negative at 200cm

//scanning parameters
#define SECTOR_WIDTH			(360.0/PROX_SECTORS)
#define LEFT_SCAN_LIMIT 		-90			//180 degree field
#define RIGHT_SCAN_LIMIT		90
#define SCAN_STEP			  	5
#define MAX_TIME_TO_SCAN		10			//max seconds between scans
#define SECTORS_PER_PASS		10			//optimum # sectors to scan per pass

//servo driving
#define SERVO_FREQUENCY			100.0		//Hz
#define SERVO_PERIOD			(1000.0/SERVO_FREQUENCY)	//ms
#define MIN_PULSE				0.9			//ms
#define MAX_PULSE				2.1			//ms
#define MID_PULSE				1.5			//ms
#define DUTY_CYCLE(angle)		((angle>=0 ? \
		(MID_PULSE + angle*(MAX_PULSE-MID_PULSE)/RIGHT_SCAN_LIMIT) : \
		(MID_PULSE + angle*(MIN_PULSE-MID_PULSE)/LEFT_SCAN_LIMIT))	/ SERVO_PERIOD)
#define MIN_MOVE_MS				50
#define MS_PER_STEP				10			//servo move time


//logfiles folder
#define LOGFILE_FOLDER "/root/logfiles"

//Logging levels
#define LOG_TO_SERIAL               LOG_ALL     //printed in real-time
#define SYSLOG_LEVEL                LOG_ALL  	//published log

#define MAX_ROUTINE 1
#define MAX_INFO 1
#define MAX_WARNING 1
#define MAX_ERROR 10

#endif
