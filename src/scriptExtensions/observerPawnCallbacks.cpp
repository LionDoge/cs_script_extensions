#include "observerPawnCallbacks.h"
#include "scriptcommon_entities.h"

void ObserverPawnCallbacks::GetObserverTarget(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto pawn = UnwrapThis<CCSPlayerPawnBase*>(context);

	if (!pawn)
		return;

	auto observerServices = (*pawn)->m_pObserverServices.Get();
	if (!observerServices)
		return;

	auto observerTarget = observerServices->m_hObserverTarget.Get();
	if (!observerTarget.IsValid())
		return;

	args.GetReturnValue().Set(ScriptExtensions::GetInstance()->CreateEntityObjectAuto(observerTarget.Get()));
}
