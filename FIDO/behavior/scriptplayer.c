/*
 * ScriptPlayer.c
 *
 *  Created on: Aug 10, 2014
 *      Author: martin
 */
// Controller of the LUA subsystem

#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>

#include "lua.h"
#include "lualib.h"
#include "RobotTypes.h"
#include "broker.h"
#include "Blackboard.h"
#include "fidobrain.h"
#include "SysLog.h"

lua_State	*luaState;

//lua call-backs
static int ChangeFidoState(lua_State *L);	//ChangeFidoState(fido.standby)
static int Activating(lua_State *L);		//Activating("sleep")
static int Print(lua_State *L);				//Print("...")
static int Log(lua_State *L);				//Log("...")
static int Info(lua_State *L);
static int Warning(lua_State *L);
static int Error(lua_State *L);

static int ChangeSetting(lua_State *L);		//ChangeSetting("name", value)
static int ChangeOption(lua_State *L);

static int FocusAttention(lua_State *L);	//FocusAttention(orient.relative, direction.front)
static int OrientBody(lua_State *L);		//OrientBody((orient.relative, direction.left)
static int Move(lua_State *L);				//Move(move.local, x, y)

//allocator for lua
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
	luaState = lua_newstate(l_alloc, NULL);

	if (luaState == NULL)
	{
		LogError("luaState create fail");
	   	return -1;
	}
	//open standard libraries
	luaopen_math(luaState);
	lua_setglobal(luaState, "math");
	luaopen_string(luaState);
	lua_setglobal(luaState, "string");

	//register call-backs
	lua_pushcfunction(luaState, ChangeFidoState);	//set internal state
	lua_setglobal(luaState, "ChangeFidoState");
	lua_pushcfunction(luaState, Activating);		//Activating Behavior
	lua_setglobal(luaState, "Activating");
	lua_pushcfunction(luaState, Print);				//Print Message
	lua_setglobal(luaState, "Print");
	lua_pushcfunction(luaState, Log);				//Log Message
	lua_setglobal(luaState, "Log");
	lua_pushcfunction(luaState, Info);				//Log Message
	lua_setglobal(luaState, "Info");
	lua_pushcfunction(luaState, Warning);			//Log Message
	lua_setglobal(luaState, "Warning");
	lua_pushcfunction(luaState, Error);				//Log Message
	lua_setglobal(luaState, "Error");

	lua_pushcfunction(luaState, ChangeSetting);		//change setting
	lua_setglobal(luaState, "ChangeSetting");
	lua_pushcfunction(luaState, ChangeOption);		//change option
	lua_setglobal(luaState, "ChangeOption");

	lua_pushcfunction(luaState, FocusAttention);	//issue focus command
	lua_setglobal(luaState, "FocusAttention");

	lua_pushcfunction(luaState, OrientBody);		//issue orient command
	lua_setglobal(luaState, "OrientBody");

	lua_pushcfunction(luaState, Move);				//issue move command
	lua_setglobal(luaState, "Move");

	SetGlobal(luaState, "LastFocusHeading",0);
	SetGlobal(luaState, "LastFocusType",0);
	SetGlobal(luaState, "LastOrientHeading",0);
	SetGlobal(luaState, "LastOrientType",0);
	SetGlobal(luaState, "LastMoveType",0);
	SetGlobal(luaState, "LastMoveX",0);
	SetGlobal(luaState, "LastMoveY",0);

	InitLuaGlobals(luaState);						//load more system globals

	return 0;
}

int ScriptProcessMessage(psMessage_t *msg)
{
	return UpdateGlobalsFromMessage(luaState, msg);
}

int InvokeScript(char *scriptName)
{
	lua_getglobal(luaState, scriptName);

	if (lua_isfunction(luaState,lua_gettop(luaState)))
	{
//		DEBUG_PRINT("fido: calling %s\n", scriptName);
		int status = lua_pcall(luaState, 0, 0, 0);
		if (status)
		{
			const char *errormsg = lua_tostring(luaState, -1);
			DEBUG_PRINT("Error: %s\n",errormsg);
			LogError("%s error: %s",scriptName, errormsg);
			return -1;
		}
		else
		{
//			DEBUG_PRINT("overmind: returned from %s\n", scriptName);
			return 0;
		}
	} else
	{
		DEBUG_PRINT("Global %s is of type %s\n",scriptName,lua_typename(luaState,lua_type(luaState,lua_gettop(luaState))) );
		LogError("No %s script!", scriptName);
		return -1;
	}
}

