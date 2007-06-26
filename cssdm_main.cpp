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


#include "cssdm_main.h"
#include "cssdm_headers.h"
#include "cssdm_utils.h"
#include "cssdm_events.h"
#include "cssdm_players.h"
#include "cssdm_ctrl.h"
#include "cssdm_config.h"
#include "cssdm_ffa.h"
#include "cssdm_callbacks.h"
#include "cssdm_weapons.h"
#include "cssdm_detours.h"

SH_DECL_HOOK3_void(IServerGameDLL, ServerActivate, SH_NOATTRIB, 0, edict_t *, int, int);
SH_DECL_HOOK0_void(IServerGameDLL, LevelShutdown, SH_NOATTRIB, 0)
SH_DECL_HOOK0_void(IServerGameDLL, DLLShutdown, SH_NOATTRIB, false);
SH_DECL_HOOK1_void(IServerGameClients, ClientCommand, SH_NOATTRIB, false, edict_t *);

Deathmatch g_DM;
IGameEventManager2 *gameevents = NULL;
IBaseFileSystem *basefilesystem = NULL;
IBinTools *bintools = NULL;
IGameConfig *g_pDmConf = NULL;
CGlobalVars *gpGlobals = NULL;
IPlayerInfoManager *playerinfomngr = NULL;
IServerGameEnts *gameents = NULL;
IServerGameClients *gameclients = NULL;
ISourcePawnEngine *spengine = NULL;
IBotManager *botmanager = NULL;
char g_GlobError[255] = {0};
bool g_IsLoadedOkay = false;
bool g_Startup = false;
bool g_IsInGlobalShutdown = false;

SMEXT_LINK(&g_DM);

#define VERIFY_SIGNATURE(name) \
	if (!g_pDmConf->GetMemSig(name, &addr) || !addr) { \
		snprintf(error, maxlength, "Could not find signature \"%s\"", name); \
		return false; \
	}

#define VERIFY_OFFSET(name) \
	if (!g_pDmConf->GetOffset(name, &offset) || !offset) { \
		snprintf(error, maxlength, "Could not find offset \"%s\"", name); \
		return false; \
	}

