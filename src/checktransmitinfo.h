#pragma once
#include "bitvec.h"
#include "utlleanvector.h"
#include "playerslot.h"
#include "entityidentity.h"

/* https://github.com/Wend4r/sourcesdk/blob/ea1dcfd4241a9e4977863ca08f542f43eb2c182b/public/iservernetworkable.h */

struct vis_info_t
{
	uint32 m_uVisBitsBufSize;
	SpawnGroupHandle_t m_SpawnGroupHandle;
	CBitVec<4096> m_VisBits;
};

class CCheckTransmitInfoCustom
{
public:
	CBitVec<MAX_EDICTS>* m_pTransmitEntity;	// entity n is already marked for transmission
	CBitVec<MAX_EDICTS>* m_pTransmitNonPlayers; 
	CBitVec<MAX_EDICTS>* m_pUnkBitVec2;
	CBitVec<MAX_EDICTS>* m_pUnkBitVec3;
	CBitVec<MAX_EDICTS>* m_pTransmitAlways; // entity n is always send even if not in PVS (HLTV and Replay only)
	CUtlLeanVector<CPlayerSlot> m_vecTargetSlots;
	vis_info_t m_VisInfo;
	CPlayerSlot m_nPlayerSlot;
	bool m_bFullUpdate = false;
};