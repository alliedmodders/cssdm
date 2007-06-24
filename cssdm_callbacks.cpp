/**
 * vim: set ts=4 :
 * ===============================================================
 * CS:S DM, Copyright (C) 2004-2007 AlliedModders LLC. 
 * By David "BAILOPAN" Anderson
 * All rights reserved.
 * ===============================================================
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301 USA
 * 
 * Version: $Id$
 */

#include "cssdm_headers.h"
#include "cssdm_callbacks.h"
#include "cssdm_config.h"
#include "cssdm_weapons.h"
#include "cssdm_players.h"
#include "cssdm_utils.h"

IForward *g_pOnClientDeath = NULL;
IForward *g_pOnClientSpawned = NULL;
IForward *g_pOnClientPostSpawned = NULL;
IForward *g_pOnSetSpawnMethod = NULL;
IForward *g_pOnStartup = NULL;
IForward *g_pOnShutdown = NULL;

void DM_InitCallbacks()
{
	g_pOnClientDeath = forwards->CreateForward("DM_OnClientDeath", ET_Hook, 1, NULL, Param_Cell);
	g_pOnClientSpawned = forwards->CreateForward("DM_OnClientSpawned", ET_Ignore, 1, NULL, Param_Cell);
	g_pOnClientPostSpawned = forwards->CreateForward("DM_OnClientPostSpawned", ET_Ignore, 1, NULL, Param_Cell);
	g_pOnSetSpawnMethod = forwards->CreateForward("DM_OnSetSpawnMethod", ET_Hook, 1, NULL, Param_String);
	g_pOnStartup = forwards->CreateForward("DM_OnStartup", ET_Ignore, 0, NULL);
	g_pOnShutdown = forwards->CreateForward("DM_OnShutdown", ET_Ignore, 0, NULL);
}

void DM_ShutdownCallbacks()
{
	forwards->ReleaseForward(g_pOnClientDeath);
	forwards->ReleaseForward(g_pOnClientSpawned);
	forwards->ReleaseForward(g_pOnClientPostSpawned);
	forwards->ReleaseForward(g_pOnSetSpawnMethod);
	forwards->ReleaseForward(g_pOnStartup);
	forwards->ReleaseForward(g_pOnShutdown);
}

bool DM_OnClientDeath(int client)
{
	cell_t res = 0;
	g_pOnClientDeath->PushCell(client);
	g_pOnClientDeath->Execute(&res);

	return (res == (cell_t)Pl_Continue);
}

void DM_OnClientSpawned(int client, bool wasDead)
{
	g_pOnClientSpawned->PushCell(client);
	g_pOnClientSpawned->Execute(NULL);
	g_pOnClientPostSpawned->PushCell(client);
	g_pOnClientPostSpawned->Execute(NULL);
}

void DM_OnSetSpawnMethod(const char *method)
{
	g_pOnSetSpawnMethod->PushString(method);
	g_pOnSetSpawnMethod->Execute(NULL);
}

void DM_OnStartup()
{
	g_pOnStartup->Execute(NULL);
}

void DM_OnShutdown()
{
	g_pOnShutdown->Execute(NULL);
}

static cell_t DMN_GetSpawnMethod(IPluginContext *pContext, const cell_t *params)
{
	pContext->StringToLocal(params[1], params[2], DM_GetSpawnMethod());
	return 1;
}

static cell_t DMN_GetWeaponID(IPluginContext *pContext, const cell_t *params)
{
	char *str;
	dm_weapon_t *weapon;
	
	pContext->LocalToString(params[1], &str);
	if ((weapon = DM_FindWeapon(str)) == NULL)
	{
		return -1;
	}

	return weapon->id;
}

static cell_t DMN_GetWeaponType(IPluginContext *pContext, const cell_t *params)
{
	dm_weapon_t *weapon;

	if ((weapon = DM_GetWeapon(params[1])) == NULL)
	{
		return pContext->ThrowNativeError("Invalid CS:S DM weapon id (%d)", params[1]);
	}

	return (cell_t)weapon->type;
}

static cell_t DMN_GetWeaponClassname(IPluginContext *pContext, const cell_t *params)
{
	dm_weapon_t *weapon;

	if ((weapon = DM_GetWeapon(params[1])) == NULL)
	{
		return pContext->ThrowNativeError("Invalid CS:S DM weapon id (%d)", params[1]);
	}

	pContext->StringToLocal(params[2], params[3], weapon->classname);

	return 1;
}

static cell_t DMN_GetWeaponName(IPluginContext *pContext, const cell_t *params)
{
	dm_weapon_t *weapon;

	if ((weapon = DM_GetWeapon(params[1])) == NULL)
	{
		return pContext->ThrowNativeError("Invalid CS:S DM weapon id (%d)", params[1]);
	}

	pContext->StringToLocal(params[2], params[3], weapon->display);

	return 1;
}

