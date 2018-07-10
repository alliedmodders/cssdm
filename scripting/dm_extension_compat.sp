#include <sourcemod>
#include <sdktools>
#include <sdkhooks>
#include <cstrike>
#include <cssdm>

//Engine Version/Native stuff
new EngineVersion:iEngine = Engine_Unknown;
//new bool:bGiveAmmoNative = false; //Enable when we compile against 1.6

//Forwards
new Handle:hOnStartup;
new Handle:hOnShutdown;
new Handle:hOnClientSpawned;
new Handle:hOnClientSpawnedPost;
new Handle:hOnClientDeath;
new Handle:hOnSetSpawnMethod;

//CVars
new Handle:hRagdollTime;
new Handle:hRespawnWait;
new Handle:hAllowC4;
new Handle:hEnabled;
new Handle:hFFAEnabled;
new Handle:hSpawnMethod;
new Handle:hRemoveDrops;

//Cvar values
new iRagdollTime;
new Float:fRespawnTime;
new bool:bAllowC4;
new bool:bRemoveDropped;

//Bools
new bool:bIsRunning = false;
new bool:bFFAEnabled = false;
new bool:bConfigRan = false;
new bool:bInRoundRestart = false;
new bool:bAllowFFA = false;

//Weapon Info
new iMaxWeapons;
new String:szWeaponNames[CSSDM_MAX_WEAPONS][64];
new String:szClassnames[CSSDM_MAX_WEAPONS][64];
new Handle:hWeaponIDTrie;
new DmWeaponType:iWeaponType[CSSDM_MAX_WEAPONS];

//Timers
new Handle:hRespawnTimers[MAXPLAYERS+1];

//Gameconf
new Handle:hGameConf;

//Bomb entity
new iBombEnt = -1;

//FFA Stuff
enum OperatingSystem
{
	OperatingSystem_Windows = 0,
	OperatingSystem_Linux,
	OperatingSystem_Mac
};

new OperatingSystem:iOS;

enum PatchType
{
	Patch_TakeDamage1 = 0,
	Patch_TakeDamage2,
	Patch_LagComp,
	Patch_CalcDom,
	Patch_IPointsForKills,
	Patch_Max
};

enum BytesType
{
	Bytes_Patch = 0,
	Bytes_Original,
	Bytes_Max
};

#define MAX_BYTES 10
new BytePatchesArray[Patch_Max][Bytes_Max][MAX_BYTES];
new Address:PatchAddressArray[Patch_Max];
new PatchOffsetsArray[Patch_Max];

public APLRes:AskPluginLoad2(Handle:myself, bool:late, String:error[], err_max)
{
	iEngine = GetEngineVersion();
	
	if(iEngine != Engine_CSGO && iEngine != Engine_CSS)
	{
		strcopy(error, err_max, "This plugin is only supported on CS");
		return APLRes_Failure;
	}
	
	hGameConf = LoadGameConfigFile("cssdm.games");
	if(hGameConf == INVALID_HANDLE)
	{
		strcopy(error, err_max, "Failed to load game config cssdm.games.txt");
		return APLRes_Failure;
	}
	
	//Prepare FFA stuff
	new offset = GameConfGetOffset(hGameConf, "g_pGameRules");
	
	if(offset == 0)
	{
		iOS = OperatingSystem_Mac;
	}
	else if(offset == 1)
	{
		iOS = OperatingSystem_Linux;
	}
	else
	{
		iOS = OperatingSystem_Windows;
	}

	CreateNative("DM_GetSpawnMethod", DMN_GetSpawnMethod);
	CreateNative("DM_GetWeaponID", DMN_GetWeaponID);
	CreateNative("DM_GetWeaponType", DMN_GetWeaponType);
	CreateNative("DM_StripBotItems", DMN_StripBotItems);
	CreateNative("DM_GetWeaponClassname", DMN_GetWeaponClassname);
	CreateNative("DM_GetClientWeapon", DMN_GetClientWeapon);
	CreateNative("DM_DropWeapon", DMN_DropWeapon);
	CreateNative("DM_GetWeaponName", DMN_GetWeaponName);
	CreateNative("DM_IsRunning", DMN_IsRunning);
	CreateNative("DM_GetSpawnWaitTime", DMN_GetSpawnWaitTime);
	CreateNative("DM_RespawnClient", DMN_RespawnClient);
	CreateNative("DM_IsClientAlive", DMN_IsClientAlive);
	CreateNative("DM_GiveClientAmmo", DMN_GiveAmmo);
	
	//New natives for 3.0
	CreateNative("DM_IsFFAEnabled", DMN_IsFFAEnabled);
	
	hWeaponIDTrie = CreateTrie();
	if(FileExists("cfg/cssdm/cssdm.weapons.txt"))
	{
		if(!ParseWeaponConfig("cfg/cssdm/cssdm.weapons.txt"))
		{
			strcopy(error, err_max, "Failed to parse weapon config cfg/cssdm/cssdm.weapons.txt");
			return APLRes_Failure;
		}
	}
	RegPluginLibrary("dm_extension_compat");
	return APLRes_Success;
}

