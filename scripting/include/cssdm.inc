/**
 * vim: set ts=4 :
 * ===============================================================
 * SourceMod (C)2004-2007 AlliedModders LLC.  All rights reserved.
 * ===============================================================
 *
 *  This file is part of the SourceMod/SourcePawn SDK.  This file may only be used 
 * or modified under the Terms and Conditions of its License Agreement, which is found 
 * in LICENSE.txt.  The Terms and Conditions for making SourceMod extensions/plugins 
 * may change at any time.  To view the latest information, see:
 *   http://www.sourcemod.net/license.php
 *
 * Version: $Id: cssdm.inc 416 2007-07-25 04:38:40Z dvander $
 */

#if defined _cssdm_included
 #endinput
#endif
#define _cssdm_included

#define CSSDM_VERSION		"2.1.7-dev"

#define CSSDM_MAX_WEAPONS		32

#include <sourcemod>
#include <sdktools>
#include <cstrike>

enum DmWeaponType
{
	DmWeapon_Invalid = -1,
	DmWeapon_Primary = 0,
	DmWeapon_Secondary = 1,
	DmWeapon_Grenade = 3,
	DmWeapon_C4 = 4,
};

/**
 * Called when CS:S DM is started/enabled.  If CS:S DM is enabled,
 * this will get called at the start of every map.
 */
forward void DM_OnStartup();

/**
 * Called when CS:S DM is disabled.  If CS:S DM is enabled, this 
 * will get called at the end of the map.
 */
forward void DM_OnShutdown();

/**
 * Called when a player spawns and CS:S DM is active.  This
 * is a wrapper around the "player_spawn" event.
 *
 * @param client		Client index.
 * @noreturn
 */
forward void DM_OnClientSpawned(int client);

/**
 * Called when a player spawns and CS:S DM is active.  This 
 * is called after all plugins are done processing 
 * DM_OnClientSpawned().
 *
 * @param client		Client index.
 * @noreturn
 */
forward void DM_OnClientPostSpawned(int client);

/**
 * Called when a player dies.
 * 
 *
 * @param client		Client index.
 * @return				Plugin_Handled to block player from respawning.
 */
forward Action DM_OnClientDeath(int client);

/**
 * Called when the cssdm_spawn_method cvar changes.
 *
 * @param method		Method name.
 * @return				Plugin_Stop to handle this method.
 */
forward Action DM_OnSetSpawnMethod(const char[] method);

/**
 * Returns whether CS:S DM is running or not.  This should be used to 
 * check whether non-CS:S DM callbacks should be processed.
 *
 * @return				True if CS:S DM is running, false otherwise.
 */
native bool DM_IsRunning();

/**
 * Retrieves the CS:S DM spawning method.
 *
 * @param method		Method name buffer.
 * @param maxlength		Maximum size of the method name buffer.
 * @noreturn
 */
native void DM_GetSpawnMethod(char[] method, int maxlength);

/**
 * Provides lookup for a weapon by its short name.
 *
 * @param name			Weapon short name (such as "usp").
 * @return				Weapon ID in CS:S DM, or -1 on failure.
 */
native int DM_GetWeaponID(const char[] name);

/**
 * Gets the type of a weapon by index.
 *
 * @param id			Weapon ID.
 * @return				Weapon type.
 * @error				Invalid weapon index.
 */
native DmWeaponType DM_GetWeaponType(int index);

/**
 * Retrieves the classname of a weapon. 
 *
 * @param id			Weapon ID.
 * @param classname		Class name buffer.
 * @param maxlength		Maximum length of the class name buffer.
 * @noreturn
 * @error				Invalid weapon index.
 */
native void DM_GetWeaponClassname(int id, char[] classname, int maxlength);

/**
 * Retrieves the display name of a weapon. 
 *
 * @param id			Weapon ID.
 * @param name			Display name buffer.
 * @param maxlength		Maximum length of the display name buffer.
 * @noreturn
 * @error				Invalid weapon index.
 */
native void DM_GetWeaponName(int id, char[] name, int maxlength);

/**
 * Retrieves the entity index of a weapon in a player's weapon slot.
 *
 * @param client		Client index.
 * @param type			Weapon slot type.
 * @return				Entity index, or -1 for none.
 */
stock int DM_GetClientWeapon(int client, DmWeaponType type)
{
	return GetPlayerWeaponSlot(client, view_as<int>(type));
}

/**
 * Drops a client's weapon.  CS:S DM removes dropped weapons from the world.
 *
 * @param client		Client index.
 * @param entity		Weapon entity index.
 * @noreturn
 * @error				Client index invalid or not in game; weapon entity invalid.
 */
stock void DM_DropWeapon(int client, int entity)
{
	if (client <= 0 || client > MaxClients)
	{
		ThrowError("Invalid client index %d", client);
	}

	if (!IsClientInGame(client))
	{
		ThrowError("Client %d is not in game", client);
	}

	if (!IsValidEntity(entity))
	{
		ThrowError("Entity %d is not a valid entity", entity);
	}

	int owner = GetEntPropEnt(entity, Prop_Send, "m_hOwnerEntity");
	if (client != owner)
	{
		AcceptEntityInput(entity, "Kill");
		return;
	}

	CS_DropWeapon(client, entity, true, true);
	AcceptEntityInput(entity, "Kill");
}

/**
 * Strips all items from a bot, excluding their player suit.
 *
 * @param client		Client index.
 * @noreturn
 * @error				Invalid client index, client not in game, nor client not a bot.
 */
native void DM_StripBotItems(int client);

/**
 * Returns the spawn waiting time.
 *
 * @return				Spawn wait time, in seconds.
 */
native float DM_GetSpawnWaitTime();

/**
 * Respawns a player using CS:S's low level respawning function.  This can be 
 * called even if the player is not dead.
 *
 * @param client		Client index.
 * @param fullRespawn	If true, client will be forwarded through player_spawn 
 *						callbacks.
 * @noreturn
 * @error				Invalid client index or client not in game.
 */
native void DM_RespawnClient(int client, bool fullRespawn=true);

/**
 * Returns whether or not a client is alive.
 *
 * @param client		Client index.
 * @return				True if alive, false if dead.
 * @error				Invalid client index or client not in game.
 */
native bool DM_IsClientAlive(int client);

/**
 * Gives ammo to a player.
 *
 * @param client		Client index.
 * @param type			Ammo type (>=0).
 * @param count			Ammo amount to give.
 * @param noSound		If true, the ammo sound will be suppressed.
 * @return				Actual ammo amount given.
 * @error				Invalid client index or client not in game.
 */
stock int DM_GiveClientAmmo(int client, int type, int count, bool noSound=false)
{
	if (client <= 0 || client > MaxClients)
	{
		ThrowError("Invalid client index %d", client);
	}

	if (!IsClientInGame(client))
	{
		ThrowError("Client %d is not in game", client);
	}

	return GivePlayerAmmo(client, count, type, noSound);
}

/**
 * Do not edit below this line!
 */
public SharedPlugin __pl_cssdm = 
{
	name = "CS:S DM",
	file = "dm_base.smx",
#if defined REQUIRE_PLUGIN
	required = 1,
#else
	required = 0,
#endif
};
