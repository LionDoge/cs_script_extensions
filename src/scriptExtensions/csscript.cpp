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

#include "csscript.h"
#include <vprof.h>
#include "scriptextensions.h"
#include "v8.h"

extern LoggingChannelID_t g_logChanScript;
extern ScriptExtensions* g_scriptExtensions;

const v8::Global<v8::Context>& CCSBaseScript::GetContext()
{
	return m_context;
}

uint64_t CCSBaseScript::GetScriptIndex()
{
	return m_globalScriptIndex;
}

void CCSBaseScript::AddCallback(CGlobalSymbol callbackName, v8::Local<v8::Function> callbackFunction)
{
	if (m_callbackMap.HasElement(callbackName))
	{
		Log_Warning(g_logChanScript, "Callback \"%s\" already registered\n", callbackName.Get());
		return;
	}
	const auto newFunc = new v8::Global<v8::Function>(v8::Isolate::GetCurrent(), callbackFunction);
	m_callbackMap.Insert(callbackName, newFunc);
}

v8::Local<v8::Value> CCSBaseScript::InvokeCallback(CGlobalSymbol callbackName, int argc, v8::Local<v8::Value> argv[])
{
	VPROF_BUDGET("CCSBaseScript::InvokeCallback", "cs_script callbacks");

	auto isolate = v8::Isolate::GetCurrent();
	v8::EscapableHandleScope handleScope(isolate);
	const auto funcHandle = m_callbackMap.Get(callbackName, nullptr);
	if (!funcHandle)
		return {};
	const auto func = funcHandle->Get(isolate);
	const auto context = m_context.Get(isolate);
	context->Enter();
	g_scriptExtensions->SwitchScriptContext(this);
	v8::TryCatch tryCatch(isolate);
	auto result = func->Call(context, context->Global(), argc, argv);
	if (tryCatch.HasCaught())
	{
		v8::Local<v8::Value> exception = tryCatch.Exception();
		v8::String::Utf8Value exception_str(isolate, exception);
		Log_Warning(g_logChanScript, "Exception during cs_script callback \"%s\"\n%s\n", callbackName.Get(), *exception_str);
		return {};
	}
	context->Exit();
	if (result.IsEmpty())
		return {};
	return handleScope.Escape(result.ToLocalChecked());
}

bool CCSBaseScript::IsCallbackRegistered(CGlobalSymbol callbackName) const
{
	return m_callbackMap.HasElement(callbackName);
}

void CCSBaseScript::PrintSummary() const
{
	auto isolate = v8::Isolate::GetCurrent();
	auto context = m_context.Get(isolate);
	Msg("%s [%d] [%s]", GetName(), m_globalScriptIndex, m_isActive ? "active": "inactive" );
	if (m_isUsingLegacyTypescript)
		Msg(" (legacy .vts)");
	Msg("\n");
	Msg("  Callbacks:\n");
	FOR_EACH_HASHTABLE(m_callbackMap, i)
	{
		auto key = m_callbackMap.Key(i);
		Msg("   - %s\n", key.Get());
	}
	Msg("  Function templates:\n");
	FOR_EACH_HASHTABLE(m_functionTemplateMap, i)
	{
		auto key = m_functionTemplateMap.Key(i);
		Msg("   - %s\n", key.Get());
	}
	Msg("  All registered types (including enums):\n");
	FOR_EACH_VEC(m_registeredTypes, i)
	{
		auto val = m_registeredTypes.Element(i);
		Msg("   - %s\n", val.Get());
	}
	Msg("  Enumerators:\n");
	FOR_EACH_HASHTABLE(m_enumMap, i)
	{
		auto key = m_enumMap.Key(i);
		Msg("   - %s\n", key.Get());
		auto val = m_enumMap.Element(i);
		auto obj = val->Get(isolate);
		auto properties = obj->GetOwnPropertyNames(context).ToLocalChecked();
		for (uint32_t j = 0; j < properties->Length(); j++)
		{
			auto propKey = properties->Get(context, j).ToLocalChecked();
			v8::String::Utf8Value utf8Key(isolate, propKey);
			auto propVal = obj->Get(context, propKey).ToLocalChecked();
			if (propVal->IsNumber())
			{
				int numVal = propVal->Int32Value(context).ToChecked();
				Msg("     - %s = %d\n", *utf8Key, numVal);
			}
		}
	}
	Msg("  Entity objects:\n");
	FOR_EACH_HASHTABLE(m_entityObjects, i)
	{
		auto key = m_entityObjects.Key(i);
		if (auto ent = key.Get())
		{
			Msg("   - %s - \"%s\"\n", ent->m_pEntity->GetClassname(), ent->m_pEntity->m_name.String());
		}
		// TODO: print stored values
	}
	Msg("  Other objects:\n");
	FOR_EACH_HASHTABLE(m_otherObjects, i)
	{
		auto key = m_otherObjects.Key(i);
	}
}

void CCSBaseScript::AddFunctionTemplate(const char* name, const v8::Local<v8::FunctionTemplate>& functionTemplate)
{
	// Use the signature game function, as apparently trying to insert ourselves leads to some kind of memory corruption
	// after the script is deallocated.
	// TODO: figure out why. m_registeredTypes field has to be also populated with the template name apparently
	// just inserting into the map is not enough
	g_scriptExtensions->ScriptRegisterFunctionTemplate(this, name, functionTemplate);
}

const v8::Global<v8::FunctionTemplate>* CCSBaseScript::GetFunctionTemplate(CGlobalSymbol name) const
{
	return m_functionTemplateMap.Get(name, nullptr);
}

bool CCSBaseScript::IsFunctionTemplateRegistered(CGlobalSymbol name) const
{
	return m_functionTemplateMap.HasElement(name);
}

bool CCSBaseScript::operator==(const CCSBaseScript& rhs) const
{
	return this->m_globalScriptIndex == rhs.m_globalScriptIndex;
}
