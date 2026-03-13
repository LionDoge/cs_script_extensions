#pragma once
#include "entitysystem.h"
#include "playermanager.h"

class CEntityListener : public IEntityListener
{
	void OnEntityCreated(CEntityInstance* pEntity) override {};
	void OnEntitySpawned(CEntityInstance* pEntity) override {};
	void OnEntityDeleted(CEntityInstance* pEntity) override;
	void OnEntityParentChanged(CEntityInstance* pEntity, CEntityInstance* pNewParent) override {};
};