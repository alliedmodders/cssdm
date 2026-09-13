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

#include <algorithm>
#include <unordered_map>
#include <vector>
#include "cssdm_weapons.h"
#include "cssdm_headers.h"

std::unordered_map<std::string, dm_weapon_t> g_WeaponLookup;
std::vector<dm_weapon_t> g_Weapons;

std::string DM_StringToLower(std::string string)
{
	std::transform(string.begin(), string.end(), string.begin(), [](const unsigned char c)
	{
		return static_cast<char>(std::tolower(c));
	});
	return string;
}

std::optional<std::reference_wrapper<dm_weapon_t>> DM_FindWeapon(std::string_view name)
{
	if (!g_WeaponLookup.contains(name.data()))
	{
		return std::nullopt;
	}

	return g_WeaponLookup[std::string(name)];
}

std::optional<std::reference_wrapper<dm_weapon_t>> DM_GetWeapon(unsigned int index)
{
	if (index >= g_Weapons.size())
	{
		return std::nullopt;
	}

	return g_Weapons[index];
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
		dm_weapon_t wp;

		/* Deal with section name */
		char name[64];
		std::string pName = DM_StringToLower(std::string(weapons->GetName()));

		snprintf(name, sizeof(name), "weapon_%s", pName.c_str());
		wp.classname = std::string(name);

		/* Deal with other strings */
		wp.display = std::string(weapons->GetString("name", pName.c_str()));

		const char *type = weapons->GetString("type", "");
		wp.type = WeaponType::WeaponType_Invalid;
		if (strcmp(type, "primary") == 0)
		{
			wp.type = WeaponType::WeaponType_Primary;
		} else if (strcmp(type, "secondary") == 0) {
			wp.type = WeaponType::WeaponType_Secondary;
		} else if (strcmp(type, "grenade") == 0) {
			wp.type = WeaponType::WeaponType_Grenade;
		} else if (strcmp(type, "c4") == 0) {
			wp.type = WeaponType::WeaponType_C4;
		}

		if (wp.type != WeaponType::WeaponType_Invalid)
		{
			wp.id = static_cast<int>(g_Weapons.size());
			g_WeaponLookup.insert({ pName.c_str(), wp });
			g_Weapons.push_back(wp);
		}
	}

	kv->deleteThis();

	return true;
}

void DM_FreeWeapons()
{
	g_Weapons.clear();
	g_WeaponLookup.clear();
}
