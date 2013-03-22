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
#include "cssdm_players.h"
#include "cssdm_utils.h"
#include "cssdm_includesdk.h"
#include <sh_list.h>
#include <sh_memory.h>

using namespace SourceHook;

List<ICallWrapper *> g_CallWrappers;
ICallWrapper *g_pRoundRespawn = NULL;
ICallWrapper *g_pRemoveEntity = NULL;
ICallWrapper *g_pWeaponGetSlot = NULL;
ICallWrapper *g_pDropWeapon = NULL;
ICallWrapper *g_pRemoveAllItems = NULL;
ICallWrapper *g_pGiveAmmo = NULL;
int g_RagdollOffset = 0;
int g_LifeStateOffset = 0;
int g_DefuserOffset = 0;

void DM_ProtectMemory(void *addr, int length, int prot);

CBaseEntity *DM_GetBaseEntity(int index)
{
	edict_t *pEdict = gamehelpers->EdictOfIndex(index);
	if (!pEdict || pEdict->IsFree())
	{
		return NULL;
	}
	IServerUnknown *pUnknown = pEdict->GetUnknown();
	if (!pUnknown)
	{
		return NULL;
	}
	return pUnknown->GetBaseEntity();
}

CBaseEntity *DM_GetAndClearRagdoll(CBaseEntity *pEntity, int &serial)
{
	if (!g_RagdollOffset)
	{
		return NULL;
	}

	unsigned char *ptr = (unsigned char *)pEntity + g_RagdollOffset;
	CBaseHandle &ph = *(CBaseHandle *)ptr;

	if (!ph.IsValid())
	{
		return NULL;
	}

	int index = ph.GetEntryIndex();
	serial = ph.GetSerialNumber();

	if (!index)
	{
		return NULL;
	}

	CBaseEntity *pRagdoll = DM_GetBaseEntity(index);

	ph.Set(NULL);

	return pRagdoll;
}

void DM_RespawnPlayer(int client)
{
	dm_player_t *player = DM_GetPlayer(client);
	if (!player || !player->pEntity)
	{
		return;
	}

	if (player->respawn_timer)
	{
		timersys->KillTimer(player->respawn_timer);
		player->respawn_timer = NULL;
	}

	CBaseEntity *pEntity = player->pEntity;
	g_pRoundRespawn->Execute(&pEntity, NULL);
}

void DM_RemoveEntity(CBaseEntity *pEntity)
{
	g_pRemoveEntity->Execute(&pEntity, NULL);
}

bool DM_IsPlayerAlive(int client)
{
	dm_player_t *player = DM_GetPlayer(client);
	if (!player || !player->pEntity || !g_LifeStateOffset)
	{
		return false;
	}

	if (!g_LifeStateOffset)
	{
		return player->pInfo->IsDead() ? false : true;
	}

	unsigned char *ptr = (unsigned char *)(player->pEntity) + g_LifeStateOffset;
	return (*(char *)ptr == LIFE_ALIVE);
}

bool DM_CheckSerial(edict_t *pEdict, int serial)
{
	int new_serial = (serial & ((1 << NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS) - 1));
	return (pEdict->m_NetworkSerialNumber == new_serial);
}

CBaseEntity *DM_GetWeaponFromSlot(CBaseEntity *pEntity, int slot)
{
	unsigned char vstk[sizeof(CBaseEntity *) + sizeof(int)];
	*(CBaseEntity **)(&vstk[0]) = pEntity;
	*(int *)(&vstk[sizeof(CBaseEntity *)]) = slot;

	CBaseEntity *pWeapon = NULL;

	g_pWeaponGetSlot->Execute(vstk, &pWeapon);

	return pWeapon;
}

void DM_DropWeapon(CBaseEntity *pEntity, CBaseEntity *pWeapon)
{
	unsigned char vstk[sizeof(CBaseEntity *) * 2 + sizeof(bool) * 2];
	unsigned char *vptr = vstk;

	*(CBaseEntity **)vptr = pEntity;
	vptr += sizeof(CBaseEntity *);
	*(CBaseEntity **)vptr = pWeapon;
	vptr += sizeof(CBaseEntity *);
	*(bool *)vptr = true;
	vptr += sizeof(bool);
	*(bool *)vptr = false;
	//vptr += sizeof(bool);

	g_pDropWeapon->Execute(vstk, NULL);
}

