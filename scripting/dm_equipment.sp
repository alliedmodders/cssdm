/**
 * dm_equipment.sp
 * Adds preset spawning to CS:S DM.
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

#define CSSDM_GUNMENU_NO		0
#define CSSDM_GUNMENU_YES		1
#define CSSDM_GUNMENU_RANDOM	2

/** Plugin Stuff */
new Handle:cssdm_enable_equipment;					/** cssdm_enable_equipment cvar */
new g_ArmorOffset = -1;								/* m_ArmorValue offset */
new g_NVOffset = -1;								/* m_bHasNightVision offset */
new g_HealthOffset = -1;							/* m_iHealth offset */
new Handle:g_SpawnTimers[MAXPLAYERS+1];				/* Post-spawn timers */
new Handle:g_hPrimaryMenu = INVALID_HANDLE;			/* Priamry menu Handle */
new Handle:g_hSecondaryMenu = INVALID_HANDLE;		/* Secondary menu Handle */
new Handle:g_hEquipMenu = INVALID_HANDLE;			/* Main equipment menu */
new bool:g_IsEnabled = false;						/* Whether the plugin should work */
new g_PrimaryChoices[MAXPLAYERS+1];					/* Primary weapon selections */
new g_SecondaryChoices[MAXPLAYERS+1];				/* Secondary weapon selections */
new bool:g_GunMenuEnabled[MAXPLAYERS+1];			/* Whether the gun menu is enabled */
new bool:g_GunMenuAvailable[MAXPLAYERS+1];			/* Whether the gun menu is available */

/** HUMAN CONFIGS */
new g_PrimaryList[CSSDM_MAX_WEAPONS];
new g_SecondaryList[CSSDM_MAX_WEAPONS];
new g_PrimaryCount = 0;
new g_SecondaryCount = 0;
new g_PrimaryMenu = CSSDM_GUNMENU_YES;
new g_SecondaryMenu = CSSDM_GUNMENU_YES;
new bool:g_AllowBuy = false;
new g_ArmorAmount = 100;
new bool:g_Helmets = true;
new g_Flashes = 0;
new bool:g_Smokes = false;
new bool:g_HEs = false;
new bool:g_NightVision = false;
new bool:g_DefuseKits = true;
new bool:g_AllowGunCommand = true;
new g_HealthAmount = 100;

/** BOT CONFIGS */
new g_BotPrimaryList[CSSDM_MAX_WEAPONS];
new g_BotSecondaryList[CSSDM_MAX_WEAPONS];
new g_BotPrimaryCount = 0;
new g_BotSecondaryCount = 0;
new g_BotArmor = 100;
new bool:g_BotHelmets = false;
new g_BotFlashes = 0;
new bool:g_BotSmokes = false;
new bool:g_BotHEs = false;
new bool:g_BotDefuseKits = true;
new g_BotHealthAmount = 100;

/** PUBLIC INFO */
public Plugin:myinfo = 
{
	name = "CS:S DM Equipment",
	author = "AlliedModders LLC",
	description = "Adds gun menu/equipment to CS:S DM",
	version = CSSDM_VERSION,
	url = "http://www.bailopan.net/cssdm/"
};

/******************
 * IMPLEMENTATION *
 ******************/
 
public OnPluginStart()
{
	LoadTranslations("cssdm.phrases");
	
	RegConsoleCmd("say", Command_Say);
	RegConsoleCmd("say_team", Command_Say);
	
	cssdm_enable_equipment = CreateConVar("cssdm_enable_equipment", "1", "Sets whether the equipment plugin is enabled");
	HookConVarChange(cssdm_enable_equipment, OnEquipmentEnableChange);
	
	g_ArmorOffset = FindSendPropOffs("CCSPlayer", "m_ArmorValue");
	g_NVOffset = FindSendPropOffs("CCSPlayer", "m_bHasNightVision");
	g_HealthOffset = FindSendPropOffs("CCSPlayer", "m_iHealth");
	
	g_hEquipMenu = CreateMenu(Menu_EquipHandler, MenuAction_DrawItem|MenuAction_DisplayItem);
	SetMenuTitle(g_hEquipMenu, "Weapon Options:");
	SetMenuExitButton(g_hEquipMenu, false);
	AddMenuItem(g_hEquipMenu, "", "New weapons");
	AddMenuItem(g_hEquipMenu, "", "Same weapons");
	AddMenuItem(g_hEquipMenu, "", "Same weapons every time");
	AddMenuItem(g_hEquipMenu, "", "Random weapons");
	AddMenuItem(g_hEquipMenu, "", "Random weapons every time");
}

