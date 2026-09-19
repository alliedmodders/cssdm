/**
 * dm_base.sp
 * Base CS:S DM admin/control functions.
 * This file is part of CS:S DM, Copyright (C) 2005-2007 AlliedModders LLC
 * by David "BAILOPAN" Anderson, http://www.bailopan.net/cssdm/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Version: $Id$
 */

#pragma newdecls required
#pragma semicolon 1
#include <sourcemod>
#include <sdktools>
#include <dhooks>
#include <cstrike>
#include <memorypatch>
#undef REQUIRE_PLUGIN
#include <cssdm>
#define REQUIRE_PLUGIN

bool g_IsCSGO = false;

enum struct Weapon
{
	char classname[64];
	char display[64];
	DmWeaponType type;
	int id;
}

ArrayList g_Weapons;
StringMap g_WeaponLookup;

#pragma unused cssdm_version
ConVar cssdm_version;
ConVar cssdm_enabled;
ConVar cssdm_ffa_enabled;
ConVar cssdm_respawn_wait;
ConVar cssdm_ragdoll_time;
ConVar cssdm_spawn_method;

MemoryPatch g_LagCompPatch;
MemoryPatch g_TakeDmgPatch1;
MemoryPatch g_CalcDomRevPatch;

GlobalForward g_StartupForward;
GlobalForward g_ShutdownForward;
GlobalForward g_OnClientSpawnedForward;
GlobalForward g_OnClientPostSpawnedForward;
GlobalForward g_OnClientDeathForward;
GlobalForward g_OnClientSetSpawnMethodForward;

bool g_FFAFailed = false;
bool g_InRoundRestart = false;
bool g_SkipNextPlayerSpawnCallback = false;
Handle g_PlayerRespawnTimers[MAXPLAYERS] = { null, ... };

DynamicHook g_IPointsForKillHook;

Handle g_RemoveAllItemsSDKCall;

public Plugin myinfo =
{
	name = "CS:S DM Base",
	author = "AlliedModders LLC",
	description = "CS:S DM Base",
	version = CSSDM_VERSION,
	url = "http://www.bailopan.net/cssdm/"
};

public APLRes AskPluginLoad2(Handle myself, bool late, char[] error, int err_max)
{
	char game[128];
	GetGameFolderName(game, sizeof(game));
	g_IsCSGO = StrEqual(game, "csgo", false);
	if (!(g_IsCSGO || StrEqual(game, "css", false)))
	{
		strcopy(error, err_max, "This plugin will only work on Counter-Strike: Source or Counter-Strike: Global Offensive");
		return APLRes_SilentFailure;
	}

	CreateNative("DM_IsRunning", Native_IsRunning);
	CreateNative("DM_GetSpawnMethod", Native_GetSpawnMethod);
	CreateNative("DM_GetWeaponID", Native_GetWeaponID);
	CreateNative("DM_GetWeaponType", Native_GetWeaponType);
	CreateNative("DM_GetWeaponClassname", Native_GetWeaponClassname);
	CreateNative("DM_GetWeaponName", Native_GetWeaponName);
	CreateNative("DM_StripBotItems", Native_StripBotItems);
	CreateNative("DM_GetSpawnWaitTime", Native_GetSpawnWaitTime);
	CreateNative("DM_RespawnClient", Native_RespawnClient);
	CreateNative("DM_IsClientAlive", Native_IsClientAlive);

	RegPluginLibrary("cssdm");

	return APLRes_Success;
}

