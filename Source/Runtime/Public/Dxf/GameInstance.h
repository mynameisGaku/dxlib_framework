#pragma once
#include "Dxf/LifecycleObject.h"
namespace Dxf
{
/** Game-specific state that survives scene transitions. It owns no platform services. */
class DGameInstance : public DLifecycleObject
{
};
}
