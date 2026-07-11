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

#include <format>
#include "scriptExtensions/scriptDomainCallbacks.h"
#include "vprof_fix.h"
#include "v8.h"
#include "usermessages.pb.h"
#include "entity/ccsplayercontroller.h"
#include "interfaces/interfaces.h"
#include "networksystem/inetworkmessages.h"
#include "schemasystem/schemasystem.h"
#include "igameevents.h"
#include "hudhintmanager.h"
#include "recipientfilters.h"
#include "common.h"
#include "scriptExtensions/userMessagesScriptExt.h"
#include "playermanager.h"
#include "schema.h"
#include "convar.h"
#include "scriptcommon.h"

void ClientPrintAll(int hud_dest, const char* msg, ...);
extern HudHintManager g_hudHintManager;
extern ScriptExtensions g_scriptExtensions;
extern PlayerManager g_playerManager;

constexpr uint32_t schemaEntityInstanceKey = hash_32_fnv1a_const("CEntityInstance");
// use FNV hash, as that's what we already have.
std::unordered_map<uint32_t, uint32_t> specificClassNetworkableOffsets = {
	{hash_32_fnv1a_const("CAttributeManager"), 2},
	{hash_32_fnv1a_const("CEconItemAttribute"), 2},
	{hash_32_fnv1a_const("CAttributeContainer"), 2},
#ifdef _WIN32
	{hash_32_fnv1a_const("CEconItemView"), 27},
#else
	{hash_32_fnv1a_const("CEconItemView"), 28},
#endif
};

void ScriptDomainCallbacks::NewMsg(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	if (args.Length() < 1 || !args[0]->IsString())
	{
		V8ThrowException(args.GetIsolate(), "Method point_script.MsgCustom requires a string argument.");
		return;
	}
	v8::Local<v8::String> v8String = args[0].As<v8::String>();
	int length = v8String->Utf8Length(isolate);
	std::string cppString(length, '\0');
	v8String->WriteUtf8(isolate, &cppString[0], length);

	PluginMsg("CustomMsg: %s", cppString.c_str());
}

template<typename T>
inline constexpr bool always_false_v = false;

template <typename T>
constexpr void SetSchemaReturnValue(v8::ReturnValue<v8::Value>& returnValue, void* ptr, size_t offset)
{
	auto isolate = v8::Isolate::GetCurrent();
	auto val = *reinterpret_cast<std::add_pointer_t<T>>(reinterpret_cast<uintptr_t>(ptr) + offset);

	if constexpr (std::is_same_v<T, bool>)
	{
		returnValue.Set(v8::Boolean::New(isolate, val));
	}
	else if constexpr (std::is_arithmetic_v<T>)
	{
		returnValue.Set(v8::Number::New(isolate, val));
	}
	else if constexpr (std::is_same_v<T, CUtlString>)
	{
		if (val.Get())
			returnValue.Set(v8::String::NewFromUtf8(isolate, val.Get()).ToLocalChecked());
		else
			returnValue.Set(v8::String::NewFromUtf8Literal(isolate, ""));
	}
	else if constexpr (std::is_same_v<T, GameTime_t>)
	{
		returnValue.Set(v8::Number::New(isolate, val.GetTime()));
	}
	else if constexpr (std::is_same_v<T, CEntityHandle>)
	{
		if (val.IsValid())
		{
			auto obj = ScriptExtensions::GetInstance()->CreateEntityObjectAuto(val.Get());
			returnValue.Set(obj);
		}
	}
	else if constexpr (std::is_same_v<T, CEntityInstance*>)
	{
		returnValue.Set(ScriptExtensions::GetInstance()->CreateEntityObjectAuto(val));
	}
	else if constexpr (std::is_same_v<T, Vector>)
	{
		auto context = isolate->GetCurrentContext();
		auto obj = CreateVectorObject(context, val);
		returnValue.Set(obj);
	}
	else if constexpr (std::is_same_v<T, QAngle>)
	{
		auto context = isolate->GetCurrentContext();
		auto obj = CreateQAngleObject(context, val);
		returnValue.Set(obj);
	}
	else
	{
		static_assert(always_false_v<T>, "Unsupported type for SetSchemaReturnValue");
	}
}