public void OnPluginStart()
{
	g_StartupForward = new GlobalForward("DM_OnStartup", ET_Ignore);
	g_ShutdownForward = new GlobalForward("DM_OnShutdown", ET_Ignore);
	g_OnClientSpawnedForward = new GlobalForward("DM_OnClientSpawned", ET_Ignore, Param_Cell);
	g_OnClientPostSpawnedForward = new GlobalForward("DM_OnClientPostSpawned", ET_Ignore, Param_Cell);
	g_OnClientDeathForward = new GlobalForward("DM_OnClientDeath", ET_Hook, Param_Cell);
	g_OnClientSetSpawnMethodForward = new GlobalForward("DM_OnSetSpawnMethod", ET_Hook, Param_String);

	cssdm_version = CreateConVar("cssdm_version", CSSDM_VERSION, "CS:S DM Version", FCVAR_NOTIFY);
	cssdm_enabled = CreateConVar("cssdm_enabled", "1", "Sets whether CS:S DM is enabled", FCVAR_NOTIFY);
	if (!g_IsCSGO)
	{
		cssdm_enabled.AddChangeHook(OnFFARelatedCvarChanged);
		cssdm_ffa_enabled = CreateConVar("cssdm_ffa_enabled", "0", "Sets whether Free-For-All mode is enabled", FCVAR_NOTIFY);
		cssdm_ffa_enabled.AddChangeHook(OnFFARelatedCvarChanged);
	}
	cssdm_respawn_wait = CreateConVar("cssdm_respawn_wait", "0.75", "Sets respawn wait time");
	cssdm_ragdoll_time = CreateConVar("cssdm_ragdoll_time", "2", "Sets ragdoll stay time", _, true, 0.0, true, 20.0);
	cssdm_spawn_method = CreateConVar("cssdm_spawn_method", "preset", "Sets how and where players are spawned");
	cssdm_spawn_method.AddChangeHook(OnSpawnMethodChanged);

	g_Weapons = new ArrayList(sizeof(Weapon));
	g_WeaponLookup = new StringMap();

	char weaponParseError[128];
	if (!ParseWeaponConfig("cfg/cssdm/cssdm.weapons.txt", weaponParseError, sizeof(weaponParseError)))
	{
		SetFailState(weaponParseError);
	}

	GameData gamedata = new GameData("cssdm.games");
	if (gamedata == null)
	{
		SetFailState("Could not load CSSDM gamedata");
	}

	StartPrepSDKCall(SDKCall_Player);
	if (!PrepSDKCall_SetFromConf(gamedata, SDKConf_Virtual, "RemoveAllItems"))
	{
		delete gamedata;
		SetFailState("Could not find offset RemoveAllItems");
	}
	PrepSDKCall_AddParameter(SDKType_Bool, SDKPass_ByValue);
	g_RemoveAllItemsSDKCall = EndPrepSDKCall();

	if (!g_IsCSGO)
	{
		g_IPointsForKillHook = DynamicHook.FromConf(gamedata, "IPointsForKill");
		if (g_IPointsForKillHook == null)
		{
			delete gamedata;
			SetFailState("Could not find offset or function info about IPointsForKill");
		}
		g_IPointsForKillHook.HookGamerules(Hook_Pre, Hook_OnIPointsForKill);

		g_LagCompPatch = new MemoryPatch(gamedata, "WantsLagComp", "LagCompPatch", "LagCompPatch");
		g_TakeDmgPatch1 = new MemoryPatch(gamedata, "OnTakeDamage", "TakeDmgPatch1", "TakeDmgPatch1");
		g_CalcDomRevPatch = new MemoryPatch(gamedata, "CalcDominationAndRevenge", "CalcDomRevPatch", "CalcDomRevPatch");
		if (g_LagCompPatch == null || g_TakeDmgPatch1 == null || g_CalcDomRevPatch == null)
		{
			LogError("FFA will not work: Failed to create one or more memory patch(es)!");
			g_FFAFailed = true;
		}
	}

	delete gamedata;

	HookEvent("player_death", Event_PlayerDeath);
	HookEvent("player_spawn", Event_PlayerSpawn);
	HookEvent("round_start", Event_RoundStart, EventHookMode_PostNoCopy);
	HookEvent("round_end", Event_RoundEnd, EventHookMode_PostNoCopy);

	AutoExecConfig(false, "cssdm", "cssdm");
}

public void OnConfigsExecuted()
{
	if (!cssdm_enabled.BoolValue)
	{
		return;
	}

	char map[128], mapDisplay[128];
	GetCurrentMap(map, sizeof(map));
	GetMapDisplayName(map, mapDisplay, sizeof(mapDisplay));

	if (strlen(mapDisplay))
	{
		ServerCommand("exec cssdm/maps/%s.cssdm.cfg", mapDisplay);
	}

	SetupFFA();

	Call_StartForward(g_StartupForward);
	Call_Finish();
}

