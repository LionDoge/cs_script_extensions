#include "entity/cpointscript.h"
#include "scriptExtensions/scriptextensions.h"

CCSScript_EntityScript* CPointScript::GetScript()
{
	return ScriptExtensions::GetScriptFromEntity(this);
}