public OnPluginStart()
{
	hOnStartup = CreateGlobalForward("DM_OnStartup", ET_Ignore);
	hOnShutdown = CreateGlobalForward("DM_OnShutdown", ET_Ignore);
	hOnClientSpawned = CreateGlobalForward("DM_OnClientSpawned", ET_Ignore, Param_Cell);
	hOnClientSpawnedPost = CreateGlobalForward("DM_OnClientPostSpawned", ET_Ignore, Param_Cell);
	hOnClientDeath = CreateGlobalForward("DM_OnClientDeath", ET_Ignore, Param_Cell);
	hOnSetSpawnMethod = CreateGlobalForward("DM_OnSetSpawnMethod", ET_Ignore, Param_String);
	
	CreateConVar("cssdm_version", CSSDM_VERSION, "CS:S DM Version", FCVAR_REPLICATED|FCVAR_NOTIFY|FCVAR_DONTRECORD);
	hRagdollTime = CreateConVar("cssdm_ragdoll_time", "2", "Sets ragdoll stay time", _, true, 0.0, true, 20.0);
	hRespawnWait = CreateConVar("cssdm_respawn_wait", "0.75", "Sets respawn wait time");
	hAllowC4 = CreateConVar("cssdm_allow_c4", "0", "Sets whether C4 is allowed");
	hEnabled = CreateConVar("cssdm_enabled", "1", "Sets whether CS:S DM is enabled", FCVAR_REPLICATED|FCVAR_NOTIFY);
	hFFAEnabled = CreateConVar("cssdm_ffa_enabled", "0", "Sets whether Free-For-All mode is enabled", FCVAR_REPLICATED|FCVAR_NOTIFY);
	hSpawnMethod = CreateConVar("cssdm_spawn_method", "preset", "Sets how and where players are spawned");
	hRemoveDrops = CreateConVar("cssdm_remove_drops", "1", "Sets whether dropped items are removed");
	
	HookConVarChange(hEnabled, ChangeStatus);
	HookConVarChange(hFFAEnabled, ChangeFFAStatus);
	HookConVarChange(hSpawnMethod, ChangeSpawnStatus);
	HookConVarChange(hRagdollTime, ChangeRagdollTime);
	HookConVarChange(hAllowC4, ChangeAllowC4);
	HookConVarChange(hRespawnWait, ConVarChange);
	HookConVarChange(hRemoveDrops, ConVarChange);
	
	bRemoveDropped = GetConVarBool(hRemoveDrops);
	fRespawnTime = GetConVarFloat(hRespawnWait);
	iRagdollTime = GetConVarInt(hRagdollTime);
	bAllowC4 = GetConVarBool(hAllowC4);
	
	bAllowFFA = PrepareFFA();
	
	if(!bAllowFFA)
	{
		LogError("Free-For-All will not work, Failed to get patch bytes");
	}
	
	AutoExecConfig(true, "cssdm", "cssdm");
	//bGiveAmmoNative = (GetFeatureStatus(FeatureType_Native, "GivePlayerAmmo") == FeatureStatus_Available)
}

public OnClientPutInServer(client)
{
	if(!bIsRunning)
		return;
	
	if(!bAllowC4)
	{
		SDKHook(client, SDKHook_WeaponCanUse, Hook_CanUse);
	}
	
	hRespawnTimers[client] = INVALID_HANDLE;
}

public OnClientDisconnect(client)
{
	KillRespawnTimer(client);
}

