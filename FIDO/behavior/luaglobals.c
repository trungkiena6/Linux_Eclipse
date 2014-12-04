/*
 * luaglobals.c
 *
 *  Created on: Aug 13, 2014
 *      Author: martin
 */
//creates and updates robot-related global variables in the LUA environment

#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "lua.h"

#include "RobotTypes.h"
#include "broker.h"
#include "fidobrain.h"
#include "SysLog.h"
#include "Blackboard.h"

//private
const char * ChunkReader(lua_State *L, void *data, size_t *size);

char *batteryStateNames[] = BATTERY_STATES;
char *systemStateNames[] = SYS_STATE_NAMES;
char *fidoStateNames[] = FIDO_STATE_NAMES;

//load all robot lua globals with default values
int InitLuaGlobals(lua_State *L)
{
	int i;
	//initialize globals

	//battery
	SetGlobal(L, "batteryvolts",  0.0f);
	SetGlobal(L, "batterystatus", (float) BATT_UNKNOWN);
	lua_createtable(L, BATT_COUNT, 0);		//BATTERY STATE constants table
	int table = lua_gettop(L);
	for (i=0; i< BATT_COUNT; i++)
	{
		lua_pushstring(L, batteryStateNames[i]);
		lua_pushnumber(L, i);
		lua_settable(L, table);
	}
	lua_setglobal(L, "battery");

	SetGlobal(L, "systemstate", (float) STATE_UNSPECIFIED);
	lua_createtable(L, SYSTEM_STATE_COUNT, 0);		//SYSTEM STATE constants table
	table = lua_gettop(L);
	for (i=0; i< SYSTEM_STATE_COUNT; i++)
	{
		lua_pushstring(L, systemStateNames[i]);
		lua_pushnumber(L, i);
		lua_settable(L, table);
	}
	lua_setglobal(L, "system");

	SetGlobal(L, "bugstate", (float) FIDO_STATE_UNKNOWN);
	lua_createtable(L, FIDO_STATE_COUNT, 0);		//BUG STATE constants table
	table = lua_gettop(L);
	for (i=0; i< FIDO_STATE_COUNT; i++)
	{
		lua_pushstring(L, fidoStateNames[i]);
		lua_pushnumber(L, i);
		lua_settable(L, table);
	}
	lua_setglobal(L, "fido");

	//settings and options
	lua_createtable(L, 0, 0);		//settings table
	table = lua_gettop(L);
#define settingmacro(n, var, min, max, def) lua_pushstring(L, n);\
	lua_pushnumber(L, def);\
	lua_settable(L, table);
#include "settings.h"
#undef settingmacro
	lua_setglobal(L, "setting");

	lua_createtable(L, 0, 0);		//options table
	table = lua_gettop(L);
#define optionmacro(n, var, min, max, def) lua_pushstring(L, n);\
		lua_pushnumber(L, def);\
		lua_settable(L, table);
#include "options.h"
#undef optionmacro
	lua_setglobal(L, "option");

	//proximity
	lua_pushnumber(L, 0);
	lua_setglobal(L, "msgdirection");

	lua_createtable(L, PROX_SECTORS, 0);		//prox table
	int proxTable = lua_gettop(L);
	char *proxDirection[] = PROX_DIRECTION_NAMES;

	for (i=0; i < PROX_SECTORS; i++)
	{
		lua_pushnumber(L, i);

		lua_createtable(L, 0, 4);	//direction table
		int dirTable = lua_gettop(L);

		lua_pushstring(L, "range");
		lua_pushnumber(L, 0);
		lua_settable(L, dirTable);

		lua_pushstring(L, "status");
		lua_pushnumber(L, 0);
		lua_settable(L, dirTable);

		lua_pushstring(L, "confidence");
		lua_pushnumber(L, 0);
		lua_settable(L, dirTable);

		lua_pushstring(L, "direction");
		lua_pushstring(L, proxDirection[i/2]);
		lua_settable(L, dirTable);

		lua_settable(L, proxTable);		//add to prox table, indexed by direction
	}
	lua_setglobal(L, "proximitysensor");

	//directions
	lua_createtable(L, 0, 0);		//directions table
	table = lua_gettop(L);
	for (i=0; i< PROX_SECTORS; i+=2)
	{
		lua_pushstring(L, proxDirection[i/2]);
		lua_pushnumber(L, i);
		lua_settable(L, proxTable);
	}
	lua_setglobal(L, "direction");

	//status codes
	lua_createtable(L, 0, 0);		//status code table
	table = lua_gettop(L);
	char *proxStatus[] = PROX_STATUS_NAMES;
	for (i=0; i< PROX_STATUS_COUNT; i++)
	{
		lua_pushstring(L, proxStatus[i]);
		lua_pushnumber(L, i);
		lua_settable(L, proxTable);
	}
	lua_setglobal(L, "proximity");

	lua_createtable(L, 0, 5);		//pose table
	int poseTable = lua_gettop(L);

	//heading
	lua_pushstring(L, "heading");

	lua_createtable(L, 0, 2);
	table = lua_gettop(L);

	lua_pushstring(L, "degrees");
	lua_pushnumber(L, 0);
	lua_settable(L, table);

	lua_pushstring(L, "variance");
	lua_pushnumber(L, 0);
	lua_settable(L, table);

	lua_settable(L, poseTable);		//add to pose table

	//pitch
	lua_pushstring(L, "pitch");

	lua_createtable(L, 0, 2);
	table = lua_gettop(L);

	lua_pushstring(L, "degrees");
	lua_pushnumber(L, 0);
	lua_settable(L, table);

	lua_pushstring(L, "variance");
	lua_pushnumber(L, 0);
	lua_settable(L, table);

	lua_settable(L, poseTable);		//add to pose table

	//roll
	lua_pushstring(L, "roll");

	lua_createtable(L, 0, 2);
	table = lua_gettop(L);

	lua_pushstring(L, "degrees");
	lua_pushnumber(L, 0);
	lua_settable(L, table);

	lua_pushstring(L, "variance");
	lua_pushnumber(L, 0);
	lua_settable(L, table);

	lua_settable(L, poseTable);		//add to pose table

	//northing
	lua_pushstring(L, "northing");

	lua_createtable(L, 0, 2);
	table = lua_gettop(L);

	lua_pushstring(L, "cm");
	lua_pushnumber(L, 0);
	lua_settable(L, table);

	lua_pushstring(L, "variance");
	lua_pushnumber(L, 0);
	lua_settable(L, table);

	lua_settable(L, poseTable);		//add to pose table

	//easting
	lua_pushstring(L, "easting");

	lua_createtable(L, 0, 2);
	table = lua_gettop(L);

	lua_pushstring(L, "cm");
	lua_pushnumber(L, 0);
	lua_settable(L, table);

	lua_pushstring(L, "variance");
	lua_pushnumber(L, 0);
	lua_settable(L, table);

	lua_settable(L, poseTable);		//add to pose table

	lua_setglobal(L, "pose");

	char *moveType[] = MOVE_TYPE_NAMES;
	lua_createtable(L, MOVE_TYPE_COUNT, 0);		//Move Type constants table
	table = lua_gettop(L);
	for (i=0; i< MOVE_TYPE_COUNT; i++)
	{
		lua_pushstring(L, moveType[i]);
		lua_pushnumber(L, i);
		lua_settable(L, table);
	}
	lua_setglobal(L, "move");

	char *orientType[] = ORIENT_TYPE_NAMES;
	lua_createtable(L, ORIENT_TYPE_COUNT, 0);		//Orient Type constants table
	table = lua_gettop(L);
	for (i=0; i< ORIENT_TYPE_COUNT; i++)
	{
		lua_pushstring(L, orientType[i]);
		lua_pushnumber(L, i);
		lua_settable(L, table);
	}
	lua_setglobal(L, "orient");

	SetGlobal(L, "xspeed", 0);
	SetGlobal(L, "yspeed", 0);
	SetGlobal(L, "zrotatespeed", 0);
	SetGlobal(L, "motionsector", 0);

	LoadAllScripts(L);

	return 0;
}

