// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiRoot.h"
#include "Dxf/UiDefaultStyles.h"
#include "UiDrawBuilder.h"
#include "UiRootState.h"
namespace Dxf
{
namespace Detail
{
// 内部状態を作る。
FUiRootState::FUiRootState(FUiRoot& Owner, FUiRootSettings InSettings)
    : pOwner(&Owner), Lifetime(Toolbox::MakeShared<FUiRootLifetime>()), Settings(Toolbox::Move(InSettings)),
      PostQueue(Toolbox::MakeShared<FUiPostQueue>()), Styles(Settings.Styles), BuiltInStyles(GetBuiltInUiStyleSheet())
{
	const auto& N = Settings.Navigation;
	if (!Toolbox::IsFinite(N.InitialRepeatDelay) || N.InitialRepeatDelay < 0 || !Toolbox::IsFinite(N.RepeatInterval) ||
	    N.RepeatInterval <= 0 || !Toolbox::IsFinite(N.TooltipDelay) || N.TooltipDelay < 0 ||
	    !Toolbox::IsFinite(N.DragThreshold) || N.DragThreshold < 0 || Settings.Limits.MaxDepth == 0 ||
	    Settings.Limits.MaxElements < 4)
	{
		throw Toolbox::FException("Invalid UI root limits or navigation timing");
	}
	Lifetime->Owner = &Owner;
}
// 状態の変化を知らせる。
void FUiRootState::NotifyStateChanged(DUiElement& Element)
{
	if (Element.m_pRoot != nullptr && Element.m_AttachState == EUiAttachState::Attached)
	{
		Element.m_pRoot->GetState_Internal().ResolveStyle(Element);
	}
	Element.StateChanged_Internal();
}
// 見た目を解決する。
void FUiRootState::ResolveStyle(DUiElement& Element)
{
	const FUiStyleSheet& Sheet = GetStyles();
	const FUiStyleSet* Set = Element.m_StyleId.IsEmpty() ? &Sheet.GetDefault() : Sheet.Find(Element.m_StyleId);
	if (Set == nullptr)
	{
		// 利用者のスタイルにないIDは組込みの定義を使い、それもなければ既定値を使う。
		Set = BuiltInStyles->Find(Element.m_StyleId);
		if (Set == nullptr)
		{
			Set = &Sheet.GetDefault();
		}
	}
	const FUiStyle Previous = Element.m_Style;
	if (Element.m_bStyleDirty)
	{
		Element.m_StyleSet = *Set;
		Element.m_bStyleDirty = false;
	}
	FUiStyle Resolved = Element.m_StyleSet.Resolve(Element.GetStyleState());
	Element.m_StyleOverride.ApplyTo(Resolved);
	Element.m_Style = Toolbox::Move(Resolved);
	if (!(Previous == Element.m_Style))
	{
		if (Previous.FontFamily != Element.m_Style.FontFamily || Previous.FontSize != Element.m_Style.FontSize ||
		    (!Element.m_bPaddingExplicit && !(Previous.Padding == Element.m_Style.Padding)))
		{
			Element.InvalidateMeasure();
		}
		Element.StyleResolved_Internal();
	}
}
} // namespace Detail

// ルートを作る。
FUiRoot::FUiRoot(FUiRootSettings Settings)
    : m_pState(Toolbox::MakeShared<Detail::FUiRootState>(*this, Toolbox::Move(Settings)))
{
	const auto Lease = m_pState;
	auto& State = *Lease;
	for (Toolbox::size_t Layer = 0; Layer < State.Layers.Size(); ++Layer)
	{
		Toolbox::TUniquePtr<DUiElement> Element;
		if (static_cast<EUiLayer>(Layer) == EUiLayer::Tooltip)
		{
			Element = Toolbox::MakeUnique<Detail::DUiTooltipLayer>();
		}
		else
		{
			Element = Toolbox::MakeUnique<Detail::DUiLayerElement>();
		}
		const TUiRef<DUiElement> Handle = Register_Internal(Toolbox::Move(Element));
		DUiElement* Layered = Handle.Get();
		static const char* const Names[] = {"Ui.Layer.Normal", "Ui.Layer.Panel", "Ui.Layer.Popup", "Ui.Layer.Tooltip"};
		Layered->SetName(Names[Layer]);
		Layered->m_AttachState = EUiAttachState::Attached;
		Layered->m_bAttachedOnce = true;
		State.Layers[Layer] = Layered;
	}
}
// 切断してから解放する。
FUiRoot::~FUiRoot()
{
	const auto Lease = m_pState;
	auto& State = *Lease;
	State.bShuttingDown = true;
	State.Lifetime->Owner = nullptr;
	State.PostQueue->Close();
	State.Tree.DetachAll(State);
	// 配送中のStateは呼出し側のLeaseが保つ。全参照はここで即時失効させる。
	State.Elements.ForEach_Internal(
	    [](DUiElement& Element)
	    {
		    Element.m_bDestroyRequested = true;
		    Element.m_pRoot = nullptr;
	    });
	State.pOwner = nullptr;
}
// 表示先への弱い参照。
FUiRootHandle FUiRoot::GetHandle() const noexcept
{
	return FUiRootHandle(Toolbox::TWeakPtr<Detail::FUiRootLifetime>(m_pState->Lifetime));
}
// 要素を登録する。
TUiRef<DUiElement> FUiRoot::Register_Internal(Toolbox::TUniquePtr<DUiElement> Element)
{
	const auto Lease = m_pState;
	auto& State = *Lease;
	if (!Element)
	{
		throw Toolbox::FException("Null UI element");
	}
	if (State.bShuttingDown)
	{
		throw Toolbox::FException("UI root is shutting down");
	}
	if (State.Elements.Size() >= State.Settings.Limits.MaxElements)
	{
		throw Toolbox::FException("UI element limit exceeded");
	}
	DUiElement* Raw = Element.Get();
	Raw->m_pRoot = this;
	Raw->m_Id = State.NextId++;
	const TUiRef<DUiElement> Handle = State.Elements.Insert(Toolbox::Move(Element));
	Raw->m_Self = Handle;
	return Handle;
}
// 重なり領域の直下へ加える。
TResult<void> FUiRoot::AddToLayer(EUiLayer Layer, const TUiRef<DUiElement>& Element)
{
	if (Layer >= EUiLayer::Count)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid UI layer");
	}
	return AddChild(m_pState->GetLayer(Layer)->m_Self, Element);
}
// 子として加える。
TResult<void> FUiRoot::AddChild(const TUiRef<DUiElement>& Parent, const TUiRef<DUiElement>& Child,
                                Toolbox::size_t Index)
{
	DUiElement* ParentElement = Parent.Get();
	DUiElement* ChildElement = Child.Get();
	if (ParentElement == nullptr || ChildElement == nullptr)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid or destroyed UI element");
	}
	const auto Lease = m_pState;
	Detail::FUiRootState::FDispatchScope Scope(*Lease);
	return Lease->Tree.AddChild(*Lease, *ParentElement, *ChildElement, Index);
}
// 親から取り外す。
TResult<void> FUiRoot::Remove(const TUiRef<DUiElement>& Element)
{
	DUiElement* Target = Element.Get();
	if (Target == nullptr)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid or destroyed UI element");
	}
	const auto Lease = m_pState;
	Detail::FUiRootState::FDispatchScope Scope(*Lease);
	return Lease->Tree.Remove(*Lease, *Target);
}
// 破棄を要求する。
bool FUiRoot::Destroy(const TUiRef<DUiElement>& Element) noexcept
{
	DUiElement* Target = Element.Get();
	if (Target == nullptr || !m_pState->Owns(Target))
	{
		return false;
	}
	const auto Lease = m_pState;
	Detail::FUiRootState::FDispatchScope Scope(*Lease);
	Lease->Tree.Destroy(*Lease, *Target);
	return true;
}
// 重なり領域の入れ物。
DUiElement& FUiRoot::GetLayer(EUiLayer Layer) noexcept
{
	const auto Index = static_cast<Toolbox::size_t>(Layer) < m_pState->Layers.Size() ? Layer : EUiLayer::Normal;
	return *m_pState->GetLayer(Index);
}
// 表示面を設定する。
void FUiRoot::SetSurface(const FUiSurface& Surface)
{
	m_pState->Surface = Surface;
}
// 表示面。
const FUiSurface& FUiRoot::GetSurface() const noexcept
{
	return m_pState->Surface;
}
// レイアウト。
TResult<void> FUiRoot::Layout()
{
	const auto Lease = m_pState;
	auto& State = *Lease;
	if (State.bFrozen || State.bShuttingDown)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "UI layout unavailable or reentrant");
	}
	State.Tree.FlushDestroyed(State);
	try
	{
		// 一度配置した実寸法で仮想行数を調整する。静止した木は再測定しない。
		for (Toolbox::int32 Pass = 0; Pass < 2; ++Pass)
		{
			const auto Preparing = State.Updating;
			for (const auto& Handle : Preparing)
			{
				if (DUiElement* Element = Handle.Get(); Detail::FUiRootState::IsLive(Element))
				{
					Detail::FUiRootState::FDispatchScope Scope(State);
					Element->PrepareLayout_Internal();
				}
			}
			if (State.bShuttingDown)
			{
				return TResult<void>::Failure(EErrorCode::InvalidState, "UI root destroyed during layout preparation");
			}
			auto Result = State.Layout.Run(State);
			if (!Result)
			{
				return Result;
			}
		}
	}
	catch (const Toolbox::FException& Error)
	{
		return TResult<void>::Failure(EErrorCode::UserException, Error.What());
	}
	catch (...)
	{
		return TResult<void>::Failure(EErrorCode::UserException, "UI layout preparation failed");
	}
	State.Tree.FlushDestroyed(State);
	return {};
}
// 入力を処理する。
TResult<FUiInputResult> FUiRoot::ProcessInput(const FUiInputFrame& Frame)
{
	const auto Lease = m_pState;
	auto& State = *Lease;
	if (State.bShuttingDown || State.bProcessingInput || State.bFrozen)
	{
		return TResult<FUiInputResult>::Failure(EErrorCode::InvalidState, "UI input unavailable or reentrant");
	}
	if (!Toolbox::IsFinite(Frame.DeltaSeconds) || Frame.DeltaSeconds < 0)
	{
		return TResult<FUiInputResult>::Failure(EErrorCode::InvalidArgument, "Invalid UI input time");
	}
	FUiInputResult Result;
	// 開閉を起こした操作自身もModalの入力として扱う。
	Result.bModal = State.GetTopModal() != nullptr;
	State.bProcessingInput = true;
	TResult<FUiInputResult> Output = TResult<FUiInputResult>::Success(Result);
	try
	{
		Detail::FUiRootState::FDispatchScope Scope(State);
		State.Input.Process(State, Frame.Pointer, Result);
		if (!State.bShuttingDown)
		{
			State.Focus.Process(State, Frame.Navigation, Frame.DeltaSeconds, Result);
		}
		Result.bModal = Result.bModal || State.GetTopModal() != nullptr;
		Output = TResult<FUiInputResult>::Success(Result);
	}
	catch (const Toolbox::FException& Error)
	{
		Output = TResult<FUiInputResult>::Failure(EErrorCode::UserException, Error.What());
	}
	catch (...)
	{
		Output = TResult<FUiInputResult>::Failure(EErrorCode::UserException, "UI input callback failed");
	}
	State.bProcessingInput = false;
	State.Tree.FlushDestroyed(State);
	return Output;
}
// 毎フレームの更新。
TResult<void> FUiRoot::Update(Toolbox::f64 DeltaSeconds)
{
	const auto Lease = m_pState;
	auto& State = *Lease;
	if (State.bUpdating || State.bShuttingDown || State.bFrozen)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "UI update unavailable or reentrant");
	}
	if (!Toolbox::IsFinite(DeltaSeconds) || DeltaSeconds < 0)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid UI delta seconds");
	}
	// 長い停止の後でも時間の進みを抑える。
	const Toolbox::f64 Delta = Toolbox::Min(DeltaSeconds, 0.25);
	State.Elapsed += Delta;
	State.bUpdating = true;
	TResult<void> Result;
	try
	{
		Detail::FUiRootState::FDispatchScope Dispatch(State);
		// 投函された処理（非同期の完了等）を所有スレッドで実行する。取り出した後の実行・破棄はロックの外。
		Toolbox::TVector<Toolbox::TFunction<void()>> Posted;
		State.PostQueue->Drain(Posted);
		for (auto& Callback : Posted)
		{
			if (State.bShuttingDown)
			{
				break;
			}
			Detail::FUiRootState::FDispatchScope Scope(State);
			Callback();
		}
		Posted.Clear();
		// 更新を受ける要素（一覧の写しを参照で確かめてから呼ぶ）。
		auto Updating = State.Updating;
		const Toolbox::size_t OriginalCount = Updating.Size();
		Toolbox::size_t Write = 0;
		FUiUpdateContext Context{Delta, State.Elapsed};
		for (Toolbox::size_t Index = 0; Index < Updating.Size() && !State.bShuttingDown; ++Index)
		{
			DUiElement* Element = Updating[Index].Get();
			if (!Detail::FUiRootState::IsLive(Element) || !Element->m_bWantsUpdate)
			{
				continue;
			}
			// 重複して登録された要素は一度だけ呼ぶ。
			bool bDuplicate = false;
			for (Toolbox::size_t Earlier = 0; Earlier < Write; ++Earlier)
			{
				bDuplicate = bDuplicate || Updating[Earlier].GetId() == Updating[Index].GetId();
			}
			if (bDuplicate)
			{
				continue;
			}
			Updating[Write++] = Updating[Index];
			Detail::FUiRootState::FDispatchScope Scope(State);
			Element->Update_Internal(Context);
		}
		// 生きている登録だけを残す（呼出し中に追加されたものは次回）。
		while (Updating.Size() > Write)
		{
			Updating.PopBack();
		}
		for (Toolbox::size_t Index = OriginalCount; Index < State.Updating.Size(); ++Index)
		{
			Updating.PushBack(State.Updating[Index]);
		}
		State.Updating = Toolbox::Move(Updating);
		if (!State.bShuttingDown)
		{
			State.Tooltip.Update(State, Delta);
		}
	}
	catch (const Toolbox::FException& Error)
	{
		Result = TResult<void>::Failure(EErrorCode::UserException, Error.What());
	}
	catch (...)
	{
		Result = TResult<void>::Failure(EErrorCode::UserException, "UI update callback failed");
	}
	State.bUpdating = false;
	State.Tree.FlushDestroyed(State);
	return Result;
}
// 描画を記録する。
TResult<void> FUiRoot::BuildDrawList(FUiDrawList& List)
{
	const auto Lease = m_pState;
	auto& State = *Lease;
	if (State.bFrozen || State.bShuttingDown)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "UI draw is unavailable or reentrant");
	}
	State.Tree.FlushDestroyed(State);
	const Toolbox::size_t Before = List.GetItems().Size();
	State.bFrozen = true;
	TResult<void> Result;
	try
	{
		Detail::FUiDrawBuilder::Build(State, List);
	}
	catch (const Toolbox::FException& Error)
	{
		Result = TResult<void>::Failure(EErrorCode::UserException, Error.What());
	}
	catch (...)
	{
		Result = TResult<void>::Failure(EErrorCode::UserException, "UI draw callback failed");
	}
	State.bFrozen = false;
	if (!Result || State.bShuttingDown)
	{
		while (List.GetItems().Size() > Before)
		{
			List.EditItems().PopBack();
		}
	}
	State.LastDrawItems = List.GetItems().Size() - Before;
	State.Tree.FlushDestroyed(State);
	return Result;
}
// 最前面の要素。
DUiElement* FUiRoot::HitTest(FVector2 Position)
{
	return Detail::FUiInputDispatcher::HitTest(*m_pState, Position);
}
// フォーカスを移す。
bool FUiRoot::SetFocus(const TUiRef<DUiElement>& Element)
{
	DUiElement* Target = Element.Get();
	if (Target == nullptr && Element.GetId().Generation != 0)
	{
		return false;
	}
	const auto Lease = m_pState;
	Detail::FUiRootState::FDispatchScope Scope(*Lease);
	return Lease->Focus.SetFocus(*Lease, Target);
}
// フォーカスのある要素。
DUiElement* FUiRoot::GetFocused() const noexcept
{
	return m_pState->Focus.GetFocused();
}
// ホバーとキャプチャを外す。
void FUiRoot::ResetPointer() noexcept
{
	const auto Lease = m_pState;
	{
		Detail::FUiRootState::FDispatchScope Scope(*Lease);
		Lease->Input.Reset(*Lease);
	}
	Lease->Tree.FlushDestroyed(*Lease);
}
// キャプチャ中の要素。
DUiElement* FUiRoot::GetCaptured() const noexcept
{
	return m_pState->Input.GetCaptured();
}
// ホバー中の要素。
DUiElement* FUiRoot::GetHovered() const noexcept
{
	return m_pState->Input.GetHovered();
}
// ツールチップの対象。
DUiElement* FUiRoot::GetTooltipTarget() const noexcept
{
	return m_pState->Tooltip.GetTarget();
}
// 最前面のModal。
DUiElement* FUiRoot::GetTopModal() const noexcept
{
	return m_pState->GetTopModal();
}
// Modalを登録する。
void FUiRoot::PushModal_Internal(const TUiRef<DUiElement>& Element)
{
	const auto Lease = m_pState;
	auto& State = *Lease;
	DUiElement* Focused = State.Focus.GetFocused();
	State.Modals.Reserve(State.Modals.Size() + 1);
	State.ModalReturnFocus.Reserve(State.ModalReturnFocus.Size() + 1);
	State.Modals.PushBack(Element);
	State.ModalReturnFocus.PushBack(Focused != nullptr ? Focused->GetRef() : TUiRef<DUiElement>{});
	// Modalの外へのキャプチャ・フォーカスを外す。
	State.Input.ValidateCapture(State);
	if (DUiElement* Captured = State.Input.GetCaptured())
	{
		bool bInside = false;
		for (DUiElement* Current = Captured; Current != nullptr; Current = Current->GetParent())
		{
			bInside = bInside || Current == Element.Get();
		}
		if (!bInside)
		{
			State.Input.Reset(State);
		}
	}
	State.Focus.Validate(State);
}
// Modalの登録を外す。
void FUiRoot::PopModal_Internal(const TUiRef<DUiElement>& Element) noexcept
{
	const auto Lease = m_pState;
	auto& State = *Lease;
	for (Toolbox::size_t Index = State.Modals.Size(); Index > 0; --Index)
	{
		if (State.Modals[Index - 1].GetId() != Element.GetId())
		{
			continue;
		}
		const TUiRef<DUiElement> ReturnFocus = State.ModalReturnFocus[Index - 1];
		const bool bTop = Index == State.Modals.Size();
		State.Modals.Erase(State.Modals.Begin() + (Index - 1));
		State.ModalReturnFocus.Erase(State.ModalReturnFocus.Begin() + (Index - 1));
		if (bTop && !State.bShuttingDown)
		{
			// 開く前のフォーカスへ戻す（戻せなければ外す）。
			try
			{
				if (!State.Focus.SetFocus(State, ReturnFocus.Get()))
				{
					State.Focus.SetFocus(State, nullptr);
				}
			}
			catch (...)
			{
			}
		}
		return;
	}
}
// スタイルを差し替える。
void FUiRoot::SetStyleSheet(Toolbox::TSharedPtr<const FUiStyleSheet> Styles)
{
	const auto Lease = m_pState;
	auto& State = *Lease;
	State.Styles = Toolbox::Move(Styles);
	// 設定全体の反映は確保なし。再描画は必要な境界で一括して行う。
	State.Elements.ForEach_Internal(
	    [](DUiElement& Element)
	    {
		    Element.InvalidateStyle();
	    });
}
// 現在のスタイル。
const FUiStyleSheet& FUiRoot::GetStyleSheet() const noexcept
{
	return m_pState->GetStyles();
}
// 文字の窓口。
IUiTextService* FUiRoot::GetTextService() const noexcept
{
	return m_pState->Settings.Text;
}
// 処理を投函する。
void FUiRoot::Post(Toolbox::TFunction<void()> Callback)
{
	(void)m_pState->PostQueue->Push(Toolbox::Move(Callback));
}
// 投函の窓口の写し。
FUiPostHandle FUiRoot::GetPostHandle() const noexcept
{
	return FUiPostHandle(Toolbox::TWeakPtr<Detail::FUiPostQueue>(m_pState->PostQueue));
}
// 「戻る」の通知。
TUiSignal<>& FUiRoot::OnCancelRequested() noexcept
{
	return m_pState->CancelRequested;
}
// 状態の数。
FUiRootStats FUiRoot::GetStats() const noexcept
{
	const auto Lease = m_pState;
	auto& State = *Lease;
	FUiRootStats Stats;
	Stats.Elements = State.Elements.Size();
	Stats.PendingDestroy = State.Tree.GetPendingDestroyCount();
	Stats.Layout = State.Layout.GetStats();
	Stats.DrawItems = State.LastDrawItems;
	Stats.LayoutErrors = State.Layout.GetErrors().Size();
	for (const auto& Handle : State.Updating)
	{
		const DUiElement* Element = Handle.Get();
		Stats.UpdatingElements += Detail::FUiRootState::IsLive(Element) && Element->m_bWantsUpdate ? 1 : 0;
	}
	// 接続済みの要素と購読を数える。
	for (DUiElement* Layer : State.Layers)
	{
		Toolbox::TVector<const DUiElement*> Stack;
		try
		{
			Stack.PushBack(Layer);
			while (!Stack.IsEmpty())
			{
				const DUiElement* Element = Stack.Back();
				Stack.PopBack();
				if (Element->m_AttachState == EUiAttachState::Attached)
				{
					++Stats.AttachedElements;
					Stats.AttachSubscriptions += Element->m_AttachScope.GetSubscriptionCount();
				}
				for (const DUiElement* Child : Element->m_Children)
				{
					Stack.PushBack(Child);
				}
			}
		}
		catch (...)
		{
		}
	}
	return Stats;
}
// 最後のレイアウトの失敗。
const Toolbox::TVector<Toolbox::FString>& FUiRoot::GetLayoutErrors() const noexcept
{
	return m_pState->Layout.GetErrors();
}
// 経過秒数。
Toolbox::f64 FUiRoot::GetElapsedSeconds() const noexcept
{
	return m_pState->Elapsed;
}
// 設定。
const FUiRootSettings& FUiRoot::GetSettings() const noexcept
{
	return m_pState->Settings;
}
} // namespace Dxf
