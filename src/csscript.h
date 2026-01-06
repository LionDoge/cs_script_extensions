#pragma once
#include "addresses.h"
#include "entitysystem.h"
#include "globalsymbol.h"
#include "utlatuolist.h"
#include "v8-function.h"

class CCSBaseScript : public IEntityListener, public CUtlAutoList<CCSBaseScript>
{
public:
	// only game can construct this properly
	CCSBaseScript() = delete;
	CCSBaseScript(const CCSBaseScript&) = delete;
	void operator=(const CCSBaseScript&) = delete;

	virtual const char* GetName() const = 0;
	virtual CGlobalSymbol GetNameSymbol() = 0;
	virtual void InitializeFunctionTemplates() = 0;
	virtual void UnloadScript() = 0; // just cleans up all the fields and deallocates memory.

	bool operator==(const CCSBaseScript& rhs) const;
	const v8::Global<v8::Context>& GetContext();
	uint64_t GetScriptIndex();
	void PrintSummary() const;

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
	CUtlVector<CUtlString> m_registeredTypes; // 0x40 (64);  // NOLINT(clang-diagnostic-padded)
	CUtlHashtable<CGlobalSymbol, v8::Global<v8::FunctionTemplate>*, GlobalSymbolHashFunctor, PointerEqualFunctor> m_functionTemplateMap; // 0x50 (80);
	CUtlHashtable<CGlobalSymbol, v8::Global<v8::Object>*, GlobalSymbolHashFunctor, PointerEqualFunctor> m_enumMap; // 0x70 (112);
	CUtlHashtable<CGlobalSymbol, v8::Global<v8::Function>*, GlobalSymbolHashFunctor, PointerEqualFunctor> m_callbackMap; // 0x90 (144);
	CUtlHashtable<CEntityHandle, v8::Global<v8::Object>*, MurmurHash2HashFunctor> m_entityObjects; // 0xb0 (176);
	CUtlHashtable<void*, v8::Global<v8::Object>*, MurmurHash2HashFunctor> m_otherObjects;
};

class CCSScript_EntityScript : public CCSBaseScript {};

enum ScriptHandleType
{
	Invalid = 0,
	Module = 1, // Only seen used when initializing 'Domain' module. 
	Entity = 2,
	GenericObject = 3, // only seen used for CSWeaponData
};

struct CSScriptEntityHandle
{
	ScriptHandleType typeIdentifier;
	uint32_t unk;
	CEntityHandle handle; // TODO: depending on the handle type it's not always an entity handle
};

// above CSScript struct - (3 * 0x8)
struct CSScriptHeader
{
	void* toolsResourceListener;
	CUtlString scriptPath;
	v8::Global<v8::String> scriptTextContents;
};