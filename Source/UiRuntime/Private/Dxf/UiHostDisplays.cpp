// SPDX-License-Identifier: NOASSERTION
#include "UiHostState.h"
#include "Dxf/GuardValue.h"
#include "Dxf/AssetService.h"
#include "Dxf/UiRenderer.h"
#include "Dxf/ViewCoordinates.h"
namespace Dxf::Detail
{
namespace
{
// 画素の矩形が点を含むか（半開区間）。
bool Contains_Internal(const FUiPixelRect& Rect, FVector2 Point) noexcept
{
	return Point.X >= static_cast<Toolbox::f32>(Rect.Left) && Point.Y >= static_cast<Toolbox::f32>(Rect.Top) &&
	       Point.X < static_cast<Toolbox::f32>(Rect.Right) && Point.Y < static_cast<Toolbox::f32>(Rect.Bottom);
}
} // namespace
// namespace
// 仲介を作る。
FUiHostState::FUiHostState(FUiSceneHostSettings Settings) : m_Settings(Toolbox::Move(Settings))
{
	m_ScreenWidth = m_Settings.ScreenWidth;
	m_ScreenHeight = m_Settings.ScreenHeight;
}
// 停止を先に確定し、コールバックより先に登録を取り出す。
void FUiHostState::Shutdown() noexcept
{
	m_bStopped = true;
	auto Displays = Toolbox::Move(m_Displays);
	for (auto& Display : Displays)
	{
		if (FUiRoot* Root = Display.Root.Get())
		{
			Root->ResetPointer();
		}
	}
}
// 表示先を加える。
FUiDisplayId FUiHostState::Add_Internal(FDisplay Display)
{
	if (m_bStopped || Display.Root.Get() == nullptr || Display.Options.Player < 0 || Display.Options.Player >= 4 ||
	    Display.Options.Navigation > EUiNavigationPolicy::Never || m_NextId == 0)
	{
		throw Toolbox::FException("Invalid UI display or stopped host");
	}
	Display.Id = m_NextId++;
	m_Displays.PushBack(Toolbox::Move(Display));
	return m_Displays.Back().Id;
}
// 全画面の表示先を加える。
FUiDisplayId FUiHostState::AddScreen(FUiRoot& Root, const FUiDisplayOptions& Options)
{
	FDisplay Display;
	Display.Kind = EUiDisplayKind::Screen;
	Display.Root = Root.GetHandle();
	Display.RootIdentity = &Root;
	Display.Options = Options;
	return Add_Internal(Toolbox::Move(Display));
}
// 分割画面の表示先を加える。
FUiDisplayId FUiHostState::AddViewport(FUiRoot& Root, FUiPixelRect Rect, const FUiDisplayOptions& Options)
{
	FDisplay Display;
	Display.Kind = EUiDisplayKind::Viewport;
	Display.Root = Root.GetHandle();
	Display.RootIdentity = &Root;
	Display.Options = Options;
	Display.Rect = Rect;
	return Add_Internal(Toolbox::Move(Display));
}
// 2Dのパネルを加える。
FUiDisplayId FUiHostState::AddWorldPanel2D(FUiRoot& Root, const FUiWorldPanel2D& Panel,
                                           const FUiDisplayOptions& Options)
{
	FDisplay Display;
	Display.Kind = EUiDisplayKind::WorldPanel2D;
	Display.Root = Root.GetHandle();
	Display.RootIdentity = &Root;
	Display.Options = Options;
	Display.Panel2D = Panel;
	return Add_Internal(Toolbox::Move(Display));
}
// 3Dのパネルを加える。
FUiDisplayId FUiHostState::AddWorldPanel3D(FUiRoot& Root, const FUiWorldPanel3D& Panel, FAssetService& Assets,
                                           const FUiDisplayOptions& Options)
{
	FDisplay Display;
	Display.Kind = EUiDisplayKind::WorldPanel3D;
	Display.Root = Root.GetHandle();
	Display.RootIdentity = &Root;
	Display.Options = Options;
	Display.Panel3D = Panel;
	Display.pAssets = &Assets;
	return Add_Internal(Toolbox::Move(Display));
}
// 表示先を探す。
FUiHostState::FDisplay* FUiHostState::Find_Internal(FUiDisplayId Id) noexcept
{
	for (auto& Display : m_Displays)
	{
		if (Display.Id == Id)
		{
			return &Display;
		}
	}
	return nullptr;
}
// 表示先を探す。
const FUiHostState::FDisplay* FUiHostState::Find_Internal(FUiDisplayId Id) const noexcept
{
	for (const auto& Display : m_Displays)
	{
		if (Display.Id == Id)
		{
			return &Display;
		}
	}
	return nullptr;
}
// 表示先を外す。
bool FUiHostState::Remove(FUiDisplayId Id) noexcept
{
	for (Toolbox::size_t Index = 0; Index < m_Displays.Size(); ++Index)
	{
		if (m_Displays[Index].Id != Id)
		{
			continue;
		}
		FDisplay Removed = Toolbox::Move(m_Displays[Index]);
		m_Displays.Erase(m_Displays.Begin() + Index);
		if (m_PointerOwner == Id)
		{
			m_PointerOwner = 0;
		}
		if (FUiRoot* Root = Removed.Root.Get())
		{
			Root->ResetPointer();
		}
		return true;
	}
	return false;
}
// 分割画面の領域を変える。
bool FUiHostState::SetViewportRect(FUiDisplayId Id, FUiPixelRect Rect) noexcept
{
	FDisplay* Display = Find_Internal(Id);
	if (Display == nullptr || Display->Kind != EUiDisplayKind::Viewport)
	{
		return false;
	}
	Display->Rect = Rect;
	return true;
}
// 2Dのパネルを変える。
bool FUiHostState::SetWorldPanel2D(FUiDisplayId Id, const FUiWorldPanel2D& Panel) noexcept
{
	FDisplay* Display = Find_Internal(Id);
	if (Display == nullptr || Display->Kind != EUiDisplayKind::WorldPanel2D)
	{
		return false;
	}
	Display->Panel2D = Panel;
	return true;
}
// 3Dのパネルを変える。
bool FUiHostState::SetWorldPanel3D(FUiDisplayId Id, const FUiWorldPanel3D& Panel) noexcept
{
	FDisplay* Display = Find_Internal(Id);
	if (Display == nullptr || Display->Kind != EUiDisplayKind::WorldPanel3D)
	{
		return false;
	}
	try
	{
		Display->Panel3D = Panel;
	}
	catch (...)
	{
		return false;
	}
	return true;
}
// 3DのパネルのViewを設定する。
void FUiHostState::SetWorldViews3D(const Toolbox::TVector<FRenderView3D>& Views)
{
	m_Views3D = Views;
}
// 表示先の設定を変える。
bool FUiHostState::SetOptions(FUiDisplayId Id, const FUiDisplayOptions& Options) noexcept
{
	FDisplay* Display = Find_Internal(Id);
	if (Display == nullptr)
	{
		return false;
	}
	Display->Options = Options;
	return true;
}
// 入力プレイヤーの機器を設定する。
void FUiHostState::SetPlayerDevices(Toolbox::int32 Player, FUiPlayerDevices Devices) noexcept
{
	if (Player >= 0 && static_cast<Toolbox::size_t>(Player) < m_Settings.Players.Size())
	{
		m_Settings.Players[static_cast<Toolbox::size_t>(Player)] = Devices;
	}
}
// 操作の割当を設定する。
void FUiHostState::SetNavigationBindings(FUiNavigationBindings Bindings)
{
	m_Settings.Bindings = Toolbox::Move(Bindings);
}
// 画面の大きさを設定する。
void FUiHostState::SetScreenSize(Toolbox::int32 Width, Toolbox::int32 Height) noexcept
{
	if (Width > 0 && Height > 0)
	{
		m_ScreenWidth = Width;
		m_ScreenHeight = Height;
	}
}
// 表示先の表示面。
FUiSurface FUiHostState::MakeSurface_Internal(const FDisplay& Display) const noexcept
{
	switch (Display.Kind)
	{
	case EUiDisplayKind::Screen:
		return FUiSurface({0, 0, m_ScreenWidth, m_ScreenHeight}, Display.Options.Scale);
	case EUiDisplayKind::Viewport:
		return FUiSurface(Display.Rect, Display.Options.Scale);
	case EUiDisplayKind::WorldPanel2D:
		return FUiSurface(Display.Panel2D.GetPixelRect(), Display.Options.Scale);
	default:
	{
		FUiSurface Surface({0, 0, Display.Panel3D.TextureWidth, Display.Panel3D.TextureHeight}, Display.Options.Scale);
		Surface.SetPremultipliedAlpha(Display.Panel3D.Composition == EUiPanelComposition::Transparent);
		return Surface;
	}
	}
}
// 表示先の表示面（公開）。
FUiSurface FUiHostState::GetSurface(FUiDisplayId Id) const noexcept
{
	const FDisplay* Display = Find_Internal(Id);
	return Display != nullptr ? MakeSurface_Internal(*Display) : FUiSurface{};
}
// 3Dのパネルの描画先テクスチャ。
FRenderTarget FUiHostState::GetWorldPanelTexture(FUiDisplayId Id) const noexcept
{
	const FDisplay* Display = Find_Internal(Id);
	return Display != nullptr ? Display->Texture : FRenderTarget{};
}
// 画面の点を表示先の論理座標へ変える。
bool FUiHostState::MapPointer_Internal(const FDisplay& Display, FVector2 Screen, FVector2& Out, Toolbox::f64* Distance,
                                       bool bCapture) const
{
	if (Distance != nullptr)
	{
		*Distance = 0;
	}
	const FUiSurface Surface = MakeSurface_Internal(Display);
	if (!Surface.IsDisplayable())
	{
		return false;
	}
	if (Display.Kind != EUiDisplayKind::WorldPanel3D)
	{
		FUiPixelRect Area = Surface.GetPixelRect();
		if (Display.Kind == EUiDisplayKind::WorldPanel2D && !Display.Panel2D.ScreenClip.IsEmpty())
		{
			Area = Area.Intersect(Display.Panel2D.ScreenClip);
		}
		Out = Surface.ToLogical(Screen);
		return Contains_Internal(Area, Screen);
	}
	// 3Dのパネル: Viewからの有限線分と平面の交点を、平面のUIの論理座標へ戻す（描画のUVと同じ対応）。
	const auto Views = m_Views3D;
	for (const FRenderView3D& View : Views)
	{
		if (View.bViewport &&
		    !Contains_Internal({View.Viewport.Left, View.Viewport.Top, View.Viewport.Right, View.Viewport.Bottom},
		                       Screen))
		{
			continue;
		}
		auto Segment = MakeViewPickSegment(View, m_ScreenWidth, m_ScreenHeight, Screen);
		if (!Segment || !Segment.Value())
		{
			continue;
		}
		const FLine3D& Line = *Segment.Value();
		Toolbox::FVector3 Hit;
		const auto Uv = IntersectUiWorldPanel3D(Display.Panel3D, Line.Start, Line.End, &Hit, bCapture);
		if (!Uv)
		{
			continue;
		}
		if (Display.Panel3D.IsOccluded && Display.Panel3D.IsOccluded(Line.Start, Hit))
		{
			continue;
		}
		const FUiSize Logical = Surface.GetLogicalSize();
		Out = {Uv->X * Logical.Width, Uv->Y * Logical.Height};
		if (Distance != nullptr)
		{
			*Distance = Toolbox::LengthSquared(Hit - Line.Start);
		}
		return Uv->X >= 0 && Uv->X < 1 && Uv->Y >= 0 && Uv->Y < 1;
	}
	return false;
}
} // namespace Dxf::Detail
// namespace Dxf::Detail