//copy data into lua globals from relevant messages
int UpdateGlobalsFromMessage(lua_State *L, psMessage_t *msg)
{
	int table;
	switch (msg->header.messageType)
	{
	case BATTERY:
		SetGlobal(L, "batterystatus",  msg->batteryPayload.status);
		SetGlobal(L, "batteryvolts",  msg->batteryPayload.volts);
		InvokeScript("batteryhook");
		break;
	case TICK_1S:
		SetGlobal(L, "systemstate", msg->tickPayload.sysState);
		SetGlobal(L, "fidostate", msg->tickPayload.fidoState);

		printf("S=%s, F=%s\n",
				systemStateNames[setSystemState.state],
				fidoStateNames[fidoSystemState.state]);

		InvokeScript("tickhook");
		break;
	case MOVE:
		SetGlobal(L, "xspeed", msg->threeFloatPayload.xSpeed);
		SetGlobal(L, "yspeed", msg->threeFloatPayload.ySpeed);
		SetGlobal(L, "zrotatespeed", msg->threeFloatPayload.zRotateSpeed);

		float direction = atan2(msg->threeFloatPayload.ySpeed, msg->threeFloatPayload.ySpeed);
		float degrees = (direction * 360.0) / (2.0 * 3.14159);
		degrees += 180.0 - 22.5;
		int sector = (int) degrees / 45;
		sector = sector % 8;

		SetGlobal(L, "motionsector", sector);

		break;
	case PROXREP:
	{
		int direction = msg->proxReportPayload.direction;
		if (direction >= 0 && direction < PROX_SECTORS)
		{
			lua_pushnumber(L, direction);
			lua_setglobal(L, "msgdirection");

			lua_getglobal(L, "proximitysensor");
			int pTable = lua_gettop(L);

			lua_pushnumber(L, direction);
			lua_gettable(L, pTable);
			int dirTable = lua_gettop(L);

			lua_pushstring(L, "range");
			lua_pushnumber(L, msg->proxReportPayload.range);
			lua_settable(L, dirTable);

			lua_pushstring(L, "status");
			lua_pushnumber(L, msg->proxReportPayload.status);
			lua_settable(L, dirTable);

			lua_pushstring(L, "confidence");
			lua_pushnumber(L, msg->proxReportPayload.confidence);
			lua_settable(L, dirTable);

			InvokeScript("proxhook");
		}
	}
	break;
	case POSE:
	{
		lua_getglobal(L, "pose");
		int pTable = lua_gettop(L);

		lua_getfield(L, pTable, "heading");
		table = lua_gettop(L);

		lua_pushstring(L, "degrees");
		lua_pushnumber(L, msg->posePayload.heading.degrees);
		lua_settable(L, table);

		lua_pushstring(L, "variance");
		lua_pushnumber(L, msg->posePayload.heading.variance);
		lua_settable(L, table);

		lua_pop(L, 1);

		lua_getfield(L, pTable, "pitch");
		table = lua_gettop(L);

		lua_pushstring(L, "degrees");
		lua_pushnumber(L, msg->posePayload.pitch.degrees);
		lua_settable(L, table);

		lua_pushstring(L, "variance");
		lua_pushnumber(L, msg->posePayload.pitch.variance);
		lua_settable(L, table);

		lua_pop(L, 1);

		lua_getfield(L, pTable, "roll");
		table = lua_gettop(L);

		lua_pushstring(L, "degrees");
		lua_pushnumber(L, msg->posePayload.roll.degrees);
		lua_settable(L, table);

		lua_pushstring(L, "variance");
		lua_pushnumber(L, msg->posePayload.roll.variance);
		lua_settable(L, table);

		lua_pop(L, 1);

		lua_getfield(L, pTable, "northing");
		table = lua_gettop(L);

		lua_pushstring(L, "cm");
		lua_pushnumber(L, msg->posePayload.northing.cm);
		lua_settable(L, table);

		lua_pushstring(L, "variance");
		lua_pushnumber(L, msg->posePayload.northing.variance);
		lua_settable(L, table);

		lua_pop(L, 1);

		lua_getfield(L, pTable, "easting");
		table = lua_gettop(L);

		lua_pushstring(L, "cm");
		lua_pushnumber(L, msg->posePayload.easting.cm);
		lua_settable(L, table);

		lua_pushstring(L, "variance");
		lua_pushnumber(L, msg->posePayload.easting.variance);
		lua_settable(L, table);

		lua_pop(L, 1);
		lua_pop(L, 1);

		InvokeScript("posehook");
	}
	break;
	case NEW_SETTING:
		lua_getglobal(L, "setting");
		table = lua_gettop(L);
		lua_pushstring(L, msg->nameFloatPayload.name);\
		lua_pushnumber(L, msg->nameFloatPayload.value);\
		lua_settable(L, table);
		break;
	case SET_OPTION:
		lua_getglobal(L, "option");
		table = lua_gettop(L);
		lua_pushstring(L, msg->nameIntPayload.name);\
		lua_pushnumber(L, msg->nameIntPayload.value);\
		lua_settable(L, table);
		break;
	case SCRIPTS:
		DEBUG_PRINT("reload scripts\n");
		LoadAllScripts(L);
		break;
	default:
		//ignores other messages
		break;
	}
	return 0;
}

