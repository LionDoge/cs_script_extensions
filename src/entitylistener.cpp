#include "entitylistener.h"

extern PlayerManager g_playerManager;

void CEntityListener::OnEntityDeleted(CEntityInstance* pEntity) {
	g_playerManager.SetEntityTransmitBlockedForAll(pEntity->GetEntityIndex(), false);
}