#pragma once
#include <stdint.h>

class CustomPlayer {
public:
	CustomPlayer(int slot, bool isFake = false);
	uint64_t GetUnauthenticatedSteamId() const;
	void SetUnauthenticatedSteamId(uint64_t xuid);
	bool IsFakePlayer() const;
	void OnAuthenticated();
	bool IsAuthenticated();
	int GetPlayerSlot() const;
private:
	int m_slot;
	uint64_t m_unauthsteamId;
	bool m_bFakePlayer;
	bool m_bIsAuthenticated;
};
