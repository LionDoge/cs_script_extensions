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
#include "addresses.h"
#include "entitysystem.h"
#include "globalsymbol.h"
#include "utlatuolist.h"
#include "v8-function.h"
#include "ehandle.h"
#include "entity/cbaseentity.h"
#include "toolsresourcelistener.h"
#include "igameevents.h"

class CCSBaseScript : public IEntityListener, public CUtlAutoList<CCSBaseScript>
{
public:
	// only game can construct this properly
	CCSBaseScript() = delete;
	CCSBaseScript(const CCSBaseScript&) = delete;
	void operator=(const CCSBaseScript&) = delete;

// These actually only appears between IEntityListener and the base class in the EntityScript derived class.
// I'm not sure what's going on here, likely some shenanigans with how Itanium ABI sets up vtables.
// For our use cases this works, since we're basically never interacting with a base class directly,
// as map scripts are the only types of script, for this moment at least.
#ifndef _WIN32
	virtual void unk01() = 0;
	virtual void unk02() = 0;
#endif

	virtual const char* GetName() const = 0;
	virtual CGlobalSymbol GetNameSymbol() = 0;
	// This is where all function templates, and enums are set up in this struct.
	// This is a good place to hook when the script gets created and initialized
	virtual void InitializeFunctionTemplates() = 0;
	// Just cleans up all the fields and deallocates memory. May also be called when reloading?
	virtual void UnloadScript() = 0;

	bool operator==(const CCSBaseScript& rhs) const;
	const v8::Global<v8::Context>& GetContext();
	uint64_t GetScriptIndex();
	bool IsActive() const;
	bool IsTypescript() const;
	void PrintSummary() const;

	// Registers a callback function for this script.
	// Callbacks are saved in a map by a string name, you may want to format the string if you need to specify arguments (like a entity input name).
	// For example, native code registers script entity input callbacks as: 'OnScriptInput:InpuNameHere'
	void AddCallback(CGlobalSymbol callbackName, v8::Local<v8::Function> callbackFunction);
	v8::Local<v8::Value> InvokeCallback(CGlobalSymbol callbackName, int argc, v8::Local<v8::Value> argv[]);
	bool IsCallbackRegistered(CGlobalSymbol callbackName) const;
	void AddFunctionTemplate(const char* name, const v8::Local<v8::FunctionTemplate>& functionTemplate);
	const v8::Global<v8::FunctionTemplate>* GetFunctionTemplate(CGlobalSymbol name) const;
	bool IsFunctionTemplateRegistered(CGlobalSymbol name) const;

private:
	// https://github.com/Wend4r/sourcesdk/blob/016a41630755cf86d650654c991069853418610a/public/entity2/entitysystem.h#L262
	struct MurmurHash2HashFunctor
	{
		unsigned int operator()(uint32 n) const { return MurmurHash2(&n, sizeof(uint32), 0x3501A674); }
	};

	uint64_t m_globalScriptIndex; // 0x20 (32) // this only seems to increase for new scripts, not reset during map change or when killing point_script, may actually not be 64bit.
	v8::Global<v8::Context> m_context; // 0x28 (40);
	bool m_isActive; // 0x30 (48); string: Invalid script. No valid imports found.\n
	bool m_isUsingLegacyTypescript; // 0x31 (49); Is using deprecated SourceTS feature set.
	// NOTE: this might not be just a CUtlString, inserting something here manually and letting the game deallocate, causes some kind of memory corruption.
	CUtlVector<CUtlString> m_registeredTypes; // 0x40 (64);  // NOLINT(clang-diagnostic-padded)
	CUtlHashtable<CGlobalSymbol, v8::Global<v8::FunctionTemplate>*, GlobalSymbolHashFunctor, PointerEqualFunctor> m_functionTemplateMap; // 0x50 (80);
	CUtlHashtable<CGlobalSymbol, v8::Global<v8::Object>*, GlobalSymbolHashFunctor, PointerEqualFunctor> m_enumMap; // 0x70 (112);
	CUtlHashtable<CGlobalSymbol, v8::Global<v8::Function>*, GlobalSymbolHashFunctor, PointerEqualFunctor> m_callbackMap; // 0x90 (144);
	CUtlHashtable<CEntityHandle, v8::Global<v8::Object>*, MurmurHash2HashFunctor> m_entityObjects; // 0xb0 (176);
	CUtlHashtable<void*, v8::Global<v8::Object>*, MurmurHash2HashFunctor> m_otherObjects; // Not fully known what this is for
};

// Entity script has potentially more fields that have not been reversed yet...
class CCSScript_EntityScript : public CCSBaseScript {
	IGameEventListener2* m_eventListener; // 0xf0 (240)
	// There seem to be more hashtables defined, but in practice I never saw them filled.
};

enum ScriptHandleType
{
	Invalid = 0,
	Module = 1, // Only seen used when initializing 'Domain' module. 
	Entity = 2,
	CSWeaponData = 3,
};

struct CSScriptHandle
{
	ScriptHandleType typeIdentifier;
	union {
		struct { // Entity handle
			uint32_t unk;
			CEntityHandle handle;
		};
		// Others unknown
	};
};

// above CSScript struct - (3 * 0x8)
// This is likely just a vtable of IToolsResourceListener which comes up all the way here cause of multiple inheritance.
// It's a good 'checkpoint' for other relevant data which seems to be included inline in the entity.
class CCSPointScriptEntity : public IToolsResourceListener
{
	const char* scriptPath;
	v8::Global<v8::String> scriptTextContents;
};