public void OnEnableCvarChanged(ConVar convar, const char[] oldValue, const char[] newValue)
{
	int old = StringToInt(oldValue);
	int value = StringToInt(newValue);
	if (old == value)
	{
		return;
	}
	if (value)
	{
		Call_StartForward(g_StartupForward);
		Call_Finish();
	}
	else
	{
		Call_StartForward(g_ShutdownForward);
		Call_Finish();
	}
}

public void OnFFARelatedCvarChanged(ConVar convar, const char[] oldValue, const char[] newValue)
{
	SetupFFA();
}

public void OnSpawnMethodChanged(ConVar convar, const char[] oldValue, const char[] newValue)
{
	bool validMethod = StrEqual(newValue, "preset") || StrEqual(newValue, "none");
	if (!validMethod)
	{
		LogMessage("Invalid spawn method passed to cssdm_spawn_method, resetting to default");
	}
	convar.RestoreDefault();
	char value[64];
	convar.GetString(value, sizeof(value));
	Call_StartForward(g_OnClientSetSpawnMethodForward);
	Call_PushString(value);
	Call_Finish();
}

public void OnClientDisconnect(int client)
{
	KillPlayerRespawnTimer(client);
}

public void OnMapStart()
{
	g_SkipNextPlayerSpawnCallback = false;
}

public void OnMapEnd()
{
	g_SkipNextPlayerSpawnCallback = false;
	for (int i = 0; i <= MaxClients; i++)
	{
		KillPlayerRespawnTimer(i);
	}
	if (cssdm_enabled.BoolValue)
	{
		Call_StartForward(g_ShutdownForward);
		Call_Finish();
	}
}

void SetupFFA()
{
	if (g_IsCSGO)
	{
		return;
	}
	if (cssdm_enabled.BoolValue && cssdm_ffa_enabled.BoolValue)
	{
		g_LagCompPatch.Enable();
		g_TakeDmgPatch1.Enable();
		g_CalcDomRevPatch.Enable();
	}
	else
	{
		g_LagCompPatch.Disable();
		g_TakeDmgPatch1.Disable();
		g_CalcDomRevPatch.Disable();
	}
}

bool ParseWeaponConfig(const char[] configPath, char[] error, int err_max)
{
	char game[128];
	GetGameFolderName(game, sizeof(game));
	StringToLower(game);

	KeyValues kv = new KeyValues("Weapons");
	if (!kv.ImportFromFile(configPath))
	{
		delete kv;
		Format(error, err_max, "Could not load weapons file \"%s\"", configPath);
		return false;
	}

	if (!kv.JumpToKey(game))
	{
		delete kv;
		Format(error, err_max, "Could not find \"%s\" section in weapons file", game);
		return false;
	}

	if (!kv.GotoFirstSubKey())
	{
		delete kv;
		strcopy(error, err_max, "No weapons defined for game");
		return false;
	}

	int id = 0;
	do
	{
		char name[64], classname[64], typeStr[32];
		DmWeaponType type = DmWeapon_Invalid;
		Weapon weapon;

		kv.GetSectionName(name, sizeof(name));
		StringToLower(name);

		Format(classname, sizeof(classname), "weapon_%s", name);
		strcopy(weapon.classname, sizeof(weapon.classname), classname);
		kv.GetString("name", name, sizeof(name));
		strcopy(weapon.display, sizeof(weapon.display), name);
		kv.GetString("type", typeStr, sizeof(typeStr));

		if (StrEqual(typeStr, "primary", false))
		{
			type = DmWeapon_Primary;
		}
		else if (StrEqual(typeStr, "secondary", false))
		{
			type = DmWeapon_Secondary;
		}
		else if (StrEqual(typeStr, "grenade", false))
		{
			type = DmWeapon_Grenade;
		}
		else if (StrEqual(typeStr, "c4", false))
		{
			type = DmWeapon_C4;
		}
		weapon.type = type;

		if (type != DmWeapon_Invalid)
		{
			weapon.id = ++id;
			g_WeaponLookup.SetArray(name, weapon, sizeof(weapon));
			g_Weapons.PushArray(weapon);
		}
	} while (kv.GotoNextKey());

	delete kv;
	return true;
}

