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
#include "cssdm_utils.h"
#include "sm_platform.h"
#include "cssdm_includesdk.h"

SH_DECL_MANUALHOOK2(CGameRules_IPointsForKill, 62+EXTRA_VTBL_OFFSET, 0, 0, int, CBasePlayer *, CBasePlayer *);

#if defined PLATFORM_WINDOWS
#define PLATFORM_NAME	"Windows"
#elif defined PLATFORM_LINUX
#define PLATFORM_NAME	"Linux"
#elif defined __APPLE__
#define PLATFORM_NAME	"Mac"
#endif

bool g_FFA_Patched = false;
bool g_FFA_Prepared = false;

void **g_gamerules_addr = NULL;

/* Lagcomp */
static int g_lagcomp_offset = 0;
static void *g_lagcomp_addr = NULL;
static dmpatch_t g_lagcomp_patch;
static dmpatch_t g_lagcomp_restore;

/* Takedamage */
static int g_takedmg_offset[5] = {0};
static void *g_takedmg_addr = NULL;
static dmpatch_t g_takedmg_patch[2];
static dmpatch_t g_takedmg_restore[2];

/* Calc domination and revenge */
static int g_domrev_offset = 0;
static void *g_domrev_addr = NULL;
static dmpatch_t g_domrev_patch;
static dmpatch_t g_domrev_restore;

int OnIPointsForKill(CBasePlayer *pl1, CBasePlayer *pl2)
{
	/* If we're hooked, FFA is always on. */
	RETURN_META_VALUE(MRES_SUPERCEDE, 1);
}

bool DM_FFA_LoadPatch(const char *name, dmpatch_t *patch, char *error, size_t maxlength)
{
	char fullname[255];
	snprintf(fullname, sizeof(fullname), "%s_%s", name, PLATFORM_NAME);

	const char *str = g_pDmConf->GetKeyValue(fullname);
	if (!str)
	{
		snprintf(error, maxlength, "Could not find signature value for \"%s\"", fullname);
		return false;
	}

	patch->bytes = DM_StringToBytes(str, patch->patch, sizeof(patch->patch));
	if (!patch->bytes)
	{
		snprintf(error, maxlength, "Invalid signature detected for \"%s\"", fullname);
		return false;
	}

	return true;
}

