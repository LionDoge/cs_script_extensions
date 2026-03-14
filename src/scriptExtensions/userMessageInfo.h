/**
 * =============================================================================
 * cs_script_extensions
 * Copyright (C) 2026 liondoge
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

#pragma once
#include <networksystem/netmessage.h>

#define IS_FIELD_REPEATED()                                       \
    if (field->label() != google::protobuf::FieldDescriptor::LABEL_REPEATED) \
        return false;                                                \

#define IS_FIELD_NOT_REPEATED()                                   \
    if (field->label() == google::protobuf::FieldDescriptor::LABEL_REPEATED) \
        return false;                                                \

#define GET_FIELD()															\
	auto field = m_pMessage->GetDescriptor()->FindFieldByName(fieldName); \
	if (!field)                                                                                   \
		return false;                                                                             \

class ScriptUserMessageInfo
{
public:
	ScriptUserMessageInfo(CNetMessagePB<google::protobuf::Message>* pMessage, uint64_t recipients, INetworkMessageInternal* messageInteral)
		: m_pMessage(pMessage), m_pNetMessageInternal(messageInteral), m_recipients(recipients)
	{
	}
	~ScriptUserMessageInfo() = default;
	bool GetFieldType(const char* fieldName, google::protobuf::FieldDescriptor::CppType& out, bool& outIsRepeated) const
	{
	 	GET_FIELD()
		outIsRepeated = (field->label() == google::protobuf::FieldDescriptor::LABEL_REPEATED);
	 	out = field->cpp_type();
		return true;
	}
	bool GetString(const char* fieldName, std::string& out) const
	{
		GET_FIELD()
		IS_FIELD_NOT_REPEATED()
		out = m_pMessage->GetReflection()->GetString(*m_pMessage, field);
		return true;
	}
	bool GetBool(const char* fieldName, bool& out) const
	{
		GET_FIELD()
		IS_FIELD_NOT_REPEATED()
		out = m_pMessage->GetReflection()->GetBool(*m_pMessage, field);
		return true;
	}
	bool SetString(const char* fieldName, const std::string& val) const
	{
		GET_FIELD()
		IS_FIELD_NOT_REPEATED()
		m_pMessage->GetReflection()->SetString(m_pMessage, field, val);
		return true;
	}
	bool GetInt32(const char* fieldName, int32_t& out) const
	{
		GET_FIELD()
		IS_FIELD_NOT_REPEATED()
		out = m_pMessage->GetReflection()->GetInt32(*m_pMessage, field);
		return true;
	}
	bool SetInt32(const char* fieldName, int32_t val) const
	{
		GET_FIELD()
		IS_FIELD_NOT_REPEATED()
		m_pMessage->GetReflection()->SetInt32(m_pMessage, field, val);
		return true;
	}
	bool GetUInt32(const char* fieldName, uint32_t& out) const
	{
		GET_FIELD()
		IS_FIELD_NOT_REPEATED()
		out = m_pMessage->GetReflection()->GetUInt32(*m_pMessage, field);
		return true;
	}
	bool SetUInt32(const char* fieldName, uint32_t val) const
	{
		GET_FIELD()
		IS_FIELD_NOT_REPEATED()
		m_pMessage->GetReflection()->SetUInt32(m_pMessage, field, val);
		return true;
	}
	bool GetInt64(const char* fieldName, int64_t& out) const
	{
		GET_FIELD()
			IS_FIELD_NOT_REPEATED()
			out = m_pMessage->GetReflection()->GetInt64(*m_pMessage, field);
		return true;
	}
	bool SetInt64(const char* fieldName, int64_t val) const
	{
		GET_FIELD()
			IS_FIELD_NOT_REPEATED()
			m_pMessage->GetReflection()->SetInt64(m_pMessage, field, val);
		return true;
	}
	bool GetUInt64(const char* fieldName, uint64_t& out) const
	{
		GET_FIELD()
		IS_FIELD_NOT_REPEATED()
		out = m_pMessage->GetReflection()->GetUInt64(*m_pMessage, field);
		return true;
	}
	bool SetUInt64(const char* fieldName, uint64_t val) const
	{
		GET_FIELD()
		IS_FIELD_NOT_REPEATED()
		m_pMessage->GetReflection()->SetUInt64(m_pMessage, field, val);
		return true;
	}
	bool GetDouble(const char* fieldName, double& out) const
	{
		GET_FIELD()
		IS_FIELD_NOT_REPEATED()
		out = m_pMessage->GetReflection()->GetDouble(*m_pMessage, field);
		return true;
	}
	bool GetFloat(const char* fieldName, float& out) const
	{
		GET_FIELD()
			IS_FIELD_NOT_REPEATED()
			out = m_pMessage->GetReflection()->GetFloat(*m_pMessage, field);
		return true;
	}
	bool SetDouble(const char* fieldName, double val) const
	{
		GET_FIELD()
		IS_FIELD_NOT_REPEATED()
		m_pMessage->GetReflection()->SetDouble(m_pMessage, field, val);
		return true;
	}
	bool SetFloat(const char* fieldName, float val) const
	{
		GET_FIELD()
			IS_FIELD_NOT_REPEATED()
			m_pMessage->GetReflection()->SetFloat(m_pMessage, field, val);
		return true;
	}
	bool SetBool(const char* fieldName, bool val) const
	{
		GET_FIELD()
		IS_FIELD_NOT_REPEATED()
		m_pMessage->GetReflection()->SetBool(m_pMessage, field, val);
		return true;
	}
	bool SetEnum(const char* fieldName, int val) const
	{
		GET_FIELD()
		IS_FIELD_NOT_REPEATED()
			/*google::protobuf::DescriptorPool
		m_pMessage->GetReflection()->SetEnum(m_pMessage, field, google::protobuf::EnumValueDescriptor());*/
		return true;
	}
	void ClearRecipients()
	{
		m_recipients = 0;
	}
	void AddRecipient(uint64_t recipient)
	{
		m_recipients |= recipient;
	}
	void AddAllRecipients()
	{
		m_recipients = 0xFFFFFFFFFFFFFFFF;
	}
	void RemoveRecipient(uint64_t recipient)
	{
		m_recipients &= ~recipient;
	}
	uint64_t GetRecipients() const
	{
		return m_recipients;
	}
	CNetMessagePB<google::protobuf::Message>* GetMessage() const
	{
		return m_pMessage;
	}
	INetworkMessageInternal* GetNetMessageInternal() const
	{
		return m_pNetMessageInternal;
	}
	void PostSend()
	{
		delete m_pMessage;
		m_pMessage = nullptr;
		m_pNetMessageInternal = nullptr;
	}
	bool CanBeSent() const
	{
		// this will both cover the scenario where this object was created from a callback (null message internal) 
		// as well as the scenario where the message was already sent.
		return m_pMessage != nullptr && m_pNetMessageInternal != nullptr;
	}
private:
	CNetMessagePB<google::protobuf::Message>* m_pMessage;
	INetworkMessageInternal* m_pNetMessageInternal; // Only used when sending!
	uint64_t m_recipients;
};
