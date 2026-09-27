// SPDX-License-Identifier: NOASSERTION
#include "InteractionLayout.h"
namespace Dxf::GameplaySample::InteractionLayout
{
const Toolbox::TArray<FLevelBox, GroundCount>& GetGround() noexcept
{
	static const Toolbox::TArray<FLevelBox, GroundCount> Boxes{
	    FLevelBox{{5.5f, -1}, {9.5f, 1}, 0, 0, DepthHalf, false},         // 床A（x∈[-4,15]、上面0）
	    FLevelBox{{22.5f, -1}, {1.5f, 1}, 0, 0, DepthHalf, false},        // 床B（x∈[21,24]、上面0）
	    FLevelBox{{29.25f, 0.5f}, {2.75f, 2.5f}, 0, 0, DepthHalf, false}, // 上の段C1（x∈[26.5,32]、上面3）
	    FLevelBox{{40, 0.5f}, {4, 2.5f}, 0, 0, DepthHalf, false}};        // 上の段C2（x∈[36,44]、上面3）
	return Boxes;
}
Toolbox::FVector2 PlatformAt(Toolbox::f64 Seconds) noexcept
{
	// 最大の速さは2.4×0.6=1.44m/s。
	return {static_cast<Toolbox::f32>(18 + 2.4 * Toolbox::Sin(0.6 * Seconds)), -PlatformHalfY};
}
Toolbox::FVector2 LiftAt(Toolbox::f64 Seconds) noexcept
{
	// 上面が0（床Bと同じ高さ）と3（上の段と同じ高さ）の間を往復する。最大の速さは1.5×0.5=0.75m/s。
	return {LiftX, static_cast<Toolbox::f32>(1.5 - 1.5 * Toolbox::Cos(0.5 * Seconds) - PlatformHalfY)};
}
Toolbox::f64 TiltAt(Toolbox::f64 Seconds) noexcept
{
	return 0.2 * Toolbox::Sin(0.8 * Seconds);
}
Toolbox::f64 YawAt(Toolbox::f64 Seconds) noexcept
{
	return 0.7 * Seconds;
}
} // namespace Dxf::GameplaySample::InteractionLayout
