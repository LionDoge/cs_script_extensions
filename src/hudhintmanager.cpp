#include "hudhintmanager.h"
#include "igameevents.h"

extern double g_flUniversalTime;
extern IGameEventManager2* g_gameEventManager;

HudHintManager::HudHintManager()
	: hintMessages()
{}

void HudHintManager::AddHintMessage(CPlayerSlot targetSlot, const std::string msg, double duration)
{
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
		return;
	}
	else
	{
		CancelHintMessage(targetSlot); // cancel any existing message for this player
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
