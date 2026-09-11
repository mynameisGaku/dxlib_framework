#pragma once
#include "Dxf/NativeBackends.h"
#include "Dxf/AppRunner.h"
#include <type_traits>
#include <utility>
namespace Dxf
{
/** Convenience entry; use FApplication directly when supplying a custom GameInstance. */
template <typename TScene, typename... TArgs>
TResult<void> Run(FApplicationSettings Settings, TArgs&&... Args)
{
	static_assert(std::is_base_of_v<DScene, TScene>);
	FDxLibBackends Backends;
	FApplication Application(Backends.GetServices(), std::move(Settings));
	FAppRunner Runner;
	return Runner.Run(Application, std::make_unique<TScene>(std::forward<TArgs>(Args)...));
}
}
