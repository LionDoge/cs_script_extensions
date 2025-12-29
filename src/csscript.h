#pragma once
#include "addresses.h"
#include "entitysystem.h"
#include "v8-function.h"
// idk, looks like a linked list since it looks to be iterated that way
//template <typename T>
//class CUtlAutoList {
//	uintptr_t someGlobalPtr;
//	uintptr_t unk01;
//	uintptr_t unk02;
//	T* nextElem;
//};

//struct CSScriptCallBackInfo
//{
//	void* idk1;
//	void* idk2;
//	//void* cbFunction; // probably v8 func
//};
//static_assert(sizeof(CSScriptCallBackInfo) == 16);

class CCSBaseScript : IEntityListener {};

class CCSScript_EntityScript : public CCSBaseScript {
private:
	byte pad0[0x18];
public:
	uint64_t globalScriptIndex; // 0x20 (32) // this only seems to increase for new scripts, not reset during map change or when killing point_script.
	v8::Global<v8::Context> context; // 0x28 (40);
	uint64_t unknownLowNumber; // 0x30 (48);
	CUtlVector< CUtlString > registeredTypes; // 0x40 (64);
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