template <typename T>
constexpr bool SetSchemaValue(const CallContext& context, v8::Local<v8::Context> v8context, const v8::Local<v8::Value>& value, void* ptr, size_t offset)
{
	auto isolate = v8::Isolate::GetCurrent();
	//auto val = *reinterpret_cast<std::add_pointer_t<T>>(reinterpret_cast<uintptr_t>(ptr) + offset);

	if constexpr (std::is_same_v<T, bool>)
	{
		*reinterpret_cast<std::add_pointer_t<bool>>(reinterpret_cast<uintptr_t>(ptr) + offset) = value->ToBoolean(isolate)->Value();
	}
	else if constexpr (std::is_arithmetic_v<T>)
	{
		auto maybeNumber = value->ToNumber(v8context);
		v8::Local<v8::Number> number;
		if (!maybeNumber.ToLocal(&number))
		{
			ThrowFunctionException(context, "Failed to convert value to number");
			return false;
		}
		*reinterpret_cast<std::add_pointer_t<T>>(reinterpret_cast<uintptr_t>(ptr) + offset) = static_cast<T>(number->Value());
	}
	else if constexpr (std::is_same_v<T, CUtlString>)
	{
		auto maybeString = value->ToString(v8context);
		v8::Local<v8::String> stringValue;
		if (!maybeString.ToLocal(&stringValue))
		{
			ThrowFunctionException(context, "Failed to convert value to string");
			return false;
		}
		const char* pStr = *v8::String::Utf8Value(isolate, stringValue);
		CUtlString newString(pStr);
		*reinterpret_cast<std::add_pointer_t<CUtlString>>(reinterpret_cast<uintptr_t>(ptr) + offset) = newString;
	}
	else if constexpr (std::is_same_v<T, CUtlSymbolLarge>)
	{
		auto maybeString = value->ToString(v8context);
		v8::Local<v8::String> stringValue;
		if (!maybeString.ToLocal(&stringValue))
		{
			ThrowFunctionException(context, "Failed to convert value to string");
			return false;
		}
		const char* pStr = *v8::String::Utf8Value(isolate, stringValue);
		CUtlSymbolLarge newString(pStr);
		*reinterpret_cast<std::add_pointer_t<CUtlSymbolLarge>>(reinterpret_cast<uintptr_t>(ptr) + offset) = newString;
	}
	else if constexpr (std::is_same_v<T, GameTime_t>)
	{
		auto maybeNumber = value->ToNumber(v8context);
		v8::Local<v8::Number> number;
		if (!maybeNumber.ToLocal(&number))
		{
			ThrowFunctionException(context, "Failed to convert value to number");
			return false;
		}
		*reinterpret_cast<std::add_pointer_t<GameTime_t>>(reinterpret_cast<uintptr_t>(ptr) + offset) = GameTime_t(number->Value());
	}
	// TODO: we should check if the templated EntityHandle type matches, otherwise we allow for setting potentially unsupported entity classes
	else if constexpr (std::is_same_v<T, CEntityHandle>)
	{
		// allow setting invalid handle
		if (value->IsNullOrUndefined())
		{
			*reinterpret_cast<std::add_pointer_t<CEntityHandle>>(reinterpret_cast<uintptr_t>(ptr) + offset) = CEntityHandle();
		}
		else
		{
			auto maybeObj = value->ToObject(v8context);
			v8::Local<v8::Object> obj;
			if (!maybeObj.ToLocal(&obj))
			{
				ThrowFunctionException(context, "Failed to convert value to object");
				return false;
			}

			auto ehandle = ExtractEntityHandleFromObject(isolate, obj);
			if (!ehandle)
			{
				ThrowFunctionException(context, "Provided object is not an entity handle");
				return false;
			}

			*reinterpret_cast<std::add_pointer_t<CEntityHandle>>(reinterpret_cast<uintptr_t>(ptr) + offset) = *ehandle;
		}
	}
	else if constexpr (std::is_same_v<T, CEntityInstance*>)
	{
		if (value->IsNullOrUndefined())
		{
			*reinterpret_cast<std::add_pointer_t<CEntityInstance*>>(reinterpret_cast<uintptr_t>(ptr) + offset) = nullptr;
		}
		else
		{
			auto maybeObj = value->ToObject(v8context);
			v8::Local<v8::Object> obj;
			if (!maybeObj.ToLocal(&obj))
			{
				ThrowFunctionException(context, "Failed to convert value to object");
				return false;
			}

			auto ehandle = ExtractEntityHandleFromObject(isolate, obj);
			if (!ehandle)
			{
				ThrowFunctionException(context, "Provided object is not an entity handle");
				return false;
			}

			*reinterpret_cast<std::add_pointer_t<CEntityInstance*>>(reinterpret_cast<uintptr_t>(ptr) + offset) = (*ehandle).Get();
		}
	}
	else if constexpr (std::is_same_v<T, Vector>)
	{
		auto maybeObj = value->ToObject(v8context);
		v8::Local<v8::Object> obj;
		if (!maybeObj.ToLocal(&obj))
		{
			ThrowFunctionException(context, "Failed to convert value to object");
			return false;
		}
		
		auto vec = ObjectToVector(v8context, obj);
		if (!vec)
		{
			ThrowFunctionException(context, "Provided object is not a Vector");
			return false;
		}

		*reinterpret_cast<std::add_pointer_t<Vector>>(reinterpret_cast<uintptr_t>(ptr) + offset) = *vec;
	}
	else if constexpr (std::is_same_v<T, QAngle>)
	{
		auto maybeObj = value->ToObject(v8context);
		v8::Local<v8::Object> obj;
		if (!maybeObj.ToLocal(&obj))
		{
			ThrowFunctionException(context, "Failed to convert value to object");
			return false;
		}

		auto ang = ObjectToQAngle(v8context, obj);
		if (!ang)
		{
			ThrowFunctionException(context, "Provided object is not a QAngle");
			return false;
		}
		
		*reinterpret_cast<std::add_pointer_t<QAngle>>(reinterpret_cast<uintptr_t>(ptr) + offset) = *ang;
	}
	else
	{
		static_assert(always_false_v<T>, "Unsupported type for SetSchemaValue");
		return false;
	}

	return true;
}