public OnEquipmentEnableChange(Handle:convar, const String:oldValue[], const String:newValue[])
{
	g_IsEnabled = StringToInt(newValue) ? true : false;
}

public OnConfigsExecuted()
{
	LoadDefaults();
	
	if ((g_IsEnabled = LoadConfigFile("cfg/cssdm/cssdm.equip.txt")) == false)
	{
		LogError("[CSSDM] Could not find equipment file \"%s\"", "cfg/cssdm/cssdm.equip.txt");
		return;
	}
	
	if (!GetConVarInt(cssdm_enable_equipment))
	{
		g_IsEnabled = false;
		return;
	}
	
	/** See if there is a map version */
	decl String:map[64];
	decl String:path[255];
	GetCurrentMap(map, sizeof(map));
	Format(path, sizeof(path), "cfg/cssdm/maps/%s.equip.txt", map);
	
	if (FileExists(path))
	{
		LoadConfigFile(path);
	}
}

public Action:DM_OnClientDeath(client)
{
	if (g_SpawnTimers[client] != INVALID_HANDLE)
	{
		KillTimer(g_SpawnTimers[client]);
		g_SpawnTimers[client] = INVALID_HANDLE;
	}
	g_GunMenuAvailable[client] = true;
}

public OnClientPutInServer(client)
{
	g_GunMenuEnabled[client] = true;
	g_GunMenuAvailable[client] = true;
	g_PrimaryChoices[client] = -1;
	g_SecondaryChoices[client] = -1;
}

public OnClientDisconnect(client)
{
	if (g_SpawnTimers[client] != INVALID_HANDLE)
	{
		KillTimer(g_SpawnTimers[client]);
		g_SpawnTimers[client] = INVALID_HANDLE;
	}
}

public DM_OnClientSpawned(client)
{
	if (!ShouldRun())
	{
		return;
	}
	
	if (!IsFakeClient(client))
	{
		if (g_ArmorAmount > 0)
		{
			GivePlayerItem(client, "item_assaultsuit");
			GivePlayerItem(client, "item_assaultsuit");
			SetClientArmor(client, g_ArmorAmount);
		}
		if (g_HealthAmount > 0)
		{
			SetClientHealth(client, g_HealthAmount);
		}
		if (g_Helmets)
		{
			GivePlayerItem(client, "item_kevlar");
			GivePlayerItem(client, "item_kevlar");
		}
		GiveGrenades(client, g_Flashes, g_HEs, g_Smokes);
		if (g_NightVision)
		{
			GiveNightVision(client);
		}
	}
	else
	{
		/* See if we'll need to strip this bot */
		if (g_BotSecondaryCount || g_BotPrimaryCount)
		{
			DM_StripBotItems(client);
		}
		if (g_BotArmor > 0)
		{
			GivePlayerItem(client, "item_assaultsuit");
			GivePlayerItem(client, "item_assaultsuit");
			SetClientArmor(client, g_BotArmor);
		} else {
			/* Make sure bot didn't buy armor */
			SetClientArmor(client, 0);
		}
		if (g_BotHealthAmount > 0)
		{
			SetClientHealth(client, g_BotHealthAmount);
		}
		if (g_BotHelmets)
		{
			GivePlayerItem(client, "item_kevlar");
			GivePlayerItem(client, "item_kevlar");
		}
		GiveGrenades(client, g_BotFlashes, g_BotHEs, g_BotSmokes);
	}
	if (!g_AllowBuy && g_SpawnTimers[client] == INVALID_HANDLE)
	{
		/* Small delay - we want to avoid ResetHUD */
		g_SpawnTimers[client] = CreateTimer(0.1, PlayerPostSpawn, client);
	}
}

