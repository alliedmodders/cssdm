/**
 * dm_preset_spawns.sp
 * Adds preset spawning to CS:S DM.
 * This file is part of CS:S DM, Copyright (C) 2005-2007 AlliedModders LLC
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
#pragma dynamic 16384
#include <sourcemod>
#include <sdktools>
#include <cssdm>

public Plugin:myinfo = 
{
	name = "CS:S DM Preset Spawns",
	author = "AlliedModders LLC",
	description = "Adds preset spawning to CS:S DM",
	version = CSSDM_VERSION,
	url = "http://www.bailopan.net/cssdm/"
};

#define MAX_SPAWNS			256


new bool:g_AreWeSpawning = false;
new g_SpawnCount = 0;
new Float:g_SpawnOrigins[MAX_SPAWNS][3];
new Float:g_SpawnAngles[MAX_SPAWNS][3];
new Handle:g_hSpawnMenu = INVALID_HANDLE;
new g_LastLocation[MAXPLAYERS+1];

public OnPluginStart()
{
	RegAdminCmd("cssdm_spawn_menu", Command_SpawnMenu, ADMFLAG_CHANGEMAP, "Edits CS:S DM spawn points");
	
	g_hSpawnMenu = CreateMenu(Menu_EditSpawns);
	SetMenuTitle(g_hSpawnMenu, "Spawn Point Editor");
	AddMenuItem(g_hSpawnMenu, "nearest", "Teleport to nearest");
	AddMenuItem(g_hSpawnMenu, "previous", "Teleport to previous");
	AddMenuItem(g_hSpawnMenu, "next", "Teleport to next");
	AddMenuItem(g_hSpawnMenu, "add", "Add position");
	AddMenuItem(g_hSpawnMenu, "preinsert", "Insert position here");
	AddMenuItem(g_hSpawnMenu, "delete", "Delete nearest");
	AddMenuItem(g_hSpawnMenu, "clear", "Delete all");
}

public OnClientPutInServer(client)
{
	g_LastLocation[client] = -1;
}

/* :TODO: we need this in core */
Float:GetDistance(const Float:vec1[3], const Float:vec2[3])
{
	decl Float:x, Float:y, Float:z;
	
	x = vec1[0] - vec2[0];
	y = vec1[1] - vec2[1];
	z = vec1[2] - vec2[2];
	
	return SquareRoot(x*x + y*y + z*z);
}

GetNearestSpawn(client)
{
	if (!g_SpawnCount)
	{
		return -1;
	}
	
	new Float:clorigin[3];
	GetClientAbsOrigin(client, clorigin);
	
	new Float:low_diff = GetDistance(g_SpawnOrigins[0], clorigin);
	new low_index = 0;
	for (new i=1; i<g_SpawnCount; i++)
	{
		new Float:diff = GetDistance(g_SpawnOrigins[i], clorigin);
		if (diff < low_diff)
		{
			low_diff = diff;
			low_index = i;
		}
	}
	
	return low_index;
}

bool:LoadMapConfig()
{
	new String:map[64];
	GetCurrentMap(map, sizeof(map));
	
	new String:path[255];
	Format(path, sizeof(path), "cfg/cssdm/spawns/%s.txt", map);
	
	g_SpawnCount = 0;
	
	new Handle:file = OpenFile(path, "rt");
	if (file == INVALID_HANDLE)
	{
		LogError("Could not find spawn point file \"%s\"", path);
		LogError("Defaulting to map-based spawns!");
		return false;
	}
	
	new String:buffer[255];
	new String:parts[6][16];
	new partCount;
	while (!IsEndOfFile(file) && ReadFileLine(file, buffer, sizeof(buffer)))
	{
		TrimString(buffer);
		partCount = ExplodeString(buffer, " ", parts, 6, 16);
		if (partCount < 6)
		{
			continue;
		}
		g_SpawnOrigins[g_SpawnCount][0] = StringToFloat(parts[0]);
		g_SpawnOrigins[g_SpawnCount][1] = StringToFloat(parts[1]);
		g_SpawnOrigins[g_SpawnCount][2] = StringToFloat(parts[2]);
		g_SpawnAngles[g_SpawnCount][0] = StringToFloat(parts[3]);
		g_SpawnAngles[g_SpawnCount][1] = StringToFloat(parts[4]);
		g_SpawnAngles[g_SpawnCount][2] = StringToFloat(parts[5]);
		g_SpawnCount++;
	}
	
	CloseHandle(file);
	
	LogMessage("Preset spawn points loaded (number %d) (map %s)", g_SpawnCount, map);
	
	return true;
}