void DM_RemoveAllItems(CBaseEntity *pEntity, bool removeSuit)
{
	unsigned char vstk[sizeof(CBaseEntity *) + sizeof(bool)];
	unsigned char *vptr = vstk;

	*(CBaseEntity **)vptr = pEntity;
	vptr += sizeof(CBaseEntity *);
	*(bool *)vptr = removeSuit;
	//vptr += sizeof(bool);

	g_pRemoveAllItems->Execute(vstk, NULL);
}

void DM_SetDefuseKit(CBaseEntity *pEntity, bool defuseKit)
{
	if (!g_DefuserOffset)
	{
		return;
	}

	*(bool *)((unsigned char *)pEntity + g_DefuserOffset) = defuseKit;
}

int DM_GiveAmmo(CBaseEntity *pEntity, int type, int count, bool noSound)
{
	unsigned char vstk[sizeof(CBaseEntity *) + sizeof(int)*2 + sizeof(bool)];
	unsigned char *vptr = vstk;

	*(CBaseEntity **)vptr = pEntity;
	vptr += sizeof(CBaseEntity *);
	*(int *)vptr = count;
	vptr += sizeof(int);
	*(int *)vptr = type;
	vptr += sizeof(int);
	*(bool *)vptr = noSound;
	//vptr += sizeof(bool);

	int ret;
	g_pGiveAmmo->Execute(vstk, &ret);

	return ret;
}

size_t DM_StringToBytes(const char *str, unsigned char buffer[], size_t maxlength)
{
	size_t real_bytes = 0;
	size_t length = strlen(str);

	for (size_t i=0; i<length; i++)
	{
		if (real_bytes >= maxlength)
		{
			break;
		}
		buffer[real_bytes++] = (unsigned char)str[i];
		if (str[i] == '\\'
			&& str[i+1] == 'x')
		{
			if (i + 3 >= length)
			{
				continue;
			}
			/* Get the hex part */
			char s_byte[3];
			int r_byte;
			s_byte[0] = str[i+2];
			s_byte[1] = str[i+3];
			s_byte[2] = '\n';
			/* Read it as an integer */
			sscanf(s_byte, "%x", &r_byte);
			/* Save the value */
			buffer[real_bytes-1] = (unsigned char)r_byte;
			/* Adjust index */
			i += 3;
		}
	}

	return real_bytes;
}

void DM_ApplyPatch(void *address, int offset, const dmpatch_t *patch, dmpatch_t *restore)
{
	unsigned char *addr = (unsigned char *)address + offset;

	SourceHook::SetMemAccess(addr, 20, SH_MEM_READ|SH_MEM_WRITE|SH_MEM_EXEC);

	if (restore)
	{
		for (size_t i=0; i<patch->bytes; i++)
		{
			restore->patch[i] = addr[i];
		}
		restore->bytes = patch->bytes;
	}

	for (size_t i=0; i<patch->bytes; i++)
	{
		addr[i] = patch->patch[i];
	}
}

void DM_SetMemPatchable(void *address, size_t size)
{
	SourceHook::SetMemAccess(address, (int)size, SH_MEM_READ|SH_MEM_WRITE|SH_MEM_EXEC);
}

#define GET_PROPERTY(cls, name, var) \
	if ( (prop = gamehelpers->FindInSendTable(cls, name)) == NULL ) { \
		snprintf(error, maxlength, "Could not find property: %s", name); \
		return false; \
	} \
	var = prop->GetOffset();