public OnConfigsExecuted()
{
	iBombEnt = -1;
	bInRoundRestart = false;
	if(ParseConfigs())
	{
		if(GetConVarBool(hEnabled) && Enable())
		{
			Call_StartForward(hOnStartup);
			Call_Finish();
		}
		if(GetConVarBool(hFFAEnabled))
		{
			EnableFFA();
		}
	}
}

public OnMapEnd()
{
	if(bConfigRan)
	{
		bConfigRan = false;
		if(bIsRunning)
		{
			Disable();
			Call_StartForward(hOnShutdown);
			Call_Finish();
		}
		if(bFFAEnabled)
		{
			DisableFFA();
		}
	}
}

public OnEntityCreated(entity, const String:classname[])
{
	if(StrEqual(classname, "weapon_c4"))
	{
		iBombEnt = entity;
	}
	else if(bIsRunning && (StrEqual(classname, "item_defuser") || StrEqual(classname, "item_cutters")))//CS:GO has both items, only hook if we are running
	{
		SDKHook(entity, SDKHook_SpawnPost, Hook_SpawnPost);
	}
}

public OnEntityDestroyed(entity)
{
	if(entity == iBombEnt)
	{
		iBombEnt = -1;
	}
}

public OnPluginEnd()
{
	//Unpatch if we patched.
	DisableFFA();
}
public Action:CS_OnCSWeaponDrop(client, weaponIndex)
{
	if(bRemoveDropped)
	{
		AcceptEntityInput(weaponIndex, "Kill");
	}
	return Plugin_Continue;
}

//Convar Hooks
public ConVarChange(Handle:convar, const String:oldValue[], const String:newValue[])
{
	if(hRespawnWait == convar)
	{
		fRespawnTime = GetConVarFloat(hRespawnWait);
	}
	else if(hRemoveDrops == convar)
	{
		bRemoveDropped = GetConVarBool(hRemoveDrops);
	}
}

public ChangeAllowC4(Handle:convar, const String:oldValue[], const String:newValue[])
{
	new bool:bNewVal = GetConVarBool(hAllowC4);
	if(!bAllowC4 && bNewVal)
	{
		for(new i = 1; i <= MaxClients; i++)
		{
			if(IsClientInGame(i))
			{
				SDKUnhook(i, SDKHook_WeaponCanUse, Hook_CanUse);
			}
		}
	}
	else if(bAllowC4 && !bNewVal)
	{
		for(new i = 1; i <= MaxClients; i++)
		{
			if(IsClientInGame(i))
			{
				SDKHook(i, SDKHook_WeaponCanUse, Hook_CanUse);
			}
		}
	}
	bAllowC4 = bNewVal
}

public ChangeRagdollTime(Handle:convar, const String:oldValue[], const String:newValue[])
{
	iRagdollTime = GetConVarInt(hRagdollTime);
}

public ChangeStatus(Handle:convar, const String:oldValue[], const String:newValue[])
{
	if(GetConVarBool(hEnabled))
	{
		Enable();
	}
	else
	{
		Disable();
	}
}

public ChangeFFAStatus(Handle:convar, const String:oldValue[], const String:newValue[])
{
	if(GetConVarBool(hFFAEnabled))
	{
		EnableFFA();
	}
	else
	{
		DisableFFA();
	}
}

public ChangeSpawnStatus(Handle:convar, const String:oldValue[], const String:newValue[])
{
	if(!StrEqual(oldValue, newValue))
	{
		Call_StartForward(hOnSetSpawnMethod);
		Call_PushString(newValue);
		Call_Finish();
	}
}

//Natives
public DMN_GetSpawnMethod(Handle:hPlugin, iNumParams)
{
	new String:szSpawnMethod[32];
	GetConVarString(hSpawnMethod, szSpawnMethod, sizeof(szSpawnMethod));
	SetNativeString(1, szSpawnMethod, GetNativeCell(2));
	return 1;
}

public DMN_GetWeaponID(Handle:hPlugin, iNumParams)
{
	new String:weaponName[64];
	new id;
	GetNativeString(1, weaponName, sizeof(weaponName));
	if(GetTrieValue(hWeaponIDTrie, weaponName, id))
	{
		return id;
	}
	return -1;
}

public DMN_GetWeaponType(Handle:hPlugin, iNumParams)
{
	new id = GetNativeCell(1);
	if(id < 0 || id >= iMaxWeapons)
	{
		return ThrowNativeError(SP_ERROR_NATIVE, "Invalid CS:S DM weapon id (%d)", id);
	}
	return _:iWeaponType[id];
}

