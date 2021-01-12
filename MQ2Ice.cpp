// MQ2Ice.cpp : 
//
// Author : Dewey2461
//
// Purpose: Moving around on ICE is a PITA. This plugin attempts to make moving around 
//          a little easier by automatically toggling run/walk and pausing nav/path/stick 
//          when it detects the player skidding on ice. 
//
// Commands: /ice [on|off] - Turns plugin on or off
// Commands: /ice [nav|path|stick|walk|buff]  [true|false] - turns individual movement parts on or off
//
// Users can now define the zone and bounding box for when to toggle walking on. May make more friendly later
// See "Area" in MQ2Ice.ini , format => [ZoneID] [Min X] [Min Y] [Min Z]  [Max X] [Max Y] [Max Z] 
//
// NOTICE : Movement controls were taken from MQ2MoveUtils 
//
// Released 2020-02-29 - Yes Leap Day!
//          2020-03-05 - Added save/restore of global error messages
//          2020-03-08 - Added buff option to toggle blocking bard selos. 
//          2020-03-09 - Added user defined area. 

#include "../MQ2Plugin.h"


PreSetup("MQ2Ice");

#define TIME unsigned long long

// *************************************************************************** 
//	Global Variables 
// *************************************************************************** 

TIME tick = 0;

int PluginON = 1;			// Should we do anything?
int PluginWALK = 1;			// Should we toggle run/walk?
int PluginNAV = 1;			// Should we toggle nav ?
int PluginPATH = 1;			// Should we toggle AdvPath ?
int PluginSTICK = 1;		// Should we toggle Stick ?
int PluginBUFF = 1;			// Should we movement buffs? 
int PluginINI = 0;			// Have we read the INI?

int iPausedNav = 0;			// Flag for when I have toggled Nav
int iPausedStick = 0;		// or stick
int iPausedPath = 0;		// or AdvPath
int iPausedRun = 0;			// or Run
int iPausedBuff = 0;		// or blocking spells

int iStrafeLeft = 0;		// Keymapping for ...
int iStrafeRight = 0;
int iForward = 0;
int iBackward = 0;
int iRunWalk = 0;

// *************************************************************************** 
//	Lets make the 'walk' zones configurable 
// *************************************************************************** 

typedef struct {
	WORD ZoneID;
	float xMin, yMin, zMin, xMax, yMax, zMax;
	void* pNext;
} AREANODE;

AREANODE* pAreaList = NULL;

void ClearAreaList(void)
{
	AREANODE* p;
	while (pAreaList)
	{
		p = pAreaList;
		pAreaList = (AREANODE *)pAreaList->pNext;
		free(p);
	}
}

void LoadAreaList(void)
{
	char szKey[MAX_STRING];
	char szVal[MAX_STRING];
	int i;
	for (i = 1; i < 50; i++)
	{
		sprintf_s(szKey, "%d", i);
		GetPrivateProfileString("Area", szKey, "", szVal, MAX_STRING, INIFileName);
		if (szVal[0]) {
			AREANODE* p = (AREANODE *)malloc(sizeof(AREANODE));
			int rt = sscanf_s(szVal, "%hd %f %f %f %f %f %f", &p->ZoneID, &p->xMin, &p->yMin, &p->zMin, &p->xMax, &p->yMax, &p->zMax);
			if (rt == 7) {
				p->pNext = pAreaList;
				pAreaList = p;
				WriteChatf("MQ2Ice Area [%d] = Zone[%d] Min[%6.1f %6.1f %6.1f] Max[%6.1f %6.1f %6.1f]", i, p->ZoneID, p->xMin, p->yMin, p->zMin, p->xMax, p->yMax, p->zMax);
			}
			else
			{
				WriteChatf("MQ2Ice Area [%d] ERROR Parsing [%s] - Syntax = ZoneID XMin YMin ZMin XMax YMax ZMax", i, szVal);
				free(p);
			}
		}
	}
}



// *************************************************************************** 
//	To Evaluate or Not to Evaluate that is the question... I choose to EVAL for easy development
// *************************************************************************** 

