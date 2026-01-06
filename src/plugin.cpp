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

#include "v8.h"
#include <stdio.h>
#include "plugin.h"

#include <fstream>
#include <sstream>
#include "entitysystem.h"
#include "schema.h"
#include "protobuf/generated/networkbasetypes.pb.h"
#include "protobuf/generated/usermessages.pb.h"
#include "module.h"
#include "funchook.h"
#include "csscript.h"
#include "hudhintmanager.h"
#include "schemasystem/schemasystem.h"
#include "igameeventsystem.h"
#include "recipientfilters.h"
#include "vprof.h"
#include "serversideclient.h"
#include "scriptextensions.h"
#include "v8callbacks.h"
#include "gameconfig.h"
#include "ctimer.h"
#include "safetyhook.hpp"
#include "convar.h"
#include "CustomPlayer.h"
#include "entity.h"
#include "scriptclasses/schemaobject.h"
#include "sigutils.h"
#include "iloopmode.h"
#include "scriptExtensions/userMessagesScriptExt.h"
#include "scriptExtensions/userMessageInfo.h"
#include "globalsymbol.h"

SH_DECL_HOOK3_void(IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool, bool, bool);
SH_DECL_HOOK4_void(IServerGameClients, ClientActive, SH_NOATTRIB, 0, CPlayerSlot, bool, const char *, uint64);
SH_DECL_HOOK5_void(IServerGameClients, ClientDisconnect, SH_NOATTRIB, 0, CPlayerSlot, ENetworkDisconnectionReason, const char *, uint64, const char *);
SH_DECL_HOOK4_void(IServerGameClients, ClientPutInServer, SH_NOATTRIB, 0, CPlayerSlot, char const *, int, uint64);
SH_DECL_HOOK1_void(IServerGameClients, ClientSettingsChanged, SH_NOATTRIB, 0, CPlayerSlot );
SH_DECL_HOOK6_void(IServerGameClients, OnClientConnected, SH_NOATTRIB, 0, CPlayerSlot, const char*, uint64, const char *, const char *, bool);
SH_DECL_HOOK6(IServerGameClients, ClientConnect, SH_NOATTRIB, 0, bool, CPlayerSlot, const char*, uint64, const char *, bool, CBufferString *);
SH_DECL_HOOK2(IGameEventManager2, FireEvent, SH_NOATTRIB, 0, bool, IGameEvent *, bool);
SH_DECL_HOOK2(IGameEventManager2, LoadEventsFromFile, SH_NOATTRIB, 0, int, const char*, bool);
SH_DECL_HOOK2_void( IServerGameClients, ClientCommand, SH_NOATTRIB, 0, CPlayerSlot, const CCommand & );
SH_DECL_HOOK1_void(IServer, SetGameSpawnGroupMgr, SH_NOATTRIB, 0, IGameSpawnGroupMgr*);
SH_DECL_HOOK0_void(IServerGameDLL, GameServerSteamAPIActivated, SH_NOATTRIB, 0);
SH_DECL_HOOK8_void(IGameEventSystem, PostEventAbstract, SH_NOATTRIB, 0, CSplitScreenSlot, bool, int, const uint64*,
	INetworkMessageInternal*, const CNetMessage*, unsigned long, NetChannelBufType_t)
//SH_DECL_HOOK3_void(INetworkServerService, StartupServer, SH_NOATTRIB, 0, const GameSessionConfiguration_t&, ISource2WorldSession*, const char*);
//SH_DECL_MANUALHOOK1(sh_ConstructAutoList, 0, 0, 0, CUtlAutoList<CCSBaseScript>*, bool);

MMSPlugin g_ThisPlugin;
IServerGameDLL *server = NULL;
IServerGameClients *gameclients = NULL;
IVEngineServer *engine = NULL;
IGameEventManager2 *gameevents = NULL;
ICvar *icvar = NULL;
IGameEventSystem* g_gameEventSystem = NULL;
//CSteamGameServerAPIContext g_steamAPI;

HMODULE g_hV8Module = NULL;
IGameEventManager2* g_gameEventManager = nullptr;
IVEngineServer2* g_pEngineServer2 = nullptr;

bool g_bHasTicked = false;
double g_flUniversalTime = 0.0;
double g_flLastTickedTime = 0.0;
int g_iSetGameSpawnGroupMgrId = -1;
CSpawnGroupMgrGameSystem* g_pSpawnGroupMgr = NULL;
HudHintManager g_hudHintManager;
LoggingChannelID_t g_logChanScript;

#define CREATE_FUNCHOOK_BASIC(funchookHandle, originalFunction, hookedFunction) \
	funchookHandle = funchook_create(); \
	funchook_prepare(funchookHandle, (void**)(&originalFunction), reinterpret_cast<void*>(hookedFunction)); \
	funchook_install(funchookHandle, 0);

