// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiSceneHost.h"
#include "Dxf/AssetService.h"
#include "Dxf/UiRenderer.h"
#include "Dxf/ViewCoordinates.h"
namespace Dxf
{
namespace
{
// 入力プレイヤーの数。
constexpr Toolbox::size_t PlayerCount = 4;
// 操作の数。
constexpr Toolbox::size_t CommandCount = static_cast<Toolbox::size_t>(EUiNavigationCommand::Count);
// 前面から順に表示先を並べるための種類の優先度（大きいほど前面）。
Toolbox::int32 KindPriority_Internal(EUiDisplayKind Kind) noexcept
{
	switch (Kind)
	{
	case EUiDisplayKind::Screen:
		return 3;
	case EUiDisplayKind::Viewport:
		return 2;
	case EUiDisplayKind::WorldPanel2D:
		return 1;
	default:
		return 0;
	}
}
// 画素の矩形が点を含むか（半開区間）。
bool Contains_Internal(const FUiPixelRect& Rect, FVector2 Point) noexcept
{
	return Point.X >= static_cast<Toolbox::f32>(Rect.Left) && Point.Y >= static_cast<Toolbox::f32>(Rect.Top) &&
	       Point.X < static_cast<Toolbox::f32>(Rect.Right) && Point.Y < static_cast<Toolbox::f32>(Rect.Bottom);
}
// キーの割当の条件を満たして押されているか。
bool IsKeyBindingDown_Internal(const FInputSnapshot& Input, const FUiKeyBinding& Binding) noexcept
{
	if (Binding.Key >= EKey::Count || !Input.IsDown(Binding.Key))
	{
		return false;
	}
	const bool bShift = Input.IsDown(EKey::LeftShift) || Input.IsDown(EKey::RightShift);
	if (Binding.Shift == EUiShiftCondition::Shift)
	{
		return bShift;
	}
	if (Binding.Shift == EUiShiftCondition::NoShift)
	{
		return !bShift;
	}
	return true;
}
} // namespace

// 仲介を作る。
FUiSceneHost::FUiSceneHost(FUiSceneHostSettings Settings) : m_Settings(Toolbox::Move(Settings))
{
	m_ScreenWidth = m_Settings.ScreenWidth;
	m_ScreenHeight = m_Settings.ScreenHeight;
}
// 接続したSceneから外す。
FUiSceneHost::~FUiSceneHost()
{
	DetachFromScene();
}
// Sceneへ接続する。
void FUiSceneHost::AttachTo(DScene& Scene) noexcept
{
	DetachFromScene();
	m_pScene = &Scene;
	Scene.SetInputRouter(this);
}
// Sceneから外す。
void FUiSceneHost::DetachFromScene() noexcept
{
	if (m_pScene != nullptr && m_pScene->GetInputRouter() == this)
	{
		m_pScene->SetInputRouter(nullptr);
	}
	m_pScene = nullptr;
}
// 表示先を加える。
FUiDisplayId FUiSceneHost::Add_Internal(FDisplay Display)
{
	Display.Id = m_NextId++;
	m_Displays.PushBack(Toolbox::Move(Display));
	return m_Displays.Back().Id;
}
// 全画面の表示先を加える。
FUiDisplayId FUiSceneHost::AddScreen(FUiRoot& Root, const FUiDisplayOptions& Options)
{
	FDisplay Display;
	Display.Kind = EUiDisplayKind::Screen;
	Display.pRoot = &Root;
	Display.Options = Options;
	return Add_Internal(Toolbox::Move(Display));
}
// 分割画面の表示先を加える。
FUiDisplayId FUiSceneHost::AddViewport(FUiRoot& Root, FUiPixelRect Rect, const FUiDisplayOptions& Options)
{
	FDisplay Display;
	Display.Kind = EUiDisplayKind::Viewport;
	Display.pRoot = &Root;
	Display.Options = Options;
	Display.Rect = Rect;
	return Add_Internal(Toolbox::Move(Display));
}
// 2Dのパネルを加える。
FUiDisplayId FUiSceneHost::AddWorldPanel2D(FUiRoot& Root, const FUiWorldPanel2D& Panel, const FUiDisplayOptions& Options)
{
	FDisplay Display;
	Display.Kind = EUiDisplayKind::WorldPanel2D;
	Display.pRoot = &Root;
	Display.Options = Options;
	Display.Panel2D = Panel;
	return Add_Internal(Toolbox::Move(Display));
}
// 3Dのパネルを加える。
FUiDisplayId FUiSceneHost::AddWorldPanel3D(FUiRoot& Root, const FUiWorldPanel3D& Panel, FAssetService& Assets,
                                           const FUiDisplayOptions& Options)
{
	FDisplay Display;
	Display.Kind = EUiDisplayKind::WorldPanel3D;
	Display.pRoot = &Root;
	Display.Options = Options;
	Display.Panel3D = Panel;
	Display.pAssets = &Assets;
	return Add_Internal(Toolbox::Move(Display));
}
// 表示先を探す。
FUiSceneHost::FDisplay* FUiSceneHost::Find_Internal(FUiDisplayId Id) noexcept
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
const FUiSceneHost::FDisplay* FUiSceneHost::Find_Internal(FUiDisplayId Id) const noexcept
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
bool FUiSceneHost::Remove(FUiDisplayId Id) noexcept
{
	for (Toolbox::size_t Index = 0; Index < m_Displays.Size(); ++Index)
	{
		if (m_Displays[Index].Id != Id)
		{
			continue;
		}
		FUiRoot* Root = m_Displays[Index].pRoot;
		m_Displays.Erase(m_Displays.Begin() + Index);
		if (m_PointerOwner == Id)
		{
			m_PointerOwner = 0;
		}
		// 同じルートの表示先が残っていなければ、ホバーとキャプチャを外す。
		bool bStillShown = false;
		for (const auto& Display : m_Displays)
		{
			bStillShown = bStillShown || Display.pRoot == Root;
		}
		if (!bStillShown && Root != nullptr)
		{
			Root->ResetPointer();
		}
		return true;
	}
	return false;
}
// 分割画面の領域を変える。
bool FUiSceneHost::SetViewportRect(FUiDisplayId Id, FUiPixelRect Rect) noexcept
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
bool FUiSceneHost::SetWorldPanel2D(FUiDisplayId Id, const FUiWorldPanel2D& Panel) noexcept
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
bool FUiSceneHost::SetWorldPanel3D(FUiDisplayId Id, const FUiWorldPanel3D& Panel) noexcept
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
void FUiSceneHost::SetWorldViews3D(const Toolbox::TVector<FRenderView3D>& Views)
{
	m_Views3D = Views;
}
// 表示先の設定を変える。
bool FUiSceneHost::SetOptions(FUiDisplayId Id, const FUiDisplayOptions& Options) noexcept
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
void FUiSceneHost::SetPlayerDevices(Toolbox::int32 Player, FUiPlayerDevices Devices) noexcept
{
	if (Player >= 0 && static_cast<Toolbox::size_t>(Player) < PlayerCount)
	{
		m_Settings.Players[static_cast<Toolbox::size_t>(Player)] = Devices;
	}
}
// 操作の割当を設定する。
void FUiSceneHost::SetNavigationBindings(FUiNavigationBindings Bindings)
{
	m_Settings.Bindings = Toolbox::Move(Bindings);
}
// 画面の大きさを設定する。
void FUiSceneHost::SetScreenSize(Toolbox::int32 Width, Toolbox::int32 Height) noexcept
{
	if (Width > 0 && Height > 0)
	{
		m_ScreenWidth = Width;
		m_ScreenHeight = Height;
	}
}
// 表示先の表示面。
FUiSurface FUiSceneHost::MakeSurface_Internal(const FDisplay& Display) const noexcept
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
		return FUiSurface({0, 0, Display.Panel3D.TextureWidth, Display.Panel3D.TextureHeight}, Display.Options.Scale);
	}
}
// 表示先の表示面（公開）。
FUiSurface FUiSceneHost::GetSurface(FUiDisplayId Id) const noexcept
{
	const FDisplay* Display = Find_Internal(Id);
	return Display != nullptr ? MakeSurface_Internal(*Display) : FUiSurface{};
}
// 3Dのパネルの描画先テクスチャ。
FRenderTarget FUiSceneHost::GetWorldPanelTexture(FUiDisplayId Id) const noexcept
{
	const FDisplay* Display = Find_Internal(Id);
	return Display != nullptr ? Display->Texture : FRenderTarget{};
}
// 画面の点を表示先の論理座標へ変える。
bool FUiSceneHost::MapPointer_Internal(const FDisplay& Display, FVector2 Screen, FVector2& Out) const
{
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
	for (const FRenderView3D& View : m_Views3D)
	{
		if (View.bViewport && !Contains_Internal({View.Viewport.Left, View.Viewport.Top, View.Viewport.Right, View.Viewport.Bottom}, Screen))
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
		const auto Uv = IntersectUiWorldPanel3D(Display.Panel3D, Line.Start, Line.End, &Hit);
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
		return true;
	}
	return false;
}
// 入力を処理し、UIを除いた入力を返す。
const FInputSnapshot& FUiSceneHost::RouteInput(const FTickContext& Context)
{
	// 同じフレームの二度目の呼出しは、UIを進めずに同じ結果を返す（1フレームに1回）。
	if (Context.Time.FrameIndex == m_LastFrame)
	{
		return m_Filtered.GetSnapshot();
	}
	m_LastFrame = Context.Time.FrameIndex;
	const FInputSnapshot& Input = Context.Input;
	const FRawInput& Raw = Input.GetRaw();
	const Toolbox::f64 Delta = Toolbox::Clamp(Context.Time.UnscaledDeltaSeconds, 0.0, 0.25);
	FUiRoutingResult Routing;
	// 前面から順の表示先（種類の優先度→登録の逆順。3Dのパネルは登録の逆順）。
	Toolbox::TVector<FDisplay*> Order;
	Order.Reserve(m_Displays.Size());
	for (Toolbox::int32 Priority = 3; Priority >= 0; --Priority)
	{
		for (Toolbox::size_t Index = m_Displays.Size(); Index > 0; --Index)
		{
			FDisplay& Display = m_Displays[Index - 1];
			if (KindPriority_Internal(Display.Kind) == Priority && Display.pRoot != nullptr)
			{
				Order.PushBack(&Display);
			}
		}
	}
	// ポインターの対象: 押し始めた表示先が離すまで受ける。そうでなければ最前面でUIの要素に当たる表示先。
	const FVector2 Screen{static_cast<Toolbox::f32>(Raw.MouseX), static_cast<Toolbox::f32>(Raw.MouseY)};
	FDisplay* Target = nullptr;
	FVector2 TargetPosition;
	bool bTargetInside = false;
	if (FDisplay* Owner = Find_Internal(m_PointerOwner))
	{
		// 押し始めた表示先は、ポインターが外へ出ても離すまで受ける（外では決定しない）。
		Target = Owner;
		bTargetInside = MapPointer_Internal(*Owner, Screen, TargetPosition);
	}
	else
	{
		m_PointerOwner = 0;
		for (FDisplay* Display : Order)
		{
			FVector2 Logical;
			if (MapPointer_Internal(*Display, Screen, Logical) && Display->pRoot->HitTest(Logical) != nullptr)
			{
				Target = Display;
				TargetPosition = Logical;
				bTargetInside = true;
				break;
			}
		}
	}
	Routing.PointerDisplay = Target != nullptr ? Target->Id : 0;
	// ルートごとの一フレームの入力（同じルートは一つにまとめる）。
	struct FRootFrame
	{
		FUiRoot* pRoot = nullptr;
		FDisplay* pDisplay = nullptr;
		FUiInputFrame Frame;
		FUiInputResult Result;
	};
	Toolbox::TVector<FRootFrame> Frames;
	auto FrameOf = [&](FDisplay& Display) -> FRootFrame&
	{
		for (auto& Item : Frames)
		{
			if (Item.pRoot == Display.pRoot)
			{
				return Item;
			}
		}
		FRootFrame Item;
		Item.pRoot = Display.pRoot;
		Item.pDisplay = &Display;
		Item.Frame.DeltaSeconds = Delta;
		Frames.PushBack(Toolbox::Move(Item));
		return Frames.Back();
	};
	for (FDisplay* Display : Order)
	{
		(void)FrameOf(*Display);
	}
	if (Target != nullptr)
	{
		FRootFrame& Item = FrameOf(*Target);
		Item.pDisplay = Target;
		FUiPointerFrame& Pointer = Item.Frame.Pointer;
		Pointer.bPresent = bTargetInside;
		Pointer.Position = TargetPosition;
		for (Toolbox::size_t Button = 0; Button < Pointer.Down.Size(); ++Button)
		{
			const auto Mouse = static_cast<EMouseButton>(Button);
			Pointer.Down[Button] = Input.IsMouseDown(Mouse);
			Pointer.Pressed[Button] = Input.WasMousePressed(Mouse);
			Pointer.Released[Button] = Input.WasMouseReleased(Mouse);
		}
		// DxLibは奥へ回すと正。UIは手前（下方向へのスクロール）を正とする。
		Pointer.WheelNotches = -static_cast<Toolbox::f32>(Raw.Wheel);
	}
	// 入力プレイヤーごとの操作と、それを受ける表示先。
	Toolbox::TArray<Toolbox::TArray<bool, CommandCount>, PlayerCount> NavigationDown{};
	Toolbox::TArray<FDisplay*, PlayerCount> NavigationTarget{};
	for (Toolbox::size_t Player = 0; Player < PlayerCount; ++Player)
	{
		const FUiPlayerDevices& Devices = m_Settings.Players[Player];
		const bool bPad = Devices.Pad >= 0 && static_cast<Toolbox::size_t>(Devices.Pad) < Raw.Pads.Size() &&
		                  Raw.Pads[static_cast<Toolbox::size_t>(Devices.Pad)].bConnected;
		for (Toolbox::size_t Command = 0; Command < CommandCount; ++Command)
		{
			bool bDown = false;
			if (Devices.bKeyboard)
			{
				for (const FUiKeyBinding& Binding : m_Settings.Bindings.Keys[Command])
				{
					bDown = bDown || IsKeyBindingDown_Internal(Input, Binding);
				}
			}
			if (bPad)
			{
				const auto PadIndex = static_cast<Toolbox::size_t>(Devices.Pad);
				for (Toolbox::size_t Button : m_Settings.Bindings.PadButtons[Command])
				{
					bDown = bDown || Input.IsPadDown(PadIndex, Button);
				}
				if (m_Settings.Bindings.bStickNavigation)
				{
					const FGamepadState& Pad = Raw.Pads[PadIndex];
					const Toolbox::f32 Threshold = m_Settings.Bindings.StickThreshold;
					const auto Kind = static_cast<EUiNavigationCommand>(Command);
					bDown = bDown || (Kind == EUiNavigationCommand::Up && Pad.LeftY < -Threshold) ||
					        (Kind == EUiNavigationCommand::Down && Pad.LeftY > Threshold) ||
					        (Kind == EUiNavigationCommand::Left && Pad.LeftX < -Threshold) ||
					        (Kind == EUiNavigationCommand::Right && Pad.LeftX > Threshold);
				}
			}
			NavigationDown[Player][Command] = bDown;
		}
		// 操作を受ける表示先: Modalのルート、フォーカスのあるルート、常に受けるルートの順（前面優先）。
		FDisplay* Chosen = nullptr;
		for (Toolbox::int32 Pass = 0; Pass < 3 && Chosen == nullptr; ++Pass)
		{
			for (FDisplay* Display : Order)
			{
				if (Display->Options.Player != static_cast<Toolbox::int32>(Player) ||
				    Display->Options.Navigation == EUiNavigationPolicy::Never)
				{
					continue;
				}
				const bool bMatch = Pass == 0   ? Display->pRoot->GetTopModal() != nullptr
				                    : Pass == 1 ? Display->pRoot->GetFocused() != nullptr
				                                : Display->Options.Navigation == EUiNavigationPolicy::Always;
				if (bMatch)
				{
					Chosen = Display;
					break;
				}
			}
		}
		NavigationTarget[Player] = Chosen;
		if (Chosen != nullptr)
		{
			FRootFrame& Item = FrameOf(*Chosen);
			if (!Item.Frame.Navigation.bActive)
			{
				Item.Frame.Navigation.bActive = true;
				for (Toolbox::size_t Command = 0; Command < CommandCount; ++Command)
				{
					Item.Frame.Navigation.Down[Command] = NavigationDown[Player][Command];
					Item.Frame.Navigation.Pressed[Command] = NavigationDown[Player][Command] && !m_PreviousNavigation[Player][Command];
				}
				Routing.NavigationDisplays[Player] = Chosen->Id;
			}
		}
		m_PreviousNavigation[Player] = NavigationDown[Player];
	}
	// 各ルートを一度だけ進める（入力→更新）。
	for (auto& Item : Frames)
	{
		auto Processed = Item.pRoot->ProcessInput(Item.Frame);
		if (!Processed)
		{
			throw Toolbox::FException(Processed.Error().Message.CStr());
		}
		Item.Result = Processed.Value();
		auto Updated = Item.pRoot->Update(Delta);
		if (!Updated)
		{
			throw Toolbox::FException(Updated.Error().Message.CStr());
		}
		++Routing.ProcessedRoots;
	}
	auto ResultOf = [&](const FDisplay* Display) -> const FUiInputResult*
	{
		for (const auto& Item : Frames)
		{
			if (Display != nullptr && Item.pRoot == Display->pRoot)
			{
				return &Item.Result;
			}
		}
		return nullptr;
	};
	// マウス: UIが受けた押下は、離すまでゲームへ届けない。
	if (const FUiInputResult* Result = ResultOf(Target))
	{
		for (Toolbox::size_t Button = 0; Button < m_OwnedMouse.Size(); ++Button)
		{
			if (Result->ButtonsClaimed[Button])
			{
				m_OwnedMouse[Button] = true;
				m_PointerOwner = Target->Id;
			}
		}
		Routing.bWheelConsumed = Result->bWheelConsumed;
	}
	bool bAnyMouseOwned = false;
	for (Toolbox::size_t Button = 0; Button < m_OwnedMouse.Size(); ++Button)
	{
		if (!Input.IsMouseDown(static_cast<EMouseButton>(Button)))
		{
			m_OwnedMouse[Button] = false;
		}
		bAnyMouseOwned = bAnyMouseOwned || m_OwnedMouse[Button];
	}
	if (!bAnyMouseOwned)
	{
		const FDisplay* Owner = Find_Internal(m_PointerOwner);
		if (Owner == nullptr || Owner->pRoot->GetCaptured() == nullptr)
		{
			m_PointerOwner = 0;
		}
	}
	// キー・パッド: UIが処理した操作の物理入力と、Modal中の入力プレイヤーの全入力は、離すまでゲームへ届けない。
	Toolbox::TArray<bool, PlayerCount> BlockedPads{};
	for (Toolbox::size_t Player = 0; Player < PlayerCount; ++Player)
	{
		const FDisplay* Chosen = NavigationTarget[Player];
		const FUiInputResult* Result = ResultOf(Chosen);
		if (Chosen == nullptr || Result == nullptr)
		{
			continue;
		}
		const FUiPlayerDevices& Devices = m_Settings.Players[Player];
		const bool bPad = Devices.Pad >= 0 && static_cast<Toolbox::size_t>(Devices.Pad) < m_OwnedPad.Size();
		const auto PadIndex = static_cast<Toolbox::size_t>(bPad ? Devices.Pad : 0);
		if (Result->bModal && Chosen->Options.bBlockGameWhileModal)
		{
			if (Devices.bKeyboard)
			{
				for (Toolbox::size_t Key = 0; Key < m_OwnedKeys.Size(); ++Key)
				{
					m_OwnedKeys[Key] = m_OwnedKeys[Key] || Raw.Keys[Key];
				}
			}
			if (bPad)
			{
				for (Toolbox::size_t Button = 0; Button < 16; ++Button)
				{
					m_OwnedPad[PadIndex][Button] = m_OwnedPad[PadIndex][Button] || Input.IsPadDown(PadIndex, Button);
				}
				BlockedPads[PadIndex] = true;
			}
			continue;
		}
		if (!Result->bNavigationConsumed)
		{
			continue;
		}
		for (Toolbox::size_t Command = 0; Command < CommandCount; ++Command)
		{
			if (Devices.bKeyboard)
			{
				for (const FUiKeyBinding& Binding : m_Settings.Bindings.Keys[Command])
				{
					if (Binding.Key < EKey::Count && IsKeyBindingDown_Internal(Input, Binding))
					{
						m_OwnedKeys[static_cast<Toolbox::size_t>(Binding.Key)] = true;
					}
				}
			}
			if (bPad)
			{
				for (Toolbox::size_t Button : m_Settings.Bindings.PadButtons[Command])
				{
					if (Button < 16 && Input.IsPadDown(PadIndex, Button))
					{
						m_OwnedPad[PadIndex][Button] = true;
					}
				}
			}
		}
	}
	// 離した入力の所有を解く。ゲームへ渡す入力を作る。
	FRawInput Filtered = Raw;
	for (Toolbox::size_t Key = 0; Key < m_OwnedKeys.Size(); ++Key)
	{
		m_OwnedKeys[Key] = m_OwnedKeys[Key] && Raw.Keys[Key];
		if (m_OwnedKeys[Key])
		{
			Filtered.Keys[Key] = false;
			++Routing.OwnedKeys;
		}
	}
	for (Toolbox::size_t Button = 0; Button < m_OwnedMouse.Size(); ++Button)
	{
		if (m_OwnedMouse[Button])
		{
			Filtered.MouseButtons[Button] = false;
			++Routing.OwnedMouseButtons;
		}
	}
	for (Toolbox::size_t Pad = 0; Pad < m_OwnedPad.Size(); ++Pad)
	{
		for (Toolbox::size_t Button = 0; Button < 16; ++Button)
		{
			m_OwnedPad[Pad][Button] = m_OwnedPad[Pad][Button] && Input.IsPadDown(Pad, Button);
			if (m_OwnedPad[Pad][Button])
			{
				Filtered.Pads[Pad].Buttons[Button] = false;
				++Routing.OwnedPadButtons;
			}
		}
		if (BlockedPads[Pad])
		{
			Filtered.Pads[Pad].LeftX = 0;
			Filtered.Pads[Pad].LeftY = 0;
		}
	}
	if (Routing.bWheelConsumed)
	{
		Filtered.Wheel = 0;
	}
	m_Filtered.Advance(Filtered);
	m_LastRouting = Routing;
	return m_Filtered.GetSnapshot();
}
// 3Dのパネルの描画先テクスチャへUIを描く。
TResult<void> FUiSceneHost::RenderWorldPanelTextures(FRenderContext& Render)
{
	for (auto& Display : m_Displays)
	{
		if (Display.Kind != EUiDisplayKind::WorldPanel3D || Display.pRoot == nullptr || Display.pAssets == nullptr)
		{
			continue;
		}
		const FUiWorldPanel3D& Panel = Display.Panel3D;
		if (Panel.TextureWidth <= 0 || Panel.TextureHeight <= 0)
		{
			continue;
		}
		if (!Display.Texture.IsValid() || Display.Texture.GetWidth() != Panel.TextureWidth || Display.Texture.GetHeight() != Panel.TextureHeight)
		{
			auto Created = Display.pAssets->CreateRenderTarget(Panel.TextureWidth, Panel.TextureHeight, true);
			if (!Created)
			{
				return TResult<void>::Failure(Created.Error());
			}
			Display.Texture = Created.Value();
		}
		const FUiSurface Surface = MakeSurface_Internal(Display);
		Display.pRoot->SetSurface(Surface);
		auto Laid = Display.pRoot->Layout();
		if (!Laid)
		{
			return Laid;
		}
		m_DrawList.Clear();
		auto Built = Display.pRoot->BuildDrawList(m_DrawList);
		if (!Built)
		{
			return Built;
		}
		auto Target = Render.SetRenderTarget(Display.Texture);
		if (!Target)
		{
			return Target;
		}
		// UIのパネルは自身の背景を描く。描画先は不透明な黒で消去する。
		auto Cleared = Render.ClearTarget({0, 0, 0, 255});
		auto Submitted = Cleared ? SubmitUiDrawList(Render.Get2D(), m_DrawList, Surface, {0, 0})
		                         : TResult<FUiRenderStats>::Failure(Cleared.Error());
		// 失敗しても画面へ戻す（最初の失敗を結果として保つ）。
		auto Back = Render.SetBackBuffer();
		if (!Submitted)
		{
			return TResult<void>::Failure(Submitted.Error());
		}
		if (!Back)
		{
			return Back;
		}
	}
	return {};
}
// 3Dのパネルを平面として描く。
TResult<void> FUiSceneHost::DrawWorldPanels3D(FRenderContext& Render, const FRenderView3D& View)
{
	// 入力の判定に使うViewを記録する（同じIdは置き換える）。
	bool bKnown = false;
	for (auto& Known : m_Views3D)
	{
		if (Known.Id == View.Id)
		{
			Known = View;
			bKnown = true;
		}
	}
	if (!bKnown)
	{
		m_Views3D.PushBack(View);
	}
	for (const auto& Display : m_Displays)
	{
		if (Display.Kind != EUiDisplayKind::WorldPanel3D || !Display.Texture.IsValid())
		{
			continue;
		}
		FTexturedQuad3D Quad;
		Quad.Texture = Display.Texture.AsTexture();
		Quad.Corners = {Display.Panel3D.TopLeft, Display.Panel3D.TopRight, Display.Panel3D.BottomRight(), Display.Panel3D.BottomLeft};
		Quad.Depth = Display.Panel3D.Depth;
		Quad.bDoubleSided = Display.Panel3D.bDoubleSided;
		auto Drawn = Render.Get3D().DrawTexturedQuad(Quad);
		if (!Drawn)
		{
			return Drawn;
		}
	}
	return {};
}
// 全画面・Viewport・2Dのパネルを描く。
TResult<void> FUiSceneHost::Draw(FRenderContext& Render)
{
	if (Render.GetTargetWidth() > 0 && Render.GetTargetHeight() > 0)
	{
		m_ScreenWidth = Render.GetTargetWidth();
		m_ScreenHeight = Render.GetTargetHeight();
	}
	// 同じルートは最初の表示先で一度だけレイアウト・記録する。
	Toolbox::TVector<FUiRoot*> Drawn;
	for (const auto& Display : m_Displays)
	{
		if (Display.pRoot == nullptr || Display.Kind == EUiDisplayKind::WorldPanel3D)
		{
			continue;
		}
		bool bDone = false;
		for (FUiRoot* Root : Drawn)
		{
			bDone = bDone || Root == Display.pRoot;
		}
		if (bDone)
		{
			continue;
		}
		Drawn.PushBack(Display.pRoot);
		const FUiSurface Surface = MakeSurface_Internal(Display);
		Display.pRoot->SetSurface(Surface);
		auto Laid = Display.pRoot->Layout();
		if (!Laid)
		{
			return Laid;
		}
		m_DrawList.Clear();
		auto Built = Display.pRoot->BuildDrawList(m_DrawList);
		if (!Built)
		{
			return Built;
		}
		// 2Dのパネルは、指定があれば画面の範囲（Viewport）で切り抜く。
		FUiSurface Clipped = Surface;
		if (Display.Kind == EUiDisplayKind::WorldPanel2D && !Display.Panel2D.ScreenClip.IsEmpty())
		{
			for (auto& Item : m_DrawList.EditItems())
			{
				const FUiPixelRect Area = Surface.GetPixelRect().Intersect(Display.Panel2D.ScreenClip);
				const FVector2 TopLeft = Surface.ToLogical({static_cast<Toolbox::f32>(Area.Left), static_cast<Toolbox::f32>(Area.Top)});
				const FVector2 BottomRight = Surface.ToLogical({static_cast<Toolbox::f32>(Area.Right), static_cast<Toolbox::f32>(Area.Bottom)});
				Item.Clip = Item.Clip.Intersect({TopLeft.X, TopLeft.Y, BottomRight.X - TopLeft.X, BottomRight.Y - TopLeft.Y});
			}
		}
		auto Submitted = SubmitUiDrawList(Render.Get2D(), m_DrawList, Clipped, {Display.Options.Layer, 0});
		if (!Submitted)
		{
			return TResult<void>::Failure(Submitted.Error());
		}
	}
	return {};
}
} // namespace Dxf
