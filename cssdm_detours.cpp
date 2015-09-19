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
#include "cssdm_detours.h"
#include "cssdm_utils.h"
#include "cssdm_events.h"
#include <sm_platform.h>
#include "CDetour/detours.h"

CDetour *csdrop_callback = NULL;
CDetour *drpwpns_callback = NULL;

#if SOURCE_ENGINE == SE_CSGO
DETOUR_DECL_MEMBER3(DetourCSWeaponDrop, void, CBaseEntity *, weapon, Vector, vec, bool, unknown)
#else
DETOUR_DECL_MEMBER3(DetourCSWeaponDrop, void, CBaseEntity *, weapon, bool, bDropShield, bool, bThrowForward)
#endif
{
#if SOURCE_ENGINE == SE_CSGO
		DETOUR_MEMBER_CALL(DetourCSWeaponDrop)(weapon, vec, unknown);
#else
		DETOUR_MEMBER_CALL(DetourCSWeaponDrop)(weapon, bDropShield, bThrowForward);
#endif
		OnClientDroppedWeapon(reinterpret_cast<CBaseEntity *>(this), weapon);
}

void DoGatePatch(unsigned char *target, void *callback, dmpatch_t *restore)
{
	DM_SetMemPatchable(target, 20);

	/* First, save restore bits */
	for (size_t i=0; i<restore->bytes; i++)
	{
		restore->patch[i] = target[i];
	}

	target[0] = 0xFF;	/* JMP */
	target[1] = 0x25;	/* MEM32 */
	*(void **)(&target[2]) = callback;
}

DETOUR_DECL_MEMBER2(DetourDrpWpns, void, bool, unknown1, bool, unknown2)
{
	OnClientDropWeapons(reinterpret_cast<CBaseEntity *>(this));
	DETOUR_MEMBER_CALL(DetourDrpWpns)(unknown1, unknown2);
}

void InitDropWeaponsDetour()
{
	drpwpns_callback = DETOUR_CREATE_MEMBER(DetourDrpWpns, "DropWeapons");
	if(drpwpns_callback)
	{
		drpwpns_callback->EnableDetour();
	}
}

void InitCSDropDetour()
{
	csdrop_callback = DETOUR_CREATE_MEMBER(DetourCSWeaponDrop, "CSWeaponDrop");
	if(csdrop_callback)
	{
		csdrop_callback->EnableDetour();
	}

}

void DM_InitDetours()
{
	CDetourManager::Init(g_pSM->GetScriptingEngine(), g_pDmConf);
	InitDropWeaponsDetour();
	InitCSDropDetour();
}

void DM_ShutdownDetours()
{
	if (drpwpns_callback)
	{
		drpwpns_callback->Destroy();
		drpwpns_callback = NULL;
	}
	if (csdrop_callback)
	{
		csdrop_callback->Destroy();
		csdrop_callback = NULL;
	}
}