void KillPlayerRespawnTimer(int client)
{
	if (g_PlayerRespawnTimers[client] != null)
	{
		delete g_PlayerRespawnTimers[client];
		g_PlayerRespawnTimers[client] = null;
	}
}

// Hooks
public void Event_PlayerDeath(Event event, const char[] name, bool dontBroadcast)
{
	if (!cssdm_enabled.BoolValue)
	{
		return;
	}

	int client = GetClientOfUserId(event.GetInt("userid"));
	if (client < 0 || !IsClientInGame(client))
	{
		return;
	}

	Call_StartForward(g_OnClientDeathForward);
	Call_PushCell(client);
	Action forwardResult = Plugin_Continue;
	Call_Finish(forwardResult);

	float ragdollStayTime = cssdm_ragdoll_time.FloatValue;
	if (ragdollStayTime >= 0.0 && ragdollStayTime <= 20.0)
	{
		int ragdollEnt = GetEntPropEnt(client, Prop_Send, "m_hRagdoll");

		if (ragdollEnt != -1 && IsValidEntity(ragdollEnt))
		{
			if (ragdollStayTime == 0.0)
			{
				AcceptEntityInput(ragdollEnt, "Kill");
			}
			else
			{
				int ragdollRef = EntIndexToEntRef(ragdollEnt);
				if (ragdollRef != INVALID_ENT_REFERENCE)
				{
					CreateTimer(ragdollStayTime, Timer_CleanUpRagdoll, ragdollRef);
				}
			}
		}
	}

	if (forwardResult == Plugin_Continue)
	{
		g_PlayerRespawnTimers[client] = CreateTimer(cssdm_ragdoll_time.FloatValue, Timer_PlayerRespawn, GetClientSerial(client));
	}
}

public Action Timer_PlayerRespawn(Handle timer, int serial)
{
	int client = GetClientFromSerial(serial);
	if (client >= 0 && IsClientInGame(client))
	{
		if (cssdm_enabled.BoolValue && !g_InRoundRestart)
		{
			CS_RespawnPlayer(client);
		}
		g_PlayerRespawnTimers[client] = null;
	}
	return Plugin_Continue;
}

public Action Timer_CleanUpRagdoll(Handle timer, int ragdollRef)
{
	if (!cssdm_enabled.BoolValue)
	{
		return Plugin_Continue;
	}
	int ent = EntRefToEntIndex(ragdollRef);
	if (ent != INVALID_ENT_REFERENCE && IsValidEntity(ent))
	{
		AcceptEntityInput(ent, "Kill");
	}
	return Plugin_Continue;
}

public void Event_PlayerSpawn(Event event, const char[] name, bool dontBroadcast)
{
	if (g_SkipNextPlayerSpawnCallback)
	{
		g_SkipNextPlayerSpawnCallback = false;
		return;
	}
	if (!cssdm_enabled.BoolValue)
	{
		return;
	}

	int client = GetClientOfUserId(event.GetInt("userid"));
	if (client < 0 || !IsClientInGame(client) || IsClientObserver(client))
	{
		return;
	}

	Call_StartForward(g_OnClientSpawnedForward);
	Call_PushCell(client);
	Call_Finish();
	Call_StartForward(g_OnClientPostSpawnedForward);
	Call_PushCell(client);
	Call_Finish();
}

public void Event_RoundStart(Event event, const char[] name, bool dontBroadcast)
{
	g_InRoundRestart = false;
}

public void Event_RoundEnd(Event event, const char[] name, bool dontBroadcast)
{
	g_InRoundRestart = true;
	for (int i = 0; i <= MaxClients; i++)
	{
		KillPlayerRespawnTimer(i);
	}
}

public MRESReturn Hook_OnIPointsForKill(DHookReturn hReturn, DHookParam hParams)
{
	if (cssdm_enabled.BoolValue && cssdm_ffa_enabled.BoolValue && !g_FFAFailed)
	{
		hReturn.Value = 1;
		return MRES_Supercede;
	}
	return MRES_Ignored;
}

// Native callbacks
public int Native_IsRunning(Handle plugin, int numParams)
{
	return view_as<int>(cssdm_enabled.BoolValue);
}