CSScriptExtensionsSystem* g_scriptExtensions;
typedef CBaseEntity* (__fastcall* CreateEntityByNameFunc_t)(const char* className, int iForceEdictIndex);
typedef CBaseEntity* (__fastcall* DispatchSpawnFunc_t)(CBaseEntity* ent, CEntityKeyValues* pEntityKeyValues);
CreateEntityByNameFunc_t g_pfnCreateEntityByName = nullptr;
DispatchSpawnFunc_t g_pfnDispatchSpawn = nullptr;

std::vector<CustomPlayer*> m_vecCustomPlayers(64);
//template <typename T = CBaseEntity>
//T* CreateEntityByName(const char* className)
//{
//	return reinterpret_cast<T*>(g_pfnCreateEntityByName(className, -1));
//}
//
//void DispatchSpawn(CBaseEntity* ent, CEntityKeyValues* pEntityKeyValues = nullptr)
//{
//	g_pfnDispatchSpawn(ent, pEntityKeyValues);
//}
typedef CServerSideClient* (FASTCALL* GetFreeClientFunc_t)(int64_t unk1, const __m128i* unk2, unsigned int unk3, int64_t unk4, char unk5, void* unk6);
GetFreeClientFunc_t g_pfnGetFreeClient = nullptr;
funchook_t* hook_getFreeClient;
CServerSideClient* FASTCALL Detour_GetFreeClient(int64_t unk1, const __m128i* unk2, unsigned int unk3, int64_t unk4, char unk5, void* unk6);

void ClientPrintAll(int hud_dest, const char* msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[256];
	V_vsnprintf(buf, sizeof(buf), msg, args);

	va_end(args);

	INetworkMessageInternal* pNetMsg = g_pNetworkMessages->FindNetworkMessagePartial("TextMsg");
	auto data = pNetMsg->AllocateMessage()->ToPB<CUserMessageTextMsg>();

	data->set_dest(hud_dest);
	data->add_param(buf);

	CRecipientFilter filter;
	filter.AddAllPlayers();
	g_gameEventSystem->PostEventAbstract(-1, false, &filter, pNetMsg, data, 0);

	delete data;

	ConMsg("%s\n", buf);
}

void ClientPrint(CCSPlayerController* player, int hud_dest, const char* msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[256];
	V_vsnprintf(buf, sizeof(buf), msg, args);

	va_end(args);

	if (!player || !player->IsConnected() || player->IsBot())
	{
		ConMsg("%s\n", buf);
		return;
	}

	INetworkMessageInternal* pNetMsg = g_pNetworkMessages->FindNetworkMessagePartial("TextMsg");
	auto data = pNetMsg->AllocateMessage()->ToPB<CUserMessageTextMsg>();

	data->set_dest(hud_dest);
	data->add_param(buf);

	CSingleRecipientFilter filter(player->GetPlayerSlot());

	g_gameEventSystem->PostEventAbstract(-1, false, &filter, pNetMsg, data, 0);

	delete data;
}

INetworkGameServer* GetNetworkGameServer()
{
	return g_pNetworkServerService->GetIGameServer();
}

CUtlVector<CServerSideClient*>* GetClientList()
{
	if (!GetNetworkGameServer())
		return nullptr;

	static int offset = g_GameConfig->GetOffset("CNetworkGameServer_ClientList");
	return (CUtlVector<CServerSideClient*>*)(&GetNetworkGameServer()[offset]);
}

CServerSideClient* GetClientBySlot(CPlayerSlot slot)
{
	CServerSideClient* pClient = nullptr;
	auto list = GetClientList();
	if (!list)
		return nullptr;

	return list->Element(slot.Get());
}

void Panic(const char* msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[1024] = {};
	V_vsnprintf(buf, sizeof(buf) - 1, msg, args);

	Warning("[cs_script_ext] %s", buf);

	va_end(args);
}


// Should only be called within the active game loop (i e map should be loaded and active)
// otherwise that'll be nullptr!
CGlobalVars *GetGameGlobals()
{
	INetworkGameServer *server = g_pNetworkServerService->GetIGameServer();

	if(!server)
		return nullptr;

	return g_pNetworkServerService->GetIGameServer()->GetGlobals();
}

CGameEntitySystem* GameEntitySystem()
{
#ifdef PLATFORM_WINDOWS
	static int offset = 88;
#else
	static int offset = 80;
#endif
	return *reinterpret_cast<CGameEntitySystem**>((uintptr_t)(g_pGameResourceServiceServer)+offset);
}

void SampleCvarFChangeCB( CConVar<float> *cvar, CSplitScreenSlot slot, const float *new_val, const float *old_val )
{
	META_CONPRINTF( "Sample convar \"%s\" was changed from %f to %f [%s]\n",
					cvar->GetName(), *old_val, *new_val,
					// When convar is first initialised with a default value, it would have FCVAR_INITIAL_SETVALUE
					// flag set, so you can check for it if needed.
					cvar->IsFlagSet( FCVAR_INITIAL_SETVALUE ) ? "initialised" : "change" );
}

