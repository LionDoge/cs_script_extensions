#pragma once
#include <iostream>
#include <ostream>
#include "platform.h"

// Likely just CUtlSymbolLarge utilizing a global symbol table.
class CGlobalSymbol
{
public:
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

inline const char* MakeGlobalSymbol(const char* str)
{
	// TODO: Move out of here, fix the linking.
	HMODULE hModule = GetModuleHandleA("tier0.dll");
	if (hModule == NULL) {
		std::cerr << "Failed to get module handle. Error: " << GetLastError() << std::endl;
	}
	FARPROC funcAddress = GetProcAddress(hModule, "_MakeGlobalSymbol");

	typedef const char* (*MakeGlobalSymbolFunc)(const char*);
	MakeGlobalSymbolFunc func = reinterpret_cast<MakeGlobalSymbolFunc>(funcAddress);
	return func(str);
}

PLATFORM_INTERFACE CGlobalSymbolCaseSensitive MakeGlobalSymbolCaseSensitive(const char* str);
PLATFORM_INTERFACE CGlobalSymbol FindGlobalSymbol(const char* str);
PLATFORM_INTERFACE CGlobalSymbol FindGlobalSymbolByHash(uint32 hash);