public DMN_StripBotItems(Handle:hPlugin, iNumParams)
{
	static iMyWeaponsMax = 0;
	
	if(!iMyWeaponsMax)
	{
		if(iEngine == Engine_CSGO)
		{
			iMyWeaponsMax = 64;
		}
		else
		{
			iMyWeaponsMax = 32;
		}
	}
	
	new client = GetNativeCell(1);
	
	if(client <= 0 || client > MaxClients)
	{
		return ThrowNativeError(SP_ERROR_NATIVE, "Invalid client index %d", client);
	}
	if(!IsClientInGame(client))
	{
		return ThrowNativeError(SP_ERROR_NATIVE, "Client %d is not in game", client);
	}
	if(!IsFakeClient(client))
	{
		return ThrowNativeError(SP_ERROR_NATIVE, "Client %d is not a bot", client);
	}
	
	new weapon;
	for(new x = 0; x < iMyWeaponsMax; x++)
	{
		weapon = GetEntPropEnt(client, Prop_Send, "m_hMyWeapons", x);
		if(weapon && IsValidEdict(weapon))
		{
			DM_DropWeaponRemove(client, weapon);
		}
	}
	SetEntProp(client, Prop_Send, "m_bHasDefuser", 0);
	SetEntProp(client, Prop_Send, "m_ArmorValue", 0);
	SetEntProp(client, Prop_Send, "m_bHasHelmet", 0);
	
	return 1;
}

public DMN_GetWeaponClassname(Handle:hPlugin, iNumParams)
{
	new id = GetNativeCell(1);
	if(id < 0 || id >= iMaxWeapons)
	{
		return ThrowNativeError(SP_ERROR_NATIVE, "Invalid CS:S DM weapon id (%d)", id);
	}
	SetNativeString(2, szClassnames[id], GetNativeCell(3));
	
	return 1;
}

public DMN_GetClientWeapon(Handle:hPlugin, iNumParams)
{
	return GetPlayerWeaponSlot(GetNativeCell(1), GetNativeCell(2));
}

public DMN_DropWeapon(Handle:hPlugin, iNumParams)
{
	new weapon = GetNativeCell(2);
	new client = GetNativeCell(1);
	
	DM_DropWeaponRemove(client, weapon);
}

public DMN_GetWeaponName(Handle:hPlugin, iNumParams)
{
	new id = GetNativeCell(1);
	if(id < 0 || id >= iMaxWeapons)
	{
		return ThrowNativeError(SP_ERROR_NATIVE, "Invalid CS:S DM weapon id (%d)", id);
	}
	SetNativeString(2, szWeaponNames[id], GetNativeCell(3));
	
	return 1;
}

public DMN_IsRunning(Handle:hPlugin, iNumParams)
{
	return bIsRunning;
}

public DMN_GetSpawnWaitTime(Handle:hPlugin, iNumParams)
{
	return _:GetConVarFloat(hRespawnWait);
}

public DMN_RespawnClient(Handle:hPlugin, iNumParams)
{
	CS_RespawnPlayer(GetNativeCell(1));
	return 1;
}

public DMN_IsClientAlive(Handle:hPlugin, iNumParams)
{
	return IsPlayerAlive(GetNativeCell(1));
}

public DMN_GiveAmmo(Handle:hPlugin, iNumParams)
{
	static Handle:hGiveAmmo = INVALID_HANDLE;
	if(!hGiveAmmo)
	{
		StartPrepSDKCall(SDKCall_Entity);
		PrepSDKCall_SetFromConf(hGameConf, SDKConf_Virtual, "GiveAmmo");
		PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
		PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
		PrepSDKCall_AddParameter(SDKType_Bool, SDKPass_Plain);
		PrepSDKCall_SetReturnInfo(SDKType_PlainOldData, SDKPass_Plain);
		
		hGiveAmmo = EndPrepSDKCall();
		
		if(!hGiveAmmo)
		{
			return ThrowNativeError(SP_ERROR_NATIVE, "Failed to create GiveAmmo SDKCall");
		}
	}
	
	return SDKCall(hGiveAmmo, GetNativeCell(1), GetNativeCell(3), GetNativeCell(2), GetNativeCell(4)? true : false);
}

public DMN_IsFFAEnabled(Handle:hPlugin, iNumParams)
{
	return bFFAEnabled;
}

