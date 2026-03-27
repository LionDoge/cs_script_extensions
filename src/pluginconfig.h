#pragma once
#include <string_view>

class PluginConfig {
public:
	PluginConfig() = default;
	PluginConfig(const PluginConfig&) = delete;
	bool Load(std::string_view path);

	bool AreDefaultFunctionsEnabled() const { return m_bDefaultFunctionsEnabled; }
	bool AreUserMessagesEnabled() const { return m_bUserMessagesEnabled; }
	bool IsSchemaReadEnabled() const { return m_bSchemaReadEnabled; }
	bool IsUserIdentificationEnabled() const { return m_bUserIdentificationEnabled; }
	bool IsTransmitStateChangeEnabled() const { return m_bTransmitStateChangeEnabled; }
	bool IsQueryConvarsEnabled() const { return m_bQueryConvarsEnabled; }
	bool AreClientNetworkRequestsEnabled() const { return m_bClientNetworkRequestsEnabled; }

private:
	bool m_bDefaultFunctionsEnabled = true;
	bool m_bUserMessagesEnabled = true;
	bool m_bSchemaReadEnabled = true;
	bool m_bUserIdentificationEnabled = true;
	bool m_bTransmitStateChangeEnabled = true;
	bool m_bQueryConvarsEnabled = true;
	bool m_bClientNetworkRequestsEnabled = true;
};

extern PluginConfig* g_pluginConfig;