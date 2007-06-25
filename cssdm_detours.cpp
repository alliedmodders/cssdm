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

dmpatch_t drpwpns_restore;
void *drpwpns_address = NULL;
void *drpwpns_callback = NULL;

dmpatch_t csdrop_restore;
void *csdrop_address = NULL;
void *csdrop_callback = NULL;

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
#if defined PLATFORM_LINUX
	IA32_Push_Rm_Disp8_ESP(jit, 4);					//push [esp+4]
#elif defined PLATFORM_WINDOWS
	IA32_Push_Reg(jit, REG_ECX);					//push ecx
#endif
	//call <function>
	jitoffs_t call = IA32_Call_Imm32(jit, 0);		//call <function>
	IA32_Write_Jump32_Abs(jit, call, (void *)OnClientDropWeapons);
#if defined PLATFORM_LINUX
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
	int offset;
	g_pDmConf->GetMemSig("CSWeaponDrop", &csdrop_address);
	g_pDmConf->GetOffset("CSWeaponDropPatch", &offset);
	csdrop_restore.bytes = offset;

	/* Patch the gate */
	DoGatePatch((unsigned char *)csdrop_address, &csdrop_callback, &csdrop_restore);

	/* Create the callback */
	csdrop_callback = spengine->ExecAlloc(100);
	JitWriter wr;
	JitWriter *jit = &wr;
	wr.outbase = (jitcode_t)csdrop_callback;
	wr.outptr = wr.outbase;

	/* This one is a POST detour instead of a pre-detour!
	 * This means we call first and ask questions later.
	 */
	//push ebp
	//mov ebp, esp
	IA32_Push_Reg(jit, REG_EBP);
	IA32_Mov_Reg_Rm(jit, REG_EBP, REG_ESP, MOD_REG);

	/* Re-push everything, but setup our next call beforehand. */
#if defined PLATFORM_LINUX
	IA32_Push_Rm_Disp8(jit, REG_EBP, 12);
	IA32_Push_Rm_Disp8(jit, REG_EBP, 8);
	/* Linux pushes four things, it seems */
	IA32_Push_Rm_Disp8(jit, REG_EBP, 20);
	IA32_Push_Rm_Disp8(jit, REG_EBP, 16);
	IA32_Push_Rm_Disp8(jit, REG_EBP, 12);
	IA32_Push_Rm_Disp8(jit, REG_EBP, 8);
#elif defined PLATFORM_WINDOWS
	IA32_Push_Rm_Disp8(jit, REG_EBP, 8);
	IA32_Push_Reg(jit, REG_ECX);
	/* Windows only pushes three because of ECX */
	IA32_Push_Rm_Disp8(jit, REG_EBP, 16);
	IA32_Push_Rm_Disp8(jit, REG_EBP, 12);
	IA32_Push_Rm_Disp8(jit, REG_EBP, 8);
#endif

	/* Make the ORIGINAL call */
	jitoffs_t orig_call = IA32_Call_Imm32(jit, 0);

#if defined PLATFORM_LINUX
	IA32_Add_Rm_Imm8(jit, REG_ESP, 16, MOD_REG);
#endif

	/* Make the POST call */
	//call <function>
	//add esp, 8
	jitoffs_t call = IA32_Call_Imm32(jit, 0);
	IA32_Write_Jump32_Abs(jit, call, (void *)OnClientDroppedWeapon);

	/* Do cleanup */
#if defined PLATFORM_LINUX
	IA32_Add_Rm_Imm8(jit, REG_ESP, 8, MOD_REG);
#elif defined PLATFORM_WINDOWS
	//this probably isn't needed at all!
	IA32_Pop_Reg(jit, REG_ECX);
	IA32_Add_Rm_Imm8(jit, REG_ESP, 4, MOD_REG);
#endif

	/* Return */
	//pop ebp
	//ret / retn 12
	IA32_Pop_Reg(jit, REG_EBP);
#if defined PLATFORM_LINUX
	IA32_Return(jit);
#elif defined PLATFORM_WINDOWS
	IA32_Return_Popstack(jit, 0xC);
#endif

	/* Jump back to the original */
	IA32_Send_Jump32_Here(jit, orig_call);

	/* Copy original bytes */
	for (size_t i=0; i<csdrop_restore.bytes; i++)
	{
		jit->write_ubyte(csdrop_restore.patch[i]);
	}

	call = IA32_Jump_Imm32(jit, 0);
	IA32_Write_Jump32_Abs(jit, call, (unsigned char *)csdrop_address + csdrop_restore.bytes);
}

void DM_InitDetours()
{
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
		/* Remove the patch */
		DM_ApplyPatch(csdrop_address, 0, &csdrop_restore, NULL);
		/* Free the gate */
		spengine->ExecFree(csdrop_callback);
		csdrop_callback = NULL;
	}
}
