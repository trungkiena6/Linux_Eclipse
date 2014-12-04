//
//  BBBMessageList.h
//  HexBug
//
//  Created by Martin Lane-Smith on 6/13/14.
//  Copyright (c) 2014 Martin Lane-Smith. All rights reserved.
//

//#define messagemacro(enum, short name, qos, topic, payload, long name)
messagemacro(BBBLOG_MSG,"LG",PS_QOS2,BBB_LOG_TOPIC,BBB_SYSLOG_PAYLOAD,"BBB Log")
messagemacro(ARB_STATE,"AS",PS_QOS2,ARB_REPORT_TOPIC,PS_BYTE_PAYLOAD,"Arbotix State")
messagemacro(ODOMETRY,"OD",PS_QOS2,RAW_ODO_TOPIC,PS_ODOMETRY_PAYLOAD,"Odometry")
messagemacro(MOVEMENT,"MV",PS_QOS1,NAV_ACTION_TOPIC,PS_MOVE_PAYLOAD,"Movement")
messagemacro(ORIENT,"OR",PS_QOS2,NAV_ACTION_TOPIC,PS_ORIENT_PAYLOAD,"Orient")
messagemacro(FOCUS,"FO",PS_QOS2,SCAN_ACTION_TOPIC,PS_ORIENT_PAYLOAD,"Focus")
messagemacro(SCRIPTS,"SC",PS_QOS2,ANNOUNCEMENTS_TOPIC,PS_NO_PAYLOAD,"Scripts")
