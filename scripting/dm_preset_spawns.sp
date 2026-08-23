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

public Plugin myinfo =
{
	name = "CS:S DM Preset Spawns",
	author = "AlliedModders LLC",
	description = "Adds preset spawning to CS:S DM",
	version = CSSDM_VERSION,
	url = "http://www.bailopan.net/cssdm/"
};

#define MAX_SPAWNS			256


bool g_AreWeSpawning = false;
int g_SpawnCount = 0;
float g_SpawnOrigins[MAX_SPAWNS][3];
float g_SpawnAngles[MAX_SPAWNS][3];
Menu g_hSpawnMenu;
int g_LastLocation[MAXPLAYERS+1];

public void OnPluginStart()
{
	RegAdminCmd("cssdm_spawn_menu", Command_SpawnMenu, ADMFLAG_CHANGEMAP, "Edits CS:S DM spawn points");

	g_hSpawnMenu = new Menu(Menu_EditSpawns);
	g_hSpawnMenu.SetTitle("Spawn Point Editor");
	g_hSpawnMenu.AddItem("nearest", "Teleport to nearest");
	g_hSpawnMenu.AddItem("previous", "Teleport to previous");
	g_hSpawnMenu.AddItem("next", "Teleport to next");
	g_hSpawnMenu.AddItem("add", "Add position");
	g_hSpawnMenu.AddItem("preinsert", "Insert position here");
	g_hSpawnMenu.AddItem("delete", "Delete nearest");
	g_hSpawnMenu.AddItem("clear", "Delete all");
}

public void OnClientPutInServer(int client)
{
	g_LastLocation[client] = -1;
}

/* :TODO: we need this in core */
float GetDistance(const float vec1[3], const float vec2[3])
{
	float x, y, z;

	x = vec1[0] - vec2[0];
	y = vec1[1] - vec2[1];
	z = vec1[2] - vec2[2];

	return SquareRoot(x*x + y*y + z*z);
}

int GetNearestSpawn(int client)
{
	if (!g_SpawnCount)
	{
		return -1;
	}

	float clorigin[3];
	GetClientAbsOrigin(client, clorigin);

	float low_diff = GetDistance(g_SpawnOrigins[0], clorigin);
	int low_index = 0;
	for (int i=1; i<g_SpawnCount; i++)
	{
		float diff = GetDistance(g_SpawnOrigins[i], clorigin);
		if (diff < low_diff)
		{
			low_diff = diff;
			low_index = i;
		}
	}

	return low_index;
}

bool LoadMapConfig()
{
	char map[64];
	GetCurrentMap(map, sizeof(map));

	char mapDisplay[64];
	GetMapDisplayName(map, mapDisplay, sizeof(mapDisplay));

	char game[64];
	GetGameFolderName(game, sizeof(game));

	char path[PLATFORM_MAX_PATH];
	Format(path, sizeof(path), "cfg/cssdm/spawns/%s/%s.txt", game, mapDisplay);

	g_SpawnCount = 0;

	File file = OpenFile(path, "rt");
	if (file == null)
	{
		LogError("Could not find spawn point file \"%s\"", path);
		LogError("Defaulting to map-based spawns!");
		return false;
	}

	char buffer[255];
	char parts[6][16];
	int partCount;
	while (!file.EndOfFile() && file.ReadLine(buffer, sizeof(buffer)))
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

	delete file;

	LogMessage("Preset spawn points loaded (number %d) (map %s)", g_SpawnCount, map);

	return true;
}

bool WriteMapConfig()
{
	char map[64];
	GetCurrentMap(map, sizeof(map));

	char mapDisplay[64];
	GetMapDisplayName(map, mapDisplay, sizeof(mapDisplay));

	char game[64];
	GetGameFolderName(game, sizeof(game));

	char path[PLATFORM_MAX_PATH];
	Format(path, sizeof(path), "cfg/cssdm/spawns/%s/%s.txt", game, mapDisplay);

	File file = OpenFile(path, "wt");
	if (file == null)
	{
		LogError("Could not open spawn point file \"%s\" for writing.", path);
		return false;
	}

	for (int i=0; i<g_SpawnCount; i++)
	{
		file.WriteLine("%f %f %f %f %f %f",
			g_SpawnOrigins[i][0],
			g_SpawnOrigins[i][1],
			g_SpawnOrigins[i][2],
			g_SpawnAngles[i][0],
			g_SpawnAngles[i][1],
			g_SpawnAngles[i][2]);
	}

	delete file;

	return true;
}