//Hook callbacks
public Action:Hook_CanUse(client, weapon)
{
	if(iBombEnt == -1 || weapon != iBombEnt || bAllowC4)
	{
		return Plugin_Continue;
	}
	
	//Dont ever allow the bomb!
	return Plugin_Handled;
}

public Hook_SpawnPost(entity)
{
	//Kill defusers
	AcceptEntityInput(entity, "Kill");
}

public Action:OnJoinClass(client, const String:command[], args) 
{
	//Respawn him next frame if he hasnt already
	if(client && IsClientInGame(client))
	{
		hRespawnTimers[client] = CreateTimer(0.1, TimeRespawnPlayer, GetClientSerial(client));
	}
	return Plugin_Continue;
}

public Action:Event_PlayerDeath(Handle:event, const String:name[], bool:dontBroadcast)
{
	if(!bIsRunning)
		return Plugin_Continue;
	
	new client = GetClientOfUserId(GetEventInt(event, "userid"));
	
	if(client && IsClientInGame(client))
	{
		SetEntProp(client, Prop_Send, "m_bHasDefuser", 0);
		
		Call_StartForward(hOnClientDeath);
		Call_PushCell(client);
		
		new Action:res = Plugin_Continue;
		Call_Finish(res);
		
		if(iRagdollTime <= 20 && iRagdollTime >= 0)
		{
			new ragdoll = GetEntPropEnt(client, Prop_Send, "m_hRagdoll");
			
			if(ragdoll && IsValidEntity(ragdoll))
			{
				if(iRagdollTime == 0)
				{
					AcceptEntityInput(ragdoll, "Kill");
				}
				else
				{
					new ref = EntIndexToEntRef(ragdoll);
					if(ref != INVALID_ENT_REFERENCE)
					{
						CreateTimer(float(iRagdollTime), TimeRagdollRemove, ref, TIMER_FLAG_NO_MAPCHANGE);
					}
				}
			}
		}
		
		if(res == Plugin_Continue)
		{
			hRespawnTimers[client] = CreateTimer(fRespawnTime, TimeRespawnPlayer, GetClientSerial(client));
		}
	}
	
	return Plugin_Continue;
}

public Action:Event_PlayerSpawn(Handle:event, const String:name[], bool:dontBroadcast)
{
	new client = GetClientOfUserId(GetEventInt(event, "userid"));
	
	if(client <= 0 || client > MaxClients || !IsClientInGame(client) || GetClientTeam(client) <= CS_TEAM_SPECTATOR)
	{
		return Plugin_Continue;
	}
	
	Call_StartForward(hOnClientSpawned);
	Call_PushCell(client);
	Call_Finish();
	Call_StartForward(hOnClientSpawnedPost);
	Call_PushCell(client);
	Call_Finish();
	
	return Plugin_Continue;
}

public Action:Event_RoundStart(Handle:event, const String:name[], bool:dontBroadcast)
{
	bInRoundRestart = false;
	
	return Plugin_Continue;
}

public Action:Event_RoundEnd(Handle:event, const String:name[], bool:dontBroadcast)
{
	bInRoundRestart = true;
	
	for(new i = 1; i <= MaxClients; i++)
	{
		KillRespawnTimer(i);
	}
	
	return Plugin_Continue;
}

//Timer callbacks
public Action:TimeRespawnPlayer(Handle:timer, any:serial)
{
	new client = GetClientFromSerial(serial);
	
	if(client && IsClientInGame(client))
	{
		if(bIsRunning && !bInRoundRestart)
		{
			CS_RespawnPlayer(client);
		}
		hRespawnTimers[client] = INVALID_HANDLE;
	}
	return Plugin_Continue;
}

public Action:TimeRagdollRemove(Handle:timer, any:ref)
{
	new index = EntRefToEntIndex(ref);
	if(bIsRunning && index != INVALID_ENT_REFERENCE)
	{
		AcceptEntityInput(index, "Kill");
	}
	return Plugin_Continue;
}

//Private functions
GetCurrentMapEx(String:map[], size)
{
	GetCurrentMap(map, size);
	
	new index = -1;
	for(new i = 0; i < strlen(map); i++)
	{
		if(StrContains(map[i], "/") != -1 || StrContains(map[i], "\\") != -1)
		{
			if(i != strlen(map) - 1)
				index = i;
		}
		else
		{
			break;
		}
	}
	strcopy(map, size, map[index+1]);
}

