/**
 * =============================================================================
 * cs_script_extensions
 * Copyright (C) 2026 liondoge
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
 */

#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "ehandle.h"
#include "entity/ccsplayercontroller.h"

struct HudHintInfo {
	HudHintInfo() : targetSlot(-1), strMessage(""), endTime(0.0) {} // map requires default constructor, ugh.
	HudHintInfo(CPlayerSlot _slot, std::string _msg, double _endTime) 
		: targetSlot(_slot), strMessage(_msg), endTime(_endTime) {}
	CPlayerSlot targetSlot; // what player to send this to (userid)
	std::string strMessage; // message contents
	double endTime; // game time at which the message expires
};

class HudHintManager {
public:

	HudHintManager();
	void AddHintMessage(CPlayerSlot targetPlr, const std::string msg, double duration);
	void CancelHintMessage(CPlayerSlot targetPlr);
	void CancelAllHintMessages();
	void Update();

private:
	void Display(const HudHintInfo& info);
	std::unordered_map<int, HudHintInfo> hintMessages; // map of player slot to their active hint message, if any
};