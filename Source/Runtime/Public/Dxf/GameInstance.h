#pragma once
#include "Dxf/LifecycleObject.h"
namespace Dxf
{
/**
 * シーン遷移後も存続するゲーム固有の状態。プラットフォームのサービスは所有しない。
 */
class DGameInstance : public DLifecycleObject
{
};
} // namespace Dxf
