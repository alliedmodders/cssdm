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
#include "cssdm_version.h"

#if METAMOD_PLAPI_VERSION < 18
SH_DECL_HOOK0_void(IServerGameDLL, DLLShutdown, SH_NOATTRIB, false);
SH_DECL_HOOK2_void(IServerGameClients, ClientCommand, SH_NOATTRIB, false, edict_t *, const CCommand &);
#endif

Deathmatch g_DM;
IGameEventManager2 *gameevents = NULL;
IBaseFileSystem *basefilesystem = NULL;
IBinTools *bintools = NULL;
ISDKTools *sdktools = NULL;
IGameConfig *g_pDmConf = NULL;
CGlobalVars *gpGlobals = NULL;
IPlayerInfoManager *playerinfomngr = NULL;
IServerGameEnts *gameents = NULL;
IServerGameClients *gameclients = NULL;
ISourcePawnEngine *spengine = NULL;
IBotManager *botmanager = NULL;
ICvar *icvar = NULL;
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
	sharesys->AddDependency(myself, "sdktools.ext", true, true);
	sharesys->RegisterLibrary(myself, "cssdm");
	sharesys->AddNatives(myself, g_BaseNatives);
	if (!gameconfs->LoadGameConfigFile("cssdm.games", &g_pDmConf, error, maxlength))
	{
		return false;
	}

	void *addr;
	int offset;
	VERIFY_SIGNATURE("RoundRespawn");
	VERIFY_OFFSET("RemoveAllItems");

	if (!DM_ParseWeapons(error, maxlength))
	{
		return false;
	}

	gpGlobals = g_SMAPI->GetCGlobals();

	SM_InitConCommandBase();

	spengine = g_pSM->GetScriptingEngine();

	return true;
}

bool Deathmatch::SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	GET_V_IFACE_CURRENT(GetEngineFactory, gameevents, IGameEventManager2, INTERFACEVERSION_GAMEEVENTSMANAGER2);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, basefilesystem, IBaseFileSystem, BASEFILESYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, playerinfomngr, IPlayerInfoManager, INTERFACEVERSION_PLAYERINFOMANAGER);
	GET_V_IFACE_ANY(GetServerFactory, gameents,IServerGameEnts, INTERFACEVERSION_SERVERGAMEENTS);
	GET_V_IFACE_ANY(GetServerFactory, botmanager, IBotManager, INTERFACEVERSION_PLAYERBOTMANAGER);
	GET_V_IFACE_ANY(GetServerFactory, gameclients, IServerGameClients, INTERFACEVERSION_SERVERGAMECLIENTS);
	GET_V_IFACE_CURRENT(GetEngineFactory, icvar, ICvar, CVAR_INTERFACE_VERSION);

	return true;
}

#if METAMOD_PLAPI_VERSION < 18
void OnDLLShutdown()
#else
KHook::Return<void> OnDLLShutdown(IServerGameDLL *server)
#endif
{
	g_IsInGlobalShutdown = true;
#if METAMOD_PLAPI_VERSION < 18
	RETURN_META(MRES_IGNORED);
#else
	return { KHook::Action::Ignore };
#endif
}

#if METAMOD_PLAPI_VERSION >= 18
KHook::Virtual<IServerGameDLL, void> Hook_DLLShutdown(&IServerGameDLL::DLLShutdown, nullptr, OnDLLShutdown);
KHook::Virtual<IServerGameClients, void, edict_t *, const CCommand &> Hook_ClientCommand(
	&IServerGameClients::ClientCommand, nullptr, OnClientCommand_Post);
#endif

bool Startup(char *error, size_t maxlength)
{
	playerhelpers->AddClientListener(&g_ClientListener);

	HOOK_EVENT2(player_death);
	HOOK_EVENT2(player_spawn);
	HOOK_EVENT2(player_team);
	HOOK_EVENT2(round_start);
	HOOK_EVENT2(round_end);
	HOOK_EVENT2(server_shutdown);

	g_Startup = true;

#if METAMOD_PLAPI_VERSION < 18
	SH_ADD_HOOK_STATICFUNC(IServerGameDLL, DLLShutdown, gamedll, OnDLLShutdown, false);
	SH_ADD_HOOK_STATICFUNC(IServerGameClients, ClientCommand, gameclients, OnClientCommand_Post, true);
#else
	Hook_DLLShutdown.Add(gamedll);
	Hook_ClientCommand.Add(gameclients);
#endif

	DM_InitCallbacks();

#if SOURCE_ENGINE != SE_CSGO
	char ffa_error[255];
	if (!DM_Prepare_FFA(ffa_error, sizeof(ffa_error)))
	{
		g_pSM->LogError(myself, "FFA will not work: %s", ffa_error);
	}
#endif

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

	/* If we were never started up, the rest of this is invalid */
	if (!g_Startup)
	{
		return;
	}

	/* Make sure stuff like FFA is cleaned up */
	DM_Disable();

	/* Destroy various internal things */
	DM_ShutdownCallbacks();
	DM_FreeWeapons();
	ShutdownUtils();

	/* Unhook everything from SourceHook */
#if METAMOD_PLAPI_VERSION < 18
	SH_REMOVE_HOOK_STATICFUNC(IServerGameClients, ClientCommand, gameclients, OnClientCommand_Post, true);
	SH_REMOVE_HOOK_STATICFUNC(IServerGameDLL, DLLShutdown, gamedll, OnDLLShutdown, false);
#else
	Hook_ClientCommand.Remove(gameclients);
	Hook_DLLShutdown.Remove(gamedll);
#endif
}

void Deathmatch::SDK_OnAllLoaded()
{
	SM_GET_LATE_IFACE(BINTOOLS, bintools);
	SM_GET_LATE_IFACE(SDKTOOLS, sdktools);

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
	SM_CHECK_IFACE(SDKTOOLS, sdktools);

	if (!g_IsLoadedOkay && g_GlobError[0] != '\0')
	{
		snprintf(error, maxlength, "%s", g_GlobError);
		return false;
	}

	return true;
}

void Deathmatch::OnCoreMapStart(edict_t *pEdictList, int edictCount, int clientMax)
{
	OnLevelInitialized();
}

void Deathmatch::OnCoreMapEnd()
{
	OnLevelEnd();
}

bool Deathmatch::QueryInterfaceDrop(SMInterface *pInterface)
{
	if (pInterface == bintools || pInterface == sdktools)
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
	if (pInterface == sdktools)
	{
		sdktools = NULL;
	}
}

const char *Deathmatch::GetExtensionVerString()
{
	return CSSDM_FULL_VERSION;
}

