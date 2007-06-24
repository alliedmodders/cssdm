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

#ifndef _INCLUDE_CSSDM_HEADERS_H_
#define _INCLUDE_CSSDM_HEADERS_H_

#include <sm_platform.h>
#include <IBinTools.h>
#include "smsdk_ext.h"
#include <iplayerinfo.h>
#include <filesystem.h>

extern IBinTools *bintools;
extern IPlayerInfoManager *playerinfomngr;
extern IBaseFileSystem *basefilesystem;
extern IGameConfig *g_pDmConf;
extern CGlobalVars *gpGlobals;
extern IServerGameEnts *gameents;
extern IServerGameClients *gameclients;
extern IBotManager *botmanager;
extern bool g_IsRunning;
extern bool g_IsInGlobalShutdown;
extern ISourcePawnEngine *spengine;

#endif //_INCLUDE_CSSDM_HEADERS_H_
