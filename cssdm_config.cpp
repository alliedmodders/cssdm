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

#include "smsdk_ext.h"
#include "cssdm_config.h"
#include "cssdm_ctrl.h"
#include "cssdm_ffa.h"
#include "cssdm_callbacks.h"
#include "cssdm_headers.h"

static void ChangeStatus(IConVar *cvar, const char *value, float flOldValue);
static void ChangeFFAStatus(IConVar *cvar, const char *value, float flOldValue);
static void ChangeSpawnStatus(IConVar *cvar, const char *value, float flOldValue);

ConVar cssdm_ragdoll_time("cssdm_ragdoll_time", "2", 0, "Sets ragdoll stay time", true, 0.0, true, 20.0);
ConVar cssdm_respawn_wait("cssdm_respawn_wait", "0.75", 0, "Sets respawn wait time");
ConVar cssdm_allow_c4("cssdm_allow_c4", "0", 0, "Sets whether C4 is allowed");
ConVar cssdm_version("cssdm_version", SMEXT_CONF_VERSION, FCVAR_REPLICATED|FCVAR_NOTIFY, "CS:S DM Version");
ConVar cssdm_enabled("cssdm_enabled", 
					 "1", 
					 FCVAR_REPLICATED|FCVAR_NOTIFY, 
					 "Sets whether CS:S DM is enabled",
					 false, 0.0f, false, 0.0f,
					 ChangeStatus);
ConVar cssdm_ffa_enabled("cssdm_ffa_enabled", 
						 "0", 
						 FCVAR_REPLICATED|FCVAR_NOTIFY, 
						 "Sets whether Free-For-All mode is enabled",
						 false, 0.0f, false, 0.0f,
						 ChangeFFAStatus);
ConVar cssdm_spawn_method("cssdm_spawn_method",
						  "preset",
						  0,
						  "Sets how and where players are spawned",
						  false, 0.0f, false, 0.0f,
						  ChangeSpawnStatus);
ConVar cssdm_remove_drops("cssdm_remove_drops", "1", 0, "Sets whether dropped items are removed");

class LinkConVars : public IConCommandBaseAccessor
{
public:
	bool RegisterConCommandBase(ConCommandBase *pBase)
	{
		return META_REGCVAR(pBase);
	}
} s_LinkConVars;

void SM_InitConCommandBase()
{
	g_pCVar = icvar;
	ConVar_Register(0, &s_LinkConVars);
}

static void ChangeStatus(IConVar *cvar, const char *value, float flOldValue)
{
	if (cssdm_enabled.GetInt())
	{
		DM_Enable();
	} else {
		DM_Disable();
	}
}

static void ChangeFFAStatus(IConVar *cvar, const char *value, float flOldValue)
{
	if (cssdm_ffa_enabled.GetInt() && !DM_FFA_IsPatched() && DM_FFA_IsPrepared())
	{
		DM_Patch_FFA();
	} else if (!cssdm_ffa_enabled.GetInt() && DM_FFA_IsPatched()) {
		DM_Unpatch_FFA();
	}
}

static void ChangeSpawnStatus(IConVar *cvar, const char *value, float flOldValue)
{
	if (strcmp(value, cssdm_spawn_method.GetString()) == 0)
	{
		return;
	}

	DM_OnSetSpawnMethod(cssdm_spawn_method.GetString());
}

const char *DM_GetSpawnMethod()
{
	return cssdm_spawn_method.GetString();
}

unsigned int DM_GetBodyStayTime()
{
	return cssdm_ragdoll_time.GetInt();
}

float DM_GetRespawnWait()
{
	return cssdm_respawn_wait.GetFloat();
}

bool DM_AllowC4()
{
	return (cssdm_allow_c4.GetInt() != 0);
}

bool DM_IsEnabled()
{
	return (cssdm_enabled.GetInt() != 0);
}

bool DM_IsFFAEnabled()
{
	return (cssdm_ffa_enabled.GetInt() != 0);
}

bool DM_ShouldRemoveDrops()
{
	return (cssdm_remove_drops.GetInt() != 0);
}
