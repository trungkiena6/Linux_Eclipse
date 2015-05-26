/*
 * ScriptPlayer.c
 *
 *  Created on: Aug 10, 2014
 *      Author: martin
 */
// Controller of the LUA subsystem

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include <dirent.h>
#include <string.h>

#include "lua.h"
#include "lualib.h"

#include "PubSubData.h"
#include "pubsub/pubsub.h"
#include "blackboard/blackboard.h"
#include "behavior/behavior.h"
#include "behavior/behaviorDebug.h"
#include "syslog/syslog.h"

lua_State	*btLuaState = NULL;

//lua call-backs
static int ChangePowerState(lua_State *L);	//ChangePowerState(power.standby)
static int Activating(lua_State *L);		//Activating("sleep")

//logging
static int Print(lua_State *L);				//Print("...")
static int Log(lua_State *L);				//Log("...")
static int Info(lua_State *L);
static int Warning(lua_State *L);
static int Error(lua_State *L);

//settings and options
static int ChangeSetting(lua_State *L);		//ChangeSetting("name", value)
static int ChangeOption(lua_State *L);

//actions
static int Focus(lua_State *L);				//Focus(orient.relative, direction.front)
static int Orient(lua_State *L);			//OrientBody((orient.relative, direction.left)
static int Move(lua_State *L);				//Move(move.local, x, y)
static int GetFix(lua_State *L);			//GetFix(mS)

//allocator for lua state
static void *l_alloc (void *ud, void *ptr, size_t osize, size_t nsize)
{
	(void)ud;  (void)osize;  /* not used */
	if (nsize == 0)
	{
		free(ptr);
		return NULL;
	}
	else
		return realloc(ptr, nsize);
}

int InitScriptingSystem()
{
	//get a new LUA state
	if (btLuaState)
	{
		//close if previously opened
		lua_close(btLuaState);
	}
	btLuaState = lua_newstate(l_alloc, NULL);

	if (btLuaState == NULL)
	{
		ERRORPRINT("luaState create fail\n");
	   	return -1;
	}
	//open standard libraries
	luaL_openlibs(btLuaState);

	luaopen_math(btLuaState);
	lua_setglobal(btLuaState, "math");
	luaopen_string(btLuaState);
	lua_setglobal(btLuaState, "string");

	//register call-backs
	lua_pushcfunction(btLuaState, ChangePowerState);	//set MCP power state
	lua_setglobal(btLuaState, "ChangePowerState");
	lua_pushcfunction(btLuaState, Activating);			//Report Activating Behavior
	lua_setglobal(btLuaState, "Activating");
	lua_pushcfunction(btLuaState, Print);				//Print Message
	lua_setglobal(btLuaState, "Print");
	lua_pushcfunction(btLuaState, Log);					//Log Message
	lua_setglobal(btLuaState, "Log");
	lua_pushcfunction(btLuaState, Info);				//Log Message
	lua_setglobal(btLuaState, "Info");
	lua_pushcfunction(btLuaState, Warning);				//Log Message
	lua_setglobal(btLuaState, "Warning");
	lua_pushcfunction(btLuaState, Error);				//Log Message
	lua_setglobal(btLuaState, "Error");

	lua_pushcfunction(btLuaState, ChangeSetting);		//change setting
	lua_setglobal(btLuaState, "ChangeSetting");
	lua_pushcfunction(btLuaState, ChangeOption);		//change option
	lua_setglobal(btLuaState, "ChangeOption");

	lua_pushcfunction(btLuaState, Focus);				//issue focus command
	lua_setglobal(btLuaState, "Focus");

	lua_pushcfunction(btLuaState, Orient);				//issue orient command
	lua_setglobal(btLuaState, "Orient");

	lua_pushcfunction(btLuaState, Move);				//issue move command
	lua_setglobal(btLuaState, "Move");

	lua_pushcfunction(btLuaState, GetFix);				//issue move command
	lua_setglobal(btLuaState, "GetFix");

	return InitLuaGlobals(btLuaState);					//load more system globals
}

