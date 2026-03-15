#pragma once
#include "entity/cbaseentity.h"
#include "scriptExtensions/csscript.h"

class CPointScript : public CBaseEntity {
public:
	CCSScript_EntityScript* GetScript();
};