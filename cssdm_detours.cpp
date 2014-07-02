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
#include <jit/jit_helpers.h>
#include <jit/x86/x86_macros.h>
#include "CDetour/detours.h"

dmpatch_t drpwpns_restore;
void *drpwpns_address = NULL;
void *drpwpns_callback = NULL;

CDetour *csdrop_callback = NULL;

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

void InitDropWeaponsDetour()
{
	int offset;
	g_pDmConf->GetMemSig("DropWeapons", &drpwpns_address);
	g_pDmConf->GetOffset("DropWeaponsPatch", &offset);
	drpwpns_restore.bytes = offset;

	/* Patch the gate */
	DoGatePatch((unsigned char *)drpwpns_address, &drpwpns_callback, &drpwpns_restore);

	/* Create the callback */
	drpwpns_callback = spengine->ExecAlloc(100);
	JitWriter wr;
	JitWriter *jit = &wr;
	wr.outbase = (jitcode_t)drpwpns_callback;
	wr.outptr = wr.outbase;

	/* Luckily for this we have no parameters!
	 * The prototype we are pushing is:
	 *
	 * void OnClientDropWeapons(CBaseEntity *pEntity);
	 */
#if defined PLATFORM_POSIX
	IA32_Push_Rm_Disp8_ESP(jit, 4);					//push [esp+4]
#elif defined PLATFORM_WINDOWS
	IA32_Push_Reg(jit, REG_ECX);					//push ecx
#endif
	//call <function>
	jitoffs_t call = IA32_Call_Imm32(jit, 0);		//call <function>
	IA32_Write_Jump32_Abs(jit, call, (void *)OnClientDropWeapons);
#if defined PLATFORM_POSIX
	IA32_Add_Rm_Imm8(jit, REG_ESP, 4, MOD_REG);		//add esp, 4
#elif defined PLATFORM_WINDOWS
	IA32_Pop_Reg(jit, REG_ECX);
#endif
	/* Patch old bytes in */
	for (size_t i=0; i<drpwpns_restore.bytes; i++)
	{
		jit->write_ubyte(drpwpns_restore.patch[i]);
	}
	/* Finally, do a jump back to the original */
	call = IA32_Jump_Imm32(jit, 0);
	IA32_Write_Jump32_Abs(jit, call, (unsigned char *)drpwpns_address + drpwpns_restore.bytes);
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
		/* Remove the patch */
		DM_ApplyPatch(drpwpns_address, 0, &drpwpns_restore, NULL);
		/* Free the gate */
		spengine->ExecFree(drpwpns_callback);
		drpwpns_callback = NULL;
	}
	if (csdrop_callback)
	{
		csdrop_callback->Destroy();
		csdrop_callback = NULL;
	}
}
