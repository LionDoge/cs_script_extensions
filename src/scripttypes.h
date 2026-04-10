#pragma once
#include "scriptExtensions/scriptextensions.h"
// This file defines type markers that can be checked against during runtime to verify that the 'this' object is of the expected type.

namespace ScriptTypeMarkers {
	inline ScriptTypeMarker userMessageInfo { "UserMessageInfo" };
};