void LoadAllScripts(lua_State *L)
{
	LoadLuaScripts(L, INIT_SCRIPT_PATH);		//load init scripts
	LoadLuaScripts(L, BEHAVIOR_SCRIPT_PATH);	//load behavior scripts
	LoadLuaScripts(L, HOOK_SCRIPT_PATH);		//load hook scripts
	LoadLuaScripts(L, OTHER_SCRIPT_PATH);		//load other scripts
}

//load all lua scripts in a folder
int LoadLuaScripts(lua_State *L, char *scriptFolder)	//load all scripts in folder
{
	DIR *dp;
	struct dirent *ep;

	DEBUG_PRINT("Script Folder: %s\n", scriptFolder);

	dp = opendir (scriptFolder);
	if (dp != NULL) {
		while ((ep = readdir (dp))) {

//			DEBUG_PRINT("File: %s\n", ep->d_name);

			//check whether the file is a lua script
			int len = strlen(ep->d_name);
			if (len < 5) continue;						//name too short

			char *suffix = ep->d_name + len - 4;
			if (strcmp(suffix, ".lua") != 0)
			{
				continue;	//wrong extension
			}

			int loadReply;
			char path[99];
			sprintf(path, "%s/%s", scriptFolder, ep->d_name);

			char *name = ep->d_name;
			name[len - 4] = '\0'; 						//chop off .lua suffix for name

			loadReply = LoadChunk(L, name, path);

			if (loadReply == LUA_OK)
			{
				//now call the chunk to initialize itself
				int status = lua_pcall(L, 0, 0, 0);

				if (status)
				{
					const char *errormsg = lua_tostring(L, -1);
					fprintf(stderr,"Error: %s\n",errormsg);
					LogError("%s error: %s",name, errormsg);
				}
				else
					LogRoutine("loaded %s", name);
			}
			else
			{
				LogError("failed to load %s - %i", name, loadReply);
			}
		}
		(void) closedir (dp);
	}
	return 0;
}

