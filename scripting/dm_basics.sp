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
#include <cssdm>

/* Plugin stuff */
new Handle:cssdm_respawn_command;
new Handle:cssdm_force_mapchanges;
new Handle:cssdm_mapchange_file;
new Handle:cssdm_refill_ammo;
new Handle:mp_timelimit;
new Handle:g_ChangeMapTimer = INVALID_HANDLE;
new bool:g_AmmoHooks = false;
new g_ActiveWepOffs = -1;

/* Player stuff */
new Float:g_DeathTimes[MAXPLAYERS+1];

/** PUBLIC INFO */
public Plugin:myinfo = 
{
	name = "CS:S DM Basics",
	author = "AlliedModders LLC",
	description = "Basic CS:S DM Commands/Features",
	version = CSSDM_VERSION,
	url = "http://www.bailopan.net/cssdm/"
};

public OnPluginStart()
{
	LoadTranslations("cssdm.phrases");
	
	RegConsoleCmd("say", Command_Say);
	RegConsoleCmd("say_team", Command_Say);
	
	cssdm_respawn_command = CreateConVar("cssdm_respawn_command", "1", "Sets whether clients can manually respawn");
	cssdm_force_mapchanges = CreateConVar("cssdm_force_mapchanges", "0", "Sets whether CS:S DM should force mapchanges");
	cssdm_mapchange_file = CreateConVar("cssdm_mapchange_file", "mapcycle.txt", "Sets the mapchange file for CS:S DM");
	cssdm_refill_ammo = CreateConVar("cssdm_refill_ammo", "1", "Sets whether CS:S DM automatically refills ammo");
	mp_timelimit = FindConVar("mp_timelimit");
	
	HookConVarChange(cssdm_force_mapchanges, CvarChange_RestartMapTimer);
	HookConVarChange(mp_timelimit, CvarChange_RestartMapTimer);	
	HookConVarChange(cssdm_refill_ammo, CvarChange_RefillAmmo);
	
	g_ActiveWepOffs = FindSendPropOffs("CCSPlayer", "m_hActiveWeapon");
}

public DM_OnStartup()
{
	RestartMapTimer();
	
	g_AmmoHooks = GetConVarInt(cssdm_refill_ammo) ? true : false;
	if (g_AmmoHooks && g_ActiveWepOffs > 0)
	{
		HookEvent("weapon_reload", Event_CheckDepleted);
		HookEvent("weapon_fire_on_empty", Event_CheckDepleted);
	}
}

public DM_OnShutdown()
{
	if (g_ChangeMapTimer != INVALID_HANDLE)
	{
		CloseHandle(g_ChangeMapTimer);
		g_ChangeMapTimer = INVALID_HANDLE;
	}
	ShutdownAmmoHooks();
}

ShutdownAmmoHooks()
{
	if (g_AmmoHooks)
	{
		UnhookEvent("weapon_reload", Event_CheckDepleted);
		UnhookEvent("weapon_fire_on_empty", Event_CheckDepleted);
		g_AmmoHooks = false;
	}
}

public CvarChange_RefillAmmo(Handle:cvar, const String:oldvalue[], const String:newvalue[])
{
	if (GetConVarInt(cvar) && !g_AmmoHooks && g_ActiveWepOffs > 0)
	{
		HookEvent("weapon_reload", Event_CheckDepleted);
		HookEvent("weapon_fire_on_empty", Event_CheckDepleted);
	} else if (!GetConVarInt(cvar) && g_AmmoHooks) {
		UnhookEvent("weapon_reload", Event_CheckDepleted);
		UnhookEvent("weapon_fire_on_empty", Event_CheckDepleted);
	}
}

public Event_CheckDepleted(Handle:event, const String:name[], bool:dontBroadcast)
{
	new client = GetClientOfUserId(GetEventInt(event, "userid"));
	if (!client || !IsClientInGame(client))
	{
		return;
	}
	
	new entity = GetEntDataEnt(client, g_ActiveWepOffs);
	if (entity < 1)
	{
		return;
	}
	
	new ammoType = GetEntProp(entity, Prop_Send, "m_iPrimaryAmmoType");
	
	/* Give something reasonable -- the game will chop it off */
	DM_GiveClientAmmo(client, ammoType, 30, false);
}

