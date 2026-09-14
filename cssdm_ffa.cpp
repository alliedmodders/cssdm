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

#include <cstdint>
#include "cssdm_headers.h"
#include "cssdm_utils.h"
#include "sm_platform.h"
#include "cssdm_includesdk.h"

// Don't patch anything on CSGO
#if SOURCE_ENGINE != SE_CSGO
#if METAMOD_PLAPI_VERSION < 18
SH_DECL_MANUALHOOK2(CGameRules_IPointsForKill, 62+EXTRA_VTBL_OFFSET, 0, 0, int, CBasePlayer *, CBasePlayer *);
#else
class CGameRules;
#endif

#if defined PLATFORM_64BITS
#define PLATFORM_ARCH_SUFFIX	"64"
#else
#define PLATFORM_ARCH_SUFFIX	""
#endif

#if defined PLATFORM_WINDOWS
#define PLATFORM_NAME	"Windows" PLATFORM_ARCH_SUFFIX
#elif defined PLATFORM_LINUX
#define PLATFORM_NAME	"Linux" PLATFORM_ARCH_SUFFIX
#elif defined __APPLE__
#define PLATFORM_NAME	"Mac" PLATFORM_ARCH_SUFFIX
#endif

bool g_FFA_Patched = false;
bool g_FFA_PointsHooked = false;
bool g_FFA_Prepared = false;

void *g_gamerules_addr = NULL;

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

#if METAMOD_PLAPI_VERSION < 18
int OnIPointsForKill(CBasePlayer *pl1, CBasePlayer *pl2)
#else
KHook::Return<int> OnIPointsForKill(CGameRules *gamerules, CBasePlayer *pl1, CBasePlayer *pl2)
#endif
{
	/* If we're hooked, FFA is always on. */
#if METAMOD_PLAPI_VERSION < 18
	RETURN_META_VALUE(MRES_SUPERCEDE, 1);
#else
	return { KHook::Action::Supersede, 1 };
#endif
}

#if METAMOD_PLAPI_VERSION >= 18
KHook::Virtual<CGameRules, int, CBasePlayer *, CBasePlayer *> Hook_IPointsForKill(62 + EXTRA_VTBL_OFFSET, nullptr, OnIPointsForKill);
#endif

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

	/* Load the GameRules pointer */
	if (!sdktools)
	{
		snprintf(error, maxlength, "SDKTools not loaded");
		return false;
	}
	/* Get the GameRules address later when map starts */

	/* Get the "IPointsForKill" offset */
	int offset;
	if (!g_pDmConf->GetOffset("IPointsForKill", &offset)
		|| !offset)
	{
		snprintf(error, maxlength, "Could not find IPointsForKills offset");
		return false;
	}
#if METAMOD_PLAPI_VERSION < 18
	SH_MANUALHOOK_RECONFIGURE(CGameRules_IPointsForKill, offset, 0, 0);
#else
	Hook_IPointsForKill.Configure(offset);
#endif

	g_FFA_Prepared = true;

	return true;
}

bool LoadGameRulesAddress()
{
	if (!sdktools)
	{
		g_pSM->LogError(myself, "SDKTools not loaded. FFA points hook will not work.");
		return false;
	}
	g_gamerules_addr = sdktools->GetGameRules();
	if (!g_gamerules_addr)
	{
		g_pSM->LogError(myself, "Could not find GameRules address. FFA points hook will not work.");
		return false;
	}
	return true;
}

bool DM_Patch_FFA()
{
	if (g_FFA_Patched || !g_FFA_Prepared)
	{
		return false;
	}

	DM_ApplyPatch(g_lagcomp_addr, g_lagcomp_offset, &g_lagcomp_patch, &g_lagcomp_restore);
	DM_ApplyPatch(g_takedmg_addr, g_takedmg_offset[0], &g_takedmg_patch[0], &g_takedmg_restore[0]);
	DM_ApplyPatch(g_domrev_addr, g_domrev_offset, &g_domrev_patch, &g_domrev_restore);

	// needs a new gamerules address on every map load
	if (!g_FFA_PointsHooked && LoadGameRulesAddress())
	{
#if METAMOD_PLAPI_VERSION < 18
		SH_ADD_MANUALHOOK_STATICFUNC(CGameRules_IPointsForKill, g_gamerules_addr, OnIPointsForKill, false);
#else
		Hook_IPointsForKill.Add(reinterpret_cast<CGameRules *>(g_gamerules_addr));
#endif
		g_FFA_PointsHooked = true;
	}

	g_FFA_Patched = true;

	return true;
}

bool DM_Unpatch_FFA()
{
	if (!g_FFA_Patched)
	{
		return false;
	}

	// g_gamerules_addr won't be null if we're already hooked
	if (!g_IsInGlobalShutdown && g_FFA_PointsHooked)
	{
#if METAMOD_PLAPI_VERSION < 18
		SH_REMOVE_MANUALHOOK_STATICFUNC(CGameRules_IPointsForKill, g_gamerules_addr, OnIPointsForKill, false);
#else
		Hook_IPointsForKill.Remove(reinterpret_cast<CGameRules *>(g_gamerules_addr));
#endif
		g_FFA_PointsHooked = false;
	}

	DM_ApplyPatch(g_lagcomp_addr, g_lagcomp_offset, &g_lagcomp_restore, NULL);
	DM_ApplyPatch(g_takedmg_addr, g_takedmg_offset[0], &g_takedmg_restore[0], NULL);
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
#endif
