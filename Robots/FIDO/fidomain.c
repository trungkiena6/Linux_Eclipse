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

#include "SoftwareProfile.h"
#include "PubSubData.h"
#include "pubsub/pubsub.h"
#include "blackboard/blackboard.h"
#include "behavior/behavior.h"
#include "syslog/syslog.h"
#include "navigator/navigator.h"
#include "navigator/GPS.h"
#include "navigator/IMU.h"
#include "autopilot/autopilot.h"
#include "scanner/scanner.h"
#include "common.h"

int *pidof (char *pname);

FILE *mainDebugFile;

#ifdef MAIN_DEBUG
#define DEBUGPRINT(...) fprintf(stdout, __VA_ARGS__);fprintf(mainDebugFile, __VA_ARGS__);fflush(mainDebugFile);
#else
#define DEBUGPRINT(...) fprintf(mainDebugFile, __VA_ARGS__);fflush(mainDebugFile);
#endif

#define ERRORPRINT(...) fprintf(stdout, __VA_ARGS__);fprintf(mainDebugFile, __VA_ARGS__);fflush(mainDebugFile);

#define PROCESS_NAME "fido.elf"

void SIGHUPhandler(int sig);
int SIGHUPflag = 0;
void fatal_error_signal (int sig);

int main(int argc, const char * argv[])
{
	bool initFail = false;
	pthread_t reply;

	mainDebugFile = fopen("/root/logfiles/fidomain.log", "w");

	DEBUGPRINT("main() starting\n");

	//kill any others
	int *pids = pidof(PROCESS_NAME);
	//kill
	while (*pids != -1) {
		if (*pids != getpid())
		{
			kill(*pids, SIGTERM);
			DEBUGPRINT("Killed pid %i (%s)\n", *pids, PROCESS_NAME);
		}
		pids++;
	}

	DEBUGPRINT("main() loading dto1\n");

	//load universal dtos
	if (!load_device_tree("cape-universal")) {
		//error enabling pins
		ERRORPRINT( "*** Load 'cape-universal' fail\n");
		bool initFail = true;
	}

	DEBUGPRINT("main() loading dto2\n");
	if (!load_device_tree("cape-univ-hdmi")) {
		//error enabling pins
		ERRORPRINT( "*** Load 'cape-univ-hdmi' fail\n");
		bool initFail = true;
	}

	//init other threads, etc.
	DEBUGPRINT("main() start init\n");

	//initialize the broker queues
	BrokerQueueInit(100);

	//syslog
	if ((reply=SysLogInit(argc, argv)) != 0)
	{
		ERRORPRINT( "*** SysLogInit() fail\n");
		bool initFail = true;
	}
	DEBUGPRINT("SysLogInit() OK\n");

	//init blackboard
	if ((reply=BlackboardInit()) < 0)
	{
		ERRORPRINT("*** BlackboardInit() fail\n");
		bool initFail = true;
	}
	else {
		DEBUGPRINT("BlackboardInit() OK\n");
	}

	//PubSub broker
	if ((reply=PubSubInit()) < 0)
	{
		ERRORPRINT("*** PubSubInit() fail\n");
		bool initFail = true;
	}
	DEBUGPRINT("PubSubInit() OK\n");

	//Serial broker
	if ((reply=SerialBrokerInit()) < 0)
	{
		ERRORPRINT("*** SerialBrokerInit() fail\n");
		bool initFail = true;
	}
	DEBUGPRINT("SerialBrokerInit() OK\n");

	//Responder
	if ((reply=ResponderInit()) < 0)
	{
		ERRORPRINT("*** ResponderInit() fail\n");
		bool initFail = true;
	}
	DEBUGPRINT("ResponderInit() OK\n");

	//syslog & broker & serialbroker running
	//can now use LogError()

	//start navigator
	if ((reply=NavigatorInit()) < 0)
	{
		ERRORPRINT("*** NavigatorInit() fail\n");
		bool initFail = true;
	}
	else {
		DEBUGPRINT("NavigatorInit() OK\n");
	}

	//start autopilot
	if ((reply=AutopilotInit()) < 0)
	{
		ERRORPRINT("*** AutopilotInit() fail\n");
		bool initFail = true;
	}
	else {
	DEBUGPRINT("AutopilotInit() OK\n");
    }

	//start IMU
	if ((reply=IMUInit()) < 0)
	{
		ERRORPRINT("*** IMUInit() fail\n");
		bool initFail = true;
	}
	else {
		DEBUGPRINT("IMUInit() OK\n");
	}

	//start GPS
	if ((reply=GPSInit()) < 0)
	{
		ERRORPRINT("*** GPSInit() fail\n");
		bool initFail = true;
	}
	else {
		DEBUGPRINT("GPSInit() OK\n");
	}
#ifdef SCANNER
	//start scanner
	if ((reply=ScannerInit()) < 0)
	{
		ERRORPRINT("*** ScannerInit() fail\n");
		bool initFail = true;
	}
	else {
		DEBUGPRINT("ScannerInit() OK\n");
	}
#endif

	//start behaviors
	if ((reply=BehaviorInit()) < 0)
	{
		ERRORPRINT("*** BehaviorInit() fail\n");
		bool initFail = true;
	}
	else {
		DEBUGPRINT("BehaviorInit() OK\n");
    }

	if (initFail)
	{
		ERRORPRINT("*** Startup cancelled\n");
		return -1;
	}

	DEBUGPRINT("All Init() Done\n");

	LogRoutine("Init complete");

	if (getppid() == 1)
	{
		//child of init/systemd
		if (signal(SIGHUP, SIGHUPhandler) == SIG_ERR)
		{
			ERRORPRINT("SIGHUP err: %s\n", strerror(errno));
		}
		else
		{
			DEBUGPRINT("SIGHUP handler set\n");
		}
		//close stdio
		fclose(stdout);
		fclose(stderr);
		stdout = fopen("/dev/null", "w");
		stderr = fopen("/dev/null", "w");
	}
	else
	{
		signal(SIGILL, fatal_error_signal);
		signal(SIGABRT, fatal_error_signal);
		signal(SIGIOT, fatal_error_signal);
		signal(SIGBUS, fatal_error_signal);
		signal(SIGFPE, fatal_error_signal);
		signal(SIGSEGV, fatal_error_signal);
		signal(SIGTERM, fatal_error_signal);
		signal(SIGCHLD, fatal_error_signal);
		signal(SIGSYS, fatal_error_signal);
		signal(SIGCHLD, fatal_error_signal);
	}

	while(1)
	{
		sleep(1);
		if (SIGHUPflag)
		{
			//SIGHUP Signal used to reload lua scripts
			psMessage_t msg;
			psInitPublish(msg, RELOAD);
			RouteMessage(&msg);

			SIGHUPflag = 0;
		}
	}

	return 0;
}
//SIGHUP
void SIGHUPhandler(int sig)
{
	SIGHUPflag = 1;
	ERRORPRINT("SIGHUP signal\n");
}

//other signals
volatile sig_atomic_t fatal_error_in_progress = 0;
void fatal_error_signal (int sig)
{
	/* Since this handler is established for more than one kind of signal, it might still get invoked recursively by delivery of some other kind of signal. Use a static variable to keep track of that. */
	if (fatal_error_in_progress)
		raise (sig);
	fatal_error_in_progress = 1;

	ERRORPRINT("Signal %i raised\n", sig);

	/* Now reraise the signal. We reactivate the signal�s default handling, which is to terminate the process. We could just call exit or abort,
but reraising the signal sets the return status
from the process correctly. */
	signal (sig, SIG_DFL);
	raise (sig);
}
