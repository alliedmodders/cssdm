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

#include "cssdm_players.h"
#include "cssdm_headers.h"
#include "cssdm_utils.h"
#include "cssdm_config.h"
#include "cssdm_includesdk.h"

ClientListener g_ClientListener;
dm_player_t g_players[65];

void DM_ResetPlayer(int client);
void DM_ActivatePlayer(int client);

void ClientListener::OnClientConnected(int client)
{
	memset(&g_players[client], 0, sizeof(dm_player_t));
}

void ClientListener::OnClientDisconnecting(int client)
{
	dm_player_t *player = DM_GetPlayer(client);

	DM_ResetPlayer(client);
}

void ClientListener::OnClientPutInServer(int client)
{
	DM_ActivatePlayer(client);
}

void DM_ResetPlayer(int client)
{
	dm_player_t *player = DM_GetPlayer(client);
	if (!player)
	{
		return;
	}

	if (player->respawn_timer)
	{
		timersys->KillTimer(player->respawn_timer);
	}

	memset(player, 0, sizeof(dm_player_t));
}

void DM_ActivatePlayer(int client)
{
	dm_player_t *player = DM_GetPlayer(client);
	if (!player)
	{
		return;
	}

	CBaseEntity *pEntity = DM_GetBaseEntity(client);
	if (!pEntity)
	{
		return;
	}

	player->pEdict = gamehelpers->EdictOfIndex(client);
	player->pEntity = pEntity;
	player->pInfo = playerinfomngr->GetPlayerInfo(player->pEdict);
	player->pBot = botmanager->GetBotController(player->pEdict);
	player->userid = engine->GetPlayerUserId(player->pEdict);
}

dm_player_t *DM_GetPlayer(int client)
{
	if (client < 1 || client > gpGlobals->maxClients)
	{
		return NULL;
	}

	return &g_players[client];
}
