//
//  BBBMsgFormatList.h
//  HexBug
//
//  Created by Martin Lane-Smith on 6/13/14.
//  Copyright (c) 2014 Martin Lane-Smith. All rights reserved.
//
//formatmacro(enum, type, var, size)

//Log
formatmacro(BBB_SYSLOG_PAYLOAD,bbbLogPayload_t,bbbLogPayload,-7)
//Arbotix
formatmacro(PS_ODOMETRY_PAYLOAD,psOdometryPayload_t,odometryPayload,sizeof(psOdometryPayload_t))
formatmacro(PS_MOVE_PAYLOAD,psMovePayload_t,movePayload,sizeof(psMovePayload_t))
formatmacro(PS_ORIENT_PAYLOAD,psOrientPayload_t,orientPayload,sizeof(psOrientPayload_t))