public Action:PlayerPostSpawn(Handle:timer, any:client)
{
	g_SpawnTimers[client] = INVALID_HANDLE;
	
	if (!ShouldRun())
	{
		return Plugin_Stop;
	}
	
	if (IsFakeClient(client))
	{
		if (g_BotSecondaryCount > 0)
		{
			new index;
			if (g_BotSecondaryCount == 1)
			{
				index = g_BotSecondaryList[0];
			} else {
				index = GetRandomInt(0, g_BotSecondaryCount-1);
				index = g_BotSecondaryList[index];
			}
			decl String:classname[64];
			DM_GetWeaponClassname(index, classname, sizeof(classname));
			GivePlayerItem(client, classname);
		}
		if (g_BotPrimaryCount > 0)
		{
			new index;
			if (g_BotPrimaryCount == 1)
			{
				index = g_BotPrimaryList[0];
			} else {
				index = GetRandomInt(0, g_BotPrimaryCount-1);
				index = g_BotPrimaryList[index];
			}
			decl String:classname[64];
			DM_GetWeaponClassname(index, classname, sizeof(classname));
			GivePlayerItem(client, classname);
		}
		if (g_BotDefuseKits && GetClientTeam(client) == CSSDM_TEAM_CT)
		{
			GivePlayerItem(client, "item_defuser");
		}
	} else {
		if (g_DefuseKits && GetClientTeam(client) == CSSDM_TEAM_CT)
		{
			GivePlayerItem(client, "item_defuser");
		}
		
		new numGiven = 0;
		/* First, check if we should only be giving one of something automatically */
		if ((g_SecondaryMenu == CSSDM_GUNMENU_RANDOM || g_SecondaryMenu == CSSDM_GUNMENU_YES)
			&& g_SecondaryCount == 1)
		{
			GiveSecondary(client, 0);
			numGiven++;
		} else if (g_PrimaryMenu == CSSDM_GUNMENU_RANDOM && g_PrimaryCount > 1) {
			GivePrimary(client, g_PrimaryCount);
			numGiven++;
		}
		
		if ((g_PrimaryMenu == CSSDM_GUNMENU_RANDOM || g_PrimaryMenu == CSSDM_GUNMENU_YES)
			&& g_PrimaryCount == 1)
		{
			GivePrimary(client, 0);
			numGiven++;
		} else if (g_SecondaryMenu == CSSDM_GUNMENU_RANDOM && g_SecondaryCount > 1) {
			GiveSecondary(client, g_SecondaryCount);
			numGiven++;
		}
		
		/* If we've already given two weapons, there is no need for a menu. */
		if (numGiven == 2)
		{
			return Plugin_Stop;
		}
		
		if (g_GunMenuEnabled[client])
		{
			DisplayMenu(g_hEquipMenu, client, MENU_TIME_FOREVER);
		} else {
			GiveBothFromChoices(client);
		}
	}
	
	return Plugin_Stop;
}

public Action:Command_Say(client, args)
{
	if (!ShouldRun())
	{
		return Plugin_Continue;
	}
	
	new String:text[192];
	GetCmdArg(1, text, sizeof(text));
	
	if (strcmp(text, "guns") == 0)
	{
		if (!g_AllowGunCommand)
		{
			PrintToChat(client, "[CSSDM] %t", "GunsMenuDisabled");
			return Plugin_Handled;
		}
		
		if (!ChooseFromSecondary() && !ChooseFromPrimary())
		{
			PrintToChat(client, "[CSSDM] %t", "GunsMenuNotAvailable");
			return Plugin_Handled;
		}
		
		if (g_GunMenuEnabled[client])
		{
			PrintToChat(client, "[CSSDM] %t", "GunsMenuAlreadyEnabled");
			return Plugin_Handled;
		}
		
		g_GunMenuEnabled[client] = true;
		if (!g_GunMenuAvailable[client])
		{
			PrintToChat(client, "[CSSDM] %t", "GunsMenuReactivated");
		} else {
			DisplayMenu(g_hEquipMenu, client, MENU_TIME_FOREVER);
		}
		
		return Plugin_Handled;
	}
	
	return Plugin_Continue;
}

