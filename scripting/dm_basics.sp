/**
 * dm_basics.sp
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

#pragma semicolon 1
#include <sourcemod>
#include <sdktools>
#include <cssdm>

/* Plugin stuff */
ConVar cssdm_respawn_command;
ConVar cssdm_force_mapchanges;
ConVar cssdm_refill_ammo;
ConVar cssdm_disable_welcome_msgs;
ConVar mp_timelimit;
Handle g_ChangeMapTimer = INVALID_HANDLE;
bool g_AmmoHooks = false;
int g_ActiveWepOffs = -1;

/* Player stuff */
float g_DeathTimes[MAXPLAYERS+1];
bool g_bHasNoAmmoInClip1[MAXPLAYERS+1] = {false,...};

Handle g_IntermissionSDKCall;

/** PUBLIC INFO */
public Plugin myinfo =
{
	name = "CS:S DM Basics",
	author = "AlliedModders LLC",
	description = "Basic CS:S DM Commands/Features",
	version = CSSDM_VERSION,
	url = "http://www.bailopan.net/cssdm/"
};

public void OnPluginStart()
{
	LoadTranslations("cssdm.phrases");

	cssdm_respawn_command = CreateConVar("cssdm_respawn_command", "1", "Sets whether clients can manually respawn");
	cssdm_force_mapchanges = CreateConVar("cssdm_force_mapchanges", "0", "Sets whether CS:S DM should force mapchanges");
	cssdm_refill_ammo = CreateConVar("cssdm_refill_ammo", "1", "Sets whether CS:S DM automatically refills ammo");
	cssdm_disable_welcome_msgs = CreateConVar("cssdm_disable_welcome_msgs", "0", "Sets whether CS:S DM should disable the welcome messages");
	mp_timelimit = FindConVar("mp_timelimit");

	cssdm_force_mapchanges.AddChangeHook(CvarChange_RestartMapTimer);
	mp_timelimit.AddChangeHook(CvarChange_RestartMapTimer);
	cssdm_refill_ammo.AddChangeHook(CvarChange_RefillAmmo);

	g_ActiveWepOffs = FindSendPropInfo("CCSPlayer", "m_hActiveWeapon");

	GameData gamedata = new GameData("cssdm.games");
	if (gamedata == null)
	{
		SetFailState("Cannot load CSSDM gamedata.");
	}

	StartPrepSDKCall(SDKCall_GameRules);
	if (!PrepSDKCall_SetFromConf(gamedata, SDKConf_Virtual, "GoToIntermission"))
	{
		delete gamedata;
		SetFailState("Cannot find GoToIntermission offset.");
	}
	g_IntermissionSDKCall = EndPrepSDKCall();

	delete gamedata;
}

public void DM_OnStartup()
{
	RestartMapTimer();

	g_AmmoHooks = cssdm_refill_ammo.BoolValue;
	if (g_AmmoHooks && g_ActiveWepOffs > 0)
	{
		HookEvent("weapon_reload", Event_CheckDepleted);
		HookEvent("weapon_fire_on_empty", Event_CheckDepleted);
	}
}

public void DM_OnShutdown()
{
	if (g_ChangeMapTimer != INVALID_HANDLE)
	{
		delete g_ChangeMapTimer;
		g_ChangeMapTimer = INVALID_HANDLE;
	}
	ShutdownAmmoHooks();
}

void ShutdownAmmoHooks()
{
	if (g_AmmoHooks)
	{
		UnhookEvent("weapon_reload", Event_CheckDepleted);
		UnhookEvent("weapon_fire_on_empty", Event_CheckDepleted);
		g_AmmoHooks = false;
	}
}

public void CvarChange_RefillAmmo(ConVar cvar, const char[] oldvalue, const char[] newvalue)
{
	if (cvar.BoolValue && !g_AmmoHooks && g_ActiveWepOffs > 0)
	{
		HookEvent("weapon_reload", Event_CheckDepleted);
		HookEvent("weapon_fire_on_empty", Event_CheckDepleted);
	} else if (!cvar.BoolValue && g_AmmoHooks) {
		UnhookEvent("weapon_reload", Event_CheckDepleted);
		UnhookEvent("weapon_fire_on_empty", Event_CheckDepleted);
	}
}

public void Event_CheckDepleted(Event event, const char[] name, bool dontBroadcast)
{
	int client = GetClientOfUserId(GetEventInt(event, "userid"));
	if (!client || !IsClientInGame(client))
	{
		return;
	}

	int entity = GetEntDataEnt2(client, g_ActiveWepOffs);
	if (entity < 1)
	{
		return;
	}

	int ammoType = GetEntProp(entity, Prop_Send, "m_iPrimaryAmmoType");

	/* Give something reasonable -- the game will chop it off */
	DM_GiveClientAmmo(client, ammoType, 200, false);
}