float Evaluate(char *zOutput, char *zFormat, ...) {
	va_list vaList;
	va_start(vaList, zFormat);
	char szTemp[MAX_STRING];
	vsprintf_s(szTemp, MAX_STRING, zFormat, vaList);
	ParseMacroData(szTemp, MAX_STRING);
	if (gszLastNormalError[0] || gszLastSyntaxError[0] || gszLastMQ2DataError[0]) {
		char szBuff[MAX_STRING];
		vsprintf_s(szBuff, MAX_STRING, zFormat, vaList);
		if (gszLastNormalError[0])	WriteChatf("MQ2Ice Error:[%s] on Evaluate[%s] = [%s]", gszLastNormalError, szBuff, szTemp);
		if (gszLastSyntaxError[0])	WriteChatf("MQ2Ice Error:[%s] on Evaluate[%s] = [%s]", gszLastSyntaxError, szBuff, szTemp);
		if (gszLastMQ2DataError[0])	WriteChatf("MQ2Ice Error:[%s] on Evaluate[%s] = [%s]", gszLastMQ2DataError, szBuff, szTemp);
		gszLastNormalError[0] = gszLastSyntaxError[0] = gszLastMQ2DataError[0] = 0;
	}
	if (zOutput) {
		char *p = szTemp;
		while (*p) *zOutput++ = *p++;
		*zOutput = 0;
	}
	if (_stricmp(szTemp, "NULL") == 0) return 0;
	if (_stricmp(szTemp, "FALSE") == 0) return 0;
	if (_stricmp(szTemp, "TRUE") == 0) return 1;
	return ((float)atof(szTemp));
}

char szLastError[3][MAX_STRING];		// Lets be nice and save/restore error messages

void SaveErrorMessages(void)
{
	strcpy_s(szLastError[0], gszLastNormalError);
	strcpy_s(szLastError[1], gszLastSyntaxError);
	strcpy_s(szLastError[2], gszLastMQ2DataError);
}

void RestoreErrorMessages(void)
{
	strcpy_s(gszLastNormalError, szLastError[0]);
	strcpy_s(gszLastSyntaxError, szLastError[1]);
	strcpy_s(gszLastMQ2DataError, szLastError[2]);
}



// *************************************************************************** 
//	Get the movement keys. This code/method is from MQ2MoveUtils
// *************************************************************************** 

void FindMappedKeys()
{
	iStrafeLeft = FindMappableCommand("strafe_left");
	iStrafeRight = FindMappableCommand("strafe_right");
	iForward = FindMappableCommand("forward");
	iBackward = FindMappableCommand("back");
	iRunWalk = FindMappableCommand("run_walk");
}


// *************************************************************************** 
//	Functions to read/write values so we're presistant
// *************************************************************************** 

void WriteKeyVal(char *FileName, char *Section, char *Key, char *zFormat, ...) {
	va_list vaList;
	va_start(vaList, zFormat);
	char szTemp[MAX_STRING];
	vsprintf_s(szTemp, MAX_STRING, zFormat, vaList);
	WritePrivateProfileString(Section, Key, szTemp, FileName);
}


void WriteINI(void)
{
	WriteKeyVal(INIFileName, "Settings", "ON", "%d", PluginON);
	WriteKeyVal(INIFileName, "Settings", "Walk", "%d", PluginWALK);
	WriteKeyVal(INIFileName, "Settings", "Nav", "%d", PluginNAV);
	WriteKeyVal(INIFileName, "Settings", "Path", "%d", PluginPATH);
	WriteKeyVal(INIFileName, "Settings", "Stick", "%d", PluginSTICK);
}


void ReadINI(void)
{
	PluginINI = 1;
	WriteChatf("ReadINI");
	PluginON = GetPrivateProfileInt("Settings", "ON", 1, INIFileName);
	PluginWALK = GetPrivateProfileInt("Settings", "Walk", 1, INIFileName);
	PluginNAV = GetPrivateProfileInt("Settings", "Nav", 1, INIFileName);
	PluginPATH = GetPrivateProfileInt("Settings", "Path", 1, INIFileName);
	PluginSTICK = GetPrivateProfileInt("Settings", "Stick", 1, INIFileName);
	LoadAreaList();
	if (!pAreaList) {
		WriteChatf("ReadINI Create Area");
		WritePrivateProfileString("Area", "1", "825 140 212 -100 270 435 100", INIFileName);
		LoadAreaList();
	}
}


// *************************************************************************** 
//	Functions to turn things on and off so we're 'friendly' ... ug ... 
// *************************************************************************** 

