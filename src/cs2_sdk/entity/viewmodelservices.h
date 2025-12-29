#pragma once

#include "ehandle.h"
#include "schema.h"

class C_BaseViewModel;
class CCSPlayer_ViewModelServices {
public:
	DECLARE_SCHEMA_CLASS(CCSPlayer_ViewModelServices);

    SCHEMA_FIELD(CHandle<C_BaseViewModel>, m_hViewModel);
};