public CvarChange_RestartMapTimer(Handle:cvar, const String:oldvalue[], const String:newvalue[])
{
	RestartMapTimer();
}

RestartMapTimer()
{
	if (g_ChangeMapTimer != INVALID_HANDLE)
	{
		CloseHandle(g_ChangeMapTimer);
		g_ChangeMapTimer = INVALID_HANDLE;
	}
	
	if (GetConVarInt(cssdm_force_mapchanges))
	{
		/* Find how much time is left in the map */
		new Float:seconds = (GetConVarInt(mp_timelimit) * 60.0) - GetGameTime();
		g_ChangeMapTimer = CreateTimer(seconds, Timer_ChangeMap);
	}
}

public Action:Timer_ChangeMap(Handle:timer)
{
	g_ChangeMapTimer = INVALID_HANDLE;
	
	decl String:changefile[64];
	GetConVarString(cssdm_mapchange_file, changefile, sizeof(changefile));
	
	new Handle:file = OpenFile(changefile, "rt");
	if (!file)
	{
		LogError("[CSSDM] Could not open mapchange file \"%s\"", changefile);
		return Plugin_Stop;
	}
	
	decl String:curmap[64];
	GetCurrentMap(curmap, sizeof(curmap));
	
	new String:firstmap[64];
	new String:lastmap[64];
	decl String:buffer[64];
	new bool:matched = false;
	while (!IsEndOfFile(file) && ReadFileLine(file, buffer, sizeof(buffer)))
	{
		TrimString(buffer);
		if (buffer[0] == '\0' || (!IsCharAlpha(buffer[0]) && !IsCharNumeric(buffer[0])))
		{
			continue;
		}
		if (!IsMapValid(buffer))
		{
			continue;
		}
		if (firstmap[0] == '\0')
		{
			strcopy(firstmap, sizeof(firstmap), buffer);
		}
		strcopy(lastmap, sizeof(lastmap), buffer);
		if (strcmp(buffer, curmap) == 0)
		{
			matched = true;
		} else if (matched) {
			break;
		}
	}
	CloseHandle(file);
	
	if (!matched || (strcmp(buffer, curmap) == 0))
	{
		strcopy(lastmap, sizeof(lastmap), firstmap);
	}
	
	if (lastmap[0] != '\0')
	{
		ServerCommand("changelevel %s", lastmap);
	}
	
	return Plugin_Stop;
}

public Action:Timer_Welcome(Handle:timer, any:userid)
{
	new client = GetClientOfUserId(userid);
	
	if (!client || !IsClientInGame(client))
	{
		return Plugin_Stop;
	}
	
	PrintToChat(client, "[CSSDM] Counter-Strike Source: Deathmatch (version %s)", CSSDM_VERSION);
	PrintToChat(client, "[CSSDM] Visit http://www.bailopan.net/cssdm to download.");
	
	return Plugin_Stop;
}

public OnClientPutInServer(client)
{
	g_DeathTimes[client] = 0.0;
	CreateTimer(10.0, Timer_Welcome, GetClientUserId(client));
}

public Action:DM_OnClientDeath(client)
{
	g_DeathTimes[client] = GetGameTime();
	return Plugin_Continue;
}

public Action:Command_Say(client, args)
{
	if (!DM_IsRunning() || !GetConVarInt(cssdm_respawn_command))
	{
		return Plugin_Continue;
	}
	
	decl String:command[32];
	GetCmdArg(1, command, sizeof(command));
	
	if (strcmp(command, "respawn") == 0)
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
		
		new team = GetClientTeam(client);
		if (team != CSSDM_TEAM_T && team != CSSDM_TEAM_CT)
		{
			PrintToChat(client, "[CSSDM] %t", "NoRespawn Team");
			return Plugin_Handled;
		}
		
		new Float:elapsed = GetGameTime() - g_DeathTimes[client];
		new Float:wait_time = DM_GetSpawnWaitTime();
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

