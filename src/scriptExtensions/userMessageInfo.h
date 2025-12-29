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
	ScriptUserMessageInfo(CNetMessagePB<google::protobuf::Message>* pMessage, const uint64_t* recipients)
		: m_pMessage(pMessage), m_recipients(recipients)
	{
	}
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
	bool SetDouble(const char* fieldName, double val) const
	{
		GET_FIELD()
		IS_FIELD_NOT_REPEATED()
		m_pMessage->GetReflection()->SetDouble(m_pMessage, field, val);
		return true;
	}
private:
	CNetMessagePB<google::protobuf::Message>* m_pMessage;
	const uint64_t* m_recipients;
};