void ScriptSetReturnChainedSchemaKey(
	const CallContext& context,
	const v8::Local<v8::Context>& v8context,
	v8::ReturnValue<v8::Value>& returnValue,
	const v8::Local<v8::Array>& fieldChainArray,
	void* obj,
	uint32_t arrayIndex, 
	const SchemaKey& schemaFieldInfo
)
{
	VPROF_BUDGET(__func__, "CSScriptExtensions")
	auto isolate = v8::Isolate::GetCurrent();
	auto offset = schemaFieldInfo.offset;
	// No more fields provided.
	if (arrayIndex + 1 >= fieldChainArray->Length())
	{
		switch (schemaFieldInfo.keyType) {
		case SchemaKeyType::Int8: SetSchemaReturnValue<int8_t>(returnValue, obj, offset); break;
		case SchemaKeyType::Uint8: SetSchemaReturnValue<uint8_t>(returnValue, obj, offset); break;
		case SchemaKeyType::Int16: SetSchemaReturnValue<int16_t>(returnValue, obj, offset); break;
		case SchemaKeyType::Uint16: SetSchemaReturnValue<uint16_t>(returnValue, obj, offset); break;
		case SchemaKeyType::Int32: SetSchemaReturnValue<int32_t>(returnValue, obj, offset); break;
		case SchemaKeyType::Uint32: SetSchemaReturnValue<uint32_t>(returnValue, obj, offset); break;
		case SchemaKeyType::Int64: SetSchemaReturnValue<int64_t>(returnValue, obj, offset); break;
		case SchemaKeyType::Uint64: SetSchemaReturnValue<uint64_t>(returnValue, obj, offset); break;
		case SchemaKeyType::Float32: SetSchemaReturnValue<float>(returnValue, obj, offset); break;
		case SchemaKeyType::Float64: SetSchemaReturnValue<double>(returnValue, obj, offset); break;
		case SchemaKeyType::Bool: SetSchemaReturnValue<bool>(returnValue, obj, offset); break;
		case SchemaKeyType::UtlSymbolLarge: // stored the same way as CUtlString for read
		case SchemaKeyType::UtlString: SetSchemaReturnValue<CUtlString>(returnValue, obj, offset); break;
		case SchemaKeyType::GameTime: SetSchemaReturnValue<GameTime_t>(returnValue, obj, offset); break;
		case SchemaKeyType::EntityHandle: SetSchemaReturnValue<CEntityHandle>(returnValue, obj, offset); break;
		case SchemaKeyType::Entity: SetSchemaReturnValue<CEntityInstance*>(returnValue, obj, offset); break;
		case SchemaKeyType::Vector: SetSchemaReturnValue<Vector>(returnValue, obj, offset); break;
		case SchemaKeyType::QAngle: SetSchemaReturnValue<QAngle>(returnValue, obj, offset); break;
		default: ThrowFunctionException(context, "This field is unsupported for direct access in scripts"); return;
		}

		return;
	}

	// Likely a component class, so try recursing. Requires the next field name to be given
	if (schemaFieldInfo.typeCategory == SCHEMA_TYPE_POINTER || schemaFieldInfo.typeCategory == SCHEMA_TYPE_DECLARED_CLASS)
	{
		auto val = fieldChainArray->Get(v8context, arrayIndex + 1);
		if (val.IsEmpty() || !val.ToLocalChecked()->IsString())
		{
			ThrowFunctionException(context, std::format("Expected string at index {} in field chain array", arrayIndex + 1));
			return;
		}

		// Adjust pointer, if underlying type is a pointer, deref it, if it's inline, then just add the offset.
		void* newPtr = obj;
		if (schemaFieldInfo.typeCategory == SCHEMA_TYPE_POINTER)
		{
			newPtr = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(obj) + offset);
		}
		else
		{
			newPtr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(obj) + offset);
		}

		auto nextFieldName = val.ToLocalChecked().As<v8::String>();
		const char* nextFieldNameStr = *v8::String::Utf8Value(isolate, nextFieldName);
		auto nextFiledNameHash = hash_32_fnv1a_const(nextFieldNameStr);

		SchemaClassInfoData_t* classInfoHandle = schemaFieldInfo.classType.Get();
		// Technically this should be set for the current types we are expecting.
		if (!classInfoHandle)
		{
			ThrowFunctionException(context, "Schema cache is missing class information for type that's declared to be a class");
			return;
		}
		SchemaKey nextSchemaKey{};
		do
		{
			// TODO: Add schema caching by class info handles to avoid having to redundantly hash class on every access.
			// For now we are just retrofitting it into the existing system.
			auto className = classInfoHandle->m_pszName;
			nextSchemaKey = schema::GetOffset(className, hash_32_fnv1a_const(className), "", nextFiledNameHash);

			if (!classInfoHandle->m_nBaseClassCount)
				break;

			classInfoHandle = classInfoHandle->m_pBaseClasses[0].m_pClass;
		} while (nextSchemaKey.typeCategory == SCHEMA_TYPE_INVALID);

		// Haven't found the next field in the inheritance chain.
		if (nextSchemaKey.typeCategory == SCHEMA_TYPE_INVALID)
		{
			ThrowFunctionException(context, std::format("field '{}' not found in schema class def '{}' or its ancestors", nextFieldNameStr, classInfoHandle->m_pszName));
			return;
		}

		ScriptSetReturnChainedSchemaKey(
			context,
			v8context,
			returnValue,
			fieldChainArray,
			newPtr,
			arrayIndex + 1,
			nextSchemaKey
		);
	}
	else
	{
		ThrowFunctionException(context, "This schema field's type is not supported in script");
		return;
	}
}


