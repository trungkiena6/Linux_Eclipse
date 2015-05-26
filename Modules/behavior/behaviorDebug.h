/*
 * behaviorDebug.h
 *
 *      Author: martin
 */

#ifndef BEHAVIORDEBUG_H_
#define BEHAVIORDEBUG_H_

#include "SoftwareProfile.h"

extern FILE *behDebugFile;

#ifdef BEHAVIOR_DEBUG
#define DEBUGPRINT(...) fprintf(stdout, __VA_ARGS__);fprintf(behDebugFile, __VA_ARGS__);fflush(behDebugFile);
#else
#define DEBUGPRINT(...)
#endif

#define ERRORPRINT(...) fprintf(stdout, __VA_ARGS__);fprintf(behDebugFile, __VA_ARGS__);fflush(behDebugFile);

#endif
