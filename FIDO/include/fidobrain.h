/*
 * fidobrain.h
 *
 *  Created on: Jul 11, 2014
 *      Author: martin
 */

#ifndef FIDOBRAIN_H_
#define FIDOBRAIN_H_

#include "RobotTypes.h"
#include "lua.h"

extern char *batteryStateNames[];
extern char *systemStateNames[];
extern char *fidoStateNames[];

//scripting
int InitScriptingSystem();
int ScriptProcessMessage(psMessage_t *msg);
int InvokeScript(char *scriptName);
//LUA
int InitLuaGlobals(lua_State *L);
void LoadAllScripts(lua_State *L);
int LoadLuaScripts(lua_State *L, char *scriptFolder);	//load chunks
int LoadChunk(lua_State *L, char *name, char *path);
int UpdateGlobalsFromMessage(lua_State *L, psMessage_t *msg);
void SetGlobal(lua_State *L, const char *name, float value);

//broker
int BrokerInit();
//navigator
int NavigatorInit();
void NavigatorProcessMessage(psMessage_t *msg);
//scanner
int ScannerInit();
void ScannerProcessMessage(psMessage_t *msg);
//behavior
int BehaviorInit();
void BehaviorProcessMessage(psMessage_t *msg);

//syslog
extern FILE *logFile;
int SysLogInit();
void LogProcessMessage(psMessage_t *msg);				//process for logging
void PrintLogMessage(FILE *f, psMessage_t *msg);

//picserial
int SerialBrokerInit();
void SerialBrokerProcessMessage(psMessage_t *msg);	//consider candidate msg to send over the uart

//blackboard
int BlackboardInit();
void BlackboardProcessMessage(psMessage_t *msg);

//responder
void ResponderProcessMessage(psMessage_t *msg);

int *pidof (char *pname);

#endif /* FIDOBRAIN_H_ */
