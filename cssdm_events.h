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

#if SOURCE_ENGINE == SE_CSGO
#define DECLARE_EVENT(name) \
	class cls_event_##name : public IGameEventListener2 \
	{ \
	public: \
		void FireGameEvent(IGameEvent *event) override; \
		int	GetEventDebugID( void ) override { return EVENT_DEBUG_ID_INIT; } \
	}; \
	extern cls_event_##name g_cls_event_##name;
#else
#define DECLARE_EVENT(name) \
	class cls_event_##name : public IGameEventListener2 \
	{ \
	public: \
		void FireGameEvent(IGameEvent *event) override; \
	}; \
	extern cls_event_##name g_cls_event_##name;
#endif

DECLARE_EVENT(player_death);
DECLARE_EVENT(player_spawn);
DECLARE_EVENT(player_team);
DECLARE_EVENT(server_shutdown);
DECLARE_EVENT(round_start);
DECLARE_EVENT(round_end);

class CCommand;

void DM_ClearRagdollTimers();
#if METAMOD_PLAPI_VERSION < 18
void OnClientCommand_Post(edict_t *edict, const CCommand &args);
#else
KHook::Return<void> OnClientCommand_Post(IServerGameClients *gameClients, edict_t *edict, const CCommand &args);
#endif

class DMData
{
public:
	DMData(int iIndex, int iSerial = 0)
	{
		this->index = iIndex;
		this->serial = iSerial;
	}

public:
	int index;
	int serial;
};

#endif //_INCLUDE_CSSDM_EVENTS_H_