void MyCommand(PSPAWNINFO pCHAR, PCHAR szLine)
{
	char szArg[MAX_STRING];
	int  iArg = -1;

	GetArg(szArg, szLine, 2);

	if (szArg[0] != 0) {
		if (_stricmp(szArg, "true") == 0) iArg = 1;
		if (_stricmp(szArg, "on") == 0) iArg = 1;
		if (_stricmp(szArg, "1") == 0) iArg = 1;
		if (_stricmp(szArg, "false") == 0) iArg = 0;
		if (_stricmp(szArg, "off") == 0) iArg = 0;
		if (_stricmp(szArg, "0") == 0) iArg = 0;
	}

	GetArg(szArg, szLine, 1);
	if (_stricmp(szArg, "on") == 0) PluginON = 1;
	if (_stricmp(szArg, "off") == 0) PluginON = 0;
	if (_stricmp(szArg, "stick") == 0 && iArg != -1) PluginSTICK = iArg;
	if (_stricmp(szArg, "path") == 0 && iArg != -1) PluginPATH = iArg;
	if (_stricmp(szArg, "nav") == 0 && iArg != -1) PluginNAV = iArg;
	if (_stricmp(szArg, "walk") == 0 && iArg != -1) PluginWALK = iArg;
	if (_stricmp(szArg, "buff") == 0 && iArg != -1) PluginBUFF = iArg;

	if (szArg[0] == 0) {
		WriteChatf("MQ2ICE Commands: /ice [on|off] -- Turn plugin on|off");
		WriteChatf("MQ2ICE Commands: /ice [nav|path|stick|walk|buff] [true|false] -- Turn feature on or off ");
	}

	WriteChatf("MQ2ICE Plugin is %s", PluginON ? "ON" : "OFF");
	WriteChatf("MQ2ICE Nav = %s", PluginNAV ? "ON" : "OFF");
	WriteChatf("MQ2ICE Path = %s", PluginPATH ? "ON" : "OFF");
	WriteChatf("MQ2ICE Stick = %s", PluginSTICK ? "ON" : "OFF");
	WriteChatf("MQ2ICE Walk = %s", PluginWALK ? "ON" : "OFF");
	WriteChatf("MQ2ICE Buff = %s", PluginBUFF ? "ON" : "OFF");

	WriteINI();
}

// *************************************************************************** 
//	Functions to determine if we're in a 'walk' only area 
// *************************************************************************** 

int EvalAreaList(void)
{
	PCHARINFO pChar = GetCharInfo();
	PSPAWNINFO pSpawn = (PSPAWNINFO)pCharSpawn;
	AREANODE* p = pAreaList;
	while (p) {
		if ((pChar->zoneId & 0x7FFF) == p->ZoneID &&
			pSpawn->X >= p->xMin && pSpawn->X <= p->xMax &&
			pSpawn->Y >= p->yMin && pSpawn->Y <= p->yMax &&
			pSpawn->Z >= p->zMin && pSpawn->Z <= p->zMax) return TRUE;

		p = (AREANODE*)p->pNext;
	}
	return FALSE;
}


// *************************************************************************** 
//	
//  Main Method - Every 100ms check:
//   1 - Are we on first floor near ice ? if so toggle run/walk 
//   2 - Are we skidding ? If so toggle nav/path/stick and actively control skid
//   3 - if we have recovered or no longer near ice turn run or unpause move
//
// *************************************************************************** 