bool:ParseWeaponConfig(const String:szWeaponCfg[])
{
	static String:game[32] = "";
	
	if(strlen(game) <= 1)
	{
		GetGameFolderName(game, sizeof(game));
	}
	
	new Handle:kv = CreateKeyValues("Weapons");
	
	if(!FileToKeyValues(kv, szWeaponCfg))
	{
		CloseHandle(kv);
		return false;
	}
	
	if(!KvJumpToKey(kv, game))
	{
		CloseHandle(kv);
		return false;
	}
	
	if(!KvGotoFirstSubKey(kv))
	{
		CloseHandle(kv);
		return false;
	}
	
	new String:name[64];
	new String:type[32];
	
	iMaxWeapons = 0;
	
	do
	{
		KvGetSectionName(kv, name, sizeof(name));
		StringToLower(name);
		SetTrieValue(hWeaponIDTrie, name, iMaxWeapons);
		Format(szClassnames[iMaxWeapons], sizeof(szClassnames[]), "weapon_%s", name);
		KvGetString(kv, "name", szWeaponNames[iMaxWeapons], sizeof(szWeaponNames[]), name);
		KvGetString(kv, "type", type, sizeof(type), "");
		
		iWeaponType[iMaxWeapons] = DmWeapon_Invalid;
		
		if(StrEqual("primary", type))
		{
			iWeaponType[iMaxWeapons] = DmWeapon_Primary;
		}
		else if(StrEqual("secondary", type))
		{
			iWeaponType[iMaxWeapons] = DmWeapon_Secondary;
		}
		else if(StrEqual("grenade", type))
		{
			iWeaponType[iMaxWeapons] = DmWeapon_Grenade;
		}
		else if(StrEqual("c4", type))
		{
			iWeaponType[iMaxWeapons] = DmWeapon_C4;
		}
		
		if(iWeaponType[iMaxWeapons] != DmWeapon_Invalid)
		{
			iMaxWeapons++;
		}
		else
		{
			szClassnames[iMaxWeapons] = "";
			szWeaponNames[iMaxWeapons] = ""
			RemoveFromTrie(hWeaponIDTrie, name);
		}
		
	}while (KvGotoNextKey(kv));
	
	CloseHandle(kv);
	return true;
}

bool:ParseConfigs()
{
	if(bConfigRan)
		return true;
	
	//Execute the map sepcific file if it exists
	new String:buffer[PLATFORM_MAX_PATH];
	new String:map[64];
	
	GetCurrentMapEx(map, sizeof(map));
	
	Format(buffer, sizeof(buffer), "exec cssdm/maps/%s.cssdm.cfg", map);
	
	bConfigRan = true;
	return true;
}

HookEvents()
{
	HookEvent("player_death", Event_PlayerDeath);
	HookEvent("player_spawn", Event_PlayerSpawn);
	HookEvent("round_start", Event_RoundStart);
	HookEvent("round_end", Event_RoundEnd);
	AddCommandListener(OnJoinClass, "joinclass");
	
	if(!bAllowC4)
	{
		for(new i = 1; i <= MaxClients; i++)
		{
			if(IsClientInGame(i))
			{
				SDKHook(i, SDKHook_WeaponCanUse, Hook_CanUse);
			}
		}
	}
}

UnhookEvents()
{
	UnhookEvent("player_death", Event_PlayerDeath);
	UnhookEvent("player_spawn", Event_PlayerSpawn);
	UnhookEvent("round_start", Event_RoundStart);
	UnhookEvent("round_end", Event_RoundEnd);
	RemoveCommandListener(OnJoinClass, "joinclass");
	
	if(!bAllowC4)
	{
		for(new i = 1; i <= MaxClients; i++)
		{
			if(IsClientInGame(i))
			{
				SDKUnhook(i, SDKHook_WeaponCanUse, Hook_CanUse);
			}
		}
	}
}

bool:Enable()
{
	//Dont return true more than once since we only want the forward called once
	if(bIsRunning || !bConfigRan)
	{
		return false;
	}
	
	HookEvents();
	bIsRunning = true;
	return true;
}
Disable()
{
	if(!bIsRunning)
	{
		return;
	}
	
	//Clear Timers
	for(new i = 1; i <= MaxClients; i++)
	{
		KillRespawnTimer(i);
	}
	
	UnhookEvents();
	bIsRunning = false;
}