public Action:OnClientCommand(client, args)
{
	if (!ShouldRun() || g_AllowBuy)
	{
		return Plugin_Continue;
	}
	
	decl String:cmd[32];
	GetCmdArg(0, cmd, sizeof(cmd));
	
	if (StrEqual(cmd, "buy")
		|| StrEqual(cmd, "autobuy")
		|| StrEqual(cmd, "rebuy")
		|| StrEqual(cmd, "buyequip")
		|| StrEqual(cmd, "buymenu"))
	{
		return Plugin_Handled;
	}
	
	return Plugin_Continue;
}

public Menu_EquipHandler(Handle:menu, MenuAction:action, param1, param2)
{
	if (!ShouldRun())
	{
		return 0;
	}
	
	if (action == MenuAction_DrawItem)
	{
		if (param2 == 1 || param2 == 2)
		{
			if (g_PrimaryChoices[param1] == -1 && g_SecondaryChoices[param1] == -1)
			{
				return ITEMDRAW_DISABLED;
			}
		}
		return ITEMDRAW_DEFAULT;
	}
	else if (action == MenuAction_Select)
	{
		if (param2 == 0)
		{
			if (ChooseFromSecondary())
			{
				DisplayMenu(g_hSecondaryMenu, param1, MENU_TIME_FOREVER);
			} else if (ChooseFromPrimary()) {
				DisplayMenu(g_hPrimaryMenu, param1, MENU_TIME_FOREVER);
			}
		} else if (param2 == 1) {
			GiveBothFromChoices(param1);
		} else if (param2 == 2) {
			GiveBothFromChoices(param1);
			g_GunMenuEnabled[param1] = false;
			PrintToChat(param1, "[CSSDM] %t", "SayGunsNotify");
		} else if (param2 == 3) {
			GivePrimary(param1, g_PrimaryCount);
			GiveSecondary(param1, g_SecondaryCount);
		} else if (param2 == 4) {
			GivePrimary(param1, g_PrimaryCount);
			GiveSecondary(param1, g_SecondaryCount);
			GiveBothFromChoices(param1);
			g_GunMenuEnabled[param1] = false;
			PrintToChat(param1, "[CSSDM] %t", "SayGunsNotify");
		}
		g_GunMenuAvailable[param1] = false;
	}
	else if (action == MenuAction_DisplayItem)
	{
		decl style;
		decl String:info[12], String:lang_phrase[32];
		
		if (!GetMenuItem(menu, param2, info, sizeof(info), style, lang_phrase, sizeof(lang_phrase)))
		{
			return 0;
		}
		
		decl String:t_phrase[64];
		Format(t_phrase, sizeof(t_phrase), "%T", lang_phrase, param1);
		
		return RedrawMenuItem(t_phrase);
	}
	
	return 0;
}

public Menu_PrimaryHandler(Handle:menu, MenuAction:action, param1, param2)
{
	if (!ShouldRun())
	{
		return 0;
	}
	
	if (action == MenuAction_DrawItem)
	{
		return ITEMDRAW_DEFAULT;
	}
	else if (action == MenuAction_Select)
	{
		GivePrimary(param1, param2);
	}
	else if (action == MenuAction_Display)
	{
		new Handle:hPanel = Handle:param2;
		decl String:title[128];
		
		Format(title, sizeof(title), "%T:", "Primary weapon", param1);
		
		SetPanelTitle(hPanel, title);
	}
	
	return 0;
}

public Menu_SecondaryHandler(Handle:menu, MenuAction:action, param1, param2)
{
	if (!ShouldRun())
	{
		return 0;
	}
	
	if (action == MenuAction_DrawItem)
	{
		return ITEMDRAW_DEFAULT;
	}
	else if (action == MenuAction_Select)
	{
		GiveSecondary(param1, param2);
		if (ChooseFromPrimary())
		{
			DisplayMenu(g_hPrimaryMenu, param1, MENU_TIME_FOREVER);
		}
	}
	else if (action == MenuAction_Cancel
			 && param2 == MenuCancel_Exit)
	{
		if (ChooseFromPrimary())
		{
			DisplayMenu(g_hPrimaryMenu, param1, MENU_TIME_FOREVER);
		}
	}
	else if (action == MenuAction_Display)
	{
		new Handle:hPanel = Handle:param2;
		decl String:title[128];
		
		Format(title, sizeof(title), "%T:", "Secondary weapon", param1);
		
		SetPanelTitle(hPanel, title);
	}
	
	return 0;
}

