#include "pluginconfig.h"
#include <fstream>
#include "json.hpp";
#include "common.h"
using json = nlohmann::json;

PluginConfig* g_pluginConfig;

bool PluginConfig::Load(std::string_view path)
{
	std::ifstream configFile(path.data());
	if (!configFile.good())
		return false;

	json configJson;
	try
	{
		configJson = json::parse(configFile, nullptr, false, true);
	}
	catch (const json::parse_error& e)
	{
		Msg("[cs_script_ext] Failed to parse config file: %s\n", e.what());
		return false;
	}

	m_bDefaultFunctionsEnabled = configJson["defaultFunctionsEnabled"].get<bool>();
	m_bUserMessagesEnabled = configJson["userMessagesEnabled"].get<bool>();
	m_bSchemaReadEnabled = configJson["schemaReadEnabled"].get<bool>();
	m_bUserIdentificationEnabled = configJson["userIdentificationEnabled"].get<bool>();
	m_bTransmitStateChangeEnabled = configJson["transmitStateChangeEnabled"].get<bool>();
	m_bQueryConvarsEnabled = configJson["queryConvarsEnabled"].get<bool>();
	m_bClientNetworkRequestsEnabled = configJson["clientNetworkRequestsEnabled"].get<bool>();

	return true;
}
