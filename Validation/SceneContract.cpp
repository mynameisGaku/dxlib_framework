#include "Dxf/TaskDispatcher.h"
#include "Dxf/Contexts.h"
#include "Dxf/Scene.h"
static_assert(sizeof(Dxf::DScene) > 0);
static_assert(sizeof(Dxf::FTickContext) > 0);
bool SceneContract(Dxf::FTickContext& Context)
{
	return Context.Tasks != nullptr && Context.Tasks->IsOwnerThread() && Context.Tasks->CanSynchronize();
}
bool RetirementContract(Dxf::FTaskDispatcher& Tasks, const Dxf::FTaskScope& Scope)
{
	return Tasks.RetireScope(Scope);
}