bool:ChooseFromPrimary()
{
	return (g_PrimaryMenu == CSSDM_GUNMENU_YES
			&& g_PrimaryCount > 1);
}

bool:ChooseFromSecondary()
{
	return (g_SecondaryMenu == CSSDM_GUNMENU_YES
			&& g_SecondaryCount > 1);
}

GiveWeapon(client, index)
{
	new DmWeaponType:type = DM_GetWeaponType(index);
	
	new entity = DM_GetClientWeapon(client, type);
	if (entity != -1)
	{
		DM_DropWeapon(client, entity);
	}
	
	new String:cls[64];
	DM_GetWeaponClassname(index, cls, sizeof(cls));
	
	return GivePlayerItem(client, cls);
}

GiveBothFromChoices(client)
{
	if (ChooseFromPrimary())
	{
		GivePrimary(client, g_PrimaryChoices[client]);
	}
	
	if (ChooseFromSecondary())
	{
		GiveSecondary(client, g_SecondaryChoices[client]);
	}
}

/**
 * Gives a primary weapon to a client.
 * If the list_index is > the count, we choose one randomly.
 * This is a hack so we can append "Random" to the choice list.
 */
GivePrimary(client, list_index)
{
	if (list_index < 0)
	{
		return;
	}
	
	new weapon_index;
	if (list_index >= g_PrimaryCount)
	{
		if (g_PrimaryCount > 1)
		{
			weapon_index = GetRandomInt(0, g_PrimaryCount-1);
			weapon_index = g_PrimaryList[weapon_index];
		} else if (g_PrimaryCount == 1) {
			weapon_index = g_PrimaryList[0];
		} else {
			return;
		}
	} else {
		weapon_index = g_PrimaryList[list_index];
	}
	
	g_PrimaryChoices[client] = list_index;
	
	GiveWeapon(client, weapon_index);
}

/**
 * Gives a secondary weapon to a client.
 * If the list_index is > the count, we choose one randomly.
 * This is a hack so we can append "Random" to the choice list.
 */
GiveSecondary(client, list_index)
{
	if (list_index < 0)
	{
		return;
	}
	
	new weapon_index;
	if (list_index >= g_SecondaryCount)
	{
		if (g_SecondaryCount > 1)
		{
			weapon_index = GetRandomInt(0, g_SecondaryCount-1);
			weapon_index = g_SecondaryList[weapon_index];
		} else if (g_SecondaryCount == 1) {
			weapon_index = g_SecondaryList[0];
		} else {
			return;
		}
	} else {
		weapon_index = g_SecondaryList[list_index];
	}
	
	g_SecondaryChoices[client] = list_index;
	
	GiveWeapon(client, weapon_index);
}

bool:ShouldRun()
{
	return (DM_IsRunning() && g_IsEnabled);
}

bool:KvGetYesOrNo(Handle:kv, const String:key[], bool:curdefault)
{
	decl String:value[12];
	KvGetString(kv, key, value, sizeof(value), curdefault ? "yes" : "no");
	return (strcmp(value, "yes") == 0);
}

KvGetGunMenu(Handle:kv, const String:key[], def)
{
	decl String:sdef[12];
	if (def == CSSDM_GUNMENU_YES)
	{
		strcopy(sdef, sizeof(sdef), "yes");
	} else if (def == CSSDM_GUNMENU_RANDOM) {
		strcopy(sdef, sizeof(sdef), "random");
	} else {
		strcopy(sdef, sizeof(sdef), "no");
	}
	
	decl String:value[12];
	KvGetString(kv, key, value, sizeof(value), sdef);
	
	if (strcmp(value, "yes") == 0)
	{
		return CSSDM_GUNMENU_YES;
	} else if (strcmp(value, "random") == 0) {
		return CSSDM_GUNMENU_RANDOM;
	}
	
	return CSSDM_GUNMENU_NO;
}

