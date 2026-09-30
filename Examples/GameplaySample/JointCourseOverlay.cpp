// SPDX-License-Identifier: NOASSERTION
#include "JointCourseOverlay.h"
#include "JointCourseSupport.h"
#include "InteractionScene2D.h"
#include "InteractionScene3D.h"
#include "Dxf/RenderContext.h"
namespace Dxf::GameplaySample
{
namespace
{
// 接続状態は観察だけで取得し、描画中に接続要求を出さない。
const char* ConnectionName_Internal(EDistanceJointConnection State)
{
	switch (State)
	{
	case EDistanceJointConnection::PendingBodies:
		return "pending";
	case EDistanceJointConnection::Connected:
		return "connected";
	case EDistanceJointConnection::Disconnected:
		return "disconnected";
	case EDistanceJointConnection::EndpointLost:
		return "endpoint lost";
	}
	return "unknown";
}
// 同じ意味の接続操作と観察を両次元で表示する。
template <typename TCourse>
void DrawStatus_Internal(FRenderContext& Render, const TCourse& Course, const FFont& Font)
{
	// 全画面2Dへ戻った後に出す文字。
	FDrawStyle Text;
	Text.Layer = 100;
	Text.Color = {200, 175, 255, 255};
	// 同じ成功観察から整える表示文字。
	char Line[256];
	// 今回調べる接続Component。
	const auto* Joint = Course.GetJoints()[0].Get();
	if (Joint == nullptr)
	{
		snprintf(Line, sizeof(Line), "Distance joint: initializing");
	}
	else
	{
		const auto Value = Joint->GetObservation();
		if (Value)
		{
			snprintf(Line, sizeof(Line), "Distance joint: %s  length %.3f / %.3f  error %.4f  step %llu", ConnectionName_Internal(Joint->GetConnectionState()), Value->CurrentLength, Value->TargetLength, Value->Error, static_cast<Toolbox::uint64>(Value->SuccessfulStep));
		}
		else
		{
			snprintf(Line, sizeof(Line), "Distance joint: %s (no successful observation)", ConnectionName_Internal(Joint->GetConnectionState()));
		}
	}
	RequireSample(Render.Get2D().DrawText(Font, Line, {16, 152}, Text));
	RequireSample(Render.Get2D().DrawText(Font, "[J] impulse [K] disconnect [L] reconnect [N] rebuild [B] carrier stop/start (disabled while paused)", {16, 180}, Text));
}
} // namespace
void DrawJointCourse2D(FRenderContext& Render, const DInteraction2DScene& Scene, Toolbox::int32 Side)
{
	// 既存Sceneの投影と同じ縮尺と領域。
	const Toolbox::f32 Scale = Scene.IsSplit() ? 24.0f : 40.0f;
	// 今回の形状または文字の描画指定。
	FDrawStyle Style;
	Style.Layer = 7;
	Style.Color = {170, 105, 230, 255};
	Style.bClip = true;
	// 今回描画した対象の横幅。
	const Toolbox::int32 Width = Scene.GetDisplayWidth();
	Style.ClipRect = {Scene.IsSplit() ? Width * Side / 2 : 0, 0, Scene.IsSplit() ? Width * (Side + 1) / 2 : Width, Scene.GetDisplayHeight()};
	const auto& Course = Scene.GetJointCourse();
	for (Toolbox::size_t Index = 0; Index < Course.GetBodies().Size(); ++Index)
	{
		if (const auto* Body = Course.GetBodies()[Index].Get(); Body != nullptr && Body->HasBody())
		{
			const auto Center = Scene.ToScreen(Body->GetRenderPosition(), Side);
			RequireSample(Render.Get2D().FillCircle(Center, Scale * (Index == 1 ? 0.28f : 0.2f), Style));
		}
	}
	if (const auto* Carrier = Course.GetCarrier(); Carrier != nullptr && Carrier->GetBodyId())
	{
		RequireSample(Render.Get2D().FillCircle(Scene.ToScreen(Carrier->GetRenderPosition(), Side), Scale * 0.2f, Style));
	}
	Style.Color = {255, 230, 100, 255};
	Style.Layer = 8;
	for (const auto& Handle : Course.GetJoints())
	{
		// 接続のA側。
		Toolbox::FVector2 A;
		// 接続のB側。
		Toolbox::FVector2 B;
		if (const auto* Joint = Handle.Get(); Joint != nullptr && Joint->GetRenderAnchors(A, B))
		{
			RequireSample(Render.Get2D().DrawLine(Scene.ToScreen(A, Side), Scene.ToScreen(B, Side), Style));
		}
	}
}
void DrawJointCourse3D(FRenderContext& Render, const DInteraction3DScene& Scene)
{
	// 今回の形状または文字の描画指定。
	FDrawStyle3D Style;
	Style.Color = {170, 105, 230, 255};
	const auto& Course = Scene.GetJointCourse();
	for (Toolbox::size_t Index = 0; Index < Course.GetBodies().Size(); ++Index)
	{
		if (const auto* Body = Course.GetBodies()[Index].Get(); Body != nullptr && Body->HasBody())
		{
			RequireSample(Render.Get3D().DrawSphere({Body->GetRenderPosition(), Index == 1 ? 0.28f : 0.2f}, Style, 12));
		}
	}
	if (const auto* Carrier = Course.GetCarrier(); Carrier != nullptr && Carrier->GetBodyId())
	{
		RequireSample(Render.Get3D().DrawSphere({Carrier->GetRenderPosition(), 0.2f}, Style, 12));
	}
	Style.Color = {255, 230, 100, 255};
	for (const auto& Handle : Course.GetJoints())
	{
		// 接続のA側。
		Toolbox::FVector3 A;
		// 接続のB側。
		Toolbox::FVector3 B;
		if (const auto* Joint = Handle.Get(); Joint != nullptr && Joint->GetRenderAnchors(A, B))
		{
			RequireSample(Render.Get3D().DrawLine(A, B, Style));
		}
	}
}
void DrawJointStatus(FRenderContext& Render, const DInteraction2DScene& Scene, const FFont& Font)
{
	DrawStatus_Internal(Render, Scene.GetJointCourse(), Font);
}
void DrawJointStatus(FRenderContext& Render, const DInteraction3DScene& Scene, const FFont& Font)
{
	DrawStatus_Internal(Render, Scene.GetJointCourse(), Font);
}
} // namespace Dxf::GameplaySample
