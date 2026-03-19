#pragma once
#include "playerslot.h"
#include <array>
#include <unordered_set>
#include "entityidentity.h"
#include "common.h"

struct CEntityIndexHashFunctor {
	std::size_t operator()(const CEntityIndex& idx) const {
		return std::hash<int>()(idx.Get());
	}
};

class CustomPlayer {
public:
	CustomPlayer(CPlayerSlot slot) : m_slot(slot) {}
	CPlayerSlot GetSlot() const { return m_slot; }
	void SetEntityTransmitBlocked(CEntityIndex entindex, bool state);
	void ClearEntityTransmitBlocks();
	const std::unordered_set<CEntityIndex, CEntityIndexHashFunctor>& GetBlockedEntityTransmits() const { return m_blockedEntityTransmits; }
private:
	CPlayerSlot m_slot;
	
	// there's likely a more efficient way to achieve efficient access in CheckTransmit hook
	// Likely won't be very noticeable in most cases anyways, but if you're up to the task, then feel free to contribute!
	std::unordered_set<CEntityIndex, CEntityIndexHashFunctor> m_blockedEntityTransmits;
};

class PlayerManager
{
public:
	void OnPlayerConnect(CPlayerSlot);
	void OnPlayerDisconnect(CPlayerSlot);

	void SetEntityTransmitBlocked(CPlayerSlot slot, CEntityIndex entindex, bool state);
	void ClearEntityTransmitBlocks(CPlayerSlot slot);
	void SetEntityTransmitBlockedForAll(CEntityIndex entindex, bool state);
	void ClearEntityTransmitBlocksForAll(CEntityIndex entindex);

	CustomPlayer* GetPlayer(CPlayerSlot slot);

private:
	std::array<CustomPlayer*, MAXPLAYERS> m_connectedPlayers;
	// Remember which ones we block for newly connected clients
	// We will apply this to new players
	// We don't do a full on override in CheckTransmit, cause there's possibility of setting transmit to chosen players after blocking for all others.
	std::unordered_set<CEntityIndex, CEntityIndexHashFunctor> m_blockedEntityTransmitsForAllClients;
};