CConVar<int> sample_cvari("sample_cvari", FCVAR_NONE, "help string", 42);
CConVar<float> sample_cvarf("sample_cvarf", FCVAR_NONE, "help string", 69.69f, true, 10.0f, true, 100.0f, SampleCvarFChangeCB );

uint64_t DecodeAddrRipRelativeMov(uint64_t code_addr) {
	// 3 bytes for Long mode MOV and first operand.
	// next 4 are rip relative offset
	uint8_t instruction_length = 7;
	int32_t offset = *reinterpret_cast<const int32_t*>((uint8_t*)code_addr + 3);

	return code_addr + instruction_length + offset;
}

int Hook_LoadEventsFromFile(const char* filename, bool bSearchAll)
{
	ExecuteOnce(g_gameEventManager = META_IFACEPTR(IGameEventManager2));

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

static void RegisterScriptFunctions()
{
	g_scriptExtensions->AddNewFunction("Domain", 
		"MsgNew",
		"point_script",
		V8Callbacks::V8NewMsg
	);

	// Entity
	g_scriptExtensions->AddNewFunction("Entity",
		"GetSchemaField",
		"point_script",
		V8Callbacks::V8GetSchemaField
	);

	// Player controller
	g_scriptExtensions->AddNewFunction("CSPlayerController",
		"ShowHudMessageHTML",
		"point_script",
		V8Callbacks::V8ShowHTMLMessage
	);

	g_scriptExtensions->AddNewFunction("CSPlayerController",
		"ShowHudHint",
		"point_script",
		V8Callbacks::V8ShowHudHint
	);

	g_scriptExtensions->AddNewFunction("Domain",
		"AddSampleCallback",
		"point_script",
		V8Callbacks::AddSampleCallback
	);

	g_scriptExtensions->AddNewFunction("Domain",
		"OnUserMessage",
		"point_script",
		ScriptUserMessage::OnUserMessage
	);

	g_scriptExtensions->RegisterCustomFunctionTemplate(ScriptUserMessage::InitUserMessageInfoTemplate);
	
}

void SetupDetours()
{
	g_pfnGetFreeClient = (GetFreeClientFunc_t)(g_GameConfig->ResolveSignature("GetFreeClient"));
	if (g_pfnGetFreeClient)
	{
		Msg("[cs_script_extensions] Failed to find GetFreeClient signature");
		return;
	}
	CREATE_FUNCHOOK_BASIC(hook_getFreeClient, g_pfnGetFreeClient, Detour_GetFreeClient);
}

static uint64 g_iFlagsToRemove = (FCVAR_HIDDEN | FCVAR_DEVELOPMENTONLY);
static constexpr const char* pUnCheatCmds[] = { "report_entities" };
static constexpr const char* pUnCheatCvars[] = { "bot_stop", "bot_freeze", "bot_zombie" };
void UnlockConVars()
{
	if (!g_pCVar)
		return;

	int iUnhiddenConVars = 0;

	for (ConVarRefAbstract ref(ConVarRef((uint16)0)); ref.IsValidRef(); ref = ConVarRefAbstract(ConVarRef(ref.GetAccessIndex() + 1)))
	{
		for (int i = 0; i < sizeof(pUnCheatCvars) / sizeof(*pUnCheatCvars); i++)
			if (!V_strcmp(ref.GetName(), pUnCheatCvars[i]))
				ref.RemoveFlags(FCVAR_CHEAT);

		if (!ref.IsFlagSet(g_iFlagsToRemove))
			continue;

		ref.RemoveFlags(g_iFlagsToRemove);
		iUnhiddenConVars++;
	}

	Msg("Removed hidden flags from %d convars\n", iUnhiddenConVars);
}

PLUGIN_EXPOSE(MMSPlugin, g_ThisPlugin);
bool MMSPlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pEngineServer2, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pGameResourceServiceServer, IGameResourceService, GAMERESOURCESERVICESERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2ServerConfig, ISource2ServerConfig, SOURCE2SERVERCONFIG_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameEntities, ISource2GameEntities, SOURCE2GAMEENTITIES_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameClients, IServerGameClients, SOURCE2GAMECLIENTS_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pNetworkServerService, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_gameEventSystem, IGameEventSystem, GAMEEVENTSYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pNetworkMessages, INetworkMessages, NETWORKMESSAGES_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pGameTypes, IGameTypes, GAMETYPES_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
	//GET_V_IFACE_ANY(GetEngineFactory, g_pNetworkStringTableServer, INetworkStringTableContainer, INTERFACENAME_NETWORKSTRINGTABLESERVER);
	// Currently doesn't work from within mm side, use GetGameGlobals() in the mean time instead
	// gpGlobals = ismm->GetCGlobals();
	// Required to get the IMetamodListener events
	g_SMAPI->AddListener( this, this );
	
	META_CONPRINTF( "Starting plugin.\n" );
	SH_ADD_HOOK(IServerGameDLL, GameFrame, g_pSource2Server, SH_MEMBER(this, &MMSPlugin::Hook_GameFrame), true);
	SH_ADD_HOOK(IServerGameClients, ClientActive, g_pSource2GameClients, SH_MEMBER(this, &MMSPlugin::Hook_ClientActive), true);
	SH_ADD_HOOK(IServerGameClients, ClientDisconnect, g_pSource2GameClients, SH_MEMBER(this, &MMSPlugin::Hook_ClientDisconnect), true);
	SH_ADD_HOOK(IServerGameClients, ClientPutInServer, g_pSource2GameClients, SH_MEMBER(this, &MMSPlugin::Hook_ClientPutInServer), true);
	SH_ADD_HOOK(IServerGameClients, ClientSettingsChanged, g_pSource2GameClients, SH_MEMBER(this, &MMSPlugin::Hook_ClientSettingsChanged), false);
	SH_ADD_HOOK(IServerGameClients, OnClientConnected, g_pSource2GameClients, SH_MEMBER(this, &MMSPlugin::Hook_OnClientConnected), false);
	SH_ADD_HOOK(IServerGameClients, ClientConnect, g_pSource2GameClients, SH_MEMBER(this, &MMSPlugin::Hook_ClientConnect), false);
	SH_ADD_HOOK(IServerGameClients, ClientCommand, g_pSource2GameClients, SH_MEMBER(this, &MMSPlugin::Hook_ClientCommand), false);
	SH_ADD_HOOK(IGameEventSystem, PostEventAbstract, g_gameEventSystem, SH_MEMBER(this, &MMSPlugin::Hook_PostEvent), false);
	//SH_ADD_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_MEMBER(this, &MMSPlugin::Hook_StartupServer), true);
	//SH_ADD_HOOK(IServerGameDLL, GameServerSteamAPIActivated, g_pSource2Server, SH_MEMBER(this, &MMSPlugin::Hook_GameServerSteamAPIActivated), false);

	// CUtlAutoList<class CCSBaseScript>
	//auto vtbl = g_serverModule->FindVirtualTable("?$CUtlAutoList@VCCSBaseScript@@");
	//SH_ADD_MANUALDVPHOOK(sh_ConstructAutoList, vtbl, Hook_ConstructScriptAutoList, true);

	META_CONPRINTF( "All hooks started!\n" );

	META_CONPRINT("Loading gamedata...");
	CBufferStringGrowable<256> gamedirpath;
	g_pEngineServer2->GetGameDir(gamedirpath);
	std::string gamedir = CGameConfig::GetDirectoryName(gamedirpath.Get());
	Msg("gamedir path %s", gamedir.c_str());
	const char* gameDataPath = "addons/cs_scriptExt/gamedata/cs_scriptExt.games.txt";

	g_GameConfig = new CGameConfig(gamedir, gameDataPath);
	char conf_error[256];
	if (!g_GameConfig->Init(g_pFullFileSystem, conf_error, sizeof(conf_error)))
	{
		snprintf(error, maxlen, "Could not read %s: %s", g_GameConfig->GetPath().c_str(), conf_error);
		Panic("%s\n", error);
		return false;
	}

	bool bRequiredInitLoaded = true;
	if (!addresses::Initialize(g_GameConfig))
		bRequiredInitLoaded = false;


	g_scriptExtensions = CSScriptExtensionsSystem::GetInstance();
	RegisterScriptFunctions();
	if(!g_scriptExtensions->Initialize(g_GameConfig))
		bRequiredInitLoaded = false;

	if (!bRequiredInitLoaded)
	{
		META_CONPRINT("Failed to load required addresses from gamedata, unloading plugin.\n");
		return false;
	}

	SetupDetours();
	UnlockConVars();
	META_CONVAR_REGISTER(FCVAR_RELEASE | FCVAR_GAMEDLL);
	g_pEngineServer2->ServerCommand("sv_long_frame_ms 50.0");

	// You can get a convar reference to an already existing cvar via CConVarRef.
	// This will pre-register it if it's not yet registered and would use default data until
	// the actual cvar is registered. You can assert data existance via IsConVarDataAvailable().
	// Make sure the type is correct here otherwise it might prevent actual convar being registered,
	// since you pre-registered it with a different type or if convar already exists you'd be left with 
	// an invalid ref, so a check for IsValidRef() is also nice to have.
	// Generally with these you should just know the type of a cvar you are referencing beforehand
	// and if not, refer to ConVarRefAbstract usage
	// 
	// Side Note: Always make sure you are working with a valid ref (IsValidRef()) before reading/writing to it
	// as otherwise you'd be reading/writing off of default convar data which is shared across
	// all the invalid convar refs.
	//CConVarRef<int> ccvar_ref_example( "mp_limitteams" );

	//if(ccvar_ref_example.IsValidRef() && ccvar_ref_example.IsConVarDataAvailable())
	//{
	//	META_CONPRINTF( "CConVarRef \"%s\" got value pre = %d [float = %f, bool = %d, string = \"%s\"]\n",
	//					ccvar_ref_example.GetName(), ccvar_ref_example.Get(),
	//					ccvar_ref_example.GetFloat(), ccvar_ref_example.GetBool(),
	//					ccvar_ref_example.GetString() );
	//	
	//	// By default if you are using CConVar or CConVarRef you should be using Get()/Set()
	//	// methods to read/write values, as these are templated for the particular type the cvar is of.
	//	// It also is usually faster since it skips all the type conversion logic of non templated methods
	//	ccvar_ref_example.Set( 5 );

	//	// As noted above there are methods that support value conversion between certain types
	//	// so stuff like this is possible on an int typed cvar for example,
	//	// refer to ConVarRefAbstract declaration for more info on these methods
	//	ccvar_ref_example.SetFloat( 8.5f );

	//	META_CONPRINTF( "CConVarRef \"%s\" got value after = %d [float = %f, bool = %d, string = \"%s\"]\n",
	//					ccvar_ref_example.GetName(), ccvar_ref_example.Get(),
	//					ccvar_ref_example.GetFloat(), ccvar_ref_example.GetBool(),
	//					ccvar_ref_example.GetString() );
	//}

	//// You can also use ConVarRefAbstract class if you don't want typisation support
	//// or don't know the actual type used, since you are responsible for picking the correct type there!
	//// And ConVarRefAbstract won't pre-register the convar in the system, as it acts as a plain ref,
	//// so make sure to check the ref for validity before usage via IsValidRef()
	//ConVarRefAbstract cvar_ref_example( "mp_limitteams" );

	//if(cvar_ref_example.IsValidRef())
	//{
	//	META_CONPRINTF( "ConVarRefAbstract \"%s\" got value pre [float = %f, bool = %d, string = \"%s\"]\n",
	//					cvar_ref_example.GetName(), cvar_ref_example.GetFloat(), cvar_ref_example.GetBool(), cvar_ref_example.GetString() );

	//	// Since the ref is not typed, you can't use direct Get() and Set() methods,
	//	// instead you need to use methods with type conversion support.
	//	cvar_ref_example.SetFloat( 10.0f );

	//	// If you work with convars of non primitive types, you can also use SetAs() methods
	//	// to try to set the value as a specific type, if type mismatches it would try to do 
	//	// conversion if possible and if not it would do nothing.
	//	// There's also an equvialent methods for reading the value, GetAs()
	//	cvar_ref_example.SetAs<Vector>( Vector( 1.0f, 2.0f, 3.0f ) );

	//	// Alternatively you can "promote" plain ref to a typed variant by passing plain ref to a constructor
	//	// but be careful, as there's a type checker in place that would invalidate convar ref
	//	// if cast to a wrong type was attempted, you can check for that either via IsValidRef()
	//	// or IsConVarDataValid() (usually IsValidRef() is enough) afterwards, but generally you should
	//	// just know the correct type of the cvar you are casting to beforehand.
	//	CConVarRef<int> promoted_ref( cvar_ref_example );
	//	if(promoted_ref.IsValidRef() && promoted_ref.IsConVarDataValid())
	//	{
	//		// If the promoted ref is valid, you can use its templated methods like with CConVarRef/CConVar
	//		promoted_ref.Set( 5 );
	//	}

	//	META_CONPRINTF( "ConVarRefAbstract \"%s\" got value after [float = %f, bool = %d, string = \"%s\"]\n",
	//					cvar_ref_example.GetName(), cvar_ref_example.GetFloat(), cvar_ref_example.GetBool(), cvar_ref_example.GetString() );
	//}

	/*ConCommandRef ccmd_ref_rents("report_entities");
	if (ccmd_ref_rents.IsValidRef())
	{
		ccmd_ref_rents.RemoveFlags(FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT);
	}

	ConVarRefAbstract cvar_ref_c4timer("mp_c4timer");
	if (cvar_ref_c4timer.IsValidRef())
	{
		cvar_ref_c4timer.RemoveFlags(FCVAR_NOTIFY);
	}

	ConCommandRef ccmd_ref_timescale("host_timescale");
	if (ccmd_ref_timescale.IsValidRef())
	{
		ccmd_ref_timescale.RemoveFlags(FCVAR_CHEAT);
	}*/

	auto gameEventMgrVtbl = (IGameEventManager2*)modules::server->FindVirtualTable("CGameEventManager");
	SH_ADD_DVPHOOK(IGameEventManager2, LoadEventsFromFile, gameEventMgrVtbl, Hook_LoadEventsFromFile, false);\

	return true;
}