void DoIce()
{
	if (MQGetTickCount64() < tick) return;
	tick = MQGetTickCount64() + 100;

	PCHARINFO pChar = GetCharInfo();
	PSPAWNINFO pSpawn = (PSPAWNINFO)pCharSpawn;
	double VF = 0;
	double VS = 0;
	double EQ2RAD = 3.14159*2.0 / 512.0;
	char zTemp[MAX_STRING];
	float drift = 0.25;

	int ShouldWalk = EvalAreaList();
	int Running = *EQADDR_RUNWALKSTATE;

	if (ShouldWalk && PluginWALK && Running) {
		iPausedRun = 1;
		EzCommand("/keypress RUN_WALK");
	}

	if (ShouldWalk && PluginBUFF && !iPausedBuff) {
		iPausedBuff = 1;
		EzCommand("/block add me 717");
	}


	// Check to see if we are on ice ....
	// ICE : AccelerationFriction = 0.02  AreaFriction = 0.99
	if (pSpawn->AccelerationFriction <= 0.02 && pSpawn->mPlayerPhysicsClient.Levitate != 2) {
		// Calculate the Velocity Vectors
		double A = pSpawn->Heading * EQ2RAD;
		VF = pSpawn->SpeedX * sin(A) + pSpawn->SpeedY * cos(A);

		A = (pSpawn->Heading - 128) * EQ2RAD;
		VS = pSpawn->SpeedX * sin(A) + pSpawn->SpeedY * cos(A);

		// Check to see if we are skidding ... if so pause any automatic movement
		if (fabs(VS) > drift) {
			SaveErrorMessages();
			if (PluginNAV && !iPausedNav && Evaluate(zTemp, "${If[${Nav.Active} && !${Nav.Paused},1,0]}")) {
				iPausedNav = 1;
				EzCommand("/nav pause");
			}
			if (PluginSTICK && !iPausedStick && Evaluate(zTemp, "${Stick.Status.Equal[ON]}")) {
				iPausedStick = 1;
				EzCommand("/stick pause");
			}
			if (PluginPATH && !iPausedPath && Evaluate(zTemp, "${If[${AdvPath.State}>0 && !${AdvPath.Paused},1,0]}")) {
				iPausedPath = 1;
				EzCommand("/play pause");
			}
			RestoreErrorMessages();
		}


		// If we have paused then zero out motion
		if (iPausedNav || iPausedStick || iPausedPath)
		{
			if (VS > drift) {
				MQ2Globals::ExecuteCmd(iStrafeLeft, 1, 0);
				MQ2Globals::ExecuteCmd(iStrafeRight, 0, 0);
			}
			else if (VS < -drift) {
				MQ2Globals::ExecuteCmd(iStrafeLeft, 0, 0);
				MQ2Globals::ExecuteCmd(iStrafeRight, 1, 0);
			}
			else {
				MQ2Globals::ExecuteCmd(iStrafeLeft, 0, 0);
				MQ2Globals::ExecuteCmd(iStrafeRight, 0, 0);
			}

			if (VF > drift) {
				MQ2Globals::ExecuteCmd(iForward, 0, 0);
				MQ2Globals::ExecuteCmd(iBackward, 1, 0);
			}
			else if (VF < -drift) {
				MQ2Globals::ExecuteCmd(iForward, 1, 0);
				MQ2Globals::ExecuteCmd(iBackward, 0, 0);
			}
			else {
				MQ2Globals::ExecuteCmd(iForward, 0, 0);
				MQ2Globals::ExecuteCmd(iBackward, 0, 0);
			}
		}
	}

	// Turn stuff back on if we're outside ice or have stop drifting 
	if (fabs(VS) < drift) {
		if (iPausedNav || iPausedStick || iPausedPath) {
			MQ2Globals::ExecuteCmd(iStrafeLeft, 0, 0);
			MQ2Globals::ExecuteCmd(iStrafeRight, 0, 0);
			MQ2Globals::ExecuteCmd(iForward, 0, 0);
			MQ2Globals::ExecuteCmd(iBackward, 0, 0);
		}
		if (PluginNAV && iPausedNav) {
			iPausedNav = 0;
			EzCommand("/nav pause");
		}
		if (PluginSTICK && iPausedStick) {
			iPausedStick = 0;
			EzCommand("/stick unpause");
		}
		if (PluginPATH && iPausedPath) {
			iPausedPath = 0;
			EzCommand("/play unpause");
		}
	}

	if (PluginWALK && !Running && !ShouldWalk && iPausedRun) {
		iPausedRun = 0;
		Running = *EQADDR_RUNWALKSTATE;
		if (!Running) EzCommand("/keypress RUN_WALK");
	}
	if (PluginBUFF && iPausedBuff && !ShouldWalk) {
		iPausedBuff = 0;
		EzCommand("/block remove me 717");
	}
}


PLUGIN_API VOID InitializePlugin(VOID)
{
	FindMappedKeys();
	AddCommand("/ice", MyCommand);
}


PLUGIN_API VOID ShutdownPlugin(VOID)
{
	RemoveCommand("/ice");
}


PLUGIN_API VOID OnPulse(VOID)
{
	if (!PluginON) return;
	if (gGameState != GAMESTATE_INGAME) {
		PluginINI = 0;
		return;
	}
	if (gGameState == GAMESTATE_INGAME && !PluginINI) ReadINI();
	if (!pCharSpawn) return;
	DoIce();
}