int ScriptProcessMessage(psMessage_t *msg)
{
	if (!btLuaState) return -1;

	switch (msg->header.messageType)
	{
	case RELOAD:
		DEBUGPRINT("Reload scripts\n");
		InitScriptingSystem();
		break;

	case ACTIVATE:
		DEBUGPRINT("Activate: %s\n", msg->namePayload.name);

		lua_getglobal(btLuaState, "activate");						//reference to 'activate(...)
		if (lua_isfunction(btLuaState,lua_gettop(btLuaState)))
		{
			//activate exists
			lua_pushstring(btLuaState, msg->namePayload.name);		//name of target function
			lua_getglobal(btLuaState, msg->namePayload.name);		//reference to the function itself
			if (lua_isfunction(btLuaState,lua_gettop(btLuaState)))
			{
				//target function exists
				int status = lua_pcall(btLuaState, 2, 0, 0);
				if (status)
				{
					const char *errormsg = lua_tostring(btLuaState, -1);
					ERRORPRINT("Script activate(%s), Error: %s\n",msg->namePayload.name,errormsg);
					return -1;
				}
			}
			else
			{
				//target function does not exist
				ERRORPRINT("Global %s is of type %s\n",msg->namePayload.name,lua_typename(btLuaState,lua_type(btLuaState,lua_gettop(btLuaState))) );
			}
		}
		else
		{
			//activate does not exist
			ERRORPRINT("Global 'activate' is of type %s\n",lua_typename(btLuaState,lua_type(btLuaState,lua_gettop(btLuaState))) );
		}
		break;

	default:
		return UpdateGlobalsFromMessage(btLuaState, msg);
	}
	return 0;
}

int InvokeScript(char *scriptName)
{
	if (!btLuaState) return -1;

	lua_getglobal(btLuaState, scriptName);

	if (lua_isfunction(btLuaState,lua_gettop(btLuaState)))
	{
//		DEBUGPRINT("LUA: calling %s\n", scriptName);
		int status = lua_pcall(btLuaState, 0, 0, 0);
		if (status)
		{
			const char *errormsg = lua_tostring(btLuaState, -1);
			ERRORPRINT("Script %s, Error: %s\n",scriptName,errormsg);
			return -1;
		}
		else
		{
//			DEBUGPRINT("LUA: returned from %s\n", scriptName);
			return 0;
		}
	}
	else
	{
		DEBUGPRINT("Global %s is of type %s\n",scriptName,lua_typename(btLuaState,lua_type(btLuaState,lua_gettop(btLuaState))) );
		ERRORPRINT("No %s script!\n", scriptName);
		return -1;
	}
}

//report available scripts
int AvailableScripts()
{
	psMessage_t msg;
	int i;
	int messageCount = 0;

	psInitPublish(msg, SCRIPT);

	lua_getglobal(btLuaState, "Activities");
	int table = lua_gettop( btLuaState);

	if (lua_istable(btLuaState, table) == 0) return 0;	//not a table

    lua_pushnil(btLuaState);  /* first key */
    while (lua_next(btLuaState, table) != 0) {
      /* uses 'key' (at index -2) and 'value' (at index -1) */
		strncpy(msg.namePayload.name, lua_tostring(btLuaState, -1), PS_NAME_LENGTH);
		RouteMessage(&msg);
		messageCount++;
      /* removes 'value'; keeps 'key' for next iteration */
      lua_pop(btLuaState, 1);
    }

	return messageCount;
}

//call-backs

//result codes
int success(lua_State *L)
{
	lua_pushstring(L, "success");
	return 1;
}
int running(lua_State *L)
{
	lua_pushstring(L, "running");
	return 1;
}int fail(lua_State *L)
{
	lua_pushstring(L, "fail");
	return 1;
}


//change Power state
int lastSentState;
static int ChangePowerState(lua_State *L)
{
	psMessage_t msg;
	int stateCommand = lua_tointeger(L, 1);
	if (stateCommand != lastSentState)
	{
		psInitPublish(msg, OVM_COMMAND);
		msg.bytePayload.value = stateCommand;
		RouteMessage(&msg);
		DEBUGPRINT("lua: Change State(%s)\n", ovmCommandNames[stateCommand]);
		lastSentState = stateCommand;
	}
	return success(L);
}

//Activating Behavior
char currentName[PS_NAME_LENGTH+1] = "";
static int Activating(lua_State *L)			//Activating("..")
{
	const char *name = lua_tostring(L,1);
	if (strncmp(currentName, name, PS_NAME_LENGTH) != 0)
	{
		//not a duplicate
		psMessage_t msg;
		psInitPublish(msg, ACTIVITY);
		strncpy(msg.namePayload.name, name, PS_NAME_LENGTH);
		strncpy(currentName, name, PS_NAME_LENGTH);
		RouteMessage(&msg);
		DEBUGPRINT("lua: %s active\n",name);
	}
	return success(L);
}

//printf
char lastPrint[80] = "";
static int Print(lua_State *L)				//Print("...")
{
	const char *text = lua_tostring(L,1);
	if (strncmp(lastPrint, text, 80) != 0)
	{
		strncpy(lastPrint, text, 80);
		DEBUGPRINT("lua: %s\n",text);
	}
	return success(L);
}