bool MMSPlugin::Unload(char *error, size_t maxlen)
{
	SH_REMOVE_HOOK(IServerGameDLL, GameFrame, server, SH_MEMBER(this, &MMSPlugin::Hook_GameFrame), true);
	SH_REMOVE_HOOK(IServerGameClients, ClientActive, gameclients, SH_MEMBER(this, &MMSPlugin::Hook_ClientActive), true);
	SH_REMOVE_HOOK(IServerGameClients, ClientDisconnect, gameclients, SH_MEMBER(this, &MMSPlugin::Hook_ClientDisconnect), true);
	SH_REMOVE_HOOK(IServerGameClients, ClientPutInServer, gameclients, SH_MEMBER(this, &MMSPlugin::Hook_ClientPutInServer), true);
	SH_REMOVE_HOOK(IServerGameClients, ClientSettingsChanged, gameclients, SH_MEMBER(this, &MMSPlugin::Hook_ClientSettingsChanged), false);
	SH_REMOVE_HOOK(IServerGameClients, OnClientConnected, gameclients, SH_MEMBER(this, &MMSPlugin::Hook_OnClientConnected), false);
	SH_REMOVE_HOOK(IServerGameClients, ClientConnect, gameclients, SH_MEMBER(this, &MMSPlugin::Hook_ClientConnect), false);
	SH_REMOVE_HOOK(IServerGameClients, ClientCommand, gameclients, SH_MEMBER(this, &MMSPlugin::Hook_ClientCommand), false);
	//SH_REMOVE_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_MEMBER(this, &MMSPlugin::Hook_StartupServer), true);
	delete g_scriptExtensions;

	return true;
}
//
//CON_COMMAND(test_script, "")
//{
//	CBaseEntity* script = CreateEntityByName("point_script");
//	CEntityKeyValues* kv = new CEntityKeyValues();
//	kv->SetString("cs_script", "scripts/main.vjs");
//	DispatchSpawn(script, kv);
//}
void MMSPlugin::Hook_GameServerSteamAPIActivated()
{
	//g_steamAPI.Init();
	//m_CallbackValidateAuthTicketResponse.Register(this, &MMSPlugin::OnValidateAuthTicket);
}