void SetGlobal(lua_State *L, const char *name, float value)
{
	lua_pushnumber(L, value);
	lua_setglobal(L, name);
}

//load a lua chunk from a file
FILE *chunkFile;
int LoadChunk(lua_State *L, char *name, char *path)
{
	int loadReply;
	DEBUG_PRINT("Loading %s: %s\n",name, path);

	chunkFile = fopen(path, "r");

	if (!chunkFile)
	{
		DEBUG_PRINT("Failed to open %s\n",path);
		return -1;
	}

	loadReply = lua_load(L, ChunkReader, (void*) 0, name, "bt");

	fclose(chunkFile);

	if  (loadReply == LUA_ERRSYNTAX)
	{
		const char *errormsg = lua_tostring(L, -1);
		fprintf(stderr, "lua: %s syntax error: %s\n", name, errormsg);
		LogError("%s syntax error - %s", name, errormsg);
	}
	else if (loadReply != LUA_OK)
	{
		fprintf(stderr, "lua_load of %s fail: %i\n", path, loadReply);
		LogError("lua_load of %s fail: %i", path, loadReply);
	}
	return loadReply;
}
//file reader call out
char buffer[100];
const char * ChunkReader(lua_State *L, void *data, size_t *size)
{
	int c;
	char *next = buffer;
	*size = 0;
	do {
		c = getc(chunkFile);
		if (c != EOF)
		{
			*next++ = c;
			(*size)++;
		}
		else
		{
			if (*size == 0) return NULL;
		}
	} while  (c != EOF && *size < 100);
	return buffer;
}
