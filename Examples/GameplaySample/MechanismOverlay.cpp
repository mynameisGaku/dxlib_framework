// SPDX-License-Identifier: NOASSERTION
#include "MechanismOverlay.h"
#include "InteractionScene2D.h"
#include "InteractionScene3D.h"
#include "JointCourseSupport.h"
#include "Dxf/RenderContext.h"
#include <stdio.h>
namespace Dxf::GameplaySample
{
namespace
{
// 通し番号はComponent自身の成功回数。World全体のフレームとは区別する。
template <typename TCourse>
void DrawStatus_Internal(FRenderContext& Render, const TCourse& Course, const FFont& Font)
{
	FDrawStyle Style;
	Style.Layer = 30;
	Style.Color = {140, 245, 220, 255};
	char Line[256];
	const auto Door = Course.GetRevolutes()[1].Get();
	const auto Gate = Course.GetPrismatics()[0].Get();
	const auto Fixed = Course.GetFixed()[0].Get();
	const auto DoorState = Door == nullptr ? decltype(Door->GetObservation()){} : Door->GetObservation();
	const auto GateState = Gate == nullptr ? decltype(Gate->GetObservation()){} : Gate->GetObservation();
	const auto FixedState = Fixed == nullptr ? decltype(Fixed->GetObservation()){} : Fixed->GetObservation();
	snprintf(Line, sizeof(Line), "Mechanism [F1]: door angle=%.3f rad speed=%.3f rad/s  gate=%.3f m rate=%.3f m/s", DoorState ? DoorState->State.Angle : 0, DoorState ? DoorState->State.AngularSpeed : 0, GateState ? GateState->State.Translation : 0, GateState ? GateState->State.TranslationRate : 0);
	RequireSample(Render.Get2D().DrawText(Font, Line, {16, 208}, Style));
	snprintf(Line, sizeof(Line), "Fixed error=%.4f m / %.4f rad; limits door[-.8,.8] gate[0,2]; successful observation=%d/%d/%d", FixedState ? FixedState->State.AnchorError : 0, FixedState ? FixedState->State.OrientationError : 0, static_cast<Toolbox::int32>(static_cast<bool>(DoorState)), static_cast<Toolbox::int32>(static_cast<bool>(GateState)), static_cast<Toolbox::int32>(static_cast<bool>(FixedState)));
	RequireSample(Render.Get2D().DrawText(Font, Line, {16, 236}, Style));
}
} // namespace
void DrawMechanismCourse2D(FRenderContext& Render, const DInteraction2DScene& Scene, Toolbox::int32 Side)
{
	// 物理更新後の補間位置。描画回数によって要求は増えない。
	FDrawStyle Style;
	Style.Color = {80, 220, 190, 255};
	Style.Layer = 9;
	Style.bClip = true;
	const Toolbox::int32 Width = Scene.GetDisplayWidth();
	Style.ClipRect = {Scene.IsSplit() ? Width * Side / 2 : 0, 0, Scene.IsSplit() ? Width * (Side + 1) / 2 : Width, Scene.GetDisplayHeight()};
	const Toolbox::f32 Scale = Scene.IsSplit() ? 24.0f : 40.0f;
	const auto& Course = Scene.GetMechanismCourse();
	for (const auto& Handle : Course.GetBodies())
	{
		if (const auto* Body = Handle.Get(); Body != nullptr && Body->HasBody())
		{
			const auto Center = Body->GetRenderPosition();
			const Toolbox::f32 Angle = Body->GetRenderAngle();
			const Toolbox::FVector2 Axis{static_cast<Toolbox::f32>(Toolbox::Cos(Angle) * 0.75), static_cast<Toolbox::f32>(Toolbox::Sin(Angle) * 0.75)};
			RequireSample(Render.Get2D().DrawLine(Scene.ToScreen(Center - Axis, Side), Scene.ToScreen(Center + Axis, Side), Style));
			RequireSample(Render.Get2D().FillCircle(Scene.ToScreen(Center, Side), Scale * 0.08f, Style));
		}
	}
	// 取付点も同じ補間Bodyから求める。
	auto DrawAnchor = [&](const auto& Joints)
	{
		for (const auto& Handle : Joints)
		{
			Toolbox::FVector2 A;
			Toolbox::FVector2 B;
			if (const auto* Joint = Handle.Get(); Joint != nullptr && Joint->GetRenderAnchors(A, B))
			{
				RequireSample(Render.Get2D().DrawLine(Scene.ToScreen(A, Side), Scene.ToScreen(B, Side), Style));
			}
		}
	};
	DrawAnchor(Course.GetRevolutes());
	DrawAnchor(Course.GetPrismatics());
	DrawAnchor(Course.GetFixed());
	// 電動扉の取付FrameとLimitの両端を、同じ補間支点から描く。
	if (const auto* Support = Course.GetBodies()[2].Get(); Support != nullptr && Support->HasBody())
	{
		const auto* Joint = Course.GetRevolutes()[1].Get();
		const auto& Description = Joint->GetDescription().Joint;
		Toolbox::FVector2 A;
		Toolbox::FVector2 B;
		if (Joint->GetRenderAnchors(A, B))
		{
			Style.Color = {230, 170, 80, 255};
			const Toolbox::f64 Angle = Support->GetRenderAngle() + Description.FrameA.LocalAngle;
			const Toolbox::f64 Limits[] = {Description.Limits.LowerAngle, Description.Limits.UpperAngle};
			for (const Toolbox::f64 Limit : Limits)
			{
				const Toolbox::FVector2 Direction{static_cast<Toolbox::f32>(Toolbox::Cos(Angle + Limit)), static_cast<Toolbox::f32>(Toolbox::Sin(Angle + Limit))};
				RequireSample(Render.Get2D().DrawLine(Scene.ToScreen(A, Side), Scene.ToScreen(A + Direction, Side), Style));
			}
		}
	}
	// レールの向きと移動上下限。観察値の位置とは分けて補間Frameを使う。
	if (const auto* Support = Course.GetBodies()[4].Get(); Support != nullptr && Support->HasBody())
	{
		const auto* Joint = Course.GetPrismatics()[0].Get();
		const auto& Description = Joint->GetDescription().Joint;
		Toolbox::FVector2 A;
		Toolbox::FVector2 B;
		if (Joint->GetRenderAnchors(A, B))
		{
			const Toolbox::f64 Angle = Support->GetRenderAngle() + Description.FrameA.LocalAngle;
			const Toolbox::FVector2 Axis{static_cast<Toolbox::f32>(Toolbox::Cos(Angle)), static_cast<Toolbox::f32>(Toolbox::Sin(Angle))};
			RequireSample(Render.Get2D().DrawLine(Scene.ToScreen(A + Axis * static_cast<Toolbox::f32>(Description.Limits.LowerTranslation), Side), Scene.ToScreen(A + Axis * static_cast<Toolbox::f32>(Description.Limits.UpperTranslation), Side), Style));
		}
	}
}
void DrawMechanismCourse3D(FRenderContext& Render, const DInteraction3DScene& Scene)
{
	FDrawStyle3D Style;
	Style.Color = {80, 220, 190, 255};
	const auto& Course = Scene.GetMechanismCourse();
	for (const auto& Handle : Course.GetBodies())
	{
		if (const auto* Body = Handle.Get(); Body != nullptr && Body->HasBody())
		{
			Toolbox::FOBB Box{Body->GetRenderPosition(), {0.75f, 0.25f, 0.45f}};
			const auto Rotation = Body->GetRenderOrientation();
			Box.Axes[0] = Rotation.Rotate({1, 0, 0});
			Box.Axes[1] = Rotation.Rotate({0, 1, 0});
			Box.Axes[2] = Rotation.Rotate({0, 0, 1});
			RequireSample(Render.Get3D().DrawBox(Box, Style));
		}
	}
	auto DrawAnchor = [&](const auto& Joints)
	{
		for (const auto& Handle : Joints)
		{
			Toolbox::FVector3 A;
			Toolbox::FVector3 B;
			if (const auto* Joint = Handle.Get(); Joint != nullptr && Joint->GetRenderAnchors(A, B))
			{
				RequireSample(Render.Get3D().DrawLine(A, B, Style));
			}
		}
	};
	DrawAnchor(Course.GetRevolutes());
	DrawAnchor(Course.GetPrismatics());
	DrawAnchor(Course.GetFixed());
	// 3Dの斜め軸と角度基準。Zが回転軸、Xが角度0の基準。
	if (const auto* Support = Course.GetBodies()[2].Get(); Support != nullptr && Support->HasBody())
	{
		const auto* Joint = Course.GetRevolutes()[1].Get();
		const auto& Description = Joint->GetDescription().Joint;
		const auto Rotation = Support->GetRenderOrientation() * Description.FrameA.LocalRotation;
		Toolbox::FVector3 A;
		Toolbox::FVector3 B;
		if (Joint->GetRenderAnchors(A, B))
		{
			Style.Color = {230, 170, 80, 255};
			RequireSample(Render.Get3D().DrawLine(A, A + Rotation.Rotate({0, 0, 1}), Style));
			const Toolbox::f64 Limits[] = {Description.Limits.LowerAngle, Description.Limits.UpperAngle};
			for (const Toolbox::f64 Limit : Limits)
			{
				RequireSample(Render.Get3D().DrawLine(A, A + Rotation.Rotate({static_cast<Toolbox::f32>(Toolbox::Cos(Limit)), static_cast<Toolbox::f32>(Toolbox::Sin(Limit)), 0}), Style));
			}
		}
	}
	if (const auto* Support = Course.GetBodies()[4].Get(); Support != nullptr && Support->HasBody())
	{
		const auto* Joint = Course.GetPrismatics()[0].Get();
		const auto& Description = Joint->GetDescription().Joint;
		const auto Rotation = Support->GetRenderOrientation() * Description.FrameA.LocalRotation;
		const auto Axis = Rotation.Rotate({1, 0, 0});
		Toolbox::FVector3 A;
		Toolbox::FVector3 B;
		if (Joint->GetRenderAnchors(A, B))
		{
			RequireSample(Render.Get3D().DrawLine(A + Axis * static_cast<Toolbox::f32>(Description.Limits.LowerTranslation), A + Axis * static_cast<Toolbox::f32>(Description.Limits.UpperTranslation), Style));
		}
	}
}
void DrawMechanismStatus(FRenderContext& Render, const DInteraction2DScene& Scene, const FFont& Font)
{
	DrawStatus_Internal(Render, Scene.GetMechanismCourse(), Font);
}
void DrawMechanismStatus(FRenderContext& Render, const DInteraction3DScene& Scene, const FFont& Font)
{
	DrawStatus_Internal(Render, Scene.GetMechanismCourse(), Font);
}
} // namespace Dxf::GameplaySample
