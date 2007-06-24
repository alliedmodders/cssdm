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

#ifndef _INCLUDE_CSSDM_UTILS_H_
#define _INCLUDE_CSSDM_UTILS_H_

#include "smsdk_ext.h"

#if defined PLATFORM_LINUX
#define EXTRA_VTBL_OFFSET	1
#elif defined PLATFORM_WINDOWS
#define EXTRA_VTBL_OFFSET	0
#endif

#define CS_TEAM_T		2
#define CS_TEAM_CT		3

struct dmpatch_t
{
	unsigned char patch[20];
	size_t bytes;
};

/** "Safe" functions */
CBaseEntity *DM_GetBaseEntity(int index);
void DM_RespawnPlayer(int client);
void DM_RemoveEntity(CBaseEntity *pEntity);
bool DM_IsPlayerAlive(int client);
bool DM_CheckSerial(edict_t *pEdict, int serial);
size_t DM_StringToBytes(const char *str, unsigned char buffer[], size_t maxlength);
void DM_ApplyPatch(void *address, int offset, const dmpatch_t *patch, dmpatch_t *restore);
CBaseEntity *DM_GetWeaponFromSlot(CBaseEntity *pEntity, int slot);
void DM_DropWeapon(CBaseEntity *pEntity, CBaseEntity *pWeapon);
void DM_RemoveAllItems(CBaseEntity *pEntity, bool removeSuit);
void DM_SetMemPatchable(void *address, size_t size);
void DM_SetDefuseKit(CBaseEntity *pEntity, bool defuseKit);
int DM_GiveAmmo(CBaseEntity *pEntity, int type, int count, bool noSound);

/** "Internal" functions */
CBaseEntity *DM_GetAndClearRagdoll(CBaseEntity *pEntity, int &serial);

/** Main functions */
bool InitializeUtils(char *error, size_t maxlength);
void ShutdownUtils();

#endif //_INCLUDE_CSSDM_UTILS_H_
