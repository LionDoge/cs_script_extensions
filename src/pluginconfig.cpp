#include "pluginconfig.h"
#include <fstream>
#include "json.hpp";
#include "common.h"
using json = nlohmann::json;

PluginConfig g_pluginConfig;

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
		PluginMsg("[Failed to parse config file: %s\n", e.what());
		return false;
	}

	m_bDefaultFunctionsEnabled = configJson.value("defaultFunctionsEnabled", false);
	m_bUserMessagesEnabled = configJson.value("userMessagesEnabled", false);
	m_bSchemaReadEnabled = configJson.value("schemaReadEnabled", false);
	m_bSchemaWriteEnabled = configJson.value("schemaWriteEnabled", false); // Default to false if the key is not present
	m_bUserIdentificationEnabled = configJson.value("userIdentificationEnabled", false);
	m_bTransmitStateChangeEnabled = configJson.value("transmitStateChangeEnabled", false);
	m_bQueryConvarsEnabled = configJson.value("queryConvarsEnabled", false);
	m_bClientNetworkRequestsEnabled = configJson.value("clientNetworkRequestsEnabled", false);

	return true;
}