bool:WriteMapConfig()
{
	new String:map[64];
	GetCurrentMap(map, sizeof(map));
	
	new String:path[255];
	Format(path, sizeof(path), "cfg/cssdm/spawns/%s.txt", map);
	
	new Handle:file = OpenFile(path, "wt");
	if (file == INVALID_HANDLE)
	{
		LogError("Could not open spawn point file \"%s\" for writing.", path);
		return false;
	}
	
	for (new i=0; i<g_SpawnCount; i++)
	{
		WriteFileLine(file, "%f %f %f %f %f %f", 
			g_SpawnOrigins[i][0],
			g_SpawnOrigins[i][1],
			g_SpawnOrigins[i][2],
			g_SpawnAngles[i][0],
			g_SpawnAngles[i][1],
			g_SpawnAngles[i][2]);
	}
	
	CloseHandle(file);
	
	return true;
}

AddSpawnFromClient(client)
{
	if (g_SpawnCount >= MAX_SPAWNS)
	{
		return -1;
	}
	
	GetClientAbsOrigin(client, g_SpawnOrigins[g_SpawnCount]);
	GetClientAbsAngles(client, g_SpawnAngles[g_SpawnCount]);
	
	new old = g_SpawnCount++;
	
	return old;
}

InsertSpawnFromClient(client, bool:pre, index)
{
	if (index == g_SpawnCount - 1 && !pre)
	{
		return AddSpawnFromClient(client);
	}
	
	if (g_SpawnCount >= MAX_SPAWNS)
	{
		return -1;
	}
	
	/* If this is a post-insertion, unmark the index for moving */
	if (!pre)
	{
		index++;
	}
	
	/* Move all of the slots down */
	for (new i=g_SpawnCount-1; i>=index; i--)
	{
		g_SpawnOrigins[i+1] = g_SpawnOrigins[i];
		g_SpawnAngles[i+1] = g_SpawnAngles[i];
	}
	
	GetClientAbsOrigin(client, g_SpawnOrigins[index]);
	GetClientAbsAngles(client, g_SpawnAngles[index]);
	
	g_SpawnCount++;
	
	return index;
}

bool:DeleteSpawn(index)
{
	if (index < 0 || index >= g_SpawnCount)
	{
		return false;
	}
	
	for (new i=index; i<g_SpawnCount-1; i++)
	{
		g_SpawnAngles[i] = g_SpawnAngles[i+1];
		g_SpawnOrigins[i] = g_SpawnOrigins[i+1];
	}
	
	g_SpawnCount--;
	
	return true;
}

public Action:Command_SpawnMenu(client, args)
{
	if (client == 0)
	{
		ReplyToCommand(client, "[CSSDM] This command is not available from the server console.");
		return Plugin_Handled;
	}
	
	DisplayMenu(g_hSpawnMenu, client, MENU_TIME_FOREVER);
	
	return Plugin_Handled;
}

public Panel_VerifyDeleteSpawns(Handle:menu, MenuAction:action, param1, param2)
{
	if (action == MenuAction_Select)
	{
		if (param2 == 1)
		{
			g_SpawnCount = 0;
			if (!WriteMapConfig())
			{
				PrintToChat(param1, "[CSSDM] Could not write to spawn config file.");
			} else {
				PrintToChat(param1, "[CSSDM] All spawn points have been deleted.");
			}
		}
		DisplayMenu(g_hSpawnMenu, param1, MENU_TIME_FOREVER);
	}
}

