// SPDX-License-Identifier: NOASSERTION
#include "UiFocusManager.h"
#include "UiRootState.h"
namespace Dxf::Detail
{
namespace
{
// 祖先（自身を含む）か。
bool IsWithin_Internal(const DUiElement* Ancestor, const DUiElement* Element) noexcept
{
	for (const DUiElement* Current = Element; Current != nullptr; Current = Current->GetParent())
	{
		if (Current == Ancestor)
		{
			return true;
		}
	}
	return false;
}
// 前順に集める。
void Collect_Internal(FUiRootState& State, DUiElement& Element, Toolbox::TVector<DUiElement*>& Out)
{
	if (!FUiRootState::IsLive(&Element) || Element.GetVisibility() != EUiVisibility::Visible || !Element.IsEnabled())
	{
		return;
	}
	if (FUiFocusManager::CanFocus(State, Element))
	{
		Out.PushBack(&Element);
	}
	for (Toolbox::size_t Index = 0; Index < Element.GetChildCount(); ++Index)
	{
		if (DUiElement* Child = Element.GetChild(Index))
		{
			Collect_Internal(State, *Child, Out);
		}
	}
}
// 繰り返す操作か（方向とTab）。
FORCEINLINE bool IsRepeatable_Internal(EUiNavigationCommand Command) noexcept
{
	return Command != EUiNavigationCommand::Confirm && Command != EUiNavigationCommand::Cancel;
}
} // namespace

// 要素がフォーカスを受けられるか。
bool FUiFocusManager::CanFocus(FUiRootState& State, const DUiElement& Element) noexcept
{
	if (!FUiRootState::IsLive(&Element) || !Element.IsFocusable() || !Element.IsVisibleInTree() ||
	    !Element.IsEnabledInTree())
	{
		return false;
	}
	DUiElement* Modal = State.GetTopModal();
	return Modal == nullptr || IsWithin_Internal(Modal, &Element);
}
// フォーカスを受けられる要素の一覧。
Toolbox::TVector<DUiElement*> FUiFocusManager::CollectFocusable(FUiRootState& State)
{
	Toolbox::TVector<DUiElement*> Out;
	if (DUiElement* Modal = State.GetTopModal())
	{
		Collect_Internal(State, *Modal, Out);
		return Out;
	}
	for (DUiElement* Layer : State.Layers)
	{
		Collect_Internal(State, *Layer, Out);
	}
	return Out;
}
// フォーカスを移す。
bool FUiFocusManager::SetFocus(FUiRootState& State, DUiElement* Element)
{
	if (Element != nullptr && !CanFocus(State, *Element))
	{
		return false;
	}
	DUiElement* Previous = m_Focused.Get();
	if (Previous == Element)
	{
		return true;
	}
	m_Focused = Element != nullptr ? Element->GetRef() : TUiRef<DUiElement>{};
	if (Previous != nullptr)
	{
		FUiRootState::Focused(*Previous) = false;
		FUiRootState::NotifyStateChanged(*Previous);
	}
	if (!State.bShuttingDown && Element != nullptr && m_Focused.Get() == Element && CanFocus(State, *Element))
	{
		FUiRootState::Focused(*Element) = true;
		FUiRootState::NotifyStateChanged(*Element);
	}
	return true;
}
// 切断した要素のフォーカスを外す。
void FUiFocusManager::OnElementDetached(FUiRootState& State, DUiElement& Element) noexcept
{
	(void)State;
	if (m_Focused.GetId() == Element.GetRef().GetId())
	{
		m_Focused = {};
		FUiRootState::Focused(Element) = false;
	}
}
// フォーカスの有効性を確かめる。
void FUiFocusManager::Validate(FUiRootState& State) noexcept
{
	DUiElement* Focused = m_Focused.Get();
	if (Focused == nullptr)
	{
		m_Focused = {};
		return;
	}
	if (!CanFocus(State, *Focused))
	{
		m_Focused = {};
		FUiRootState::Focused(*Focused) = false;
		try
		{
			FUiRootState::NotifyStateChanged(*Focused);
		}
		catch (...)
		{
		}
	}
}
// 押し続けの状態を捨てる。
void FUiFocusManager::ResetRepeat() noexcept
{
	for (Toolbox::size_t Index = 0; Index < m_Held.Size(); ++Index)
	{
		m_Held[Index] = 0;
		m_NextRepeat[Index] = 0;
	}
}
// 操作を一つ届ける。
bool FUiFocusManager::Dispatch_Internal(FUiRootState& State, EUiNavigationCommand Command, bool bRepeat)
{
	FUiNavigationEvent Event;
	Event.Command = Command;
	Event.bRepeat = bRepeat;
	if (DUiElement* Focused = m_Focused.Get())
	{
		// 経路を先に集め、直前に参照を確かめて届ける。
		Toolbox::TVector<TUiRef<DUiElement>> Path;
		for (DUiElement* Current = Focused; Current != nullptr; Current = Current->GetParent())
		{
			Path.PushBack(Current->GetRef());
		}
		for (const auto& Handle : Path)
		{
			DUiElement* Element = Handle.Get();
			if (!FUiRootState::IsLive(Element) || !Element->IsEnabledInTree())
			{
				continue;
			}
			FUiRootState::FDispatchScope Scope(State);
			Element->Navigation_Internal(Event);
			if (Event.bHandled)
			{
				return true;
			}
		}
	}
	return State.bShuttingDown ? true : DefaultAction_Internal(State, Command);
}
// 既定の動作。
bool FUiFocusManager::DefaultAction_Internal(FUiRootState& State, EUiNavigationCommand Command)
{
	if (Command == EUiNavigationCommand::Cancel)
	{
		if (DUiElement* Modal = State.GetTopModal())
		{
			FUiNavigationEvent Cancel;
			Cancel.Command = Command;
			FUiRootState::FDispatchScope Scope(State);
			Modal->Navigation_Internal(Cancel);
			if (Cancel.bHandled || State.bShuttingDown)
			{
				return true;
			}
		}
		if (State.CancelRequested.GetSubscriberCount() > 0 || State.GetTopModal() != nullptr)
		{
			FUiRootState::FDispatchScope Scope(State);
			State.CancelRequested.Emit();
			return true;
		}
		return false;
	}
	if (Command == EUiNavigationCommand::Confirm)
	{
		return m_Focused.Get() != nullptr;
	}
	const auto Candidates = CollectFocusable(State);
	if (Candidates.IsEmpty())
	{
		return false;
	}
	DUiElement* Focused = m_Focused.Get();
	if (Focused == nullptr)
	{
		// フォーカスがなければ最初の要素へ（Previousは最後の要素へ）。
		return SetFocus(State, Command == EUiNavigationCommand::Previous ? Candidates.Back() : Candidates[0]);
	}
	if (Command == EUiNavigationCommand::Next || Command == EUiNavigationCommand::Previous)
	{
		Toolbox::size_t Current = 0;
		for (Toolbox::size_t Index = 0; Index < Candidates.Size(); ++Index)
		{
			if (Candidates[Index] == Focused)
			{
				Current = Index;
			}
		}
		const Toolbox::size_t Count = Candidates.Size();
		const Toolbox::size_t Next =
		    Command == EUiNavigationCommand::Next ? (Current + 1) % Count : (Current + Count - 1) % Count;
		return SetFocus(State, Candidates[Next]);
	}
	// 方向: 中心から指定方向にある要素のうち、進む距離＋横ずれ×2が最小のもの。
	const FUiRect From = Focused->GetRect();
	const Toolbox::f32 FromX = From.X + From.Width * 0.5f;
	const Toolbox::f32 FromY = From.Y + From.Height * 0.5f;
	DUiElement* Best = nullptr;
	Toolbox::f32 BestScore = UiUnbounded;
	for (DUiElement* Candidate : Candidates)
	{
		if (Candidate == Focused)
		{
			continue;
		}
		const FUiRect To = Candidate->GetRect();
		const Toolbox::f32 DeltaX = To.X + To.Width * 0.5f - FromX;
		const Toolbox::f32 DeltaY = To.Y + To.Height * 0.5f - FromY;
		Toolbox::f32 Forward = 0;
		Toolbox::f32 Side = 0;
		switch (Command)
		{
		case EUiNavigationCommand::Up:
			Forward = -DeltaY;
			Side = DeltaX;
			break;
		case EUiNavigationCommand::Down:
			Forward = DeltaY;
			Side = DeltaX;
			break;
		case EUiNavigationCommand::Left:
			Forward = -DeltaX;
			Side = DeltaY;
			break;
		default:
			Forward = DeltaX;
			Side = DeltaY;
			break;
		}
		if (!(Forward > 0.5f))
		{
			continue;
		}
		const Toolbox::f32 Score = Forward + Toolbox::Abs(Side) * 2;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}
	if (Best != nullptr)
	{
		return SetFocus(State, Best);
	}
	// 端では移動しないが、フォーカスのある操作としてUIが受けたことにする。
	return true;
}
// 一フレームの操作を処理する。
void FUiFocusManager::Process(FUiRootState& State, const FUiNavigationFrame& Frame, Toolbox::f64 DeltaSeconds,
                              FUiInputResult& Result)
{
	Validate(State);
	if (!Frame.bActive)
	{
		ResetRepeat();
		return;
	}
	const FUiNavigationSettings& Settings = State.Settings.Navigation;
	bool bAny = false;
	for (Toolbox::size_t Index = 0; Index < Frame.Pressed.Size() && !State.bShuttingDown; ++Index)
	{
		const auto Command = static_cast<EUiNavigationCommand>(Index);
		Toolbox::uint32 FireCount = 0;
		if (Frame.Pressed[Index])
		{
			FireCount = 1;
			m_Held[Index] = 0;
			m_NextRepeat[Index] = Settings.InitialRepeatDelay;
		}
		else if (Frame.Down[Index] && IsRepeatable_Internal(Command))
		{
			m_Held[Index] += Toolbox::Clamp(DeltaSeconds, 0.0, 0.25);
			// 絶対的な予定時刻を進める。長い1フレームでも経過時間分の入力を再現する。
			while (m_Held[Index] + 1e-12 >= m_NextRepeat[Index] && FireCount < 64)
			{
				++FireCount;
				m_NextRepeat[Index] += Settings.RepeatInterval;
			}
		}
		else
		{
			m_Held[Index] = 0;
			m_NextRepeat[Index] = Settings.InitialRepeatDelay;
		}
		for (Toolbox::uint32 Count = 0; Count < FireCount && !State.bShuttingDown; ++Count)
		{
			const bool bHadFocus = m_Focused.Get() != nullptr;
			const bool bHandled = Dispatch_Internal(State, Command, !Frame.Pressed[Index]);
			bAny = bAny || bHandled || bHadFocus;
		}
	}
	Result.bHasFocus = m_Focused.Get() != nullptr;
	Result.bNavigationConsumed =
	    Result.bNavigationConsumed || bAny || Result.bHasFocus || State.GetTopModal() != nullptr;
}
} // namespace Dxf::Detail