void ScriptSetChainedSchemaKeyValue(
	const CallContext& context,
	const v8::Local<v8::Context>& v8context,
	const v8::Local<v8::Value>& value,
	const v8::Local<v8::Array>& fieldChainArray,
	void* obj,
	SchemaMetaInfoHandle_t<SchemaClassInfoData_t> containingClass,
	uint32_t arrayIndex,
	const SchemaKey& schemaFieldInfo
)
{
	VPROF_BUDGET(__func__, "CSScriptExtensions")
	auto isolate = v8::Isolate::GetCurrent();
	auto offset = schemaFieldInfo.offset;
	// No more fields provided.
	if (arrayIndex + 1 >= fieldChainArray->Length())
	{
		bool success = false;
		switch (schemaFieldInfo.keyType) {
		case SchemaKeyType::Int8: success = SetSchemaValue<int8_t>(context, v8context, value, obj, offset); break;
		case SchemaKeyType::Uint8: success = SetSchemaValue<uint8_t>(context, v8context, value, obj, offset); break;
		case SchemaKeyType::Int16: success = SetSchemaValue<int16_t>(context, v8context, value, obj, offset); break;
		case SchemaKeyType::Uint16: success = SetSchemaValue<uint16_t>(context, v8context, value, obj, offset); break;
		case SchemaKeyType::Int32: success = SetSchemaValue<int32_t>(context, v8context, value, obj, offset); break;
		case SchemaKeyType::Uint32: success = SetSchemaValue<uint32_t>(context, v8context, value, obj, offset); break;
		case SchemaKeyType::Int64: success = SetSchemaValue<int64_t>(context, v8context, value, obj, offset); break;
		case SchemaKeyType::Uint64: success = SetSchemaValue<uint64_t>(context, v8context, value, obj, offset); break;
		case SchemaKeyType::Float32: success = SetSchemaValue<float>(context, v8context, value, obj, offset); break;
		case SchemaKeyType::Float64: success = SetSchemaValue<double>(context, v8context, value, obj, offset); break;
		case SchemaKeyType::Bool: success = SetSchemaValue<bool>(context, v8context, value, obj, offset); break;
		case SchemaKeyType::UtlSymbolLarge: SetSchemaValue<CUtlSymbolLarge>(context, v8context, value, obj, offset); break;
		case SchemaKeyType::UtlString: success = SetSchemaValue<CUtlString>(context, v8context, value, obj, offset); break;
		case SchemaKeyType::GameTime: success = SetSchemaValue<GameTime_t>(context, v8context, value, obj, offset); break;
		case SchemaKeyType::EntityHandle: success = SetSchemaValue<CEntityHandle>(context, v8context, value, obj, offset); break;
		case SchemaKeyType::Entity: success = SetSchemaValue<CEntityInstance*>(context, v8context, value, obj, offset); break;
		case SchemaKeyType::Vector: success = SetSchemaValue<Vector>(context, v8context, value, obj, offset); break;
		case SchemaKeyType::QAngle: success = SetSchemaValue<QAngle>(context, v8context, value, obj, offset); break;
		default: ThrowFunctionException(context, "This field is unsupported for direct access in scripts"); return;
		}

		if (success && schemaFieldInfo.networked)
		{
			const char* containingClassName = containingClass.Get()->m_pszCPPName;
			uint32_t containingClassNameHash = hash_32_fnv1a_const(containingClassName);
			auto chainOffs = schema::FindChainOffset(containingClassName, containingClassNameHash);

			if (chainOffs != 0)
			{
				::ChainNetworkStateChanged(reinterpret_cast<uintptr_t>(obj) + chainOffs, schemaFieldInfo.offset);
			}
			else
			{
				if (specificClassNetworkableOffsets.contains(containingClassNameHash))
				{
					::NetworkVarStateChanged(reinterpret_cast<uintptr_t>(obj), schemaFieldInfo.offset, specificClassNetworkableOffsets[containingClassNameHash]);
				}
				else
				{
					// no chain offset? Check if it's inline class (not entity one), as it has different network update offset
					// TODO: this info could also be potentially cached for performance in hot loops.
					SchemaClassInfoData_t* baseClassInfo = containingClass.Get();
					while (baseClassInfo->m_nBaseClassCount)
					{
						baseClassInfo = baseClassInfo->m_pBaseClasses[0].m_pClass;
					}

					if (hash_32_fnv1a_const(baseClassInfo->m_pszCPPName) == schemaEntityInstanceKey)
					{
						::EntityNetworkStateChanged(reinterpret_cast<uintptr_t>(obj), schemaFieldInfo.offset);
					}
					else
					{
						::NetworkVarStateChanged(reinterpret_cast<uintptr_t>(obj), schemaFieldInfo.offset, 1);
					}

				}
			}
		}
		return;
	}

	// Likely a component class, so try recursing. Requires the next field name to be given
	if (schemaFieldInfo.typeCategory == SCHEMA_TYPE_POINTER || schemaFieldInfo.typeCategory == SCHEMA_TYPE_DECLARED_CLASS)
	{
		auto val = fieldChainArray->Get(v8context, arrayIndex + 1);
		if (val.IsEmpty() || !val.ToLocalChecked()->IsString())
		{
			ThrowFunctionException(context, std::format("Expected string at index {} in field chain array", arrayIndex + 1));
			return;
		}

		// Adjust pointer, if underlying type is a pointer, deref it, if it's inline, then just add the offset.
		void* newPtr = obj;
		if (schemaFieldInfo.typeCategory == SCHEMA_TYPE_POINTER)
		{
			newPtr = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(obj) + offset);
		}
		else
		{
			newPtr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(obj) + offset);
		}

		auto nextFieldName = val.ToLocalChecked().As<v8::String>();
		const char* nextFieldNameStr = *v8::String::Utf8Value(isolate, nextFieldName);
		auto nextFiledNameHash = hash_32_fnv1a_const(nextFieldNameStr);

		SchemaClassInfoData_t* classInfoHandle = schemaFieldInfo.classType.Get();
		// Technically this should be set for the current types we are expecting.
		if (!classInfoHandle)
		{
			ThrowFunctionException(context, "Schema cache is missing class information for type that's declared to be a class");
			return;
		}
		SchemaKey nextSchemaKey{};
		do
		{
			// TODO: Add schema caching by class info handles to avoid having to redundantly hash class on every access.
			// For now we are just retrofitting it into the existing system.
			auto className = classInfoHandle->m_pszName;
			nextSchemaKey = schema::GetOffset(className, hash_32_fnv1a_const(className), "", nextFiledNameHash);

			if (!classInfoHandle->m_nBaseClassCount)
				break;

			classInfoHandle = classInfoHandle->m_pBaseClasses[0].m_pClass;
		} while (nextSchemaKey.typeCategory == SCHEMA_TYPE_INVALID);

		// Haven't found the next field in the inheritance chain.
		if (nextSchemaKey.typeCategory == SCHEMA_TYPE_INVALID)
		{
			ThrowFunctionException(context, std::format("field '{}' not found in schema class def '{}' or its ancestors", nextFieldNameStr, classInfoHandle->m_pszName));
			return;
		}

		if (auto classInfo = schemaFieldInfo.classType; classInfo.Get() != nullptr)
		{
			ScriptSetChainedSchemaKeyValue(
				context,
				v8context,
				value,
				fieldChainArray,
				newPtr,
				classInfo,
				arrayIndex + 1,
				nextSchemaKey
			);
		}
		else
		{
			// keep previous classname
			ScriptSetChainedSchemaKeyValue(
				context,
				v8context,
				value,
				fieldChainArray,
				newPtr,
				schemaFieldInfo.classType.Get(),
				arrayIndex + 1,
				nextSchemaKey
			);
		}
	}
	else
	{
		ThrowFunctionException(context, "This schema field's type is not supported in script");
		return;
	}
}

