#include "v8.h"
#include "csv8.h"
// v8 version is 11.9.169.6
HMODULE hV8Module = NULL;
typedef v8::Isolate* (__fastcall* fnIsolateGetCurrent_t)(void);
fnIsolateGetCurrent_t fnIsolateGetCurrent = NULL;

void Init()
{
	hV8Module = GetModuleHandleA("v8.dll");

	FARPROC fp = GetProcAddress(hV8Module, "?GetCurrent@Isolate@v8@@SAPEAV12@XZ");
	fnIsolateGetCurrent = reinterpret_cast<fnIsolateGetCurrent_t>(fp);
}

//v8::Isolate* v8::CustomIsolate::GetCurrent()
//{
//	return fnIsolateGetCurrent();
//}