//call-backs
//change FIDOSTATE
int lastSentState;
static int ChangeFidoState(lua_State *L)
{
	psMessage_t msg;
	int newState = lua_tointeger(L, 1);
	if (newState != fidoSystemState.state &&
			newState != lastSentState)
	{
		psInitPublish(msg, FIDO_STATE);
		msg.bytePayload.value = newState;
		NewBrokerMessage(&msg);
		LogInfo("lua: ChangeFidoState(%s)", fidoStateNames[msg.bytePayload.value]);
		lastSentState = newState;
	}
	return 0;
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
		psInitPublish(msg, BEHAVIOR);
		strncpy(msg.namePayload.name, name, PS_NAME_LENGTH);
		strncpy(currentName, name, PS_NAME_LENGTH);
		NewBrokerMessage(&msg);
		LogInfo("lua: %s active",name);
	}
	return 0;
}
//printf
char lastPrint[80] = "";
static int Print(lua_State *L)				//Print("...")
{
	const char *text = lua_tostring(L,1);
	if (strncmp(lastPrint, text, 80) != 0)
	{
		strncpy(lastPrint, text, 80);
		DEBUG_PRINT("lua: %s\n",text);
	}
	return 0;
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
	return 0;
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
	return 0;
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
	return 0;
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
	return 0;
}

static int ChangeOption(lua_State *L)		//ChangeOption("name", value)
{
	const char *name = lua_tostring(L,1);
	int value = lua_tonumber(L, 2);
	psMessage_t msg;
	psInitPublish(msg, SET_OPTION);
	strncpy(msg.nameIntPayload.name, name, PS_NAME_LENGTH);
	msg.nameIntPayload.value = value;
	NewBrokerMessage(&msg);
	LogInfo("lua: ChangeOption(%s, %i)",name, value);
	lua_pushnumber(L, value);
	return 1;
}
static int ChangeSetting(lua_State *L)   	//ChangeSetting("name", value)
{
	const char *name = lua_tostring(L,1);
	float value = lua_tonumber(L, 2);
	psMessage_t msg;
	psInitPublish(msg, NEW_SETTING);
	strncpy(msg.nameFloatPayload.name, name, PS_NAME_LENGTH);
	msg.nameFloatPayload.value = value;
	NewBrokerMessage(&msg);
	LogInfo("lua: ChangeSetting(%s, %f)",name, value);
	lua_pushnumber(L, value);
	return 1;
}
psOrientPayload_t lastFocusPayload = {0,0};
static int FocusAttention(lua_State *L)			//sensor focus
{
	int orientType = lua_tointeger(L, 1);
	int heading = lua_tointeger(L, 2);
	if (lastFocusPayload.orientType != orientType
			|| lastFocusPayload.heading != heading)
	{
		psMessage_t msg;
		psInitPublish(msg, FOCUS);
		msg.orientPayload.orientType = orientType;
		msg.orientPayload.heading = heading;
		lua_setglobal(L, "LastFocusHeading");
		lua_setglobal(L, "LastFocusType");
		NewBrokerMessage(&msg);
		LogInfo("lua: FocusAttention(%i)",msg.orientPayload.heading);
	}
	return 0;
}
psOrientPayload_t lastOrientPayload = {0,0};
static int OrientBody(lua_State *L)				//orientation
{
	int orientType = lua_tointeger(L, 1);
	int heading = lua_tointeger(L, 2);
	if (lastOrientPayload.orientType != orientType
			|| lastOrientPayload.heading != heading)
	{
		psMessage_t msg;
		psInitPublish(msg, ORIENT);
		msg.orientPayload.orientType = orientType;
		msg.orientPayload.heading = heading;
		lua_setglobal(L, "LastOrientHeading");
		lua_setglobal(L, "LastOrientType");
		NewBrokerMessage(&msg);
		LogInfo("lua: OrientBody(%i)",msg.orientPayload.heading);
	}
	return 0;
}
psMovePayload_t lastMovePayload = {0,0};
time_t lastMoveMessage = 0;
static int Move(lua_State *L)					//walking
{
	int moveType = lua_tointeger(L, 1);
	int x = lua_tointeger(L, 2);
	int y = lua_tointeger(L, 3);
	if (lastMovePayload.moveType != moveType
			|| lastMovePayload.x != x
			|| lastMovePayload.y != y
			|| lastMoveMessage < time(NULL) + moveInterval)
	{
		psMessage_t msg;
		psInitPublish(msg, MOVEMENT);
		msg.movePayload.moveType = moveType;
		msg.movePayload.x = x;
		msg.movePayload.y = y;
		lua_setglobal(L, "LastMoveType");
		lua_setglobal(L, "LastMoveX");
		lua_setglobal(L, "LastMoveY");
		NewBrokerMessage(&msg);
		LogInfo("lua: Move(%i, %i)",msg.movePayload.x,msg.movePayload.y);
	}
	return 0;
}