#define HOOK_EVENT2(name) \
	if (!gameevents->AddListener(&g_cls_event_##name, #name, true)) { \
		snprintf(error, maxlength, "Could not hook event \"%s\"", #name); \
		return false; \
	}

#define UNHOOK_EVENT2(name) gameevents->RemoveListener(&g_cls_event_##name);

bool Deathmatch::SDK_OnLoad(char *error, size_t maxlength, bool late)
{
	sharesys->AddDependency(myself, "bintools.ext", true, true);
	sharesys->AddNatives(myself, g_BaseNatives);
	if (!gameconfs->LoadGameConfigFile("cssdm.games", &g_pDmConf, error, maxlength))
	{
		return false;
	}

	void *addr;
	int offset;
	VERIFY_SIGNATURE("UTIL_Remove");
	VERIFY_SIGNATURE("RoundRespawn");
	VERIFY_SIGNATURE("CSWeaponDrop");
	VERIFY_SIGNATURE("DropWeapons");
	VERIFY_OFFSET("Weapon_GetSlot");
	VERIFY_OFFSET("RemoveAllItems");
	VERIFY_OFFSET("DropWeaponsPatch");
	VERIFY_OFFSET("CSWeaponDropPatch");
	VERIFY_OFFSET("GiveAmmo");

	if (!DM_ParseWeapons(error, maxlength))
	{
		return false;
	}

	gpGlobals = g_SMAPI->pGlobals();

	SM_InitConCommandBase();

	spengine = g_pSM->GetScriptingEngine();

	return true;
}

bool Deathmatch::SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	GET_V_IFACE_CURRENT(engineFactory, gameevents, IGameEventManager2, INTERFACEVERSION_GAMEEVENTSMANAGER2);
	GET_V_IFACE_CURRENT(fileSystemFactory, basefilesystem, IBaseFileSystem, BASEFILESYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(serverFactory, playerinfomngr, IPlayerInfoManager, INTERFACEVERSION_PLAYERINFOMANAGER);
	GET_V_IFACE_ANY(serverFactory, gameents,IServerGameEnts, INTERFACEVERSION_SERVERGAMEENTS);
	GET_V_IFACE_ANY(serverFactory, botmanager, IBotManager, INTERFACEVERSION_PLAYERBOTMANAGER);
	GET_V_IFACE_ANY(serverFactory, gameclients, IServerGameClients, INTERFACEVERSION_SERVERGAMECLIENTS);

	return true;
}

void OnDLLShutdown()
{
	g_IsInGlobalShutdown = true;
	RETURN_META(MRES_IGNORED);
}

void ServerActivate(edict_t *pEdictList, int edictCount, int clientMax)
{
	OnLevelInitialized();
	RETURN_META(MRES_IGNORED);
}

void LevelShutdown()
{
	OnLevelEnd();
	RETURN_META(MRES_IGNORED);
}

bool Startup(char *error, size_t maxlength)
{
	playerhelpers->AddClientListener(&g_ClientListener);

	HOOK_EVENT2(player_death);
	HOOK_EVENT2(player_spawn);
	HOOK_EVENT2(player_team);
	HOOK_EVENT2(round_start);
	HOOK_EVENT2(round_end);
	HOOK_EVENT2(server_shutdown);
	HOOK_EVENT2(item_pickup);

	g_Startup = true;

	SH_ADD_HOOK_STATICFUNC(IServerGameDLL, ServerActivate, gamedll, ServerActivate, true);
	SH_ADD_HOOK_STATICFUNC(IServerGameDLL, LevelShutdown, gamedll, LevelShutdown, false);
	SH_ADD_HOOK_STATICFUNC(IServerGameDLL, DLLShutdown, gamedll, OnDLLShutdown, false);
	SH_ADD_HOOK_STATICFUNC(IServerGameClients, ClientCommand, gameclients, OnClientCommand_Post, true);

	DM_InitCallbacks();

	char ffa_error[255];
	if (!DM_Prepare_FFA(ffa_error, sizeof(ffa_error)))
	{
		g_pSM->LogError(myself, "FFA will not work: %s", ffa_error);
	}

	DM_InitDetours();

	return InitializeUtils(error, maxlength);
}

void Shutdown()
{
	/* Remove hooks that could have happened whether we got started up or not */
	playerhelpers->RemoveClientListener(&g_ClientListener);

	UNHOOK_EVENT2(player_death);
	UNHOOK_EVENT2(player_spawn);
	UNHOOK_EVENT2(player_team);
	UNHOOK_EVENT2(round_start);
	UNHOOK_EVENT2(round_end);
	UNHOOK_EVENT2(server_shutdown);
	UNHOOK_EVENT2(item_pickup);

	/* If we were never started up, the rest of this is invalid */
	if (!g_Startup)
	{
		return;
	}

	/* Make sure stuff like FFA is cleaned up */
	DM_Disable();

	/* Destroy various internal things */
	DM_ShutdownDetours();
	DM_ShutdownCallbacks();
	DM_FreeWeapons();
	ShutdownUtils();

	/* Unhook everything from SourceHook */
	SH_REMOVE_HOOK_STATICFUNC(IServerGameClients, ClientCommand, gameclients, OnClientCommand_Post, true);
	SH_REMOVE_HOOK_STATICFUNC(IServerGameDLL, DLLShutdown, gamedll, OnDLLShutdown, false);
	SH_REMOVE_HOOK_STATICFUNC(IServerGameDLL, LevelShutdown, gamedll, LevelShutdown, false);
	SH_REMOVE_HOOK_STATICFUNC(IServerGameDLL, ServerActivate, gamedll, ServerActivate, true);
}

void Deathmatch::SDK_OnAllLoaded()
{
	SM_GET_LATE_IFACE(BINTOOLS, bintools);

	g_IsLoadedOkay = Startup(g_GlobError, sizeof(g_GlobError));

	if (!QueryRunning(NULL, 0))
	{
		return;
	}
}

void Deathmatch::SDK_OnUnload()
{
	DM_FreeWeapons();
	Shutdown();
	gameconfs->CloseGameConfigFile(g_pDmConf);
}

bool Deathmatch::QueryRunning(char *error, size_t maxlength)
{
	SM_CHECK_IFACE(BINTOOLS, bintools);

	if (!g_IsLoadedOkay && g_GlobError[0] != '\0')
	{
		snprintf(error, maxlength, "%s", g_GlobError);
		return false;
	}

	return true;
}

bool Deathmatch::QueryInterfaceDrop(SMInterface *pInterface)
{
	if (pInterface == bintools)
	{
		return false;
	}

	return true;
}

void Deathmatch::NotifyInterfaceDrop(SMInterface *pInterface)
{
	/* We have to take care of bintools early then... */
	if (pInterface == bintools)
	{
		ShutdownUtils();
		bintools = NULL;
	}
}
