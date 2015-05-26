/*
 * luaglobals.c
 *
 *  Created on: Aug 13, 2014
 *      Author: martin
 */
//creates and updates robot-related global variables in the LUA environment

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include <dirent.h>
#include <string.h>
#include <math.h>
#include "lua.h"

#include "PubSubData.h"
#include "pubsub/pubsub.h"
#include "blackboard/blackboard.h"
#include "behavior/behavior.h"
#include "behavior/behaviorDebug.h"
#include "syslog/syslog.h"


//load all (constant) lua globals with default values
int InitLuaGlobals(lua_State *L)
{
	int i;
	//initialize globals

	//notifications
	lua_createtable(L, NOTIFICATION_COUNT, 0);		//Notification table
	int table = lua_gettop(L);
	for (i=1; i< NOTIFICATION_COUNT; i++)
	{
		lua_pushstring(L, eventNames[i]);
		lua_pushinteger(L, i);
		lua_settable(L, table);
	}
	lua_setglobal(L, "notification");				//list of notifications

	lua_createtable(L, COMMAND_COUNT, 0);			//User system commands constants table
	table = lua_gettop(L);
	for (i=0; i< COMMAND_COUNT; i++)
	{
		lua_pushstring(L, stateCommandNames[i]);
		lua_pushnumber(L, i);
		lua_settable(L, table);
	}
	lua_setglobal(L, "user_state_command");

	lua_createtable(L, POWER_STATE_COUNT, 0);		//Power state constants table
	table = lua_gettop(L);
	for (i=0; i< POWER_STATE_COUNT; i++)
	{
		lua_pushstring(L, powerStateNames[i]);
		lua_pushnumber(L, i);
		lua_settable(L, table);
	}
	lua_setglobal(L, "system_power_state");

	lua_createtable(L, OVERMIND_ACTION_COUNT, 0);		//System commands constants table
	table = lua_gettop(L);
	for (i=0; i< OVERMIND_ACTION_COUNT; i++)
	{
		lua_pushstring(L, ovmCommandNames[i]);
		lua_pushnumber(L, i);
		lua_settable(L, table);
	}
	lua_setglobal(L, "system_command");

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

	//directions
	static char *proxDirection[] = PROX_DIRECTION_NAMES;
	lua_createtable(L, 0, 0);		//directions table
	table = lua_gettop(L);
	for (i=0; i< PROX_SECTORS; i+=2)
	{
		lua_pushstring(L, proxDirection[i/2]);
		lua_pushnumber(L, i);
		lua_settable(L, table);
	}
	lua_setglobal(L, "sector");

	static char *moveType[] = MOVE_TYPE_NAMES;
	lua_createtable(L, MOVE_TYPE_COUNT, 0);			//Move Type constants table
	table = lua_gettop(L);
	for (i=0; i< MOVE_TYPE_COUNT; i++)
	{
		lua_pushstring(L, moveType[i]);
		lua_pushnumber(L, i);
		lua_settable(L, table);
	}
	lua_setglobal(L, "move");

	static char *orientType[] = ORIENT_TYPE_NAMES;
	lua_createtable(L, ORIENT_TYPE_COUNT, 0);		//Orient Type constants table
	table = lua_gettop(L);
	for (i=0; i< ORIENT_TYPE_COUNT; i++)
	{
		lua_pushstring(L, orientType[i]);
		lua_pushnumber(L, i);
		lua_settable(L, table);
	}
	lua_setglobal(L, "orient");

	return LoadAllScripts(L);
}