void ScriptDomainCallbacks::GetSchemaField(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	VPROF_BUDGET(__func__, "CSScriptExtensions")
	SCRIPT_SETUP(args);

	auto v8context = ScriptExtensions::GetCurrentCsScriptInstance()->GetContext().Get(isolate);
	auto targetEntHandle = UnwrapThis<CEntityHandle>(context);

	if (!targetEntHandle)
		return;

	if (!targetEntHandle->IsValid())
	{
		ThrowFunctionException(context, "invalid entity handle of 'this' object");
		return;
	}

	auto ent = targetEntHandle->Get();
	if (!ent)
	{
		ThrowFunctionException(context, "called on invalid entity instance.");
		return;
	}
	
	if (context.args.Length() <= 0)
	{
		ThrowFunctionException(context, "argument 0 is required as a string or string[]");
		return;
	}

	// for compatibility with non-array param
	v8::Local<v8::Array> fieldArray;
	const char* firstFireldName = nullptr;
	if (args[0]->IsString())
	{
		fieldArray = v8::Array::New(isolate, 1);
		fieldArray->Set(v8context, 0, args[0]).Check();
	}
	else if (args[0]->IsArray())
	{
		fieldArray = args[0].As<v8::Array>();
	}
	else
	{
		ThrowFunctionException(context, "argument 0 must be a string or string[]");
		return;
	}

	auto schemaDyn = ent->Schema_DynamicBinding();
	auto classInfoHandle = schemaDyn.Get();
	if (!classInfoHandle)
	{
		Log_Warning(g_logChanScript, "GetSchemaField: Entity does not have schema binding information");
		return;
	}

	auto originalClassName = classInfoHandle->m_pszName;
	auto fieldName = fieldArray->Get(v8context, 0).ToLocalChecked()->ToString(v8context).ToLocalChecked();
	auto fieldNameStr = *v8::String::Utf8Value(isolate, fieldName);
	auto fieldNameHash = hash_32_fnv1a_const(fieldNameStr);

	// First, find the field in this or base classes if possible
	SchemaKey schemaKey{};
	do
	{
		// TODO: Add schema caching by class info handles to avoid having to redundantly hash class on every access.
		// For now we are just retrofitting it into the existing system.
		auto className = classInfoHandle->m_pszName;
		schemaKey = schema::GetOffset(className, hash_32_fnv1a_const(className), "", fieldNameHash);

		if (!classInfoHandle->m_nBaseClassCount)
			break;

		classInfoHandle = classInfoHandle->m_pBaseClasses[0].m_pClass;
	} while (schemaKey.typeCategory == SCHEMA_TYPE_INVALID);

	// Did not find anything, just throw.
	if (schemaKey.typeCategory == SCHEMA_TYPE_INVALID)
	{
		ThrowFunctionException(context, std::format("field '{}' not found in schema class def '{}' or its ancestors", fieldNameStr, originalClassName));
		return;
	}

	// Try returning the field, or delegate next searches through the field array, 
	// if it ends up at a valid field with a basic type then the field will be set as a return value.
	auto returnValue = args.GetReturnValue();
	ScriptSetReturnChainedSchemaKey(
		context,
		v8context,
		returnValue,
		fieldArray,
		(void*)ent,
		0,
		schemaKey
	);
}

void ScriptDomainCallbacks::SetSchemaField(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	VPROF_BUDGET(__func__, "CSScriptExtensions")
	SCRIPT_SETUP(args);

	auto v8context = ScriptExtensions::GetCurrentCsScriptInstance()->GetContext().Get(isolate);
	auto targetEntHandle = UnwrapThis<CEntityHandle>(context);

	if (!targetEntHandle)
		return;

	if (!targetEntHandle->IsValid())
	{
		ThrowFunctionException(context, "invalid entity handle of 'this' object");
		return;
	}

	auto ent = targetEntHandle->Get();
	if (!ent)
	{
		ThrowFunctionException(context, "called on invalid entity instance.");
		return;
	}

	if (context.args.Length() <= 0)
	{
		ThrowFunctionException(context, "argument 0 is required as a string or string[]");
		return;
	}

	if (context.args.Length() <= 1)
	{
		ThrowFunctionException(context, "argument 1 is required - the value to set");
		return;
	}

	// for compatibility with non-array param
	v8::Local<v8::Array> fieldArray;
	const char* firstFireldName = nullptr;
	if (args[0]->IsString())
	{
		fieldArray = v8::Array::New(isolate, 1);
		fieldArray->Set(v8context, 0, args[0]).Check();
	}
	else if (args[0]->IsArray())
	{
		fieldArray = args[0].As<v8::Array>();
	}
	else
	{
		ThrowFunctionException(context, "argument 0 must be a string or string[]");
		return;
	}

	auto schemaDyn = ent->Schema_DynamicBinding();
	auto classInfoHandle = schemaDyn.Get();
	if (!classInfoHandle)
	{
		Log_Warning(g_logChanScript, "SetSchemaField: Entity does not have schema binding information");
		return;
	}

	auto originalClassName = classInfoHandle->m_pszName;
	auto fieldName = fieldArray->Get(v8context, 0).ToLocalChecked()->ToString(v8context).ToLocalChecked();
	auto fieldNameStr = *v8::String::Utf8Value(isolate, fieldName);
	auto fieldNameHash = hash_32_fnv1a_const(fieldNameStr);

	// First, find the field in this or base classes if possible
	SchemaKey schemaKey{};
	do
	{
		// TODO: Add schema caching by class info handles to avoid having to redundantly hash class on every access.
		// For now we are just retrofitting it into the existing system.
		auto className = classInfoHandle->m_pszName;
		schemaKey = schema::GetOffset(className, hash_32_fnv1a_const(className), "", fieldNameHash);

		if (!classInfoHandle->m_nBaseClassCount)
			break;

		classInfoHandle = classInfoHandle->m_pBaseClasses[0].m_pClass;
	} while (schemaKey.typeCategory == SCHEMA_TYPE_INVALID);

	// Did not find anything, just throw.
	if (schemaKey.typeCategory == SCHEMA_TYPE_INVALID)
	{
		ThrowFunctionException(context, std::format("field '{}' not found in schema class def '{}' or its ancestors", fieldNameStr, originalClassName));
		return;
	}

	// Try setting the filed, or delegate next searches through the field array, 
	// if it ends up at a valid field with a basic type then the field will be set with the value.
	ScriptSetChainedSchemaKeyValue(
		context,
		v8context,
		args[1],
		fieldArray,
		(void*)ent,
		classInfoHandle,
		0,
		schemaKey
	);
}