int AddSpawnFromClient(int client)
{
	if (g_SpawnCount >= MAX_SPAWNS)
	{
		return -1;
	}

	GetClientAbsOrigin(client, g_SpawnOrigins[g_SpawnCount]);
	GetClientAbsAngles(client, g_SpawnAngles[g_SpawnCount]);

	int old = g_SpawnCount++;

	return old;
}

int InsertSpawnFromClient(int client, bool pre, int index)
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
	for (int i=g_SpawnCount-1; i>=index; i--)
	{
		g_SpawnOrigins[i+1] = g_SpawnOrigins[i];
		g_SpawnAngles[i+1] = g_SpawnAngles[i];
	}

	GetClientAbsOrigin(client, g_SpawnOrigins[index]);
	GetClientAbsAngles(client, g_SpawnAngles[index]);

	g_SpawnCount++;

	return index;
}

bool DeleteSpawn(int index)
{
	if (index < 0 || index >= g_SpawnCount)
	{
		return false;
	}

	for (int i=index; i<g_SpawnCount-1; i++)
	{
		g_SpawnAngles[i] = g_SpawnAngles[i+1];
		g_SpawnOrigins[i] = g_SpawnOrigins[i+1];
	}

	g_SpawnCount--;

	return true;
}

public Action Command_SpawnMenu(int client, int args)
{
	if (client == 0)
	{
		ReplyToCommand(client, "[CSSDM] This command is not available from the server console.");
		return Plugin_Handled;
	}

	g_hSpawnMenu.Display(client, MENU_TIME_FOREVER);

	return Plugin_Handled;
}

public void Panel_VerifyDeleteSpawns(Menu menu, MenuAction action, int param1, int param2)
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
		g_hSpawnMenu.Display(param1, MENU_TIME_FOREVER);
	}
}

public void Menu_EditSpawns(Menu menu, MenuAction action, int param1, int param2)
{
	if (action == MenuAction_Select)
	{
		if (param2 == 0)
		{
			int index = GetNearestSpawn(param1);
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
				int index = g_LastLocation[param1] + 1;
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
				int index = g_LastLocation[param1] - 1;
				if (index < 0)
				{
					index = g_SpawnCount - 1;
				}
				TeleportEntity(param1, g_SpawnOrigins[index], g_SpawnAngles[index], NULL_VECTOR);
				PrintToChat(param1, "[CSSDM] Teleported to spawn #%d (%d total).", index, g_SpawnCount);
				g_LastLocation[param1] = index;
			}
		} else if (param2 == 5) {
			int index = GetNearestSpawn(param1);
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
			int index;
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
			int index = g_LastLocation[param1];
			bool pre = true;
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
			Panel panel = new Panel();
			panel.SetTitle("Delete all spawn points?");
			panel.DrawItem("Yes");
			panel.DrawItem("No");
			panel.Send(param1, Panel_VerifyDeleteSpawns, MENU_TIME_FOREVER);
			delete panel;
			return;
		}
		/* Redraw the menu */
		g_hSpawnMenu.Display(param1, MENU_TIME_FOREVER);
	}
}

public void DM_OnStartup()
{
	char method[32];
	DM_GetSpawnMethod(method, sizeof(method));

	if (!StrEqual(method, "preset"))
	{
		return;
	}

	g_AreWeSpawning = LoadMapConfig();
}

public Action DM_OnSetSpawnMethod(const char[] method)
{
	g_AreWeSpawning = StrEqual(method, "preset");

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

public void DM_OnClientSpawned(int client)
{
	if (!g_AreWeSpawning || !g_SpawnCount)
	{
		return;
	}

	int startPoint = GetRandomInt(0, g_SpawnCount-1);

	/* Prefetch player origins */
	float origins[65][3];
	int numToCheck = 0;

	for (int i=1; i<=MaxClients; i++)
	{
		if (i == client || !IsClientInGame(i))
		{
			continue;
		}
		GetClientAbsOrigin(i, origins[numToCheck]);
		numToCheck++;
	}

	/* Cycle through until we get a spawn point */
	bool use_this_point;
	int checked = 0;
	while (checked < g_SpawnCount)
	{
		if (startPoint >= g_SpawnCount)
		{
			startPoint = 0;
		}

		use_this_point = true;
		for (int i=0; i<numToCheck; i++)
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
