/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023 Source2ZE
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * Modified by liondoge on 06.03.2026
 * Changes: Renamed log tag.
 */

#pragma once

#include "platform.h"
#include "dbg.h"

// #define USE_DEBUG_CONSOLE

#define CS_TEAM_NONE        0
#define CS_TEAM_SPECTATOR   1
#define CS_TEAM_T           2
#define CS_TEAM_CT          3

#define HUD_PRINTNOTIFY		1
#define HUD_PRINTCONSOLE	2
#define HUD_PRINTTALK		3
#define HUD_PRINTCENTER		4
#define HUD_PRINTALERT		6

#define MAXPLAYERS 64

#ifdef _WIN32
#define ROOTBIN "/bin/win64/"
#define GAMEBIN "/csgo/bin/win64/"
#else
#define ROOTBIN "/bin/linuxsteamrt64/"
#define GAMEBIN "/csgo/bin/linuxsteamrt64/"
#endif

// CONVAR_TODO
// Need to replace with actual cvars once available in SDK
#define FAKE_CVAR(name, description, variable_name, variable_type, variable_type_format, variable_default, protect)		\
	CON_COMMAND_F(name, description, FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY | (protect ? FCVAR_PROTECTED : FCVAR_NONE))	\
	{																													\
		if (args.ArgC() < 2)																							\
			Msg("%s " #variable_type_format "\n", args[0], variable_name);												\
		else																											\
			variable_name = V_StringTo##variable_type(args[1], variable_default);										\
	}

#define FAKE_INT_CVAR(name, description, variable_name, variable_default, protect)										\
	FAKE_CVAR(name, description, variable_name, Int32, %i, variable_default, protect)

#define FAKE_BOOL_CVAR(name, description, variable_name, variable_default, protect)										\
	FAKE_CVAR(name, description, variable_name, Bool, %i, variable_default, protect)

#define FAKE_FLOAT_CVAR(name, description, variable_name, variable_default, protect)									\
	FAKE_CVAR(name, description, variable_name, Float32, %f, variable_default, protect)

// assumes std::string variable
#define FAKE_STRING_CVAR(name, description, variable_name, protect)														\
	CON_COMMAND_F(name, description, FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY | (protect ? FCVAR_PROTECTED : FCVAR_NONE))	\
	{																													\
		if (args.ArgC() < 2)																							\
			Msg("%s %s\n", args[0], variable_name.c_str());																\
		else																											\
			variable_name = args[1];																					\
	}

inline void MsgCrit(const tchar* pMsg, ...) {
	Msg("[cs_script_ext] [CRIT] %s", pMsg);
}

// Would be cool to have in SDK, but I'll just put it there
#define Log_Debug( Channel, /* [Color], Message, */ ... ) InternalMsg( Channel, LS_DETAILED, /* [Color], Message, */ ##__VA_ARGS__ )