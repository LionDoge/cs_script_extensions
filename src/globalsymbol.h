#ifndef GLOBALSYMBOL_H
#define GLOBALSYMBOL_H

#ifdef _WIN32
#pragma once
#endif

#include "platform.h"


class CGlobalSymbol
{
public:
	CGlobalSymbol(const char* str) { m_handle = str; }
	operator const char*() const { return Get(); }
	const char* Get() const { return m_handle; }
	uint32_t Hash() const { return *(uint32_t*)(m_handle - 4); }
	bool operator==(const CGlobalSymbol& rhs) const { return m_handle == rhs.Get(); }
private:
	const char* m_handle;
};

using CGlobalSymbolCaseSensitive = CGlobalSymbol;

struct GlobalSymbolHashFunctor
{
	inline unsigned int operator()(const CGlobalSymbol& globalSymbol) const;
};
inline unsigned int GlobalSymbolHashFunctor::operator()(const CGlobalSymbol& globalSymbol) const
{
	return globalSymbol.Hash();
}

PLATFORM_INTERFACE CGlobalSymbol FindGlobalSymbolByHash(uint32 hash);
PLATFORM_INTERFACE CGlobalSymbol FindGlobalSymbol(const char* str);
//PLATFORM_INTERFACE CGlobalSymbol MakeGlobalSymbol(const char* str);
inline CGlobalSymbol MakeGlobalSymbol(const char* str)
{
	// TODO: Move out of here, fix the linking.
	HMODULE hModule = GetModuleHandleA("tier0.dll");
	if (hModule == NULL) {
		std::cerr << "Failed to get module handle. Error: " << GetLastError() << std::endl;
	}
	FARPROC funcAddress = GetProcAddress(hModule, "_MakeGlobalSymbol");

	typedef const char* (*MakeGlobalSymbolFunc)(const char*);
	MakeGlobalSymbolFunc func = reinterpret_cast<MakeGlobalSymbolFunc>(funcAddress);
	return {func(str)};
}
PLATFORM_INTERFACE CGlobalSymbolCaseSensitive MakeGlobalSymbolCaseSensitive(const char* str);

#endif // GLOBALSYMBOL_H