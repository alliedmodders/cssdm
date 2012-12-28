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

#include <string.h>
#include "cssdm_weapons.h"
#include "cssdm_headers.h"
#include <sm_trie_tpl.h>
#include <sh_vector.h>

using namespace SourceHook;

KTrie<dm_weapon_t *> g_WeaponLookup;
CVector<dm_weapon_t *> g_Weapons;

char *DM_StringToLower(const char *str)
{
	size_t length = strlen(str);
	char *ptr = new char[length+1];

	for(size_t i = 0; i < length; i++)
	{
		if (str[i] >= 'A' && str[i] <= 'Z')
			ptr[i] = tolower(str[i]);
		else
			ptr[i] = str[i];
	}
	ptr[length] = '\0';

	return ptr;
}

dm_weapon_t *DM_FindWeapon(const char *name)
{
	dm_weapon_t **pWp;

	if ((pWp = g_WeaponLookup.retrieve(name)) == NULL)
	{
		return NULL;
	}

	return *pWp;
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
		snprintf(error, maxlength, "Could not load weapons file \"%s\"", path);
		return false;
	}

	const char *game = g_pSM->GetGameFolderName();
	KeyValues *weapons = kv->FindKey(game, false);
	if (!weapons)
	{
		kv->deleteThis();
		snprintf(error, maxlength, "Could not find \"%s\" section in weapons file", game);
		return false;
	}

	for (weapons = weapons->GetFirstTrueSubKey(); weapons != NULL; weapons = weapons->GetNextTrueSubKey())
	{
		dm_weapon_t *wp = new dm_weapon_t;

		/* Deal with section name */
		char name[64];
		char *pName = DM_StringToLower(weapons->GetName());
		snprintf(name, sizeof(name), "weapon_%s", pName);
		wp->classname = DM_CopyString(name);

		/* Deal with other strings */
		wp->display = DM_CopyString(weapons->GetString("name", pName));

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
			g_WeaponLookup.insert(pName, wp);
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
	g_WeaponLookup.clear();
}
