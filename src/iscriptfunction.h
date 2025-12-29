#pragma once
#include "v8.h"
class IScriptFunction
{
public:
	virtual void Run() = 0;
};