#pragma once
#include "entity/cbaseentity.h"
#include "scriptExtensions/csscript.h"

class CPointScript : public CBaseEntity {
public:
	CCSScript_EntityScript* GetScript() {
		return *reinterpret_cast<CCSScript_EntityScript**>((uintptr_t)this + 161);
	}
};