EnableFFA()
{
	if(!bAllowFFA || !bIsRunning || bFFAEnabled)
		return;	
	
	for(new i = 0; i < _:Patch_Max; i++)
	{
		if(!CheckIfShouldPatch(LoadFromAddress(PatchAddressArray[i]+Address:(PatchOffsetsArray[i]), NumberType_Int8)))
		{
			LogError("Failed to enable FFA. Failed to find valid byte to patch for PatchType (%i)", i);
			return;
		}
	}
	
	new byte = 0;
	
	for(new i = 0; i < _:Patch_Max; i++)
	{
		byte = 0;
		
		if(iEngine == Engine_CSS && i == _:Patch_TakeDamage2)
			continue;
		
		while(BytePatchesArray[i][Bytes_Patch][byte] != -1)
		{
			if(BytePatchesArray[i][Bytes_Original][byte] == -1)//Save the bytes if its our first time
			{
				BytePatchesArray[i][Bytes_Original][byte] = LoadFromAddress(PatchAddressArray[i]+Address:(byte+PatchOffsetsArray[i]), NumberType_Int8);
			}
			StoreToAddress(PatchAddressArray[i]+Address:(byte+PatchOffsetsArray[i]), BytePatchesArray[i][Bytes_Patch][byte], NumberType_Int8);
			byte++;
		}
	}

	bFFAEnabled = true;
}

DisableFFA()
{
	if(!bFFAEnabled)
		return;
	
	new byte = 0;
	
	for(new i = 0; i < _:Patch_Max; i++)
	{
		byte = 0;
		
		if(iEngine == Engine_CSS && i == _:Patch_TakeDamage2)
			continue;
		
		while(BytePatchesArray[i][Bytes_Original][byte] != -1)
		{
			StoreToAddress(PatchAddressArray[i]+Address:(byte+PatchOffsetsArray[i]), BytePatchesArray[i][Bytes_Original][byte], NumberType_Int8);
			byte++;
		}
	}
	
	bFFAEnabled = false;
}

