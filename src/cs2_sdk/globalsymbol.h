#pragma once
#include <iostream>
#include <ostream>
#include "platform.h"

#ifndef _WIN32
#include <dlfcn.h>
#endif

// Likely just CUtlSymbolLarge utilizing a global symbol table.
class CGlobalSymbol
{
public:
	CGlobalSymbol() = default;
	CGlobalSymbol(const char* str);

	uint32_t Hash() const;
	const char* Get() const;
	operator const char* () const;
	bool operator==(const CGlobalSymbol& rhs) const;
private:
	const char* m_handle;
};

using CGlobalSymbolCaseSensitive = CGlobalSymbol;

struct GlobalSymbolHashFunctor
{
	unsigned int operator() (const CGlobalSymbol& globalSymbol) const;
};

namespace {
	typedef const char* (*MakeGlobalSymbolFunc)(const char*);
	inline static MakeGlobalSymbolFunc _makeGlobalSymbolFunc = nullptr;
}

inline const char* MakeGlobalSymbol(const char* str)
{
	// TODO: Move out of here, fix the linking.
#ifdef _WIN32
	if (!_makeGlobalSymbolFunc)
	{
		HMODULE hModule = GetModuleHandleA("tier0.dll");
		if (hModule == NULL) {
			std::cerr << "Failed to get module handle. Error: " << GetLastError() << std::endl;
			return nullptr;
		}
		FARPROC funcAddress = GetProcAddress(hModule, "_MakeGlobalSymbol");

		_makeGlobalSymbolFunc = reinterpret_cast<MakeGlobalSymbolFunc>(funcAddress);
	}
	return _makeGlobalSymbolFunc(str);
#else
	if (!_makeGlobalSymbolFunc)
	{
		void* hModule = dlopen("libtier0.so", RTLD_LAZY);
		if (hModule == NULL) {
			std::cerr << "Failed to load library. Error: " << dlerror() << std::endl;
		}
		void* funcAddress = dlsym(hModule, "_MakeGlobalSymbol");
		if (funcAddress == NULL) {
			std::cerr << "Failed to find symbol. Error: " << dlerror() << std::endl;
		}

		_makeGlobalSymbolFunc = reinterpret_cast<MakeGlobalSymbolFunc>(funcAddress);
	}
	return _makeGlobalSymbolFunc(str);
#endif
}

PLATFORM_INTERFACE CGlobalSymbolCaseSensitive MakeGlobalSymbolCaseSensitive(const char* str);
PLATFORM_INTERFACE CGlobalSymbol FindGlobalSymbol(const char* str);
PLATFORM_INTERFACE CGlobalSymbol FindGlobalSymbolByHash(uint32 hash);