void ScriptDomainCallbacks::ShowHudHintAll(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto message = UnwrapArg<std::string>(context, 0);
	auto isAlert = UnwrapArg<bool>(context, 1, true).value_or(false);

	if (!message)
		return;

	ClientPrintAll(isAlert ? HUD_PRINTALERT : HUD_PRINTCENTER, message->c_str());
}

void ScriptDomainCallbacks::AddSampleCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);
	auto script = (CCSScript_EntityScript*)ScriptExtensions::GetCurrentCsScriptInstance();

	if(args.Length() < 1 || !args[0]->IsFunction())
	{
		ThrowFunctionException(context, "First argument must be a function.");
		return;
	}
	auto callback = args[0].As<v8::Function>();
	script->AddCallback("OnSampleCallback", callback);
}

void ScriptDomainCallbacks::SetEntityMoveType(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto entHandle = UnwrapThis<CEntityHandle>(context);
	auto moveType = UnwrapArg<uint8>(context, 0, false);
	if (!entHandle || !moveType)
		return;

	if (entHandle->IsValid())
		static_cast<CBaseEntity*>(entHandle->Get())->SetMoveType(static_cast<MoveType_t>(*moveType));
}

void ScriptDomainCallbacks::EmitSound(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args)
	auto v8Context = isolate->GetCurrentContext();

	if (args.Length() != 1)
	{
		ThrowFunctionException(context, "requires an object with EmitSound info");
		return;
	}
	if (!args[0]->IsObject())
	{
		ThrowFunctionException(context, "argument 0 must be an object {soundName: string, source?: Entity, volume?: number, pitch?: number, recipients?: CSPlayerController[]}\n");
		return;
	}

	auto obj = args[0]->ToObject(v8Context).ToLocalChecked();
	auto maybeSoundName = obj->Get(v8Context, v8::String::NewFromUtf8(isolate, "soundName").ToLocalChecked());
	if (maybeSoundName.IsEmpty())
		return;
	auto soundName = maybeSoundName.ToLocalChecked();
	if (!soundName->IsString())
	{
		ThrowFunctionException(context, "argument 0.msgName must be a string\n");
		return;
	}
	v8::String::Utf8Value soundNameUtf8(isolate, soundName);
	auto maybeSourceEntity = obj->Get(v8Context, v8::String::NewFromUtf8(isolate, "source").ToLocalChecked());
	// entIndex -1 should be fine, it will just play globally.
	CEntityIndex entIndex = -1;
	if (!maybeSourceEntity.IsEmpty())
	{
		auto sourceEntityVal = maybeSourceEntity.ToLocalChecked();
		if (!sourceEntityVal->IsUndefined())
		{
			if (!sourceEntityVal->IsObject())
			{
				V8ThrowException(isolate, "EmitSound argument 0.source must be an Entity");
				return;
			}
			auto sourceEntityObj = sourceEntityVal.As<v8::Object>();
			CEntityHandle entHandle = ScriptExtensions::GetEntityHandleFromScriptObject(sourceEntityObj);
			if (!entHandle.IsValid())
			{
				ThrowFunctionException(context, "argument 0.source is not a valid Entity");
				return;
			}
			entIndex = entHandle.GetEntryIndex();
		}
	}

	float volume = 1.0f;
	auto maybeVolume = obj->Get(v8Context, v8::String::NewFromUtf8(isolate, "volume").ToLocalChecked());
	if (!maybeVolume.IsEmpty())
	{
		auto volumeVal = maybeVolume.ToLocalChecked();
		if (!volumeVal->IsUndefined())
		{
			if (!volumeVal->IsNumber())
			{
				ThrowFunctionException(context, "argument 0.volume must be a number");
				return;
			}
			volume = static_cast<float>(volumeVal.As<v8::Number>()->Value());
		}

		if (volume < 0.0f)
			volume = 0.0f;
	}

	int pitch = 1;
	auto maybePitch = obj->Get(v8Context, v8::String::NewFromUtf8(isolate, "pitch").ToLocalChecked());
	if (!maybePitch.IsEmpty())
	{
		auto pitchVal = maybePitch.ToLocalChecked();
		if (!pitchVal->IsUndefined())
		{
			if (!pitchVal->IsNumber())
			{
				ThrowFunctionException(context, "argument 0.pitch must be a number");
				return;
			}
			pitch = static_cast<int>(pitchVal.As<v8::Number>()->Value());
		}
			
		if (pitch < 0)
			pitch = 0;
	}

	CRecipientFilter filter;
	auto maybeRecipientsArr = obj->Get(v8Context, v8::String::NewFromUtf8(isolate, "recipients").ToLocalChecked());
	if (!maybeRecipientsArr.IsEmpty())
	{
		if (const auto recipientsVal = maybeRecipientsArr.ToLocalChecked(); recipientsVal->IsArray())
		{
			auto recipientsArr = recipientsVal.As<v8::Array>();
			uint32_t length = recipientsArr->Length();
			for (uint32_t i = 0; i < length; ++i)
			{
				auto recipientVal = recipientsArr->Get(v8Context, i).ToLocalChecked();
				if (!recipientsVal->IsUndefined() && !recipientVal->IsObject())
				{
					ThrowFunctionException(context, "argument 0.recipients must be an array of CSPlayerController objects");
					return;
				}
				auto recipientObj = recipientVal.As<v8::Object>();
				auto recipientEntHandle = ScriptExtensions::GetEntityHandleFromScriptObject(recipientObj);
				if (!recipientEntHandle.IsValid())
					continue;

				auto recipientController = static_cast<CCSPlayerController*>(recipientEntHandle.Get());
				if (!recipientController || !recipientController->IsController())
				{
					ThrowFunctionException(context, "argument 0.recipients must be an array of CSPlayerController objects");
					return;
				}
				filter.AddRecipient(recipientController->GetPlayerSlot());
			}
		}
		else
		{
			if (recipientsVal->IsUndefined())
			{
				filter.AddAllPlayers();
			}
			else
			{
				ThrowFunctionException(context, "argument 0.recipients must be an array of CSPlayerController objects");
				return;
			}
		}

	}
	EmitSound_t emitSoundInfo;
	emitSoundInfo.m_pSoundName = *soundNameUtf8;
	emitSoundInfo.m_flVolume = volume;
	emitSoundInfo.m_nPitch = static_cast<int>(pitch);
	addresses::CBaseEntity_EmitSoundFilter(filter, entIndex, emitSoundInfo);
}