void MMSPlugin::AllPluginsLoaded()
{
	/* This is where we'd do stuff that relies on the mod or other plugins 
	 * being initialized (for example, cvars added and events registered).
	 */
	
}

void MMSPlugin::Hook_ClientActive( CPlayerSlot slot, bool bLoadGame, const char *pszName, uint64 xuid )
{
	//META_CONPRINTF( "Hook_ClientActive(%d, %d, \"%s\", %d)\n", slot, bLoadGame, pszName, xuid );
}

void MMSPlugin::Hook_ClientCommand( CPlayerSlot slot, const CCommand &args )
{
	//META_CONPRINTF( "Hook_ClientCommand(%d, \"%s\")\n", slot, args.GetCommandString() );
}

void MMSPlugin::Hook_ClientSettingsChanged( CPlayerSlot slot )
{
	//META_CONPRINTF( "Hook_ClientSettingsChanged(%d)\n", slot );
}

void MMSPlugin::Hook_OnClientConnected( CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, const char *pszAddress, bool bFakePlayer )
{
	//META_CONPRINTF( "Hook_OnClientConnected(%d, \"%s\", %d, \"%s\", \"%s\", %d)\n", slot, pszName, xuid, pszNetworkID, pszAddress, bFakePlayer );
	if (bFakePlayer)
	{
		auto plr = new CustomPlayer(slot.Get(), true);
		m_vecCustomPlayers[slot.Get()] = plr;
	}
	
}

