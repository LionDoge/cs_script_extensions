/**
 * vim: set ts=4 sw=4 tw=99 noet :
 * ======================================================
 * Metamod:Source Sample Plugin
 * Written by AlliedModders LLC.
 * ======================================================
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from 
 * the use of this software.
 *
 * This sample plugin is public domain.
 */

#ifndef _INCLUDE_METAMOD_SOURCE_PLUGIN_H_
#define _INCLUDE_METAMOD_SOURCE_PLUGIN_H_

#include <ISmmPlugin.h>
#include <igameevents.h>
#include <sh_vector.h>
#include "version_gen.h"
#include "addresses.h"
#include "steam/isteamuser.h"
#include "iserver.h"
#include "steam/steam_api_common.h"
#include "checktransmitinfo.h"
#include "scriptExtensions/csscript.h"

extern double g_flUniversalTime;
extern INetworkMessages* g_pNetworkMessages;
class MMSPlugin : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late);
	bool Unload(char *error, size_t maxlen);
	void AllPluginsLoaded();
public: //hooks
	void OnLevelInit( char const *pMapName,
				 char const *pMapEntities,
				 char const *pOldLevel,
				 char const *pLandmarkName,
				 bool loadGame,
				 bool background );
	void Hook_PostEvent(CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64* clients,
	                    INetworkMessageInternal* pEvent, const CNetMessage* pData, unsigned long nSize,
	                    NetChannelBufType_t bufType);
	void Hook_CheckTransmit(CCheckTransmitInfo** ppInfoList, int infoCount, CBitVec<16384>& unionTransmitEdicts, CBitVec<16384>&, const Entity2Networkable_t** pNetworkables, const uint16* pEntityIndicies, int nEntities);
	void OnLevelShutdown();
	void Hook_GameFrame( bool simulating, bool bFirstTick, bool bLastTick );
	void Hook_ClientActive( CPlayerSlot slot, bool bLoadGame, const char *pszName, uint64 xuid );
	void Hook_ClientDisconnect( CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid, const char *pszNetworkID );
	void Hook_ClientPutInServer( CPlayerSlot slot, char const *pszName, int type, uint64 xuid );
	void Hook_ClientSettingsChanged( CPlayerSlot slot );
	void Hook_OnClientConnected( CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, const char *pszAddress, bool bFakePlayer );
	bool Hook_ClientConnect( CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, bool unk1, CBufferString *pRejectReason );
	void Hook_ClientCommand( CPlayerSlot nSlot, const CCommand &_cmd );
	void Hook_SetGameSpawnGroupMgr(IGameSpawnGroupMgr* pSpawnGroupMgr);
	void Hook_StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession* pSession, const char* pszMapName);
	void Hook_DispatchConCommand(ConCommandRef cmd, const CCommandContext& context, const CCommand& command);
	void Hook_GameServerSteamAPIActivated();
	/* Returns a ifstream of a file relative to the game path (so game/csgo/) */
	std::ifstream OpenGameRelativeFile(std::string_view relativePath);
	bool RunScriptFromFile(CCSScript_EntityScript* script, std::string_view relativePath, bool savePath = false);
public:
	const char *GetAuthor() { return PLUGIN_AUTHOR; }
	const char *GetName() { return PLUGIN_DISPLAY_NAME; }
	const char *GetDescription() { return PLUGIN_DESCRIPTION; }
	const char *GetURL() { return PLUGIN_URL; }
	const char *GetLicense() { return PLUGIN_LICENSE; }
	const char *GetVersion() { return PLUGIN_FULL_VERSION; }
	const char *GetDate() { return __DATE__; }
	const char *GetLogTag() { return PLUGIN_LOGTAG; }
};

extern MMSPlugin g_ThisPlugin;

PLUGIN_GLOBALVARS();

#endif //_INCLUDE_METAMOD_SOURCE_PLUGIN_H_
