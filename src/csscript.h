#pragma once
#include "addresses.h"
#include "entitysystem.h"
#include "utlatuolist.h"
#include "v8-function.h"

// I haven't reversed what is the second vtable...
class FakeVtbl
{
	void* vtbl1;
};

class CCSBaseScript : public IEntityListener, public FakeVtbl, public CUtlAutoList<CCSBaseScript>
{
	
};

class CCSScript_EntityScript : public CCSBaseScript {
public:
	uint64_t globalScriptIndex; // 0x20 (32) // this only seems to increase for new scripts, not reset during map change or when killing point_script, may actually not be 64bit.
	v8::Global<v8::Context> context; // 0x28 (40);
	bool isInitialized; // 0x30 (48); string: Invalid script. No valid imports found.\n
	CUtlVector< CUtlString > registeredTypes; // 0x40 (64);  // NOLINT(clang-diagnostic-padded)
	CUtlHashtable< CUtlString, v8::Global<v8::FunctionTemplate>*, CaselessStringHashFunctor > functionTemplateMap; // 0x50 (80);
	CUtlHashtable< CUtlString, v8::Global<v8::Object>*, CaselessStringHashFunctor > enumMap; // 0x70 (112);
	CUtlHashtable< CUtlString, v8::Global<v8::Function>*, CaselessStringHashFunctor > callbackMap; // 0x90 (144);
};

struct CSScriptEntityHandle
{
	uint32_t typeIdentifier; // looks like it's equal to 2 usually?
	uint32_t unk;
	CEntityHandle handle;
};

// above CSScript struct - (3 * 0x8)
struct CSScriptHeader
{
	void* toolsResourceListener;
	CUtlString scriptPath;
	CUtlString** scriptTextContents;
};