bool MMSPlugin::Hook_ClientConnect( CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, bool unk1, CBufferString *pRejectReason )
{
	auto plr = new CustomPlayer(slot.Get());
	plr->SetUnauthenticatedSteamId(xuid);
	m_vecCustomPlayers[slot.Get()] = plr;
	RETURN_META_VALUE(MRES_IGNORED, true);
	//META_CONPRINTF( "Hook_ClientConnect(%d, \"%s\", %d, \"%s\", %d, \"%s\")\n", slot, pszName, xuid, pszNetworkID, unk1, pRejectReason->Get() );
}

void MMSPlugin::Hook_ClientPutInServer( CPlayerSlot slot, char const *pszName, int type, uint64 xuid )
{
	if (!g_pSpawnGroupMgr)
		return;

	CUtlVector<SpawnGroupHandle_t> vecActualSpawnGroups;
	addresses::GetSpawnGroups(g_pSpawnGroupMgr, &vecActualSpawnGroups);

	CServerSideClient* pClient = GetClientBySlot(slot);

	if (pClient && pClient->m_vecLoadedSpawnGroups.Count() != vecActualSpawnGroups.Count())
		pClient->m_vecLoadedSpawnGroups = vecActualSpawnGroups;
}

void MMSPlugin::Hook_ClientDisconnect( CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid, const char *pszNetworkID )
{
	delete m_vecCustomPlayers[slot.Get()];
	//META_CONPRINTF( "Hook_ClientDisconnect(%d, %d, \"%s\", %d, \"%s\")\n", slot, reason, pszName, xuid, pszNetworkID );
}

void MMSPlugin::Hook_GameFrame( bool simulating, bool bFirstTick, bool bLastTick )
{
	/**
	 * simulating:
	 * ***********
	 * true  | game is ticking
	 * false | game is not ticking
	 */
	CGlobalVars* globals = GetGameGlobals();
	if (globals)
	{
		if (simulating && g_bHasTicked)
			g_flUniversalTime += globals->curtime - g_flLastTickedTime;

		g_flLastTickedTime = globals->curtime;
		g_bHasTicked = true;
	}
	RunTimers();
	g_hudHintManager.Update();
}

