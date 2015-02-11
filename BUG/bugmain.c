//
//  overmind.c
//
//  Created by Martin Lane-Smith on 7/2/14.
//  Copyright (c) 2014 Martin Lane-Smith. All rights reserved.
//
// Starts all robot processes

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include "lua.h"

#include "RobotTypes.h"
#include "SoftwareProfile.h"
#include "broker.h"
#include "overmind.h"
#include "SysLog.h"

#define PROCESS_NAME "overmind.elf"

void SIGHUPhandler(int sig);
int SIGHUPflag = 0;

int main(int argc, const char * argv[])
{
	int reply;

	//kill any others
	int *pids = pidof(PROCESS_NAME);
	//kill
	while (*pids != -1) {
		if (*pids != getpid())
		{
			kill(*pids, SIGTERM);
			printf("Killed pid %i (%s)\n", *pids, PROCESS_NAME);
		}
		pids++;
	}

	//init other threads, etc.
	//blackboard
	if ((reply=BlackboardInit()) != 0)
	{
		fprintf(stderr, "BlackboardInit() fail\n");
		return -1;
	}
	//syslog
	if ((reply=SysLogInit(argc, argv)) != 0)
	{
		fprintf(stderr, "SysLogInit() fail\n");
		return -1;
	}

	//start arbotix
	if ((reply=ArbotixInit()) < 0)
	{
		LogError("ArbotixInit()");
		return -1;
	}
	LogRoutine("ArbotixInit() OK");

	//start navigator
	if ((reply=NavigatorInit()) < 0)
	{
		LogError("NavigatorInit()");
		return -1;
	}
	LogRoutine("NavigatorInit() OK");

	//start scanner
//	if ((reply=ScannerInit()) < 0)
//	{
//		LogError("ScannerInit()");
//		return -1;
//	}
//	LogRoutine("ScannerInit() OK");
//
//	printf("scannerinit\n");

	//start behaviors
	if ((reply=BehaviorInit()) < 0)
	{
		LogError("BehaviorInit()");
		return -1;
	}

	printf("behaviorinit\n");

	//broker
	if ((reply=BrokerInit(argc, argv)) != 0)
	{
		fprintf(logFile, "BrokerInit() fail\n");
		fprintf(stderr, "BrokerInit() fail\n");
		return -1;
	}
	LogRoutine("BrokerInit() OK");

	if ((reply=SerialBrokerInit()) != 0)
	{
		LogError("SerialBrokerInit()");
		return -1;
	}
	LogRoutine("SerialBrokerInit() OK");

	LogRoutine("Init complete");

	if (getppid() == 1)
	{
		//child of init
		if (signal(SIGHUP, SIGHUPhandler) == SIG_ERR)
		{
			LogError("SIGHUP err: %s", strerror(errno));
		}
		else
		{
			LogRoutine("SIGHUP handler set");
		}
		fclose(stdout);
		fclose(stderr);
		stdout = fopen("/dev/null", "w");
		stderr = fopen("/dev/null", "w");
	}


	while(1)
	{
		sleep(1);
		if (SIGHUPflag)
		{
			psMessage_t msg;
			psInitPublish(msg, SCRIPTS);
			NewBrokerMessage(&msg);

			SIGHUPflag = 0;
		}
	}

	return 0;
}
//SIGHUP
void SIGHUPhandler(int sig)
{
	SIGHUPflag = 1;
}
