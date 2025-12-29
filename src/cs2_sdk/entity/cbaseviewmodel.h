#pragma once

#include "ehandle.h"
#include "schema.h"

class C_BasePlayerWeapon;
class C_BaseModelEntity;

class C_BaseViewModel : public C_BaseModelEntity {
public:
	DECLARE_SCHEMA_CLASS(C_BaseViewModel);

    SCHEMA_FIELD(CHandle<C_BasePlayerWeapon>, m_hWeapon)
};