public void Native_GetSpawnMethod(Handle plugin, int numParams)
{
	char spawnMethod[32];
	cssdm_spawn_method.GetString(spawnMethod, sizeof(spawnMethod));
	SetNativeString(1, spawnMethod, GetNativeCell(2));
}

public int Native_GetWeaponID(Handle plugin, int numParams)
{
	char name[64];
	GetNativeString(1, name, sizeof(name));
	Weapon weapon;
	if (!g_WeaponLookup.GetArray(name, weapon, sizeof(weapon)))
	{
		return -1;
	}
	return weapon.id;
}

public int Native_GetWeaponType(Handle plugin, int numParams)
{
	int id = GetNativeCell(1);
	Weapon weapon;
	if (id >= g_Weapons.Length)
	{
		return ThrowNativeError(SP_ERROR_NATIVE, "Invalid CS:S DM weapon id (%d)", id);
	}
	g_Weapons.GetArray(id, weapon, sizeof(weapon));
	return view_as<int>(weapon.type);
}

public int Native_GetWeaponClassname(Handle plugin, int numParams)
{
	int id = GetNativeCell(1);
	Weapon weapon;
	if (id >= g_Weapons.Length)
	{
		return ThrowNativeError(SP_ERROR_NATIVE, "Invalid CS:S DM weapon id (%d)", id);
	}
	g_Weapons.GetArray(id, weapon, sizeof(weapon));
	SetNativeString(2, weapon.classname, GetNativeCell(3));
	return 1;
}

public int Native_GetWeaponName(Handle plugin, int numParams)
{
	int id = GetNativeCell(1);
	Weapon weapon;
	if (id >= g_Weapons.Length)
	{
		return ThrowNativeError(SP_ERROR_NATIVE, "Invalid CS:S DM weapon id (%d)", id);
	}
	g_Weapons.GetArray(id, weapon, sizeof(weapon));
	SetNativeString(2, weapon.display, GetNativeCell(3));
	return 1;
}

// native void DM_StripBotItems(int client);
public int Native_StripBotItems(Handle plugin, int numParams)
{
	int client = GetNativeCell(1);

	if (client <= 0 || client > MaxClients)
	{
		return ThrowNativeError(SP_ERROR_NATIVE, "Invalid client index %d", client);
	}

	if (!IsClientInGame(client))
	{
		return ThrowNativeError(SP_ERROR_NATIVE, "Client %d is not in game", client);
	}

	if (!IsFakeClient(client))
	{
		return ThrowNativeError(SP_ERROR_NATIVE, "Client %d is not a bot", client);
	}

	SDKCall(g_RemoveAllItemsSDKCall, client, false);

	return 1;
}

// native float DM_GetSpawnWaitTime();
public any Native_GetSpawnWaitTime(Handle plugin, int numParams)
{
	return cssdm_respawn_wait.FloatValue;
}

// native void DM_RespawnClient(int client, bool fullRespawn=true);
public int Native_RespawnClient(Handle plugin, int numParams)
{
	int client = GetNativeCell(1);

	if (client <= 0 || client > MaxClients)
	{
		return ThrowNativeError(SP_ERROR_NATIVE, "Invalid client index %d", client);
	}

	if (!IsClientInGame(client))
	{
		return ThrowNativeError(SP_ERROR_NATIVE, "Client %d is not in game", client);
	}

	// this will probably cause some race condition, but we don't this param anyways
	int fullRespawn = GetNativeCell(2);
	if (!fullRespawn)
	{
		g_SkipNextPlayerSpawnCallback = true;
	}

	CS_RespawnPlayer(client);

	return 1;
}

// native bool DM_IsClientAlive(int client);
public int Native_IsClientAlive(Handle plugin, int numParams)
{
	int client = GetNativeCell(1);

	if (client <= 0 || client > MaxClients)
	{
		return ThrowNativeError(SP_ERROR_NATIVE, "Invalid client index %d", client);
	}

	if (!IsClientInGame(client))
	{
		return ThrowNativeError(SP_ERROR_NATIVE, "Client %d is not in game", client);
	}

	CS_RespawnPlayer(client);

	return IsPlayerAlive(client);
}

// Utils
void StringToLower(char[] str)
{
	for (int i = 0; str[i] != '\0'; i++)
	{
		str[i] = CharToLower(str[i]);
	}
}
