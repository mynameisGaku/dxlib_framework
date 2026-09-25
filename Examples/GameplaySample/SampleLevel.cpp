// SPDX-License-Identifier: NOASSERTION
#include "SampleLevel.h"
#include "Dxf/RigidBodyComponent2D.h"
#include "Dxf/RigidBodyComponent3D.h"
namespace Dxf::GameplaySample
{
const Toolbox::TArray<FLevelBox, LevelBoxCount>& GetLevelBoxes() noexcept
{
	static const Toolbox::TArray<FLevelBox, LevelBoxCount> Boxes{
	    FLevelBox{{8, -1}, {22, 1}, 0, 0, 10, false},                     // 床（x∈[-14,30]、上面0）
	    FLevelBox{{3, 0.1f}, {1, 0.1f}, 0, 0, 4, false},                  // 低い段差（高さ0.2、上れる）
	    FLevelBox{{-6, 0.3f}, {1, 0.3f}, 0, 0, 4, false},                 // 高い段差（高さ0.6、上れない）
	    FLevelBox{{-2.5f, 1.6f}, {1.5f, 0.5f}, 0, 0, 4, false},           // 低い天井（下面1.1）
	    FLevelBox{{8.848f, 1.067f}, {3, 0.5f}, 0.5235988f, 0, 4, false},  // 30度の坂（x=6から上面3まで）
	    FLevelBox{{13.2f, 1.5f}, {2, 1.5f}, 0, 0, 4, false},              // 坂の上の台（上面3）
	    FLevelBox{{18.433f, 1.482f}, {2, 0.5f}, 1.0471976f, 0, 4, false}, // 60度の急坂（x=17から）
	    FLevelBox{{26, 3}, {1, 3}, 0, 0, 6, false},                       // 右端の壁（x∈[25,27]）
	    FLevelBox{{19, 3}, {7, 3}, 0, 5, 0.5f, true}}; // 3Dの奥の壁（z∈[4.5,5.5]、右端の壁と角を作る）
	return Boxes;
}
FColor LevelBoxColor(Toolbox::size_t Index) noexcept
{
	constexpr FColor Colors[LevelBoxCount] = {{60, 70, 90, 255},   {90, 110, 150, 255},  {120, 90, 150, 255},
	                                          {150, 80, 80, 255},  {80, 140, 90, 255},   {90, 120, 100, 255},
	                                          {160, 120, 60, 255}, {100, 100, 120, 255}, {110, 110, 130, 255}};
	return Colors[Index];
}
void LevelBoxCorners(const FLevelBox& Box, Toolbox::FVector2 (&Out)[4]) noexcept
{
	const Toolbox::f32 Cosine = static_cast<Toolbox::f32>(Toolbox::Cos(Toolbox::f64(Box.Angle)));
	const Toolbox::f32 Sine = static_cast<Toolbox::f32>(Toolbox::Sin(Toolbox::f64(Box.Angle)));
	const Toolbox::FVector2 AxisX{Cosine * Box.Half.X, Sine * Box.Half.X};
	const Toolbox::FVector2 AxisY{-Sine * Box.Half.Y, Cosine * Box.Half.Y};
	Out[0] = Box.Center - AxisX - AxisY;
	Out[1] = Box.Center + AxisX - AxisY;
	Out[2] = Box.Center + AxisX + AxisY;
	Out[3] = Box.Center - AxisX + AxisY;
}
Toolbox::FOBB LevelBoxShape3D(const FLevelBox& Box) noexcept
{
	Toolbox::FOBB Shape{{Box.Center.X, Box.Center.Y, Box.CenterZ}, {Box.Half.X, Box.Half.Y, Box.HalfZ}};
	const Toolbox::f32 Cosine = static_cast<Toolbox::f32>(Toolbox::Cos(Toolbox::f64(Box.Angle)));
	const Toolbox::f32 Sine = static_cast<Toolbox::f32>(Toolbox::Sin(Toolbox::f64(Box.Angle)));
	Shape.Axes = {Toolbox::FVector3{Cosine, Sine, 0}, Toolbox::FVector3{-Sine, Cosine, 0}, Toolbox::FVector3{0, 0, 1}};
	return Shape;
}

TResult<void> DLevel2D::OnInitialize(const FInitContext&)
{
	FBodyDescription2D Body;
	Body.Type = EBodyType::Static;
	auto Rigid = AddComponent<DRigidBody2DComponent>(Body);
	if (!Rigid)
	{
		return TResult<void>::Failure(Rigid.Error());
	}
	const auto& Boxes = GetLevelBoxes();
	for (Toolbox::size_t Index = 0; Index < Boxes.Size(); ++Index)
	{
		if (Boxes[Index].bOnly3D)
		{
			continue;
		}
		FColliderDescription2D Collider;
		Collider.Shape = Toolbox::FOrientedBox2D{Boxes[Index].Center, Boxes[Index].Half, Boxes[Index].Angle};
		auto Attached = AddComponent<DCollider2DComponent>(Collider);
		if (!Attached)
		{
			return TResult<void>::Failure(Attached.Error());
		}
	}
	return {};
}
TResult<void> DLevel3D::OnInitialize(const FInitContext&)
{
	FBodyDescription3D Body;
	Body.Type = EBodyType::Static;
	auto Rigid = AddComponent<DRigidBody3DComponent>(Body);
	if (!Rigid)
	{
		return TResult<void>::Failure(Rigid.Error());
	}
	const auto& Boxes = GetLevelBoxes();
	for (Toolbox::size_t Index = 0; Index < Boxes.Size(); ++Index)
	{
		FColliderDescription3D Collider;
		Collider.Shape = LevelBoxShape3D(Boxes[Index]);
		auto Attached = AddComponent<DCollider3DComponent>(Collider);
		if (!Attached)
		{
			return TResult<void>::Failure(Attached.Error());
		}
	}
	return {};
}
} // namespace Dxf::GameplaySample
