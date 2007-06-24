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

#ifndef _INCLUDE_CSSDM_EVENTS_H_
#define _INCLUDE_CSSDM_EVENTS_H_

#include <sm_platform.h>
#include <edict.h>
#include <igameevents.h>

#define DECLARE_EVENT(name) \
	class cls_event_##name : public IGameEventListener2 \
	{ \
	public: \
		virtual void FireGameEvent(IGameEvent *event); \
	}; \
	extern cls_event_##name g_cls_event_##name;

DECLARE_EVENT(player_death);
DECLARE_EVENT(player_spawn);
DECLARE_EVENT(player_team);
DECLARE_EVENT(server_shutdown);
DECLARE_EVENT(round_start);
DECLARE_EVENT(round_end);
DECLARE_EVENT(item_pickup);

void OnClientCommand_Post(edict_t *edict);
void OnClientDropWeapons(CBaseEntity *pEntity);
void OnClientDroppedWeapon(CBaseEntity *pEntity, CBaseEntity *pWeapon);

#endif //_INCLUDE_CSSDM_EVENTS_H_
