#include "playermanager.h"

void PlayerManager::OnPlayerConnect(CPlayerSlot slot)
{
	m_connectedPlayers[slot.Get()] = new CustomPlayer(slot);
}

void PlayerManager::OnPlayerDisconnect(CPlayerSlot slot)
{
	auto plr = m_connectedPlayers[slot.Get()];
	delete plr;
	m_connectedPlayers[slot.Get()] = nullptr;
}

void PlayerManager::SetEntityTransmitBlocked(CPlayerSlot slot, CEntityIndex entindex, bool state)
{
	auto plr = GetPlayer(slot);
	if(plr)
		plr->SetEntityTransmitBlocked(entindex, state);
}

void PlayerManager::ClearEntityTransmitBlocks(CPlayerSlot slot)
{
	auto plr = GetPlayer(slot);
	if(plr)
		plr->ClearEntityTransmitBlocks();
}

void PlayerManager::SetEntityTransmitBlockedForAll(CEntityIndex entindex, bool state)
{
	for (auto& player : m_connectedPlayers)
	{
		if (!player)
			continue;
		 player->SetEntityTransmitBlocked(entindex, state);
	}
}

CustomPlayer* PlayerManager::GetPlayer(CPlayerSlot slot)
{
	return m_connectedPlayers[slot.Get()];
}

void CustomPlayer::SetEntityTransmitBlocked(CEntityIndex entindex, bool state)
{
	if (!state)
		m_blockedEntityTransmits.erase(entindex);
	else
		m_blockedEntityTransmits.insert(entindex);
}

void CustomPlayer::ClearEntityTransmitBlocks()
{
	m_blockedEntityTransmits.clear();
}

