#include "CustomPlayer.h"

CustomPlayer::CustomPlayer(int slot, bool isFake)
	: m_slot(slot), m_unauthsteamId(0), m_bFakePlayer(isFake), m_bIsAuthenticated(false) {
}

uint64_t CustomPlayer::GetUnauthenticatedSteamId() const {
	return m_unauthsteamId;
}

void CustomPlayer::SetUnauthenticatedSteamId(uint64_t xuid) {
	m_unauthsteamId = xuid;
}

bool CustomPlayer::IsFakePlayer() const {
	return m_bFakePlayer;
}

void CustomPlayer::OnAuthenticated() {
	m_bIsAuthenticated = true;
}

bool CustomPlayer::IsAuthenticated() {
	return m_bIsAuthenticated;
}

inline int CustomPlayer::GetPlayerSlot() const {
	return m_slot;
}
