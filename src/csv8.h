#pragma once
#include "v8.h"
#include "platform.h"

void Init();
namespace v8 {
	class CustomIsolate {
	public:
		//static Isolate* GetCurrent();
	private:
		HMODULE g_hV8Module;
	};
}