void ScriptDomainCallbacks::SetTransmitState(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);
	auto v8Context = isolate->GetCurrentContext();

	auto targetEnt = UnwrapThis<CEntityHandle>(context);
	auto plrEnt = UnwrapArg<CEntityHandle>(context, 0, false);
	auto state = UnwrapArg<bool>(context, 1, false);

	if (!targetEnt || !plrEnt || !state)
		return;

	if (!targetEnt->IsValid())
	{
		ThrowFunctionException(context, "failed to get entity from 'this' object.");
		return;
	}

	auto targetEntValue = static_cast<CBaseEntity*>(targetEnt->Get());
	if (!targetEntValue || targetEntValue->IsController())
	{
		ThrowFunctionException(context, "can not change transmit state on a player controller");
		return;
	}

	if (!plrEnt->IsValid())
	{
		ThrowFunctionException(context, "target player entity is not valid");
		return;
	}

	auto targetPlr = static_cast<CCSPlayerController*>(plrEnt->Get());
	if (!targetPlr || !targetPlr->IsController())
	{
		ThrowFunctionException(context, "target player entity is not a player controller");
		return;
	}

	auto entIndex = targetEnt->GetEntryIndex();
	g_playerManager.SetEntityTransmitBlocked(targetPlr->GetPlayerSlot(), entIndex, !*state);
}

void ScriptDomainCallbacks::SetTransmitStateAll(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto targetEntHandle = UnwrapThis<CEntityHandle>(context);
	auto state = UnwrapArg<bool>(context, 0, false);

	if(!targetEntHandle || !state)
		return;

	if (!targetEntHandle->IsValid())
	{
		ThrowFunctionException(context, "failed to get entity from 'this' object.");
		return;
	}
	
	auto entity = static_cast<CBaseEntity*>(targetEntHandle->Get());
	if (!entity || entity->IsController())
	{
		ThrowFunctionException(context, "can not set transmit state on player controllers");
		return;
	}

	auto entIndex = targetEntHandle->GetEntryIndex();
	g_playerManager.SetEntityTransmitBlockedForAll(entIndex, !*state);
}

void ScriptDomainCallbacks::GetConVarValue(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);
	auto v8Context = isolate->GetCurrentContext();

	auto cvarName = UnwrapArg<std::string>(context, 0);
	if (!cvarName)
		return;

	ConVarRefAbstract cvar(cvarName->c_str());
	if (!cvar.IsValidRef())
	{
		ThrowFunctionException(context, std::format("ConVar {} does not exist", *cvarName));
		return;
	}

	if (!cvar.IsConVarDataAvailable())
	{
		ThrowFunctionException(context, std::format("ConVar {} was registered partially, it's not possible to retrieve it's data", *cvarName));
		return;
	}

	if (cvar.IsFlagSet(FCVAR_PROTECTED) || cvar.IsFlagSet(FCVAR_DONTRECORD))
	{
		ThrowFunctionException(context, std::format("ConVar {} is protected or marked as 'dontrecord', its value cannot be accessed", *cvarName));
		return;
	}

	auto cvarType = cvar.GetType();
	switch (cvarType)
	{
	case EConVarType_Bool:
		args.GetReturnValue().Set(v8::Boolean::New(isolate, cvar.GetBool())); break;
	case EConVarType_Int16:
	case EConVarType_UInt16:
	case EConVarType_Int32:
	case EConVarType_UInt32:
	case EConVarType_Int64:
	case EConVarType_UInt64:
		args.GetReturnValue().Set(v8::Number::New(isolate, cvar.GetInt())); break;
	case EConVarType_Float32:
	case EConVarType_Float64:
		args.GetReturnValue().Set(v8::Number::New(isolate, cvar.GetFloat())); break;
	
	case EConVarType_Vector3:
	{
		auto vec = cvar.GetAs<Vector>();
		auto vecObj = CreateVectorObject(v8Context, vec);
		args.GetReturnValue().Set(vecObj);
		break;
	}
	case EConVarType_Qangle:
	{
		auto ang = cvar.GetAs<QAngle>();
		auto angObj = CreateQAngleObject(v8Context, ang);
		args.GetReturnValue().Set(angObj);
		break;
	}
	case EConVarType_Color:
	{
		auto clr = cvar.GetAs<Color>();
		auto clrObj = CreateColorObject(v8Context, clr);
		args.GetReturnValue().Set(clrObj);
		break;
	}
	case EConVarType_String:
	default: // some types aren't handled here, just return as string.
		args.GetReturnValue().Set(v8::String::NewFromUtf8(isolate, cvar.GetString()).ToLocalChecked());
	}
}

void ScriptDomainCallbacks::PrintToChatAll(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto message = UnwrapArg<std::string>(context, 0);

	if (!message)
		return;

	ClientPrintAll(HUD_PRINTTALK, message->c_str());
}

void ScriptDomainCallbacks::OnDispatchClientCommand(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto callbackVal = UnwrapArg<v8::Local<v8::Value>>(context, 0);
	if (!callbackVal)
		return;

	if (!(*callbackVal)->IsFunction())
	{
		ThrowFunctionException(context, "provided argument must be a function");
		return;
	}

	auto callback = (*callbackVal).As<v8::Function>();
	const auto script = ScriptExtensions::GetCurrentCsScriptInstance();
	if(script)
		script->AddCallback("OnDispatchClientCommand", callback);
}