void MMSPlugin::OnLevelInit( char const *pMapName,
									 char const *pMapEntities,
									 char const *pOldLevel,
									 char const *pLandmarkName,
									 bool loadGame,
									 bool background )
{
	META_CONPRINTF("OnLevelInit(%s)\n", pMapName);
}

void MMSPlugin::OnLevelShutdown()
{
	META_CONPRINTF("OnLevelShutdown()\n");
}

void MMSPlugin::Hook_SetGameSpawnGroupMgr(IGameSpawnGroupMgr* pSpawnGroupMgr)
{
	// This also resets our stored pointer on deletion, since null gets passed into this function, nice!
	g_pSpawnGroupMgr = (CSpawnGroupMgrGameSystem*)pSpawnGroupMgr;
}

void MMSPlugin::Hook_StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession* pSession, const char* pszMapName)
{

	if (g_pNetworkServerService->GetIGameServer())
		g_iSetGameSpawnGroupMgrId = SH_ADD_HOOK(IServer, SetGameSpawnGroupMgr, g_pNetworkServerService->GetIGameServer(), SH_MEMBER(this, &MMSPlugin::Hook_SetGameSpawnGroupMgr), false);

	Msg("Hook_StartupServer: %s\n", pszMapName);

	/*RegisterEventListeners();

	if (g_bHasTicked)
		RemoveTimers(TIMERFLAG_MAP);*/
	auto entitySystem = GameEntitySystem();

	g_bHasTicked = false;
}

void MMSPlugin::Hook_PostEvent(CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64* clients,
	INetworkMessageInternal* pEvent, const CNetMessage* pData, unsigned long nSize, NetChannelBufType_t bufType)
{
	auto scripts = CSScriptExtensionsSystem::GetScripts();

	if (!scripts.empty())
	{
		NetMessageInfo_t* info = pEvent->GetNetMessageInfo();
		CBufferString scriptCallbackName("OnUserMessage:");
		scriptCallbackName.AppendFormat("%d", info->m_MessageId);
		CGlobalSymbol callbackSymbol(scriptCallbackName.Get());

		ScriptUserMessageInfo* userMessageInfo = nullptr;

		bool scriptCallbackInitialized = false;
		for (CEntityInstance* scriptEnt : scripts)
		{
			v8::HandleScope handleScope(v8::Isolate::GetCurrent());
			auto script = CSScriptExtensionsSystem::GetScriptFromEntity(scriptEnt);
			if (!script || !script->IsCallbackRegistered(callbackSymbol))
				continue;

			if (!scriptCallbackInitialized)
			{
				auto msg = const_cast<CNetMessage*>(pData)->ToPB<google::protobuf::Message>();
				userMessageInfo = new ScriptUserMessageInfo(msg, clients);
				scriptCallbackInitialized = true;
			}
			auto obj = ScriptUserMessage::CreateUserMessageInfoInstance(script, userMessageInfo);
			v8::Local<v8::Value> jsArgs[] = { obj };

			auto result = script->InvokeCallback(callbackSymbol, 1, jsArgs);
			// script returned false - clear all recipients (block).
			if (!result.IsEmpty() && result->IsBoolean() && !result->ToBoolean(v8::Isolate::GetCurrent())->Value())
			{
				*const_cast<uint64*>(clients) = 0;
			}
		}
	}
}

CServerSideClient* FASTCALL Detour_GetFreeClient(int64_t unk1, const __m128i* unk2, unsigned int unk3, int64_t unk4, char unk5, void* unk6)
{
	// Not sure if this function can even be called in this state, but if it is, we can't do shit anyways
	if (!GetClientList() || !GetGameGlobals())
		return nullptr;

	// Check if there is still unused slots, this should never break so just fall back to original behaviour for ease (we don't have a CServerSideClient constructor)
	if (GetGameGlobals()->maxClients != GetClientList()->Count())
		return g_pfnGetFreeClient(unk1, unk2, unk3, unk4, unk5, unk6);

	// Phantom client fix
	for (int i = 0; i < GetClientList()->Count(); i++)
	{
		CServerSideClient* pClient = (*GetClientList())[i];

		if (pClient && pClient->GetSignonState() < SIGNONSTATE_CONNECTED)
			return pClient;
	}

	// Server is actually full for real
	return nullptr;
}

// remember the script path by its id for reloading purposes
// yes this doesn't get cleaned properly, but I don't care right now.
static std::unordered_map<uint64_t, std::string> scriptPathMap;

