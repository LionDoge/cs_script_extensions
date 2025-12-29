
#pragma once
//#include "globaltypes.h"
#include <entity/ccsweaponbase.h>
//#include <entity/ccsplayerpawn.h>

class CCSPlayerController;
class CCSPlayerPawn;

class CPlayerPawnComponent
{
public:
	DECLARE_SCHEMA_CLASS(CPlayerPawnComponent);

	SCHEMA_FIELD(CCSPlayerPawn*, __m_pChainEntity)

		CCSPlayerPawn* GetPawn() { return __m_pChainEntity; }
};

class CPlayerControllerComponent
{
public:
	DECLARE_SCHEMA_CLASS(CPlayerControllerComponent)

	SCHEMA_FIELD(CCSPlayerController*, __m_pChainEntity)

		CCSPlayerController* GetController() { return __m_pChainEntity; }
};

class CPlayer_WeaponServices : public CPlayerPawnComponent
{
public:
	DECLARE_SCHEMA_CLASS(CPlayer_WeaponServices);

	SCHEMA_FIELD_POINTER(CUtlVector<CHandle<CBasePlayerWeapon>>, m_hMyWeapons)
	SCHEMA_FIELD(CHandle<CBasePlayerWeapon>, m_hActiveWeapon)
};

class CCSPlayer_WeaponServices : public CPlayer_WeaponServices
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayer_WeaponServices);

	SCHEMA_FIELD(GameTime_t, m_flNextAttack)
		SCHEMA_FIELD(bool, m_bIsLookingAtWeapon)
		SCHEMA_FIELD(bool, m_bIsHoldingLookAtWeapon)

		SCHEMA_FIELD(CHandle<CBasePlayerWeapon>, m_hSavedWeapon)
		SCHEMA_FIELD(int32_t, m_nTimeToMelee)
		SCHEMA_FIELD(int32_t, m_nTimeToSecondary)
		SCHEMA_FIELD(int32_t, m_nTimeToPrimary)
		SCHEMA_FIELD(int32_t, m_nTimeToSniperRifle)
		SCHEMA_FIELD(bool, m_bIsBeingGivenItem)
		SCHEMA_FIELD(bool, m_bIsPickingUpItemWithUse)
		SCHEMA_FIELD(bool, m_bPickedUpWeapon)

};

enum class MedalRank_t : std::uint32_t
{
	MEDAL_RANK_NONE = 0x0,
	MEDAL_RANK_BRONZE = 0x1,
	MEDAL_RANK_SILVER = 0x2,
	MEDAL_RANK_GOLD = 0x3,
	MEDAL_RANK_COUNT = 0x4,
};

class CCSPlayerController_InventoryServices : public CPlayerControllerComponent {
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerController_InventoryServices)

	SCHEMA_FIELD(uint16_t, m_unMusicID)
	// m_rank[6]
	SCHEMA_FIELD_POINTER(MedalRank_t, m_rank)
};