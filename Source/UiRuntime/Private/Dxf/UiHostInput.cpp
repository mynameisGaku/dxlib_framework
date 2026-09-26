// SPDX-License-Identifier: NOASSERTION
#include "UiHostState.h"
#include "Dxf/GuardValue.h"
#include "Toolbox/Algorithm.h"
namespace Dxf::Detail
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
// namespace
// 入力を処理し、UIを除いた入力を返す。
FInputSnapshot FUiHostState::RouteInput(const FTickContext& Context)
{
	if (m_bRouting)
	{
		throw Toolbox::FException("Reentrant UI input routing");
	}

	TGuardValue Busy(m_bRouting, true);
	if (m_bStopped)
	{
		return Context.Input;
	}
	// 同じフレームの二度目の呼出しは、UIを進めずに同じ結果を返す（1フレームに1回）。
	if (Context.Time.FrameIndex == m_LastFrame)
	{
		return m_Filtered.GetSnapshot();
	}
	const FInputSnapshot& Input = Context.Input;
	const FRawInput& Raw = Input.GetRaw();
	const Toolbox::f64 Delta = Toolbox::Clamp(Context.Time.UnscaledDeltaSeconds, 0.0, 0.25);
	FUiRoutingResult Routing;
	// 通知中の追加・削除・並べ替えで配送先ポインターを無効にしない値スナップショット。
	auto Displays = m_Displays;
	// 前面から順の表示先（種類の優先度→登録の逆順。3Dのパネルは登録の逆順）。
	Toolbox::TVector<FDisplay*> Order;
	Order.Reserve(Displays.Size());
	for (Toolbox::int32 Priority = 3; Priority >= 0; --Priority)
	{
		for (Toolbox::size_t Index = Displays.Size(); Index > 0; --Index)
		{
			FDisplay& Display = Displays[Index - 1];
			if (KindPriority_Internal(Display.Kind) == Priority && Display.Root.Get() != nullptr)
			{
				Order.PushBack(&Display);
			}
		}
	}
	// 2Dの前後は描画のLayerと安定した登録順と一致させる。
	Toolbox::StableSort(Order.Begin(), Order.End(),
	                    [](const FDisplay* A, const FDisplay* B)
	                    {
		                    const bool FlatA = A->Kind != EUiDisplayKind::WorldPanel3D;
		                    const bool FlatB = B->Kind != EUiDisplayKind::WorldPanel3D;
		                    if (FlatA != FlatB)
		                    {
			                    return FlatA;
		                    }
		                    if (A->Options.Layer != B->Options.Layer)
		                    {
			                    return A->Options.Layer > B->Options.Layer;
		                    }
		                    return A->Id > B->Id;
	                    });
	// ポインターの対象: 押し始めた表示先が離すまで受ける。そうでなければ最前面でUIの要素に当たる表示先。
	const FVector2 Screen{static_cast<Toolbox::f32>(Raw.MouseX), static_cast<Toolbox::f32>(Raw.MouseY)};
	FDisplay* Target = nullptr;
	FVector2 TargetPosition = m_LastPointerPosition;
	bool bTargetInside = false;
	FDisplay* SavedOwner = nullptr;
	for (auto& Display : Displays)
	{
		if (Display.Id == m_PointerOwner && Display.Root.Get() != nullptr)
		{
			SavedOwner = &Display;
		}
	}
	if (FDisplay* Owner = SavedOwner)
	{
		// 押し始めた表示先は、ポインターが外へ出ても離すまで受ける（外では決定しない）。
		Target = Owner;
		bTargetInside = MapPointer_Internal(*Owner, Screen, TargetPosition, nullptr, true);
	}
	else
	{
		m_PointerOwner = 0;
		Toolbox::f64 BestDistance = Toolbox::TNumericLimits<Toolbox::f64>::Max();
		for (FDisplay* Display : Order)
		{
			FUiRoot* LiveRoot = Display->Root.Get();
			if (LiveRoot == nullptr || Find_Internal(Display->Id) == nullptr)
			{
				continue;
			}
			LiveRoot->SetSurface(MakeSurface_Internal(*Display));
			auto Laid = LiveRoot->Layout();
			if (!Laid)
			{
				throw Toolbox::FException(Laid.Error().Message);
			}
			if (m_bStopped || Display->Root.Get() == nullptr)
			{
				continue;
			}
			FVector2 Logical;
			Toolbox::f64 Distance = 0;
			const bool Mapped = MapPointer_Internal(*Display, Screen, Logical, &Distance);
			if (m_bStopped || Display->Root.Get() == nullptr || Find_Internal(Display->Id) == nullptr)
			{
				continue;
			}
			if (Mapped && Display->Root.Get()->HitTest(Logical) != nullptr && Distance < BestDistance)
			{
				Target = Display;
				TargetPosition = Logical;
				bTargetInside = true;
				BestDistance = Distance;
				if (Display->Kind != EUiDisplayKind::WorldPanel3D)
				{
					break;
				}
			}
		}
	}
	if (Target != nullptr)
	{
		m_LastPointerPosition = TargetPosition;
	}
	Routing.PointerDisplay = Target != nullptr ? Target->Id : 0;
	// ルートごとの一フレームの入力（同じルートは一つにまとめる）。
	struct FRootFrame
	{
		FUiRootHandle Root;
		const FUiRoot* Identity = nullptr;
		FDisplay* pDisplay = nullptr;
		FUiInputFrame Frame;
		FUiInputResult Result;
	};
	Toolbox::TVector<FRootFrame> Frames;
	auto FrameOf = [&](FDisplay& Display) -> FRootFrame&
	{
		for (auto& Item : Frames)
		{
			if (Item.Identity == Display.RootIdentity)
			{
				return Item;
			}
		}
		FRootFrame Item;
		Item.Root = Display.Root;
		Item.Identity = Display.RootIdentity;
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
		// Shiftを押している間のホイールは横のスクロール（奥へ回すと左）。
		const Toolbox::f32 Notches = -static_cast<Toolbox::f32>(Raw.Wheel);
		if (Input.IsDown(EKey::LeftShift) || Input.IsDown(EKey::RightShift))
		{
			Pointer.WheelNotchesX = Notches;
		}
		else
		{
			Pointer.WheelNotches = Notches;
		}
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
				if (Display->Root.Get() == nullptr || Find_Internal(Display->Id) == nullptr ||
				    Display->Options.Player != static_cast<Toolbox::int32>(Player) ||
				    Display->Options.Navigation == EUiNavigationPolicy::Never)
				{
					continue;
				}
				const bool bMatch = Pass == 0   ? Display->Root.Get()->GetTopModal() != nullptr
				                    : Pass == 1 ? Display->Root.Get()->GetFocused() != nullptr
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
					Item.Frame.Navigation.Pressed[Command] =
					    NavigationDown[Player][Command] && !m_PreviousNavigation[Player][Command];
				}
				Routing.NavigationDisplays[Player] = Chosen->Id;
			}
		}
		m_PreviousNavigation[Player] = NavigationDown[Player];
	}
	// 各ルートを一度だけ進める（入力→更新）。
	for (auto& Item : Frames)
	{
		if (m_bStopped || Find_Internal(Item.pDisplay->Id) == nullptr || Item.Root.Get() == nullptr)
		{
			continue;
		}
		Item.Root.Get()->SetSurface(MakeSurface_Internal(*Item.pDisplay));
		auto Prepared = Item.Root.Get()->Layout();
		if (!Prepared)
		{
			throw Toolbox::FException(Prepared.Error().Message);
		}
		if (Item.Root.Get() == nullptr || m_bStopped)
		{
			continue;
		}
		auto Processed = Item.Root.Get()->ProcessInput(Item.Frame);
		if (!Processed)
		{
			throw Toolbox::FException(Processed.Error().Message.CStr());
		}
		Item.Result = Processed.Value();
		++Routing.ProcessedRoots;
		if (Item.Root.Get() == nullptr || m_bStopped)
		{
			continue;
		}
		auto Updated = Item.Root.Get()->Update(Delta);
		if (!Updated)
		{
			throw Toolbox::FException(Updated.Error().Message.CStr());
		}
	}
	auto ResultOf = [&](const FDisplay* Display) -> const FUiInputResult*
	{
		for (const auto& Item : Frames)
		{
			if (Display != nullptr && Item.pDisplay != nullptr && Item.Identity == Display->RootIdentity)
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
		if (Owner == nullptr || Owner->Root.Get() == nullptr || Owner->Root.Get()->GetCaptured() == nullptr)
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
		for (const FDisplay* Display : Order)
		{
			const FUiInputResult* Candidate = ResultOf(Display);
			if (Display->Options.Player == static_cast<Toolbox::int32>(Player) &&
			    Display->Options.bBlockGameWhileModal && Candidate != nullptr && Candidate->bModal)
			{
				Chosen = Display;
				Result = Candidate;
				break;
			}
		}
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
				m_OwnedStick[PadIndex] = true;
			}
			continue;
		}
		if (!Result->bNavigationConsumed)
		{
			continue;
		}
		if (bPad && m_Settings.Bindings.bStickNavigation &&
		    (Toolbox::Abs(Raw.Pads[PadIndex].LeftX) >= m_Settings.Bindings.StickThreshold ||
		     Toolbox::Abs(Raw.Pads[PadIndex].LeftY) >= m_Settings.Bindings.StickThreshold))
		{
			m_OwnedStick[PadIndex] = true;
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
		if (!Raw.bFocused || !Raw.Pads[Pad].bConnected ||
		    (Toolbox::Abs(Raw.Pads[Pad].LeftX) < 0.2f && Toolbox::Abs(Raw.Pads[Pad].LeftY) < 0.2f))
		{
			m_OwnedStick[Pad] = false;
		}
		if (BlockedPads[Pad] || m_OwnedStick[Pad])
		{
			Filtered.Pads[Pad].LeftX = 0;
			Filtered.Pads[Pad].LeftY = 0;
		}
	}
	if (Routing.bWheelConsumed)
	{
		Filtered.Wheel = 0;
	}
	// 最初の仲介で既に押されていた入力を新規押下へ変えない。
	if (!m_bSeeded)
	{
		FRawInput Previous = Filtered;
		for (Toolbox::size_t Key = 0; Key < Previous.Keys.Size(); ++Key)
		{
			Previous.Keys[Key] = Previous.Keys[Key] && !Input.WasPressed(static_cast<EKey>(Key));
		}
		for (Toolbox::size_t Button = 0; Button < Previous.MouseButtons.Size(); ++Button)
		{
			Previous.MouseButtons[Button] =
			    Previous.MouseButtons[Button] && !Input.WasMousePressed(static_cast<EMouseButton>(Button));
		}
		for (Toolbox::size_t Pad = 0; Pad < Previous.Pads.Size(); ++Pad)
		{
			for (Toolbox::size_t Button = 0; Button < Previous.Pads[Pad].Buttons.Size(); ++Button)
			{
				Previous.Pads[Pad].Buttons[Button] =
				    Previous.Pads[Pad].Buttons[Button] && !Input.WasPadPressed(Pad, Button);
			}
		}
		m_Filtered.Advance(Previous);
		m_bSeeded = true;
	}
	m_Filtered.Advance(Filtered);
	m_LastFrame = Context.Time.FrameIndex;
	m_LastRouting = Routing;
	return m_Filtered.GetSnapshot();
}
} // namespace Dxf::Detail
// namespace Dxf::Detail
