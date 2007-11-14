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

#include "cssdm_events.h"
#include "cssdm_players.h"
#include "cssdm_utils.h"
#include "cssdm_config.h"
#include "cssdm_headers.h"
#include "cssdm_callbacks.h"
#include "cssdm_weapons.h"
#include "cssdm_detours.h"
#include <sh_list.h>

using namespace SourceHook;

bool g_InRoundRestart = false;
List<ITimer *> g_RagdollTimers;

class RagdollRemoval : public ITimedEvent
{
public:
	ResultType OnTimer(ITimer *pTimer, void *pData)
	{
		if (g_IsRunning)
		{
			IDataPack *pack = (IDataPack *)pData;
			pack->Reset();
			int index = pack->ReadCell();
			int serial = pack->ReadCell();
			edict_t *pEdict = engine->PEntityOfEntIndex(index);
			if (pEdict && !pEdict->IsFree() && DM_CheckSerial(pEdict, serial))
			{
				CBaseEntity *pEntity = DM_GetBaseEntity(index);
				if (pEntity)
				{
					DM_RemoveEntity(pEntity);
				}
			}
		}
		g_RagdollTimers.remove(pTimer);

		return Pl_Stop;
	}

	void OnTimerEnd(ITimer *pTimer, void *pData)
	{
		g_pSM->FreeDataPack((IDataPack *)pData);
	}
} s_RagdollRemover;

class PlayerSpawner : public ITimedEvent
{
public:
	ResultType OnTimer(ITimer *pTimer, void *pData)
	{
		if (!g_IsRunning)
		{
			return Pl_Stop;
		}

		/* Read pack data */
		IDataPack *pack = (IDataPack *)pData;
		pack->Reset();
		int client = pack->ReadCell();

		dm_player_t *player = DM_GetPlayer(client);

		/* Make sure the team is valid */
		int team = player->pInfo->GetTeamIndex();
		if (team != CS_TEAM_T && team != CS_TEAM_CT)
		{
			return Pl_Stop;
		}

		/* Make sure they're dead */
		if (DM_IsPlayerAlive(client))
		{
			return Pl_Stop;
		}

		DM_RespawnPlayer(client);

		return Pl_Stop;
	}

	void OnTimerEnd(ITimer *pTimer, void *pData)
	{
		IDataPack *pack = (IDataPack *)pData;
		pack->Reset();
		int client = pack->ReadCell();

		dm_player_t *player = DM_GetPlayer(client);
		if (player->respawn_timer == pTimer)
		{
			player->respawn_timer = NULL;
		}

		g_pSM->FreeDataPack((IDataPack *)pData);
	}
} s_PlayerSpawner;

void DM_ClearRagdollTimers()
{
	List<ITimer *>::iterator iter = g_RagdollTimers.begin();
	while (iter != g_RagdollTimers.end())
	{
		timersys->KillTimer((*iter));
		iter = g_RagdollTimers.erase(iter);
	}
}

void DM_SchedRespawn(int client)
{
	dm_player_t *player = DM_GetPlayer(client);

	if (player->respawn_timer)
	{
		timersys->KillTimer(player->respawn_timer);
	}

	IDataPack *pack = g_pSM->CreateDataPack();
	pack->PackCell(client);

	player->respawn_timer = timersys->CreateTimer(&s_PlayerSpawner, DM_GetRespawnWait(), pack, 0);
}

void OnClientCommand_Post(edict_t *edict)
{
	if (!g_IsRunning)
	{
		return;
	}

	const char *cmd = engine->Cmd_Argv(0);

	if (strcmp(cmd, "joinclass") == 0)
	{
		int client = engine->IndexOfEdict(edict);
		dm_player_t *player = DM_GetPlayer(client);
		if (!player || !player->pEntity)
		{
			return;
		}
	
		if (DM_IsPlayerAlive(client))
		{
			return;
		}
	
		if (!player->will_respawn_on_class)
		{
			return;
		}

		player->will_respawn_on_class = false;

		/* Respawn! */
		DM_SchedRespawn(client);
	}
}

void OnClientDropWeapons(CBaseEntity *pEntity)
{
	if (g_IsRunning)
	{
		/* Block them from having a defuse kit so they don't drop it. */
		DM_SetDefuseKit(pEntity, false);
	}
}

void OnClientDroppedWeapon(CBaseEntity *pEntity, CBaseEntity *pWeapon)
{
	if (!g_IsRunning || !pWeapon || !DM_ShouldRemoveDrops())
	{
		return;
	}

	if (DM_AllowC4())
	{
		edict_t *pEdict = gameents->BaseEntityToEdict(pWeapon);
		if (pEdict && strcmp(pEdict->GetClassName(), "weapon_c4") == 0)
		{
			return;
		}
	}

	DM_RemoveEntity(pWeapon);
}