bool:PrepareFFA()
{
	for(new i = 0; i < MAX_BYTES; i++)
	{
		for(new x = 0; x < _:Patch_Max; x++)
		{
			for(new j = 0; j < _:Bytes_Max; j++)
			{
				BytePatchesArray[x][j][i] = -1;
			}
		}
	}
	
	new String:szKey[32];
	new String:szTakeDmg1[32];
	new String:szTakeDmg2[32];
	new String:szCalcDom[32];
	new String:szLagComp[32];
	new String:szIPoints[32];
	new String:szOS[10];
	
	if(iOS == OperatingSystem_Linux)
	{
		strcopy(szOS, sizeof(szOS), "Linux");
	}
	else if(iOS == OperatingSystem_Mac)
	{
		strcopy(szOS, sizeof(szOS), "Mac");
	}
	else
	{
		strcopy(szOS, sizeof(szOS), "Windows");
	}

	
	Format(szTakeDmg1, sizeof(szTakeDmg1), "TakeDmgPatch1_%s", szOS);
	Format(szTakeDmg2, sizeof(szTakeDmg2), "TakeDmgPatch2_%s", szOS);
	Format(szCalcDom, sizeof(szCalcDom), "CalcDomRevPatch_%s", szOS);
	Format(szLagComp, sizeof(szLagComp), "LagCompPatch_%s", szOS);
	Format(szIPoints, sizeof(szIPoints), "IPointsForKillPatch_%s", szOS);
	
	if(!GameConfGetKeyValue(hGameConf, szTakeDmg1, szKey, sizeof(szKey)) || !GetBytes(szKey, BytePatchesArray[Patch_TakeDamage1][Bytes_Patch]))
	{
		LogError("Failed to get %s", szTakeDmg1);
		return false;
	}
	
	if(!GameConfGetKeyValue(hGameConf, szTakeDmg2, szKey, sizeof(szKey)) || !GetBytes(szKey, BytePatchesArray[Patch_TakeDamage2][Bytes_Patch]))
	{
		LogError("Failed to get %s", szTakeDmg2);
		return false;
	}
	
	if(!GameConfGetKeyValue(hGameConf, szCalcDom, szKey, sizeof(szKey)) || !GetBytes(szKey, BytePatchesArray[Patch_CalcDom][Bytes_Patch]))
	{
		LogError("Failed to get %s", szCalcDom);
		return false;
	}
	
	if(!GameConfGetKeyValue(hGameConf, szLagComp, szKey, sizeof(szKey)) || !GetBytes(szKey, BytePatchesArray[Patch_LagComp][Bytes_Patch]))
	{
		LogError("Failed to get %s", szLagComp);
		return false;
	}
	
	if(!GameConfGetKeyValue(hGameConf, szIPoints, szKey, sizeof(szKey)) || !GetBytes(szKey, BytePatchesArray[Patch_IPointsForKills][Bytes_Patch]))
	{
		LogError("Failed to get %s", szIPoints);
		return false;
	}
	
	if(!(Address:PatchAddressArray[Patch_TakeDamage1] = Address:PatchAddressArray[Patch_TakeDamage2] = GameConfGetAddress(hGameConf, "TakeDamage_Addr")))
	{
		LogError("Failed to locate TakeDamage Address");
		return false;
	}
	if(!(Address:PatchAddressArray[Patch_CalcDom] = GameConfGetAddress(hGameConf, "CalcDominationAndRevenge_Addr")))
	{
		LogError("Failed to locate CalcDominationAndRevenge Address");
		return false;
	}
	if(!(Address:PatchAddressArray[Patch_LagComp] = GameConfGetAddress(hGameConf, "WantsLagComp_Addr")))
	{
		LogError("Failed to locate WantsLagComp Address");
		return false;
	}
	if(!(Address:PatchAddressArray[Patch_IPointsForKills] = GameConfGetAddress(hGameConf, "IPointsForKill_Addr")))
	{
		LogError("Failed to locate IPointsForKill Address");
		return false;
	}
	
	if((PatchOffsetsArray[Patch_TakeDamage1] = GameConfGetOffset(hGameConf, "TakeDmgPatch1")) == -1)
	{
		LogError("Failed to locate TakeDmgPatch1 patch offset");
		return false;
	}
	if((PatchOffsetsArray[Patch_TakeDamage2] = GameConfGetOffset(hGameConf, "TakeDmgPatch2")) == -1)
	{
		LogError("Failed to locate TakeDmgPatch2 patch offset");
		return false;
	}
	if((PatchOffsetsArray[Patch_LagComp] = GameConfGetOffset(hGameConf, "LagCompPatch")) == -1)
	{
		LogError("Failed to locate WantsLagComp patch offset");
		return false;
	}
	if((PatchOffsetsArray[Patch_CalcDom] = GameConfGetOffset(hGameConf, "CalcDomRevPatch")) == -1)
	{
		LogError("Failed to locate CalcDominationAndRevenge patch offset");
		return false;
	}
	if((PatchOffsetsArray[Patch_IPointsForKills] = GameConfGetOffset(hGameConf, "IPointsForKillPatch")) == -1)
	{
		LogError("Failed to locate IPointsForKill patch offset");
		return false;
	}

	return true;
}

bool:GetBytes(const String:szBytes[], iBytes[MAX_BYTES])
{
	new max = (strlen(szBytes)/4);
	
	if(max > MAX_BYTES || max < 1)
		return false;
	
	new x = 0;
	for(new i = 0; i < max; i++)
	{
		if(strncmp(szBytes[i*4], "\\x90", 4) == 0)
		{
			iBytes[x] = 0x90;
		}
		else if(strncmp(szBytes[i*4], "\\xEB", 4) == 0)
		{
			iBytes[x] = 0xEB;
		}
		else if(strncmp(szBytes[i*4], "\\xE9", 4) == 0)
		{
			iBytes[x] = 0xE9;
		}
		else
		{
			return false;
		}
		x++;
	}
	
	return true;
}

bool:CheckIfShouldPatch(iByte)
{
	if(iByte == 0x74 || iByte == 0x75 || iByte == 0x0F)
		return true;
	
	return false;
}
DM_DropWeaponRemove(client, weapon)
{
	CS_DropWeapon(client, weapon, true);
	AcceptEntityInput(weapon, "Kill");
}

KillRespawnTimer(client)
{
	if(hRespawnTimers[client])
	{
		KillTimer(hRespawnTimers[client]);
		hRespawnTimers[client] = INVALID_HANDLE;
	}
}

StringToLower(String:buffer[])
{
	for(new i = 0; i < strlen(buffer); i++)
	{
		buffer[i] = CharToLower(buffer[i]);
	}
}