//
//  BBBMessageFormats.h
//  Hex
//
//  Created by Martin Lane-Smith on 6/16/14.
//  Copyright (c) 2014 Martin Lane-Smith. All rights reserved.
//

#include <unistd.h>

//BBB_SYSLOG,

#define BBB_MAX_LOG_TEXT 64

typedef struct {
    uint8_t severity;
    char file[5];
    char text[BBB_MAX_LOG_TEXT];
} bbbLogPayload_t;

//PS_ODOMETRY

typedef struct {
    int8_t xMovement;	//cm
    int8_t yMovement;
    int8_t zRotation;	//degrees
} psOdometryPayload_t;

//PS_MOVE_PAYLOAD

typedef struct {
		uint8_t moveType;
		union {
			struct {
				int16_t x;			//cm local frame
				int16_t y;			//cm
			};
			struct {
				int16_t northing;	//cm compass frame
				int16_t easting;	//cm
			};
		};
} psMovePayload_t;
typedef enum {MOVE_LOCAL, MOVE_COMPASS, MOVE_ABSOLUTE,  MOVE_TYPE_COUNT} MoveType_enum;
#define MOVE_TYPE_NAMES {"relative", "compass", "absolute"}

//PS_ORIENT_PAYLOAD

typedef struct {
	int16_t heading;			//degrees
	uint8_t orientType;
} psOrientPayload_t;
typedef enum {ORIENT_RELATIVE, ORIENT_COMPASS, ORIENT_TYPE_COUNT} OrientType_enum;
#define ORIENT_TYPE_NAMES {"relative", "compass"}
