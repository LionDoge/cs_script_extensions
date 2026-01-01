#pragma once
#include "platform.h"

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

#pragma comment(linker, "/alternatename:__imp_MakeGlobalSymbol=_MakeGlobalSymbol")
#pragma comment(linker, "/alternatename:__imp_MakeGlobalSymbolCaseSensitive=_MakeGlobalSymbolCaseSensitive")
#pragma comment(linker, "/alternatename:__imp_FindGlobalSymbol=_FindGlobalSymbol")
#pragma comment(linker, "/alternatename:__imp_FindGlobalSymbolByHash=_FindGlobalSymbolByHash")

PLATFORM_INTERFACE CGlobalSymbol MakeGlobalSymbol(const char* str);
PLATFORM_INTERFACE CGlobalSymbolCaseSensitive MakeGlobalSymbolCaseSensitive(const char* str);
PLATFORM_INTERFACE CGlobalSymbol FindGlobalSymbol(const char* str);
PLATFORM_INTERFACE CGlobalSymbol FindGlobalSymbolByHash(uint32 hash);