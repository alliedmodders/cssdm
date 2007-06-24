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

#include "cssdm_weapons.h"
#include "cssdm_headers.h"
#include "sm_trie.h"
#include <sh_vector.h>

using namespace SourceHook;

Trie *g_pWeaponLookup = NULL;
CVector<dm_weapon_t *> g_Weapons;

dm_weapon_t *DM_FindWeapon(const char *name)
{
	dm_weapon_t *wp = NULL;
	if (!sm_trie_retrieve(g_pWeaponLookup, name,  (void **)&wp))
	{
		return NULL;
	}
	return wp;
}

dm_weapon_t *DM_GetWeapon(unsigned int index)
{
	if (index >= g_Weapons.size())
	{
		return NULL;
	}

	return g_Weapons[index];
}

char *DM_CopyString(const char *str)
{
	size_t length = strlen(str);
	char *ptr = new char[length+1];
	strcpy(ptr, str);
	return ptr;
}

bool DM_ParseWeapons(char *error, size_t maxlength)
{
	char path[PLATFORM_MAX_PATH];
	g_pSM->BuildPath(Path_Game, path, sizeof(path), "cfg/cssdm/cssdm.weapons.txt");

	KeyValues *kv = new KeyValues("Weapons");
	if (!kv->LoadFromFile(basefilesystem, path))
	{
		kv->deleteThis();
		snprintf(error, maxlength, "Could not load weapons file \"%s\"", error);
		return false;
	}

	KeyValues *weapons = kv->FindKey("cstrike", false);
	if (!weapons)
	{
		kv->deleteThis();
		snprintf(error, maxlength, "Could not find \"cstrike\" section in weapons file");
		return false;
	}

	g_pWeaponLookup = sm_trie_create();

	for (weapons = weapons->GetFirstTrueSubKey(); weapons != NULL; weapons = weapons->GetNextTrueSubKey())
	{
		dm_weapon_t *wp = new dm_weapon_t;
		wp->ammoType = -1;
		wp->maxCarry = -1;
		
		/* Deal with section name */
		char name[64];
		snprintf(name, sizeof(name), "weapon_%s", weapons->GetName());
		wp->classname = DM_CopyString(name);

		/* Deal with other strings */
		wp->display = DM_CopyString(weapons->GetString("name", weapons->GetName()));

		const char *type = weapons->GetString("type", "");
		wp->type = WeaponType_Invalid;
		if (strcmp(type, "primary") == 0)
		{
			wp->type = WeaponType_Primary;
		} else if (strcmp(type, "secondary") == 0) {
			wp->type = WeaponType_Secondary;
		} else if (strcmp(type, "grenade") == 0) {
			wp->type = WeaponType_Grenade;
		} else if (strcmp(type, "c4") == 0) {
			wp->type = WeaponType_C4;
		}

		if (wp->type != WeaponType_Invalid)
		{
			wp->id = (int)g_Weapons.size();
			sm_trie_insert(g_pWeaponLookup, weapons->GetName(), wp);
			g_Weapons.push_back(wp);
		}
	}

	kv->deleteThis();

	return true;
}

void DM_FreeWeapons()
{
	for (size_t i=0; i<g_Weapons.size(); i++)
	{
		dm_weapon_t *wp = g_Weapons[i];
		delete [] wp->classname;
		delete [] wp->display;
		delete wp;
	}
	g_Weapons.clear();

	if (g_pWeaponLookup)
	{
		sm_trie_destroy(g_pWeaponLookup);
		g_pWeaponLookup = NULL;
	}
}