bool DM_Prepare_FFA(char *error, size_t maxlength)
{
	void *gamerules = NULL;
	if (!g_pDmConf->GetMemSig("OnTakeDamage", &g_takedmg_addr) || !g_takedmg_addr)
	{
		snprintf(error, maxlength, "Could not find \"OnTakeDamage\" signature!");
		return false;
	}
	if (!g_pDmConf->GetMemSig("WantsLagComp", &g_lagcomp_addr) || !g_lagcomp_addr)
	{
		snprintf(error, maxlength, "Could not find \"WantsLagComp\" signature!");
		return false;
	}
	if (!g_pDmConf->GetMemSig("CalcDominationAndRevenge", &g_domrev_addr) || !g_domrev_addr)
	{
		snprintf(error, maxlength, "Could not find \"CalcDominationAndRevenge\" signature!");
		return false;
	}
	if (!g_pDmConf->GetMemSig("CGameRules", &gamerules) || !gamerules)
	{
		snprintf(error, maxlength, "Could not find \"CGameRules\" signature!");
		return false;
	}
	
	if (!g_pDmConf->GetOffset("LagCompPatch", &g_lagcomp_offset)
		|| !g_lagcomp_offset)
	{
		snprintf(error, maxlength, "Could not find LagCompPatch offset");
		return false;
	}
	if (!DM_FFA_LoadPatch("LagCompPatch", &g_lagcomp_patch, error, maxlength))
	{
		return false;
	}

	if (!g_pDmConf->GetOffset("CalcDomRevPatch", &g_domrev_offset)
		|| !g_domrev_offset)
	{
		snprintf(error, maxlength, "Could not find CalcDomRevPatch offset");
		return false;
	}
	if (!DM_FFA_LoadPatch("CalcDomRevPatch", &g_domrev_patch, error, maxlength))
	{
		return false;
	}

	if (!g_pDmConf->GetOffset("TakeDmgPatch1", &g_takedmg_offset[0])
		|| !g_takedmg_offset[0])
	{
		snprintf(error, maxlength, "Could not find TakeDmgPatch1 offset");
		return false;
	}
	if (!DM_FFA_LoadPatch("TakeDmgPatch1", &g_takedmg_patch[0], error, maxlength))
	{
		return false;
	}

#if SOURCE_ENGINE == SE_CSGO
	if (!g_pDmConf->GetOffset("TakeDmgPatch2", &g_takedmg_offset[1])
		|| !g_takedmg_offset[1])
	{
		snprintf(error, maxlength, "Could not find TakeDmgPatch1 offset");
		return false;
	}
	if (!DM_FFA_LoadPatch("TakeDmgPatch2", &g_takedmg_patch[1], error, maxlength))
	{
		return false;
	}
#endif

	/* Load the GameRules pointer */
	int offset;
#if defined(_MSC_VER) || SOURCE_ENGINE == SE_CSGO
	if (!g_pDmConf->GetOffset("g_pGameRules", &offset)
		|| !offset)
	{
		snprintf(error, maxlength, "Could not find g_pGameRules offset");
		return false;
	}
	g_gamerules_addr = (void **)*(void **)((unsigned char *)gamerules + offset);
#else
	g_gamerules_addr = reinterpret_cast<void **>(gamerules);
#endif

	/* Get the "IPointsForKill" offset */
	if (!g_pDmConf->GetOffset("IPointsForKill", &offset)
		|| !offset)
	{
		snprintf(error, maxlength, "Could not find IPointsForKills offset");
		return false;
	}
	SH_MANUALHOOK_RECONFIGURE(CGameRules_IPointsForKill, offset, 0, 0);

	g_FFA_Prepared = true;

	return true;
}

bool DM_Patch_FFA()
{
	if (g_FFA_Patched || !g_FFA_Prepared || *g_gamerules_addr == NULL)
	{
		return false;
	}

	DM_ApplyPatch(g_lagcomp_addr, g_lagcomp_offset, &g_lagcomp_patch, &g_lagcomp_restore);
	DM_ApplyPatch(g_takedmg_addr, g_takedmg_offset[0], &g_takedmg_patch[0], &g_takedmg_restore[0]);
#if SOURCE_ENGINE == SE_CSGO
	DM_ApplyPatch(g_takedmg_addr, g_takedmg_offset[1], &g_takedmg_patch[1], &g_takedmg_restore[1]);
#endif
	DM_ApplyPatch(g_domrev_addr, g_domrev_offset, &g_domrev_patch, &g_domrev_restore);

	SH_ADD_MANUALHOOK_STATICFUNC(CGameRules_IPointsForKill, *g_gamerules_addr, OnIPointsForKill, false);

	g_FFA_Patched = true;

	return true;
}

bool DM_Unpatch_FFA()
{
	if (!g_FFA_Patched)
	{
		return false;
	}

	if (!g_IsInGlobalShutdown)
	{
		SH_REMOVE_MANUALHOOK_STATICFUNC(CGameRules_IPointsForKill, *g_gamerules_addr, OnIPointsForKill, false);
	}

	DM_ApplyPatch(g_lagcomp_addr, g_lagcomp_offset, &g_lagcomp_restore, NULL);
	DM_ApplyPatch(g_takedmg_addr, g_takedmg_offset[0], &g_takedmg_restore[0], NULL);
#if SOURCE_ENGINE == SE_CSGO
	DM_ApplyPatch(g_takedmg_addr, g_takedmg_offset[1], &g_takedmg_restore[1], NULL);
#endif
	DM_ApplyPatch(g_domrev_addr, g_domrev_offset, &g_domrev_restore, NULL);

	g_FFA_Patched = false;

	return true;
}

bool DM_FFA_IsPatched()
{
	return g_FFA_Patched;
}

bool DM_FFA_IsPrepared()
{
	return g_FFA_Prepared;
}
