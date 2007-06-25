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
#include "cssdm_config.h"
#include "cssdm_ctrl.h"
#include "cssdm_ffa.h"
#include "cssdm_callbacks.h"

extern ConVar cssdm_enabled;
extern ConVar cssdm_ffa_enabled;
extern bool g_IsLoadedOkay;

bool g_IsRunning = false;
bool g_IsMapStarted = false;
bool g_HasConfigRan = false;

void DM_StartEverything()
{
	if (!g_IsMapStarted || g_HasConfigRan)
	{
		return;
	}

	g_HasConfigRan = true;

	/* If we're not running but we should be, start up now.
	 * g_IsRunning should never be true here.
	 */
	if (!g_IsRunning && cssdm_enabled.GetInt() == 1)
	{
		/* We should be enabled! */
		DM_Enable();
	}
}

void OnLevelInitialized()
{
	if (!g_IsLoadedOkay)
	{
		return;
	}

	g_IsMapStarted = true;
	engine->ServerCommand("exec cssdm/cssdm.cfg\n");

	char map_cmd[128];
	snprintf(map_cmd, sizeof(map_cmd), "exec cssdm/maps/%s.cssdm.cfg\n", STRING(gpGlobals->mapname));
	engine->ServerCommand(map_cmd);
	
	engine->ServerCommand("cssdm internal 1\n");
}

void OnLevelEnd()
{
	/* We fully disable CS:S DM when the map ends.
	 * This is so our DM_Enable call is properly paired, but
	 * also so CGameRules hooks get flushed.
	 */
	if (g_IsRunning)
	{
		DM_Disable();
	}

	g_IsMapStarted = false;
	g_HasConfigRan = false;
}

bool DM_Disable()
{
	if (!g_IsLoadedOkay || !g_IsRunning)
	{
		return false;
	}

	g_IsRunning = false;

	/* If FFA is enabled, we must unpatch it. */
	if (DM_FFA_IsPatched())
	{
		DM_Unpatch_FFA();
	}

	DM_OnShutdown();

	return true;
}

bool DM_Enable()
{
	if (!g_IsLoadedOkay || g_IsRunning || !g_IsMapStarted || !g_HasConfigRan)
	{
		return false;
	}

	g_IsRunning = true;

	DM_OnStartup();

	/* If FFA should be enabled, patch it. */
	if (cssdm_ffa_enabled.GetInt() != 0
		&& DM_FFA_IsPrepared()
		&& !DM_FFA_IsPatched())
	{
		DM_Patch_FFA();
	}

	return true;
}

CON_COMMAND(cssdm, "CS:S DM console menu")
{
	int args = engine->Cmd_Argc();
	if (args < 2)
	{
		/* :TODO: display menu */
		return;
	}

	const char *arg = engine->Cmd_Argv(1);
	if (strcmp(arg, "internal") == 0)
	{
		if (args < 3)
		{
			return;
		}
		int num = atoi(engine->Cmd_Argv(2));
		if (num == 1)
		{
			DM_StartEverything();
		}
	}
}
