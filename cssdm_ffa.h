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

#ifndef _INCLUDE_CSSDM_FFA_H_
#define _INCLUDE_CSSDM_FFA_H_

bool DM_FFA_IsPatched();
bool DM_FFA_IsPrepared();

/* Internal */
bool DM_Prepare_FFA(char *error, size_t maxlength);
void DM_Shutdown_FFA();
bool DM_Unpatch_FFA();
bool DM_Patch_FFA();

#endif //_INCLUDE_CSSDM_FFA_H_
