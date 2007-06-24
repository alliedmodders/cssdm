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


#ifndef _INCLUDE_SOURCEMOD_SIMPLE_TRIE_H_
#define _INCLUDE_SOURCEMOD_SIMPLE_TRIE_H_

struct Trie;

Trie *sm_trie_create();
void sm_trie_destroy(Trie *trie);
bool sm_trie_insert(Trie *trie, const char *key, void *value);
bool sm_trie_retrieve(Trie *trie, const char *key, void **value);
bool sm_trie_delete(Trie *trie, const char *key);
void sm_trie_clear(Trie *trie);

#endif //_INCLUDE_SOURCEMOD_SIMPLE_TRIE_H_