public void CvarChange_RestartMapTimer(ConVar cvar, const char[] oldvalue, const char[] newvalue)
{
	RestartMapTimer();
}

void RestartMapTimer()
{
	if (g_ChangeMapTimer != INVALID_HANDLE)
	{
		delete g_ChangeMapTimer;
		g_ChangeMapTimer = INVALID_HANDLE;
	}

	if (cssdm_force_mapchanges.IntValue >= 1)
	{
		if (mp_timelimit.IntValue <= 0)
		{
			return;
		}
		/* Find how much time is left in the map */
		float seconds = (mp_timelimit.IntValue * 60.0) - GetGameTime();
		g_ChangeMapTimer = CreateTimer(seconds, Timer_ChangeMap);
	}
}

public Action Timer_ChangeMap(Handle timer)
{
	g_ChangeMapTimer = INVALID_HANDLE;

	if (cssdm_force_mapchanges.IntValue != 2)
	{
		char nextmap[PLATFORM_MAX_PATH];
		if (!GetNextMap(nextmap, sizeof(nextmap)))
		{
			return Plugin_Stop;
		}
		ForceChangeLevel(nextmap, "Switching map due to cssdm_force_mapchanges");
	}
	else
	{
		SDKCall(g_IntermissionSDKCall);
	}

	return Plugin_Stop;
}

public Action Timer_Welcome(Handle timer, int userid)
{
	int client = GetClientOfUserId(userid);

	if (!client || !IsClientInGame(client))
	{
		return Plugin_Stop;
	}

	PrintToChat(client, "[CSSDM] %t", "Welcome Version", CSSDM_VERSION);
	PrintToChat(client, "[CSSDM] %t", "Welcome Download");

	return Plugin_Stop;
}

public void OnClientPutInServer(int client)
{
	g_DeathTimes[client] = 0.0;
	if (!cssdm_disable_welcome_msgs.BoolValue)
	{
		CreateTimer(10.0, Timer_Welcome, GetClientUserId(client));
	}

	g_bHasNoAmmoInClip1[client] = false;
}

public Action DM_OnClientDeath(int client)
{
	g_DeathTimes[client] = GetGameTime();
	return Plugin_Continue;
}

public Action OnClientSayCommand(int client, const char[] command, const char[] sArgs)
{
	if (client <= 0 || IsFakeClient(client))
	{
		return Plugin_Continue;
	}

	if (!DM_IsRunning() || !cssdm_respawn_command.BoolValue)
	{
		return Plugin_Continue;
	}

	if (StrEqual(sArgs, "respawn", false))
	{
		if (!IsClientInGame(client))
		{
			PrintToChat(client, "[CSSDM] %t", "NoRespawn NotInGame");
			return Plugin_Handled;
		}

		if (DM_IsClientAlive(client))
		{
			PrintToChat(client, "[CSSDM] %t", "NoRespawn Alive");
			return Plugin_Handled;
		}

		int team = GetClientTeam(client);
		if (team != CSSDM_TEAM_T && team != CSSDM_TEAM_CT)
		{
			PrintToChat(client, "[CSSDM] %t", "NoRespawn Team");
			return Plugin_Handled;
		}

		float elapsed = GetGameTime() - g_DeathTimes[client];
		float wait_time = DM_GetSpawnWaitTime();
		if (elapsed < wait_time)
		{
			PrintToChat(client, "[CSSDM] %t", "NoRespawn Wait", wait_time - elapsed);
			return Plugin_Handled;
		}

		/* We passed the tests... respawn! */
		DM_RespawnClient(client);

		return Plugin_Handled;
	}

	return Plugin_Continue;
}

public Action OnPlayerRunCmd(int client, int& buttons, int& impulse, float vel[3], float angles[3], int& weapon,
	int& subtype, int& cmdnum, int& tickcount, int& seed, int mouse[2])
{
    if(!g_AmmoHooks)
        return Plugin_Continue;

    int entity = GetEntDataEnt2(client, g_ActiveWepOffs);
    if (entity < 1)
    {
        g_bHasNoAmmoInClip1[client] = false;
        return Plugin_Continue;
    }

    // He's out of ammo -> reloading
    int iClip1 = GetEntProp(entity, Prop_Send, "m_iClip1");
    if(iClip1 == 0)
    {
        g_bHasNoAmmoInClip1[client] = true;
    }
    // He got ammo in clip1 again but hadn't before -> reloaded.
    else if(g_bHasNoAmmoInClip1[client])
    {
        g_bHasNoAmmoInClip1[client] = false;

        int ammoType = GetEntProp(entity, Prop_Send, "m_iPrimaryAmmoType");

        if(ammoType <= 0)
        {
            return Plugin_Continue;
        }

        // Give some ammo.
        DM_GiveClientAmmo(client, ammoType, 200, false);
    }

    return Plugin_Continue;
}