static cell_t DMN_StripBotItems(IPluginContext *pContext, const cell_t *params)
{
	if (params[1] < 1 || params[1] > gpGlobals->maxClients)
	{
		return pContext->ThrowNativeError("Invalid client index %d", params[1]);
	}

	dm_player_t *player = DM_GetPlayer(params[1]);
	if (!player->pEntity)
	{
		return pContext->ThrowNativeError("Client %d is not in game", params[1]);
	}

	if (!player->pBot)
	{
		return pContext->ThrowNativeError("Client %d is not a bot", params[1]);
	}

	player->pBot->RemoveAllItems(false);

	return 1;
}

static cell_t DMN_GetClientWeapon(IPluginContext *pContext, const cell_t *params)
{
	if (params[1] < 1 || params[1] > gpGlobals->maxClients)
	{
		return pContext->ThrowNativeError("Invalid client index %d", params[1]);
	}

	dm_player_t *player = DM_GetPlayer(params[1]);
	if (!player->pEntity)
	{
		return pContext->ThrowNativeError("Client %d is not in game", params[1]);
	}

	CBaseEntity *pEntity = DM_GetWeaponFromSlot(player->pEntity, params[2]);
	if (!pEntity)
	{
		return -1;
	}

	edict_t *pEdict = gameents->BaseEntityToEdict(pEntity);
	if (!pEdict)
	{
		return -1;
	}

	return engine->IndexOfEdict(pEdict);
}

static cell_t DMN_DropWeapon(IPluginContext *pContext, const cell_t *params)
{
	if (params[1] < 1 || params[1] > gpGlobals->maxClients)
	{
		return pContext->ThrowNativeError("Invalid client index %d", params[1]);
	}

	dm_player_t *player = DM_GetPlayer(params[1]);
	if (!player->pEntity)
	{
		return pContext->ThrowNativeError("Client %d is not in game", params[1]);
	}

	CBaseEntity *pWeapon = DM_GetBaseEntity(params[2]);
	if (!pWeapon)
	{
		return pContext->ThrowNativeError("Entity %d is not a valid entity", params[2]);
	}

	DM_DropWeapon(player->pEntity, pWeapon);

	return 1;
}

static cell_t DMN_IsRunning(IPluginContext *pContext, const cell_t *params)
{
	return g_IsRunning ? true : false;
}

static cell_t DMN_GetSpawnWaitTime(IPluginContext *pContext, const cell_t *params)
{
	float f = DM_GetRespawnWait();
	return sp_ftoc(f);
}

static cell_t DMN_RespawnClient(IPluginContext *pContext, const cell_t *params)
{
	if (params[1] < 1 || params[1] > gpGlobals->maxClients)
	{
		return pContext->ThrowNativeError("Invalid client index %d", params[1]);
	}

	dm_player_t *player = DM_GetPlayer(params[1]);
	if (!player->pEntity)
	{
		return pContext->ThrowNativeError("Client %d is not in game", params[1]);
	}

	player->is_spawned = params[2] ? false : true;

	DM_RespawnPlayer(params[1]);

	return 1;
}

static cell_t DMN_IsClientAlive(IPluginContext *pContext, const cell_t *params)
{
	if (params[1] < 1 || params[1] > gpGlobals->maxClients)
	{
		return pContext->ThrowNativeError("Invalid client index %d", params[1]);
	}

	dm_player_t *player = DM_GetPlayer(params[1]);
	if (!player->pEntity)
	{
		return pContext->ThrowNativeError("Client %d is not in game", params[1]);
	}

	return DM_IsPlayerAlive(params[1]) ? 1 : 0;
}

static cell_t DMN_GiveAmmo(IPluginContext *pContext, const cell_t *params)
{
	if (params[1] < 1 || params[1] > gpGlobals->maxClients)
	{
		return pContext->ThrowNativeError("Invalid client index %d", params[1]);
	}

	dm_player_t *player = DM_GetPlayer(params[1]);
	if (!player->pEntity)
	{
		return pContext->ThrowNativeError("Client %d is not in game", params[1]);
	}

	return DM_GiveAmmo(player->pEntity, params[2], params[3], params[4] ? true : false);
}

sp_nativeinfo_t g_BaseNatives[] = 
{
	{"DM_GetSpawnMethod",		DMN_GetSpawnMethod},
	{"DM_GetWeaponID",			DMN_GetWeaponID},
	{"DM_GetWeaponType",		DMN_GetWeaponType},
	{"DM_StripBotItems",		DMN_StripBotItems},
	{"DM_GetWeaponClassname",	DMN_GetWeaponClassname},
	{"DM_GetClientWeapon",		DMN_GetClientWeapon},
	{"DM_DropWeapon",			DMN_DropWeapon},
	{"DM_GetWeaponName",		DMN_GetWeaponName},
	{"DM_IsRunning",			DMN_IsRunning},
	{"DM_GetSpawnWaitTime",		DMN_GetSpawnWaitTime},
	{"DM_RespawnClient",		DMN_RespawnClient},
	{"DM_IsClientAlive",		DMN_IsClientAlive},
	{"DM_GiveClientAmmo",		DMN_GiveAmmo},
	{NULL,						NULL},
};