void ScriptDomainCallbacks::OnClientCommand(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto callbackVal = UnwrapArg<v8::Local<v8::Value>>(context, 0);
	if (!callbackVal)
		return;

	if (!(*callbackVal)->IsFunction())
	{
		ThrowFunctionException(context, "provided argument must be a function");
		return;
	}

	auto callback = (*callbackVal).As<v8::Function>();
	const auto script = ScriptExtensions::GetCurrentCsScriptInstance();
	if(script)
		script->AddCallback("OnClientCommand", callback);
}

void ScriptDomainCallbacks::CreateEntity(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto config = UnwrapArg<v8::Local<v8::Value>>(context, 0);
	if (!config)
		return;

	if (!(*config)->IsObject())
	{
		ThrowFunctionException(context, "argument 0 must be an object {classname: string, origin: Vector, keyValues?: EntityKeyValues }");
		return;
	}

	auto v8Context = ScriptExtensions::GetCurrentCsScriptInstance()->GetContext().Get(isolate);
	const auto obj = (*config)->ToObject(v8Context).ToLocalChecked();
	auto maybeClassName = obj->Get(v8Context, v8::String::NewFromUtf8(isolate, "classname").ToLocalChecked());
	if (maybeClassName.IsEmpty())
	{
		ThrowFunctionException(context, "config.classname is missing");
		return;
	}

	auto className = maybeClassName.ToLocalChecked();
	if (!className->IsString())
	{
		ThrowFunctionException(context, "config.classname must be a string");
		return;
	}
	auto classNameUtf8 = v8::String::Utf8Value(isolate, className);

	auto maybeOrigin = obj->Get(v8Context, v8::String::NewFromUtf8(isolate, "origin").ToLocalChecked());
	if (maybeOrigin.IsEmpty())
	{
		ThrowFunctionException(context, "config.origin is missing");
		return;
	}

	auto originVal = maybeOrigin.ToLocalChecked();
	if (!originVal->IsObject())
	{
		ThrowFunctionException(context, "config.origin must be a Vector");
		return;
	}
	auto origin = ObjectToVector(v8Context, originVal.As<v8::Object>());
	if (!origin)
	{
		ThrowFunctionException(context, "config.origin must be a Vector");
		return;
	}

	auto maybeKeyValues = obj->Get(v8Context, v8::String::NewFromUtf8(isolate, "keyValues").ToLocalChecked());
	CEntityKeyValues* pKeyValues = nullptr;
	if (!maybeKeyValues.IsEmpty())
	{
		auto keyValues = maybeKeyValues.ToLocalChecked();
		if (!keyValues->IsObject())
		{
			ThrowFunctionException(context, "config.keyValues must be an object");
			return;
		}
		auto kvObject = keyValues->ToObject(v8Context).ToLocalChecked();
		auto props = kvObject->GetOwnPropertyNames(v8Context).ToLocalChecked();

		pKeyValues = new CEntityKeyValues();
		for (int i = 0; i < props->Length(); i++)
		{
			auto propNameVal = props->Get(v8Context, i).ToLocalChecked();
			if (!propNameVal->IsString())
				continue;

			auto propName = v8::String::Utf8Value(isolate, propNameVal);
			auto propValue = kvObject->Get(v8Context, propNameVal).ToLocalChecked();

			if (propValue->IsString())
			{
				pKeyValues->SetString(*propName, *v8::String::Utf8Value(isolate, propValue));
			}
			else if (propValue->IsNumber())
			{
				pKeyValues->SetDouble(*propName, propValue.As<v8::Number>()->Value());
			}
			else if (propValue->IsBoolean())
			{
				pKeyValues->SetBool(*propName, propValue.As<v8::Boolean>()->Value());
			}
			else if (propValue->IsObject())
			{
				auto propObject = propValue->ToObject(v8Context).ToLocalChecked();
				// if the object doesn't have the correct fields for a type it will fall through to the correct one, or none
				if (auto vecObj = ObjectToVector(v8Context, propObject); vecObj.has_value())
				{
					pKeyValues->SetVector(*propName, *vecObj);
				}
				else if (auto angObj = ObjectToQAngle(v8Context, propObject); angObj.has_value())
				{
					pKeyValues->SetQAngle(*propName, *angObj);
				}
				else if (auto clrObj = ObjectToColor(v8Context, propObject); clrObj.has_value())
				{
					pKeyValues->SetColor(*propName, *clrObj);
				}
				else
				{
					Log_Debug(g_logChanScript, "CreateEntity: type of KeyValue %s does not match any compatible type, skipping.\n", *propName);
				}
			}
			else
			{
				Log_Debug(g_logChanScript, "CreateEntity: type of KeyValue %s is not supported, skipping.\n", *propName);
			}
		}
	}

	auto entity = addresses::CreateEntityByName(*classNameUtf8, -1);
	if (entity)
	{
		addresses::DispatchSpawn(entity, pKeyValues);
		entity->SetAbsOrigin(*origin);
		auto entObj = ScriptExtensions::GetInstance()->CreateEntityObjectAuto(entity);
		args.GetReturnValue().Set(entObj);
	}
}

void ScriptDomainCallbacks::CreateUserMessage(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto messageName = UnwrapArg<std::string>(context, 0);

	if (!messageName)
		return;

	INetworkMessageInternal* pNetMsg = g_pNetworkMessages->FindNetworkMessagePartial(messageName->c_str());
	if(!pNetMsg)
	{
		ThrowFunctionException(context, std::format("failed to find message with name '{}'", *messageName));
		return;
	}

	auto data = pNetMsg->AllocateMessage()->ToPB<google::protobuf::Message>();
	if(!data)
	{
		ThrowFunctionException(context, std::format("failed to allocate message for '{}'", *messageName));
		return;
	}

	ScriptUserMessageInfo* msgInfo = new ScriptUserMessageInfo(data, 0, pNetMsg);
	auto script = (CCSScript_EntityScript*)ScriptExtensions::GetCurrentCsScriptInstance();
	auto msg = ScriptUserMessage::CreateUserMessageInfoInstance(script, msgInfo);
	args.GetReturnValue().Set(msg);
}