bool RunScriptFromFile(CCSScript_EntityScript* script, std::string_view relativePath, bool savePath = false)
{
	if (!script)
		return false;

	CBufferStringGrowable<256> gamedirpath;
	g_pEngineServer2->GetGameDir(gamedirpath);

	std::stringstream pathStream;
	pathStream << gamedirpath.Get();
	pathStream << "/" << relativePath;

	if (std::ifstream inFile(pathStream.str(), std::ios::in); inFile.good())
	{
		std::string fileContents{ std::istreambuf_iterator<char>(inFile), std::istreambuf_iterator<char>() };
		g_scriptExtensions->RunScriptString(script, "nul.vjs_c", fileContents.c_str());

		if (savePath)
			scriptPathMap[script->GetScriptIndex()] = std::string(relativePath);
	}
	else
	{
		META_CONPRINT("Failed to open file for reading!\n");
		return false;
	}

	return true;
}

CON_COMMAND_F(csscript_load, "Creates a script entity and loads the provided file (raw .js)", FCVAR_NONE)
{
	if (args.ArgC() < 2)
	{
		META_CONPRINT("Usage: script_load <script_file> [script_entity_name]\n");
	}

	const char* scriptName = "script";
	if (args.ArgC() > 2)
		scriptName = args[2];

	auto point_script = addresses::CreateEntityByName("point_script", -1);
	auto kv = new CEntityKeyValues();
	kv->SetString("cs_script", "nul.vjs_c");
	kv->SetString("targetname", scriptName);
	addresses::DispatchSpawn(point_script, kv);

	auto script = CSScriptExtensionsSystem::GetScriptFromEntity(point_script);
	if (!RunScriptFromFile(script, args[1], true))
	{
		point_script->Remove();
	}
}

CON_COMMAND_F(csscript_reload, "Reload a script (loaded by script_load only)", FCVAR_NONE)
{
	if (args.ArgC() < 2)
	{
		META_CONPRINT("Usage: script_reload <script_entity_name>\n");
	}

	CBaseEntity* ent = nullptr;
	while ((ent = UTIL_FindEntityByName(ent, args[1])))
	{
		auto script = CSScriptExtensionsSystem::GetScriptFromEntity(ent);
		if (!script)
			continue;

		auto index = script->GetScriptIndex();
		if (scriptPathMap.contains(index))
		{
			RunScriptFromFile(script, scriptPathMap[index]);
		}
		else
		{
			META_CONPRINT("Provided script entity was not loaded through script_load\n");
		}
	}
}

CON_COMMAND_F(remove_scripts, "Remove all point_script entities", FCVAR_NONE)
{
	CBaseEntity* ent = nullptr;
	while ((ent = UTIL_FindEntityByClassname(ent, "point_script")))
	{
		ent->Remove();
	}
	scriptPathMap.clear();
}

CON_COMMAND_F(script_run_code, "Run code inside an existing script", FCVAR_NONE)
{
	if (args.ArgC() < 3)
	{
		META_CONPRINT("Usage: script_code <script_entity_name> <js code>\n");
	}
	
	const auto isolate = v8::Isolate::GetCurrent();
	CBaseEntity* ent = nullptr;
	while ((ent = UTIL_FindEntityByName(ent, args[1])))
	{
		if (!V_stricmp_fast(ent->GetClassname(), "point_script"))
		{
			if (const auto script = CSScriptExtensionsSystem::GetScriptFromEntity(ent))
			{
				v8::HandleScope handleScope(isolate);
				const auto& scriptContext = script->GetContext().Get(isolate);
				g_scriptExtensions->SwitchScriptContext(script);
				v8::TryCatch tryCatch(isolate);

				v8::Local<v8::String> source =
					v8::String::NewFromUtf8(isolate, args[2]).ToLocalChecked();

				v8::Local<v8::Script> compiledScript;
				if (!v8::Script::Compile(scriptContext, source).ToLocal(&compiledScript))
				{
					v8::Local<v8::String> exceptionStr;
					if (exceptionStr->ToString(scriptContext).ToLocal(&exceptionStr)) {
						v8::String::Utf8Value error_msg(isolate, exceptionStr);
						META_CONPRINTF("Error: %s\n", *error_msg);
					}
					META_CONPRINTF("Failed to compile string: %s\n", *exceptionStr);
					return;
				}

				v8::Local<v8::Value> result;
				if (!compiledScript->Run(scriptContext).ToLocal(&result))
				{
					v8::Local<v8::Value> exception = tryCatch.Exception();

					v8::Local<v8::String> exceptionStr;
					if (exception->ToString(scriptContext).ToLocal(&exceptionStr)) {
						v8::String::Utf8Value error_msg(isolate, exceptionStr);
						META_CONPRINTF("Error: %s\n", *error_msg);
					}
				}
			}

		}
	}
}

CON_COMMAND_F(script_summary, "List registered function templates on scripts", FCVAR_NONE)
{
	auto isolate = v8::Isolate::GetCurrent();
	for (CEntityInstance* scriptEnt : CSScriptExtensionsSystem::GetScripts())
	{
		v8::HandleScope handleScope(isolate);
		auto script = CSScriptExtensionsSystem::GetScriptFromEntity(scriptEnt);
		script->PrintSummary();
	}
}