#define IMPLEMENT_EVENT(name) \
	cls_event_##name g_cls_event_##name; \
	void cls_event_##name::FireGameEvent(IGameEvent *event)

IMPLEMENT_EVENT(player_death)
{
	if (!g_IsRunning)
	{
		return;
	}

	int userid = event->GetInt("userid");

	int client = playerhelpers->GetClientOfUserId(userid);
	if (!client)
	{
		return;
	}

	dm_player_t *player = DM_GetPlayer(client);
	if (!player)
	{
		return;
	}

	player->is_spawned = false;

	/* Remove ragdoll */
	unsigned int ragdollTime = DM_GetBodyStayTime();
	if (ragdollTime <= 20 && !g_InRoundRestart)
	{
		int serial;
		CBaseEntity *ragdoll = DM_GetAndClearRagdoll(player->pEntity, serial);
		if (ragdollTime == 0)
		{
			DM_RemoveEntity(ragdoll);
		} else {
			edict_t *pEdict = gameents->BaseEntityToEdict(ragdoll);
			if (pEdict)
			{
				int index = engine->IndexOfEdict(pEdict);
				IDataPack *pack = g_pSM->CreateDataPack();
				pack->PackCell(index);
				pack->PackCell(serial);
				ITimer *timer = timersys->CreateTimer(&s_RagdollRemover, ragdollTime, pack, 0);
				g_RagdollTimers.push_back(timer);
			}
		}
	}

	if (!DM_OnClientDeath(client))
	{
		return;
	}

	DM_SchedRespawn(client);
}

IMPLEMENT_EVENT(player_team)
{
	if (!g_IsRunning)
	{
		return;
	}

	int userid = event->GetInt("userid");
	int client = playerhelpers->GetClientOfUserId(userid);
	if (!client)
	{
		return;
	}

	dm_player_t *player = DM_GetPlayer(client);
	if (!player || !player->pEntity)
	{
		return;
	}

	if (DM_IsPlayerAlive(client) || event->GetBool("disconnect"))
	{
		return;
	}

	int old_team = event->GetInt("oldteam");
	if (old_team == CS_TEAM_T || old_team == CS_TEAM_CT)
	{
		return;
	}

	int new_team = event->GetInt("team");
	if (new_team != CS_TEAM_T && new_team != CS_TEAM_CT)
	{
		return;
	}

	if (player->pBot)
	{
		DM_SchedRespawn(client);
	} else {
		player->will_respawn_on_class = true;
	}
}

IMPLEMENT_EVENT(player_spawn)
{
	if (!g_IsRunning)
	{
		return;
	}

	int userid = event->GetInt("userid");
	int client = playerhelpers->GetClientOfUserId(userid);
	if (!client)
	{
		return;
	}

	dm_player_t *player = DM_GetPlayer(client);
	if (!player || !player->pEntity || (player->is_spawned && !g_InRoundRestart))
	{
		return;
	}

	/* This can be fired before a player gets a team! */
	int team_idx = player->pInfo->GetTeamIndex();
	if (team_idx != CS_TEAM_T
		&& team_idx != CS_TEAM_CT)
	{
		return;
	}

	bool old_value = player->is_spawned;
	player->is_spawned = true;

	/* If we were waiting to respawn, cancel the timer */
	if (player->respawn_timer)
	{
		timersys->KillTimer(player->respawn_timer);
		player->respawn_timer = NULL;
	}

	DM_OnClientSpawned(client, old_value);
}

IMPLEMENT_EVENT(server_shutdown)
{
	g_IsInGlobalShutdown = true;
}

IMPLEMENT_EVENT(round_start)
{
	g_InRoundRestart = false;
}

IMPLEMENT_EVENT(round_end)
{
	g_InRoundRestart = true;

	DM_ClearRagdollTimers();

	for (int i=1; i<=gpGlobals->maxClients; i++)
	{
		dm_player_t *player = DM_GetPlayer(i);
		player->is_spawned = false;
	}
}

IMPLEMENT_EVENT(item_pickup)
{
	if (!g_IsRunning || DM_AllowC4())
	{
		return;
	}

	const char *weapon = event->GetString("item");
	if (strcmp(weapon, "c4") != 0)
	{
		return;
	}

	int userid = event->GetInt("userid");
	int client = playerhelpers->GetClientOfUserId(userid);
	if (!client)
	{
		return;
	}

	dm_player_t *player = DM_GetPlayer(client);
	if (!player || !player->pEntity)
	{
		return;
	}

	CBaseEntity *pWeapon = DM_GetWeaponFromSlot(player->pEntity, (int)WeaponType_C4);
	if (pWeapon)
	{
		DM_DropWeapon(player->pEntity, pWeapon);
	}
}

