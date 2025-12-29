#pragma once
#include "common.h"

#define RESOLVE_SIG(gameConfig, name, variable)                        \
	variable = (decltype(variable))gameConfig->ResolveSignature(name); \
	if (!variable) {													\
		return false;													\
	}                                                                