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

#include "hudhintmanager.h"
#include "igameevents.h"

extern double g_flUniversalTime;
extern IGameEventManager2* g_gameEventManager;

HudHintManager::HudHintManager()
	: hintMessages()
{}

void HudHintManager::AddHintMessage(CPlayerSlot targetSlot, const std::string& msg, double duration)
{
	// cancel any existing message for this player
	CancelHintMessage(targetSlot);
	// if it's a one-off then do it directly to save time.
	if (duration <= 0.0)
	{
		auto event = g_gameEventManager->CreateEvent("show_survival_respawn_status", true);
		if (event)
		{
			event->SetInt("duration", 1);
			event->SetInt("userid", targetSlot.Get());
			event->SetString("loc_token", msg.c_str());

			g_gameEventManager->FireEvent(event);
		}
	}
	else
	{
		HudHintInfo info(targetSlot, msg, g_flUniversalTime + duration);
		hintMessages[targetSlot.Get()] = info;
	}
}

void HudHintManager::CancelHintMessage(CPlayerSlot targetSlot)
{
	hintMessages.erase(targetSlot.Get());
}

void HudHintManager::CancelAllHintMessages()
{
	hintMessages.clear();
}

void HudHintManager::Update()
{
	for (auto it = hintMessages.begin(); it != hintMessages.end(); )
	{
		if (g_flUniversalTime >= it->second.endTime)
			it = hintMessages.erase(it);
		else
		{
			Display(it->second);
			++it;
		}
	}
}

void HudHintManager::Display(const HudHintInfo& info)
{
	auto event = g_gameEventManager->CreateEvent("show_survival_respawn_status", true);
	if (event)
	{
		event->SetInt("duration", 1);
		event->SetInt("userid", info.targetSlot.Get());
		event->SetString("loc_token", info.strMessage.c_str());

		g_gameEventManager->FireEvent(event);
	}
}