LoadDefaults()
{
	g_PrimaryMenu = CSSDM_GUNMENU_YES;
	g_SecondaryMenu = CSSDM_GUNMENU_YES;
	g_AllowBuy = false;
	g_ArmorAmount = 100;
	g_Helmets = true;
	g_Flashes = 0;
	g_Smokes = false;
	g_HEs = false;
	g_NightVision = false;
	g_DefuseKits = true;
	g_HealthAmount = 100;
	g_AllowGunCommand = true;
}

bool:LoadConfigFile(const String:path[])
{
	new Handle:kv = CreateKeyValues("Equipment");
	if (!FileToKeyValues(kv, path))
	{
		return false;
	}
	
	decl String:value[255];
	
	/* Load settings */
	if (KvJumpToKey(kv, "Settings"))
	{
		g_AllowGunCommand = KvGetYesOrNo(kv, "guns_command", g_AllowGunCommand);
		KvGoBack(kv);
	}
	
	/* Load menu options */
	if (KvJumpToKey(kv, "Menus"))
	{
		g_PrimaryMenu = KvGetGunMenu(kv, "primary", g_PrimaryMenu);
		g_SecondaryMenu = KvGetGunMenu(kv, "secondary", g_SecondaryMenu);
		g_AllowBuy = KvGetYesOrNo(kv, "buy", g_AllowBuy);
		KvGoBack(kv);
	}
	
	/* Load automatic stuff */
	if (KvJumpToKey(kv, "AutoItems"))
	{
		g_ArmorAmount = KvGetNum(kv, "armor", g_ArmorAmount);
		g_Flashes = KvGetNum(kv, "flashbangs", g_Flashes);
		g_Helmets = KvGetYesOrNo(kv, "helmet", g_Helmets);
		g_Smokes = KvGetYesOrNo(kv, "smokegrenade", g_Smokes);
		g_HEs = KvGetYesOrNo(kv, "hegrenade", g_HEs);
		g_NightVision = KvGetYesOrNo(kv, "nightvision", g_NightVision);
		g_DefuseKits = KvGetYesOrNo(kv, "defusekits", g_DefuseKits);
		g_HealthAmount = KvGetNum(kv, "health", g_HealthAmount);
		KvGoBack(kv);
	}
	
	/* Load bot items */
	if (KvJumpToKey(kv, "BotItems"))
	{
		/* Clear out old lists first */
		g_BotPrimaryCount = 0;
		g_BotSecondaryCount = 0;
		
		if (KvGotoFirstSubKey(kv, false))
		{
			do
			{
				KvGetSectionName(kv, value, sizeof(value));
				if (strcmp(value, "weapon") == 0)
				{
					KvGetString(kv, NULL_STRING, value, sizeof(value));
					new index = DM_GetWeaponID(value);
					if (index != -1)
					{
						new DmWeaponType:type = DM_GetWeaponType(index);
						if (type == DmWeapon_Primary && g_BotPrimaryCount < CSSDM_MAX_WEAPONS)
						{
							g_BotPrimaryList[g_BotPrimaryCount++] = index;
						} else if (type == DmWeapon_Secondary && g_BotSecondaryCount < CSSDM_MAX_WEAPONS) {
							g_BotSecondaryList[g_BotSecondaryCount++] = index;
						}
					}					
				} else if (strcmp(value, "armor") == 0) {
					g_BotArmor = KvGetNum(kv, NULL_STRING, 100);
				} else if (strcmp(value, "helmet") == 0) {
					g_BotHelmets = KvGetYesOrNo(kv, NULL_STRING, true);
				} else if (strcmp(value, "flashbangs") == 0) {
					g_BotFlashes = KvGetNum(kv, NULL_STRING);
				} else if (strcmp(value, "smokegrenade") == 0) {
					g_BotSmokes = KvGetYesOrNo(kv, NULL_STRING, false);
				} else if (strcmp(value, "hegrenade") == 0) {
					g_BotHEs = KvGetYesOrNo(kv, NULL_STRING, false);
				} else if (strcmp(value, "defusekits") == 0) {
					g_BotDefuseKits = KvGetYesOrNo(kv, NULL_STRING, true);
				} else if (strcmp(value, "health") == 0) {
					g_BotHealthAmount = KvGetNum(kv, NULL_STRING, 100);
				}
			} while (KvGotoNextKey(kv, false));
			KvGoBack(kv);
		}
		KvGoBack(kv);
	}
	
	/* Load secondary weapons */
	if (KvJumpToKey(kv, "SecondaryMenu"))
	{
		/* Clear out old list first */
		g_SecondaryCount = 0;
		
		if (KvGotoFirstSubKey(kv, false))
		{
			do
			{
				KvGetSectionName(kv, value, sizeof(value));
				if (strcmp(value, "weapon") == 0)
				{
					KvGetString(kv, NULL_STRING, value, sizeof(value));
					new index = DM_GetWeaponID(value);
					if (index != -1 && DM_GetWeaponType(index) == DmWeapon_Secondary)
					{
						g_SecondaryList[g_SecondaryCount++] = index;
					}
				}
			} while (KvGotoNextKey(kv, false) && g_SecondaryCount < CSSDM_MAX_WEAPONS);
			KvGoBack(kv);
		}
		KvGoBack(kv);
	}
	
	/* Load primary weapons */
	if (KvJumpToKey(kv, "PrimaryMenu"))
	{
		/* Clear out old list first */
		g_PrimaryCount = 0;
		
		if (KvGotoFirstSubKey(kv, false))
		{
			do
			{
				KvGetSectionName(kv, value, sizeof(value));
				if (strcmp(value, "weapon") == 0)
				{
					KvGetString(kv, NULL_STRING, value, sizeof(value));
					new index = DM_GetWeaponID(value);
					if (index != -1 && DM_GetWeaponType(index) == DmWeapon_Primary)
					{
						g_PrimaryList[g_PrimaryCount++] = index;
					}
				}
			} while (KvGotoNextKey(kv, false) && g_PrimaryCount < CSSDM_MAX_WEAPONS);
			KvGoBack(kv);
		}
		KvGoBack(kv);
	}
	
	CloseHandle(kv);
	
	/* Build the primary menu */
	if (g_hPrimaryMenu != INVALID_HANDLE)
	{
		CloseHandle(g_hPrimaryMenu);
	}
	g_hPrimaryMenu = CreateMenu(Menu_PrimaryHandler, MenuAction_DrawItem|MenuAction_Display);
	SetMenuTitle(g_hPrimaryMenu, "Primary weapon:");
	for (new i=0; i<g_PrimaryCount; i++)
	{
		new index = g_PrimaryList[i];
		DM_GetWeaponName(index, value, sizeof(value));
		AddMenuItem(g_hPrimaryMenu, "", value);
	}
	AddMenuItem(g_hPrimaryMenu, "", "Random");
	
	/* Build the secondary menu */
	if (g_hSecondaryMenu != INVALID_HANDLE)
	{
		CloseHandle(g_hSecondaryMenu);
	}
	g_hSecondaryMenu = CreateMenu(Menu_SecondaryHandler, MenuAction_DrawItem|MenuAction_Display);
	SetMenuTitle(g_hSecondaryMenu, "Secondary weapon:");
	for (new i=0; i<g_SecondaryCount; i++)
	{
		new index = g_SecondaryList[i];
		DM_GetWeaponName(index, value, sizeof(value));
		AddMenuItem(g_hSecondaryMenu, "", value);
	}
	AddMenuItem(g_hSecondaryMenu, "", "Random");
	
	return true;
}

SetClientArmor(client, armor)
{
	if (g_ArmorOffset == -1)
	{
		return;
	}
	
	SetEntData(client, g_ArmorOffset, armor, 4, true);
}

SetClientHealth(client, health)
{
	if (g_HealthOffset == -1)
	{
		return;
	}
	
	SetEntData(client, g_HealthOffset, health, 4, true);
}

GiveGrenades(client, flashnum, bool:he, bool:smoke)
{
	if (he)
	{
		GivePlayerItem(client, "weapon_hegrenade");
	}
	for (new i=0; i<flashnum; i++)
	{
		GivePlayerItem(client, "weapon_flashbang");
	}
	if (smoke)
	{
		GivePlayerItem(client, "weapon_smokegrenade");
	}
}

GiveNightVision(client)
{
	if (g_NVOffset == -1)
	{
		return;
	}
	
	SetEntData(client, g_NVOffset, true, 1, true);
}