bool InitializeUtils(char *error, size_t maxlength)
{
	void *addr;
	int offset;
	SourceMod::PassInfo pass[4];

	/** ROUNDRESPAWN */
	g_pDmConf->GetMemSig("RoundRespawn", &addr);
	g_pRoundRespawn = bintools->CreateCall(addr, CallConv_ThisCall, NULL, NULL, 0);
	g_CallWrappers.push_back(g_pRoundRespawn);

	/** UTIL_REMOVE */
	g_pDmConf->GetMemSig("UTIL_Remove", &addr);
	pass[0].flags = PASSFLAG_BYVAL;
	pass[0].size = sizeof(CBaseEntity *);
	pass[0].type = PassType_Basic;
	g_pRemoveEntity = bintools->CreateCall(addr, CallConv_Cdecl, NULL, pass, 1);
	g_CallWrappers.push_back(g_pRemoveEntity);
	
	/** WEAPON_GETSLOT */
	g_pDmConf->GetOffset("Weapon_GetSlot", &offset);
	pass[0].flags = PASSFLAG_BYVAL;
	pass[0].size = sizeof(int);
	pass[0].type = PassType_Basic;
	pass[1].flags = PASSFLAG_BYVAL;
	pass[1].size = sizeof(CBaseEntity *);
	pass[1].type = PassType_Basic;
	g_pWeaponGetSlot = bintools->CreateVCall(offset, 0, 0, &pass[1], &pass[0], 1);
	g_CallWrappers.push_back(g_pWeaponGetSlot);

	/** CSWEAPONDROP */
	g_pDmConf->GetMemSig("CSWeaponDrop", &addr);
	pass[0].flags = pass[1].flags = pass[2].flags  = PASSFLAG_BYVAL;
	pass[0].type = pass[1].type = pass[2].type = PassType_Basic;
	pass[0].size = sizeof(CBaseEntity *);
	pass[1].size = pass[2].size = sizeof(bool);
	g_pDropWeapon = bintools->CreateCall(addr, CallConv_ThisCall, NULL, pass, 3);
	g_CallWrappers.push_back(g_pDropWeapon);

	/** REMOVEALLITEMS */
	g_pDmConf->GetOffset("RemoveAllItems", &offset);
	pass[0].flags = PASSFLAG_BYVAL;
	pass[0].size = sizeof(bool);
	pass[0].type = PassType_Basic;
	g_pRemoveAllItems = bintools->CreateVCall(offset, 0, 0, NULL, pass, 1);
	g_CallWrappers.push_back(g_pRemoveAllItems);

	/** GIVEAMMO */
	g_pDmConf->GetOffset("GiveAmmo", &offset);
	pass[0].flags = pass[1].flags = pass[2].flags = pass[3].flags = PASSFLAG_BYVAL;
	pass[0].size = pass[1].size = pass[2].size = sizeof(int);
	pass[3].size = sizeof(bool);
	pass[0].type = pass[1].type = pass[2].type = pass[3].type = PassType_Basic;
	g_pGiveAmmo = bintools->CreateVCall(offset, 0, 0, &pass[3], pass, 3);
	g_CallWrappers.push_back(g_pGiveAmmo);

	/** PROPERTIES */
	sm_sendprop_info_t prop;
	if(!gamehelpers->FindSendPropInfo("CCSPlayer", "m_hRagdoll", &prop))
	{
		snprintf(error, maxlength, "Failed to get prop info for m_hRagdoll");
		return false;
	}
	g_RagdollOffset = prop.actual_offset;
	if(!gamehelpers->FindSendPropInfo("CCSPlayer", "m_lifeState", &prop))
	{
		snprintf(error, maxlength, "Failed to get prop info for m_lifeState");
		return false;
	}
	g_LifeStateOffset = prop.actual_offset;
	if(!gamehelpers->FindSendPropInfo("CCSPlayer", "m_bHasDefuser", &prop))
	{
		snprintf(error, maxlength, "Failed to get prop info for m_bHasDefuser");
		return false;
	}
	g_DefuserOffset = prop.actual_offset;

	return true;
}

void ShutdownUtils()
{
	List<ICallWrapper *>::iterator iter;
	for (iter = g_CallWrappers.begin();
		 iter != g_CallWrappers.end();
		 iter++)
	{
		(*iter)->Destroy();
	}
	g_CallWrappers.clear();
}
