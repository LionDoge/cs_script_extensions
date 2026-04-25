/**
 * =============================================================================
 * cs_script_extensions
 * Copyright (C) 2026 liondoge
 *
 * CS2Fixes
 * Copyright (C) 2023-2026 Source2ZE
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <safetyhook.hpp>
#ifndef _WIN32
#include <dlfcn.h>
#endif // !_WIN32
#include <filesystem>
#include "plugin.h"
#include <fstream>
#include <sstream>
#include "entitysystem.h"
#include "schemasystem/schemasystem.h"
#include "convar.h"
#include "iloopmode.h"
#include "igameeventsystem.h"
#include "recipientfilters.h"
#include "serversideclient.h"
#include "entity.h"
#include "sigutils.h"
#include "globalsymbol.h"
#include "networkbasetypes.pb.h"
#include "usermessages.pb.h"
#include "module.h"
#include "ctimer.h"
#include "gameconfig.h"
#include "hudhintmanager.h"
#include <vprof.h>
#include "scriptExtensions/scriptDomainCallbacks.h"
#include "scriptExtensions/scriptextensions.h"
#include "scriptExtensions/userMessagesScriptExt.h"
#include "scriptExtensions/userMessageInfo.h"
#include "scriptExtensions/playerControllerCallbacks.h"
#include "v8.h"
#include "playermanager.h"
#include "entitylistener.h"
#include "pluginconfig.h"
#include "filesystem.h"
#include "iserver.h"
#include "scripttypes.h"

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
	INetworkMessageInternal*, const CNetMessage*, unsigned long, NetChannelBufType_t);
SH_DECL_HOOK7_void(ISource2GameEntities, CheckTransmit, SH_NOATTRIB, 0, CCheckTransmitInfo**, int, CBitVec<16384>&, CBitVec<16384>&, const Entity2Networkable_t**, const uint16*, int);
class GameSessionConfiguration_t {};
SH_DECL_HOOK3_void(INetworkServerService, StartupServer, SH_NOATTRIB, 0, const GameSessionConfiguration_t&, ISource2WorldSession*, const char*)
SH_DECL_HOOK3_void(ICvar, DispatchConCommand, SH_NOATTRIB, 0, ConCommandRef, const CCommandContext&, const CCommand&);

MMSPlugin g_ThisPlugin;
IServerGameDLL *server = NULL;
IServerGameClients *gameclients = NULL;
IVEngineServer *engine = NULL;
ICvar *icvar = NULL;
IGameEventSystem* g_gameEventSystem = NULL;
IGameEventManager2* g_gameEventManager = nullptr;
IVEngineServer2* g_pEngineServer2 = nullptr;

bool g_bHasTicked = false;
double g_flUniversalTime = 0.0;
double g_flLastTickedTime = 0.0;
int g_iSetGameSpawnGroupMgrId = -1;
CSpawnGroupMgrGameSystem* g_pSpawnGroupMgr = NULL;
HudHintManager g_hudHintManager;
PlayerManager g_playerManager;
CEntityListener g_entityListener;
LoggingChannelID_t g_logChanScript;

ScriptExtensions* g_scriptExtensions;
SafetyHookInline g_v8ExceptionHook{};

bool Hook_V8_TryCatch_HasCaught(v8::TryCatch* tryCatch)
{
	auto res = g_v8ExceptionHook.call<bool>(tryCatch);
	if (!res)
	{
		return res;
	}

	auto script = ScriptExtensions::GetCurrentCsScriptInstance();
	if (!script)
	{
		PluginMsg("V8 exception thrown but no current script context found! Stack trace unavailable.\n");
		return res;
	}

	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);
	v8::Local<v8::Context> context = isolate->GetCurrentContext();

	v8::Local<v8::Message> message = tryCatch->Message();
	if (message.IsEmpty())
	{
		PluginMsg("V8 exception thrown but no message available! Stack trace unavailable.\n");
		return res;
	}

	auto stackTrace = message->GetStackTrace();
	if (!stackTrace.IsEmpty()) {
		int frameCount = stackTrace->GetFrameCount();
		Log_Warning(g_logChanScript, "Stack trace (top is latest call):\n");

		for (int i = 0; i < frameCount; i++) {
			v8::Local<v8::StackFrame> frame = stackTrace->GetFrame(isolate, i);

			v8::String::Utf8Value funcName(isolate, frame->GetFunctionName());
			v8::String::Utf8Value scriptName(isolate, frame->GetScriptName());

			std::string scriptFinalPath;
			std::string scriptPathStr(*scriptName);
			if (!scriptPathStr.empty())
			{
				std::string scriptPathStripped = scriptPathStr.substr(scriptPathStr.find(":///") + strlen(":///"));

				std::filesystem::path scriptPath(scriptPathStripped);
				std::filesystem::path gameDir(Plat_GetGameDirectory());
				scriptFinalPath = std::filesystem::relative(scriptPath, gameDir).string();
				if (scriptFinalPath.empty()) // failed to get relative? just return full.
				{
					scriptFinalPath = scriptPathStr;
				}
			}

			int line = frame->GetLineNumber();
			int col = frame->GetColumn();

			Log_Warning(g_logChanScript, "  at %s (%s:%d:%d)\n",
				*funcName ? *funcName : "<anonymous>",
				!scriptFinalPath.empty() ? scriptFinalPath.c_str() : "<unknown>",
				line, col
			);
		}
	}
	return res;
}


void InitScriptExceptionHook()
{
#ifdef _WIN32
	HMODULE hModule = GetModuleHandleA("v8.dll");
	if (hModule == NULL) {
		PluginMsg("Failed to get module handle for v8. Error: %s\n", GetLastError());
		return;
	}
	FARPROC funcAddress = GetProcAddress(hModule, "?HasCaught@TryCatch@v8@@QEBA_NXZ");
	if (!funcAddress)
	{
		PluginMsg("Failed to get address for v8::TryCatch::HasCaught. Error: %s\n", GetLastError());
		return;
	}
#else
	void* hModule = dlopen("libv8.so", RTLD_LAZY);
	if (!hModule) {
		PluginMsg("Failed to get module handle for v8. Error: %s\n", dlerror());
		return;
	}
	void* funcAddress = dlsym(hModule, "_ZNK2v88TryCatch9HasCaughtEv");
	if (!funcAddress)
	{
		PluginMsg("Failed to get address for v8::TryCatch::HasCaught. Error: %s\n", dlerror());
		return;
	}
#endif
	g_v8ExceptionHook = safetyhook::create_inline((void*)funcAddress, Hook_V8_TryCatch_HasCaught);
}

// Snippet from: CS2Fixes
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

// Snippet from: CS2Fixes
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

void PluginMsg(const char* msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[1024] = {};
	V_vsnprintf(buf, sizeof(buf) - 1, msg, args);

	Msg("[cs_script_ext] %s", buf);

	va_end(args);
}


// Snippet from: CS2Fixes
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
	static int offset = g_GameConfig->GetOffset("GameEntitySystem");
	return *reinterpret_cast<CGameEntitySystem**>((uintptr_t)(g_pGameResourceServiceServer)+offset);
}

int Hook_LoadEventsFromFile(const char* filename, bool bSearchAll)
{
	ExecuteOnce(g_gameEventManager = META_IFACEPTR(IGameEventManager2));

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

CConVar<bool> cvar_enable_mapspawn_script("mm_enable_mapspawn_script", FCVAR_RELEASE | FCVAR_GAMEDLL, "If enabled, will instantiate a point_script entity with scripts/mapspawn on each map load, if available.", true);
static void RegisterScriptFunctions()
{
	g_scriptExtensions->IncludeFunctions(
	"Domain",
		{
			{ "MsgNew", ScriptDomainCallbacks::NewMsg },
			{ "AddSampleCallback", ScriptDomainCallbacks::AddSampleCallback },
			{ "EmitSound", ScriptDomainCallbacks::EmitSound },
			{ "PrintToChatAll", ScriptDomainCallbacks::PrintToChatAll },
			{ "OnDispatchClientCommand", ScriptDomainCallbacks::OnDispatchClientCommand },
			{ "OnClientCommand", ScriptDomainCallbacks::OnClientCommand },
			{ "CreateEntity", ScriptDomainCallbacks::CreateEntity }
		});

	if (g_pluginConfig.IsQueryConvarsEnabled())
	{
		g_scriptExtensions->IncludeFunctions("Domain", { { "GetConVarValue", ScriptDomainCallbacks::GetConVarValue } });
	}

	if (g_pluginConfig.AreUserMessagesEnabled())
	{
		g_scriptExtensions->IncludeFunctions(
			"Domain", 
			{ 
				{ "CreateUserMessage", ScriptDomainCallbacks::CreateUserMessage },
				{ "OnUserMessage", ScriptUserMessage::OnUserMessage }
			});
	}

	g_scriptExtensions->IncludeFunctions(
		"Entity",
		{
			{ "SetMoveType", ScriptDomainCallbacks::SetEntityMoveType },
		});

	if (g_pluginConfig.IsSchemaReadEnabled())
	{
		g_scriptExtensions->IncludeFunctions("Entity", { { "GetSchemaField", ScriptDomainCallbacks::GetSchemaField } });
	}

	if (g_pluginConfig.IsTransmitStateChangeEnabled())
	{
		g_scriptExtensions->IncludeFunctions(
			"Entity",
			{
				{ "SetTransmitState", ScriptDomainCallbacks::SetTransmitState },
				{ "SetTransmitStateAll", ScriptDomainCallbacks::SetTransmitStateAll }
			});
	}

	g_scriptExtensions->IncludeFunctions(
		"CSPlayerController",
		{
			{ "ShowHudHint", ScriptPlayerControllerCallbacks::ShowHudHint },
			{ "ShowHudMessageHTML", ScriptPlayerControllerCallbacks::ShowHTMLMessage },
			{ "Respawn", ScriptPlayerControllerCallbacks::Respawn },
			{ "PrintToChat", ScriptPlayerControllerCallbacks::PrintToChat }
		});

	if (g_pluginConfig.IsUserIdentificationEnabled())
	{
		g_scriptExtensions->IncludeFunctions("CSPlayerController", { { "GetSteamID", ScriptPlayerControllerCallbacks::GetSteamID } });
	}

	if (g_pluginConfig.AreUserMessagesEnabled())
	{
		g_scriptExtensions->RegisterCustomFunctionTemplate(
			"UserMessageInfo",
			{
				{ "GetField", ScriptUserMessage::UserMessageInfo_GetField },
				{ "SetField", ScriptUserMessage::UserMessageInfo_SetField },
				{ "AddAllRecipients", ScriptUserMessage::UserMessageInfo_AddAllRecipients },
				{ "AddRecipient", ScriptUserMessage::UserMessageInfo_AddRecipient },
				{ "ClearRecipients", ScriptUserMessage::UserMessageInfo_ClearRecipients },
				{ "RemoveRecipient", ScriptUserMessage::UserMessageInfo_RemoveRecipient },
				{ "Send", ScriptUserMessage::UserMessageInfo_Send }
			},
			1, // internal fields
			&ScriptTypeMarkers::userMessageInfo,
			std::nullopt // Inherits from
		);
	}
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
	
	g_SMAPI->AddListener( this, this );
	
	SH_ADD_HOOK(IServerGameDLL, GameFrame, g_pSource2Server, SH_MEMBER(this, &MMSPlugin::Hook_GameFrame), true);
	SH_ADD_HOOK(IServerGameClients, ClientActive, g_pSource2GameClients, SH_MEMBER(this, &MMSPlugin::Hook_ClientActive), true);
	SH_ADD_HOOK(IServerGameClients, ClientDisconnect, g_pSource2GameClients, SH_MEMBER(this, &MMSPlugin::Hook_ClientDisconnect), true);
	SH_ADD_HOOK(IServerGameClients, ClientPutInServer, g_pSource2GameClients, SH_MEMBER(this, &MMSPlugin::Hook_ClientPutInServer), true);
	SH_ADD_HOOK(IServerGameClients, ClientSettingsChanged, g_pSource2GameClients, SH_MEMBER(this, &MMSPlugin::Hook_ClientSettingsChanged), false);
	SH_ADD_HOOK(IServerGameClients, OnClientConnected, g_pSource2GameClients, SH_MEMBER(this, &MMSPlugin::Hook_OnClientConnected), false);
	SH_ADD_HOOK(IServerGameClients, ClientConnect, g_pSource2GameClients, SH_MEMBER(this, &MMSPlugin::Hook_ClientConnect), false);
	SH_ADD_HOOK(IServerGameClients, ClientCommand, g_pSource2GameClients, SH_MEMBER(this, &MMSPlugin::Hook_ClientCommand), false);
	SH_ADD_HOOK(IGameEventSystem, PostEventAbstract, g_gameEventSystem, SH_MEMBER(this, &MMSPlugin::Hook_PostEvent), false);
	SH_ADD_HOOK(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities, SH_MEMBER(this, &MMSPlugin::Hook_CheckTransmit), true);
	SH_ADD_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_MEMBER(this, &MMSPlugin::Hook_StartupServer), true);
	SH_ADD_HOOK(ICvar, DispatchConCommand, g_pCVar, SH_MEMBER(this, &MMSPlugin::Hook_DispatchConCommand), false);

	META_CONPRINT("[cs_script_ext] Loading gamedata...\n");
	CBufferStringGrowable<256> gamedirpath;
	g_pEngineServer2->GetGameDir(gamedirpath);
	std::string gamedir = CGameConfig::GetDirectoryName(gamedirpath.Get());
	const char* gameDataPath = "addons/cs_scriptExt/gamedata/cs_scriptExt.games.txt";

	g_GameConfig = new CGameConfig(gamedir, gameDataPath);
	char conf_error[256];
	if (!g_GameConfig->Init(g_pFullFileSystem, conf_error, sizeof(conf_error)))
	{
		snprintf(error, maxlen, "Could not read %s: %s", g_GameConfig->GetPath().c_str(), conf_error);
		Panic("%s\n", error);
		return false;
	}

	std::filesystem::path gamedirFsPath(gamedirpath.Get());
	std::filesystem::path configPath = gamedirFsPath / "addons" / "cs_scriptExt" / "configs" / "features.jsonc";
	if (!g_pluginConfig.Load(configPath.string()))
	{
		PluginMsg("Could not read features.json config, using defaults\n");
	}

	bool bRequiredInitLoaded = true;
	if (!addresses::Initialize(g_GameConfig))
		bRequiredInitLoaded = false;


	g_scriptExtensions = ScriptExtensions::GetInstance();
	if (g_pluginConfig.AreDefaultFunctionsEnabled())
		RegisterScriptFunctions();

	if(!g_scriptExtensions->Initialize(g_GameConfig))
		bRequiredInitLoaded = false;

	if (!bRequiredInitLoaded)
	{
		META_CONPRINT("[cs_script_ext] Failed to load required addresses from gamedata, unloading plugin.\n");
		return false;
	}

	auto gameEventMgrVtbl = (IGameEventManager2*)modules::server->FindVirtualTable("CGameEventManager");
	SH_ADD_DVPHOOK(IGameEventManager2, LoadEventsFromFile, gameEventMgrVtbl, Hook_LoadEventsFromFile, false);
	InitScriptExceptionHook();

	META_CONVAR_REGISTER(FCVAR_RELEASE | FCVAR_GAMEDLL);

	if (late)
	{
		const auto entitySystem = GameEntitySystem();
		if (entitySystem)
			entitySystem->AddListenerEntity(&g_entityListener);
	}
	
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
	SH_REMOVE_HOOK(IGameEventSystem, PostEventAbstract, g_gameEventSystem, SH_MEMBER(this, &MMSPlugin::Hook_PostEvent), false);
	SH_REMOVE_HOOK(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities, SH_MEMBER(this, &MMSPlugin::Hook_CheckTransmit), true);
	SH_REMOVE_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_MEMBER(this, &MMSPlugin::Hook_StartupServer), true);
	SH_REMOVE_HOOK(ICvar, DispatchConCommand, g_pCVar, SH_MEMBER(this, &MMSPlugin::Hook_DispatchConCommand), false);

	delete g_scriptExtensions;
	g_v8ExceptionHook = {};
	const auto entitySystem = GameEntitySystem();
	if (entitySystem)
		entitySystem->RemoveListenerEntity(&g_entityListener);

	return true;
}

// Empty implementations left for convenience

void MMSPlugin::Hook_GameServerSteamAPIActivated()
{
}

void MMSPlugin::AllPluginsLoaded()
{
}

void MMSPlugin::Hook_ClientActive( CPlayerSlot slot, bool bLoadGame, const char *pszName, uint64 xuid )
{
}

void MMSPlugin::Hook_ClientCommand( CPlayerSlot slot, const CCommand &args )
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);
	const auto scripts = ScriptExtensions::GetScripts();
	CGlobalSymbol callbackSymbol("OnClientCommand");
	for (CPointScript* scriptEnt : scripts)
	{
		const auto script = scriptEnt->GetScript();
		if (!script || !script->IsCallbackRegistered(callbackSymbol))
			continue;

		const auto v8Context = script->GetContext().Get(isolate);
		v8Context->Enter();

		v8::Local<v8::Number> v8Slot = v8::Number::New(isolate, slot.Get());
		v8::Local<v8::Array> v8ArgsArray = v8::Array::New(isolate, args.ArgC());
		for (int i = 0; i < args.ArgC(); i++)
		{
			v8ArgsArray->Set(isolate->GetCurrentContext(), i, v8::String::NewFromUtf8(isolate, args.Arg(i)).ToLocalChecked()).Check();
		}
		v8::Local<v8::Value> jsArgs[] = { v8Slot, v8ArgsArray };
		script->InvokeCallback(callbackSymbol, 2, jsArgs);
	}
}

void MMSPlugin::Hook_ClientSettingsChanged( CPlayerSlot slot )
{
}

void MMSPlugin::Hook_OnClientConnected( CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, const char *pszAddress, bool bFakePlayer )
{
}

bool MMSPlugin::Hook_ClientConnect( CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, bool unk1, CBufferString *pRejectReason )
{
	g_playerManager.OnPlayerConnect(slot);
	RETURN_META_VALUE(MRES_IGNORED, true);
}

void MMSPlugin::Hook_ClientPutInServer( CPlayerSlot slot, char const *pszName, int type, uint64 xuid )
{
}

void MMSPlugin::Hook_ClientDisconnect( CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid, const char *pszNetworkID )
{
	g_playerManager.OnPlayerDisconnect(slot);
}


// Snippet from: CS2Fixes
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
	if (cvar_enable_mapspawn_script.GetBool())
	{
		// Check if there's a raw js mapspawn in the game directory (maybe provided by server admin).
		// If not, then use the file from the asset system, which could be provided by an addon.
		const char* rawScriptPath = "scripts/mapspawn.js";
		auto scriptStream = OpenGameRelativeFile(rawScriptPath);
		
		auto point_script = (CPointScript*)addresses::CreateEntityByName("point_script", -1);
		if (!point_script)
			return;

		auto kv = new CEntityKeyValues();
		if (!kv)
			return;

		// TODO: check through IFilesystem if the vjs asset exists
		kv->SetString("targetname", "script_mapspawn");
		if (!scriptStream.good())
			kv->SetString("cs_script", "scripts/mapspawn.vjs");
		else
			kv->SetString("cs_script", "");

		addresses::DispatchSpawn(point_script, kv);
		const auto script = point_script->GetScript();

		if (!scriptStream.good())
		{
			if (script && !script->IsActive())
				point_script->Remove();
			return;
		}

		Log_Debug(g_logChanScript, "Loading %s script...", rawScriptPath);
		if (script)
		{
			std::string fileContents{ std::istreambuf_iterator<char>(scriptStream), std::istreambuf_iterator<char>() };
			g_scriptExtensions->RunScriptString(script, rawScriptPath, fileContents.c_str());
		}
		else
		{
			Log_Warning(g_logChanScript, "Failed to find script component on the point_script entity! mapspawn script will not be ran");
			point_script->Remove();
			return;
		}
	}
}

// remember the script path by its id for reloading purposes
static std::unordered_map<uint64_t, std::string> scriptPathMap;
void MMSPlugin::OnLevelShutdown()
{
	g_hudHintManager.CancelAllHintMessages();
	scriptPathMap.clear();
}

void MMSPlugin::Hook_SetGameSpawnGroupMgr(IGameSpawnGroupMgr* pSpawnGroupMgr)
{
	// This also resets our stored pointer on deletion, since null gets passed into this function, nice!
	g_pSpawnGroupMgr = (CSpawnGroupMgrGameSystem*)pSpawnGroupMgr;
}

void MMSPlugin::Hook_StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession* pSession, const char* pszMapName)
{
	GameEntitySystem()->AddListenerEntity(&g_entityListener);
	if (g_pNetworkServerService->GetIGameServer())
		g_iSetGameSpawnGroupMgrId = SH_ADD_HOOK(IServer, SetGameSpawnGroupMgr, g_pNetworkServerService->GetIGameServer(), SH_MEMBER(this, &MMSPlugin::Hook_SetGameSpawnGroupMgr), false);

	g_bHasTicked = false;
}

void MMSPlugin::Hook_DispatchConCommand(ConCommandRef cmd, const CCommandContext& context, const CCommand& command)
{
	auto isolate = v8::Isolate::GetCurrent();
	if (context.GetPlayerSlot() == -1)
		RETURN_META(MRES_HANDLED); // only handle client commands, not server console commands

	v8::HandleScope handleScope(isolate);
	const auto scripts = ScriptExtensions::GetScripts();
	CGlobalSymbol callbackSymbol("OnDispatchClientCommand");
	for (CPointScript* scriptEnt : scripts)
	{
		const auto script = scriptEnt->GetScript();
		if (!script || !script->IsCallbackRegistered(callbackSymbol))
			continue;

		const auto v8Context = script->GetContext().Get(isolate);
		v8Context->Enter();

		v8::Local<v8::Number> v8Slot = v8::Number::New(isolate, context.GetPlayerSlot().Get());
		v8::Local<v8::Array> v8ArgsArray = v8::Array::New(isolate, command.ArgC());
		for (int i = 0; i < command.ArgC(); i++)
		{
			v8ArgsArray->Set(isolate->GetCurrentContext(), i, v8::String::NewFromUtf8(isolate, command.Arg(i)).ToLocalChecked()).Check();
		}
		v8::Local<v8::Value> jsArgs[] = { v8Slot, v8ArgsArray };

		const auto result = script->InvokeCallback(callbackSymbol, 2, jsArgs);
		if (!result.IsEmpty() && result->IsBoolean() && !result->ToBoolean(isolate)->Value())
		{
			RETURN_META(MRES_SUPERCEDE);
		}
	}
}

void MMSPlugin::Hook_PostEvent(CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64* clients,
	INetworkMessageInternal* pEvent, const CNetMessage* pData, unsigned long nSize, NetChannelBufType_t bufType)
{
	auto scripts = ScriptExtensions::GetScripts();

	if (!scripts.empty())
	{
		NetMessageInfo_t* info = pEvent->GetNetMessageInfo();
		CBufferString scriptCallbackName("OnUserMessage:");
		scriptCallbackName.AppendFormat("%d", info->m_MessageId);
		CGlobalSymbol callbackSymbol(scriptCallbackName.Get());

		ScriptUserMessageInfo* userMessageInfo = nullptr;

		for (CPointScript* scriptEnt : scripts)
		{
			v8::HandleScope handleScope(v8::Isolate::GetCurrent());
			auto script = scriptEnt->GetScript();
			if (!script || !script->IsCallbackRegistered(callbackSymbol))
				continue;

			if (!userMessageInfo)
			{
				auto msg = const_cast<CNetMessage*>(pData)->ToPB<google::protobuf::Message>();
				userMessageInfo = new ScriptUserMessageInfo(msg, *clients, nullptr);
			}
			auto obj = ScriptUserMessage::CreateUserMessageInfoInstance(script, userMessageInfo);
			v8::Local<v8::Value> jsArgs[] = { obj };

			auto result = script->InvokeCallback(callbackSymbol, 1, jsArgs);
			// update recipients in case script modified them
			*const_cast<uint64*>(clients) = userMessageInfo->GetRecipients();
			// script returned false - clear all recipients (block).
			if (!result.IsEmpty() && result->IsBoolean() && !result->ToBoolean(v8::Isolate::GetCurrent())->Value())
			{
				*const_cast<uint64*>(clients) = 0;
			}
		}

		if(userMessageInfo)
			userMessageInfo->Invalidate();
	}
}

void MMSPlugin::Hook_CheckTransmit(CCheckTransmitInfo** ppInfoList, int infoCount, CBitVec<16384>& unionTransmitEdicts,
	CBitVec<16384>&, const Entity2Networkable_t** pNetworkables, const uint16* pEntityIndicies, int nEntities)
{
	VPROF("CCSScriptExtensions::Hook_CheckTransmit");
	auto ppInfoListC = (CCheckTransmitInfoCustom**)ppInfoList;

	for (int i = 0; i < infoCount; i++)
	{
		auto& pInfo = ppInfoListC[i];
		auto plrSlot = pInfo->m_nPlayerSlot;
		auto plr = g_playerManager.GetPlayer(plrSlot);
		if (!plr)
			continue;

		const auto& blockedTransmits = plr->GetBlockedEntityTransmits();
		for (auto entIndex : blockedTransmits)
		{
			pInfo->m_pTransmitEntity->Clear(entIndex.Get());
			// TODO: check for player pawns, and if they're dead or no, otherwise we might crash clients!
		}
	}
}

std::ifstream MMSPlugin::OpenGameRelativeFile(std::string_view relativePath)
{
	CBufferStringGrowable<256> gamedirpath;
	g_pEngineServer2->GetGameDir(gamedirpath);

	std::stringstream pathStream;
	pathStream << gamedirpath.Get();
	pathStream << "/" << relativePath;

	return std::ifstream(pathStream.str(), std::ios::in);
}

bool MMSPlugin::RunScriptFromFile(CCSScript_EntityScript* script, std::string_view relativePath, bool savePath)
{
	if (!script)
		return false;

	auto inFile = OpenGameRelativeFile(relativePath);
	if (inFile.good())
	{
		std::string fileContents{ std::istreambuf_iterator<char>(inFile), std::istreambuf_iterator<char>() };
		g_scriptExtensions->RunScriptString(script, relativePath.data(), fileContents.c_str());

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
		META_CONPRINT("Usage: csscript_load <script_file> [script_entity_name]\n");
		return;
	}

	if (!GameEntitySystem())
		return;

	const char* scriptName = "script";
	if (args.ArgC() > 2)
		scriptName = args[2];


	auto point_script = addresses::CreateEntityByName("point_script", -1);
	auto kv = new CEntityKeyValues();
	kv->SetString("cs_script", args[1]);
	kv->SetString("targetname", scriptName);
	addresses::DispatchSpawn(point_script, kv);

	auto script = ScriptExtensions::GetScriptFromEntity(point_script);
	if (!g_ThisPlugin.RunScriptFromFile(script, args[1], true))
	{
		point_script->Remove();
	}
}

CON_COMMAND_F(csscript_reload, "Reload a script (loaded by script_load only)", FCVAR_NONE)
{
	if (args.ArgC() < 2)
	{
		META_CONPRINT("Usage: csscript_reload <script_entity_name>\n");
		return;
	}

	if (!GameEntitySystem())
		return;

	CBaseEntity* ent = nullptr;
	while ((ent = UTIL_FindEntityByName(ent, args[1])))
	{
		auto script = ScriptExtensions::GetScriptFromEntity(ent);
		if (!script)
			continue;

		auto index = script->GetScriptIndex();
		if (scriptPathMap.contains(index))
		{
			g_ThisPlugin.RunScriptFromFile(script, scriptPathMap[index]);
		}
		else
		{
			META_CONPRINT("Provided script entity was not loaded through script_load\n");
		}
	}
}

CON_COMMAND_F(remove_scripts, "Remove all point_script entities", FCVAR_NONE)
{
	if (!GameEntitySystem())
		return;

	CBaseEntity* ent = nullptr;
	while ((ent = UTIL_FindEntityByClassname(ent, "point_script")))
	{
		ent->Remove();
	}
	scriptPathMap.clear();
}

// Currently non-functional
CON_COMMAND_F(script_run_code, "Run code inside an existing script", FCVAR_NONE)
{
	if (args.ArgC() < 3)
	{
		META_CONPRINT("Usage: script_code <script_entity_name> <js code>\n");
		return;
	}
	
	const auto isolate = v8::Isolate::GetCurrent();
	CBaseEntity* ent = nullptr;
	while ((ent = UTIL_FindEntityByName(ent, args[1])))
	{
		if (V_stricmp_fast(ent->GetClassname(), "point_script"))
			continue;

		if (const auto script = ScriptExtensions::GetScriptFromEntity(ent))
		{
			v8::HandleScope handleScope(isolate);
			const auto& scriptContext = script->GetContext().Get(isolate);
			//scriptContext->Enter();
			g_scriptExtensions->SwitchScriptContext(script);

			v8::Local<v8::Object> global = scriptContext->Global();
			v8::MaybeLocal<v8::Value> mbShellEval = global->Get(scriptContext, v8::String::NewFromUtf8(isolate, "__shellEval").ToLocalChecked());
			if (mbShellEval.IsEmpty())
			{
				META_CONPRINT("No shell eval function found on script context\n");
				return;
			}
			v8::Local<v8::Value> shellEvalLocal = mbShellEval.ToLocalChecked();
			if (!shellEvalLocal->IsFunction())
			{
				META_CONPRINT("Shell eval property is not a function\n");
				return;
			}
			v8::Local<v8::Function> shellEval = shellEvalLocal.As<v8::Function>();

			v8::Local<v8::Value> arg = v8::String::NewFromUtf8(isolate, args[2]).ToLocalChecked();
			v8::Local<v8::Value> argv[] = { arg };

			v8::TryCatch try_catch(isolate);
			v8::Local<v8::Value> result;
			if (!shellEval->Call(scriptContext, v8::Undefined(isolate), 1, argv).ToLocal(&result)) {
				v8::String::Utf8Value error(isolate, try_catch.Exception());
				PluginMsg("Error: %s\n", *error);
			}

			/*v8::TryCatch tryCatch(isolate);
			v8::ScriptOrigin origin(isolate, v8::String::NewFromUtf8(isolate, "console").ToLocalChecked());

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
			}*/
		}
	}
}

CON_COMMAND_F(script_summary, "List registered function templates on scripts", FCVAR_NONE)
{
	auto isolate = v8::Isolate::GetCurrent();
	for (CPointScript* scriptEnt : ScriptExtensions::GetScripts())
	{
		v8::HandleScope handleScope(isolate);
		if (const auto script = scriptEnt->GetScript())
			script->PrintSummary();
	}
}