//copy data into lua globals from relevant messages
int UpdateGlobalsFromMessage(lua_State *L, psMessage_t *msg)
{
	int table, i;
	switch (msg->header.messageType)
	{
	case BATTERY:
		lua_pushnumber(L, msg->batteryPayload.status);
		lua_pushnumber(L, msg->batteryPayload.volts);
		InvokeScript("batteryhook");
		break;
	case TICK_1S:
		lua_pushnumber(L, msg->tickPayload.systemPowerState);
		InvokeScript("tickhook");
		break;
	case NEW_SETTING:
		lua_getglobal(L, "setting");
		table = lua_gettop(L);
		lua_pushstring(L, msg->nameFloatPayload.name);
		lua_pushnumber(L, msg->nameFloatPayload.value);
		lua_settable(L, table);
		lua_pushstring(L, msg->nameFloatPayload.name);
		lua_pushnumber(L, msg->nameFloatPayload.value);
		InvokeScript("settinghook");
		break;
	case SET_OPTION:
		lua_getglobal(L, "option");
		table = lua_gettop(L);
		lua_pushstring(L, msg->nameIntPayload.name);
		lua_pushnumber(L, msg->nameIntPayload.value);
		lua_settable(L, table);
		lua_pushstring(L, msg->nameIntPayload.name);
		lua_pushnumber(L, msg->nameIntPayload.value);
		InvokeScript("optionhook");
		break;
	case NOTIFICATION:
		lua_pushstring(L, msg->nameIntPayload.name);
		lua_pushnumber(L, msg->nameIntPayload.value);
		InvokeScript("notificationhook");
		break;
	default:
		//ignores other messages
		break;
	}
	return 0;
}

void SetGlobal(lua_State *L, const char *name, float value)
{
	lua_pushnumber(L, value);
	lua_setglobal(L, name);
}

//----------------------------------------LOADING LUA SCRIPTS
int LoadAllScripts(lua_State *L)
{
	int reply = LoadLuaScripts(L, INIT_SCRIPT_PATH);	//load init/default scripts first
	reply += LoadLuaScripts(L, MAIN_SCRIPT_PATH);		//load main scripts
	reply += LoadLuaScripts(L, BEHAVIOR_SCRIPT_PATH);	//load behavior tree scripts
	reply += LoadLuaScripts(L, HOOK_SCRIPT_PATH);		//load hook scripts

	if (reply < 0) return -1;
	else return 0;
}

//private
const char * ChunkReader(lua_State *L, void *data, size_t *size);

//load all lua scripts in a folder
int LoadLuaScripts(lua_State *L, char *scriptFolder)	//load all scripts in folder
{
	DIR *dp;
	struct dirent *ep;

	DEBUGPRINT("Script Folder: %s\n", scriptFolder);

	dp = opendir (scriptFolder);
	if (dp != NULL) {
		while ((ep = readdir (dp))) {

//			DEBUGPRINT("File: %s\n", ep->d_name);

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
					ERRORPRINT("Error: %s\n",errormsg);
//					LogError("%s error: %s",name, errormsg);
					(void) closedir (dp);
					return -1;
				}
				else
					DEBUGPRINT("loaded %s\n", name);
			}
			else
			{
				ERRORPRINT("failed to load %s - %i\n", name, loadReply);
				(void) closedir (dp);
				return -1;
			}
		}
		(void) closedir (dp);
	}
	return 0;
}

//load a lua chunk from a file
FILE *chunkFile;
int LoadChunk(lua_State *L, char *name, char *path)
{
	int loadReply;
	DEBUGPRINT("Loading %s: %s\n",name, path);

	chunkFile = fopen(path, "r");

	if (!chunkFile)
	{
		DEBUGPRINT("Failed to open %s\n",path);
		return -1;
	}

	loadReply = lua_load(L, ChunkReader, (void*) 0, name, "bt");

	fclose(chunkFile);

	if  (loadReply == LUA_ERRSYNTAX)
	{
		const char *errormsg = lua_tostring(L, -1);
		ERRORPRINT("lua: %s syntax error: %s\n", name, errormsg);
//		LogError("%s syntax error - %s", name, errormsg);
		return -1;
	}
	else if (loadReply != LUA_OK)
	{
		ERRORPRINT("lua_load of %s fail: %i\n", path, loadReply);
//		LogError("lua_load of %s fail: %i", path, loadReply);
		return -1;
	}
	return 0;
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
