/*
 * behavior.h
 *
 *  Created on: Jul 11, 2014
 *      Author: martin
 */

#ifndef BEHAVIOR_H_
#define BEHAVIOR_H_

#include "SoftwareProfile.h"
#include "PubSubData.h"
#include "lua.h"

//behavior init
pthread_t BehaviorInit();

//update globals and hooks
void BehaviorProcessMessage(psMessage_t *msg);

int ReportAvailableScripts();

//scripting system
int InitScriptingSystem();
int ScriptProcessMessage(psMessage_t *msg);
int InvokeScript(char *scriptName);
int AvailableScripts();

//LUA globals and scripts
int InitLuaGlobals(lua_State *L);
int LoadAllScripts(lua_State *L);
int LoadLuaScripts(lua_State *L, char *scriptFolder);	//load chunks
int LoadChunk(lua_State *L, char *name, char *path);
int UpdateGlobalsFromMessage(lua_State *L, psMessage_t *msg);
void SetGlobal(lua_State *L, const char *name, float value);

#endif /* BEHAVIOR_H_ */