char lastLog[BBB_MAX_LOG_TEXT] = "";
static int Log(lua_State *L)				//Log("...")
{
	const char *text = lua_tostring(L,1);
	if (strncmp(lastLog, text, BBB_MAX_LOG_TEXT) != 0)
	{
		strncpy(lastLog, text, BBB_MAX_LOG_TEXT);
		LogRoutine(text);
	}
	return success(L);
}

char lastInfo[BBB_MAX_LOG_TEXT] = "";
static int Info(lua_State *L)				//Info("...")
{
	const char *text = lua_tostring(L,1);
	if (strncmp(lastInfo, text, BBB_MAX_LOG_TEXT) != 0)
	{
		strncpy(lastInfo, text, BBB_MAX_LOG_TEXT);
		LogInfo(text);
	}
	return success(L);
}

char lastWarn[BBB_MAX_LOG_TEXT] = "";
static int Warning(lua_State *L)			//Warn("...")
{
	const char *text = lua_tostring(L,1);
	if (strncmp(lastWarn, text, BBB_MAX_LOG_TEXT) != 0)
	{
		strncpy(lastWarn, text, BBB_MAX_LOG_TEXT);
		LogWarning(text);
	}
	return success(L);
}

char lastError[BBB_MAX_LOG_TEXT] = "";
static int Error(lua_State *L)				//Error("...")
{
	const char *text = lua_tostring(L,1);
	if (strncmp(lastError, text, BBB_MAX_LOG_TEXT) != 0)
	{
		strncpy(lastError, text, BBB_MAX_LOG_TEXT);
		LogError(text);
	}
	return success(L);
}

static int ChangeOption(lua_State *L)		//ChangeOption("name", value)
{
	const char *name = lua_tostring(L,1);
	int value = lua_tonumber(L, 2);
	psMessage_t msg;
	psInitPublish(msg, SET_OPTION);
	strncpy(msg.nameIntPayload.name, name, PS_NAME_LENGTH);
	msg.nameIntPayload.value = value;
	RouteMessage(&msg);
	DEBUGPRINT("lua: ChangeOption(%s, %i)\n",name, value);
	return success(L);
}

static int ChangeSetting(lua_State *L)   	//ChangeSetting("name", value)
{
	const char *name = lua_tostring(L,1);
	float value = lua_tonumber(L, 2);
	psMessage_t msg;
	psInitPublish(msg, NEW_SETTING);
	strncpy(msg.nameFloatPayload.name, name, PS_NAME_LENGTH);
	msg.nameFloatPayload.value = value;
	RouteMessage(&msg);
	DEBUGPRINT("lua: ChangeSetting(%s, %f)\n",name, value);
	return success(L);
}

static char *orientTypes[] = ORIENT_TYPE_NAMES;
static int Focus(lua_State *L)			//Focus(byCompass, bearing)
{
	int orientType = lua_tointeger(L, 1);
	int heading = lua_tointeger(L, 2);

	psMessage_t msg;
	psInitPublish(msg, FOCUS);
	msg.orientPayload.orientType = orientType;
	msg.orientPayload.heading = heading;
	RouteMessage(&msg);
	DEBUGPRINT("lua: Focus %s(%i)\n", orientTypes[msg.orientPayload.orientType], msg.orientPayload.heading);

	return success(L);
}

static int Orient(lua_State *L)				//Orient(orient.<type>, heading)
{
	int orientType = lua_tointeger(L, 1);
	int heading = lua_tointeger(L, 2);

	psMessage_t msg;
	psInitPublish(msg, ORIENT);
	msg.orientPayload.orientType = orientType;
	msg.orientPayload.heading = heading;
	RouteMessage(&msg);
	DEBUGPRINT("lua: Orient %s(%i)\n",orientTypes[msg.orientPayload.orientType], msg.orientPayload.heading);

	return success(L);
}

static char *moveTypes[] = MOVE_TYPE_NAMES;
static int Move(lua_State *L)					//Move(move.<type>, x, y)
{
	int moveType = lua_tointeger(L, 1);
	int x = lua_tointeger(L, 2);
	int y = lua_tointeger(L, 3);

	psMessage_t msg;
	psInitPublish(msg, MOVEMENT);
	msg.movePayload.moveType = moveType;
	msg.movePayload.x = x;
	msg.movePayload.y = y;
	RouteMessage(&msg);
	DEBUGPRINT("lua: Move(%s, %i, %i)\n",moveTypes[msg.movePayload.moveType], msg.movePayload.x, msg.movePayload.y);

	return success(L);
}

static int GetFix(lua_State *L)				//GetFix(time_in_seconds)
{
	int time = lua_tointeger(L, 1);

	psMessage_t msg;
	psInitPublish(msg, GETAFIX);
	msg.intPayload.value = time;
	RouteMessage(&msg);
	DEBUGPRINT("lua: GetFix(%i)\n", time);

	return success(L);
}