public Menu_EditSpawns(Handle:menu, MenuAction:action, param1, param2)
{
	if (action == MenuAction_Select)
	{
		if (param2 == 0)
		{
			new index = GetNearestSpawn(param1);
			if (index == -1)
			{
				PrintToChat(param1, "[CSSDM] There are no spawn points.");
			} else {
				TeleportEntity(param1, g_SpawnOrigins[index], g_SpawnAngles[index], NULL_VECTOR);
				PrintToChat(param1, "[CSSDM] Teleported to spawn #%d (%d total).", index, g_SpawnCount);
				g_LastLocation[param1] = index;
			}
		} else if (param2 == 2) {
			if (g_SpawnCount == 0)
			{
				PrintToChat(param1, "[CSSDM] There are no spawn points.");
			} else {
				new index = g_LastLocation[param1] + 1;
				if (index >= g_SpawnCount)
				{
					index = 0;
				}
				TeleportEntity(param1, g_SpawnOrigins[index], g_SpawnAngles[index], NULL_VECTOR);
				PrintToChat(param1, "[CSSDM] Teleported to spawn #%d (%d total).", index, g_SpawnCount);
				g_LastLocation[param1] = index;
			}
		} else if (param2 == 1) {
			if (g_SpawnCount == 0)
			{
				PrintToChat(param1, "[CSSDM] There are no spawn points.");
			} else {
				new index = g_LastLocation[param1] - 1;
				if (index < 0)
				{
					index = g_SpawnCount - 1;
				}
				TeleportEntity(param1, g_SpawnOrigins[index], g_SpawnAngles[index], NULL_VECTOR);
				PrintToChat(param1, "[CSSDM] Teleported to spawn #%d (%d total).", index, g_SpawnCount);
				g_LastLocation[param1] = index;
			}
		} else if (param2 == 5) {
			new index = GetNearestSpawn(param1);
			if (index == -1)
			{
				PrintToChat(param1, "[CSSDM] There are no spawn points.");
			} else {
				if (!DeleteSpawn(index))
				{
					PrintToChat(param1, "[CSSDM] Could not delete spawn #%d.", index);
				} else {
					if (!WriteMapConfig())
					{
						PrintToChat(param1, "[CSSDM] Could not write to spawn config file!");
					} else {
						PrintToChat(param1, "[CSSDM] Deleted spawn #%d (%d total).", index, g_SpawnCount);
					}
				}
			}
		} else if (param2 == 3) {
			new index;
			if ((index = AddSpawnFromClient(param1)) == -1)
			{
				PrintToChat(param1, "[CSSDM] Could not add spawn (max limit reached).");
			} else {
				if (!WriteMapConfig())
				{
					PrintToChat(param1, "[CSSDM] Could not write to spawn config file!");
				} else {
					PrintToChat(param1, "[CSSDM] Added spawn #%d (%d total).", index, g_SpawnCount);
				}
			}
		} else if (param2 == 4) {
			new index = g_LastLocation[param1];
			new bool:pre = true;
			if (index == -1 || index >= g_SpawnCount)
			{
				index = g_SpawnCount - 1;
				pre = false;
			}
			if ((index = InsertSpawnFromClient(param1, pre, index)) == -1)
			{
				PrintToChat(param1, "[CSSDM] Could not add spawn (max limit reached).");
			} else {
				if (!WriteMapConfig())
				{
					PrintToChat(param1, "[CSSDM] Could not write to spawn config file!");
				} else {
					PrintToChat(param1, "[CSSDM] Inserted spawn at #%d (%d total).", index, g_SpawnCount);
				}
			}
		} else if (param2 == 6) {
			/* Of course, we ask the user first. */
			new Handle:panel = CreatePanel();
			SetPanelTitle(panel, "Delete all spawn points?");
			DrawPanelItem(panel, "Yes");
			DrawPanelItem(panel, "No");
			SendPanelToClient(panel, param1, Panel_VerifyDeleteSpawns, MENU_TIME_FOREVER);
			CloseHandle(panel);
			return;
		}
		/* Redraw the menu */
		DisplayMenu(g_hSpawnMenu, param1, MENU_TIME_FOREVER);	
	}
}

public DM_OnStartup()
{
	new String:method[32];
	DM_GetSpawnMethod(method, sizeof(method));
	
	if (strcmp(method, "preset") != 0)
	{
		return;
	}
	
	g_AreWeSpawning = LoadMapConfig();
}

public Action:DM_OnSetSpawnMethod(const String:method[])
{
	g_AreWeSpawning = (strcmp(method, "preset") == 0);
	
	if (!g_AreWeSpawning)
	{
		return Plugin_Continue;
	}
	
	if (!LoadMapConfig())
	{
		g_AreWeSpawning = false;
		return Plugin_Continue;
	}
	
	return Plugin_Stop;
}

public DM_OnClientSpawned(client)
{
	if (!g_AreWeSpawning || !g_SpawnCount)
	{
		return;
	}
	
	new maxClients = GetMaxClients();
	new startPoint = GetRandomInt(0, g_SpawnCount-1);
	
	/* Prefetch player origins */
	decl Float:origins[65][3];
	new numToCheck = 0;
	
	for (new i=1; i<=maxClients; i++)
	{
		if (i == client || !IsClientInGame(i))
		{
			continue;
		}
		GetClientAbsOrigin(i, origins[numToCheck]);
		numToCheck++;
	}
	
	/* Cycle through until we get a spawn point */
	new bool:use_this_point;
	new checked = 0;
	while (checked < g_SpawnCount)
	{
		if (startPoint >= g_SpawnCount)
		{
			startPoint = 0;
		}
		
		use_this_point = true;
		for (new i=0; i<numToCheck; i++)
		{
			if (GetDistance(g_SpawnOrigins[startPoint], origins[i]) < 600.0)
			{
				use_this_point = false;
				break;
			}
		}
		
		if (use_this_point)
		{
			break;
		}
		
		checked++;
		startPoint++;
	}
	
	if (startPoint >= g_SpawnCount)
	{
		startPoint = 0;
	}
		
	TeleportEntity(client, g_SpawnOrigins[startPoint], g_SpawnAngles[startPoint], NULL_VECTOR);
}
