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
    : pOwner(&Owner), Settings(Toolbox::Move(InSettings)), PostQueue(Toolbox::MakeShared<FUiPostQueue>()),
      Styles(Settings.Styles), BuiltInStyles(GetBuiltInUiStyleSheet())
{
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
    : m_pState(Toolbox::MakeUnique<Detail::FUiRootState>(*this, Toolbox::Move(Settings)))
{
	auto& State = *m_pState;
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
	auto& State = *m_pState;
	State.bShuttingDown = true;
	State.PostQueue->Close();
	State.Tree.DetachAll(State);
	// 格納領域の破棄で要素を解放する（要素のデストラクタは購読を解除するだけ）。
}
// 要素を登録する。
TUiRef<DUiElement> FUiRoot::Register_Internal(Toolbox::TUniquePtr<DUiElement> Element)
{
	auto& State = *m_pState;
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
	return m_pState->Tree.AddChild(*m_pState, *ParentElement, *ChildElement, Index);
}
// 親から取り外す。
TResult<void> FUiRoot::Remove(const TUiRef<DUiElement>& Element)
{
	DUiElement* Target = Element.Get();
	if (Target == nullptr)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid or destroyed UI element");
	}
	return m_pState->Tree.Remove(*m_pState, *Target);
}
// 破棄を要求する。
bool FUiRoot::Destroy(const TUiRef<DUiElement>& Element) noexcept
{
	DUiElement* Target = Element.Get();
	if (Target == nullptr || !m_pState->Owns(Target))
	{
		return false;
	}
	m_pState->Tree.Destroy(*m_pState, *Target);
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
	auto& State = *m_pState;
	State.Tree.FlushDestroyed(State);
	return State.Layout.Run(State);
}
// 入力を処理する。
TResult<FUiInputResult> FUiRoot::ProcessInput(const FUiInputFrame& Frame)
{
	auto& State = *m_pState;
	FUiInputResult Result;
	if (State.bShuttingDown)
	{
		return TResult<FUiInputResult>::Success(Result);
	}
	try
	{
		// 入力は前回のレイアウトで判定する（描いた位置と一致させる）。
		State.Input.Process(State, Frame.Pointer, Result);
		State.Focus.Process(State, Frame.Navigation, Frame.DeltaSeconds, Result);
	}
	catch (const Toolbox::FException& Error)
	{
		State.Tree.FlushDestroyed(State);
		return TResult<FUiInputResult>::Failure(EErrorCode::UserException, Error.What());
	}
	Result.bModal = GetTopModal() != nullptr;
	State.Tree.FlushDestroyed(State);
	return TResult<FUiInputResult>::Success(Result);
}
// 毎フレームの更新。
TResult<void> FUiRoot::Update(Toolbox::f64 DeltaSeconds)
{
	auto& State = *m_pState;
	if (!Toolbox::IsFinite(DeltaSeconds) || DeltaSeconds < 0)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid UI delta seconds");
	}
	// 長い停止の後でも時間の進みを抑える。
	const Toolbox::f64 Delta = Toolbox::Min(DeltaSeconds, 0.25);
	State.Elapsed += Delta;
	TResult<void> Result;
	try
	{
		// 投函された処理（非同期の完了等）を所有スレッドで実行する。取り出した後の実行・破棄はロックの外。
		Toolbox::TVector<Toolbox::TFunction<void()>> Posted;
		State.PostQueue->Drain(Posted);
		for (auto& Callback : Posted)
		{
			Detail::FUiRootState::FDispatchScope Scope(State);
			Callback();
		}
		Posted.Clear();
		// 更新を受ける要素（一覧の写しを参照で確かめてから呼ぶ）。
		auto Updating = State.Updating;
		Toolbox::size_t Write = 0;
		FUiUpdateContext Context{Delta, State.Elapsed};
		for (Toolbox::size_t Index = 0; Index < Updating.Size(); ++Index)
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
		for (Toolbox::size_t Index = Updating.Size(); Index < State.Updating.Size(); ++Index)
		{
			Updating.PushBack(State.Updating[Index]);
		}
		State.Updating = Toolbox::Move(Updating);
		State.Tooltip.Update(State, Delta);
	}
	catch (const Toolbox::FException& Error)
	{
		Result = TResult<void>::Failure(EErrorCode::UserException, Error.What());
	}
	State.Tree.FlushDestroyed(State);
	return Result;
}
// 描画を記録する。
TResult<void> FUiRoot::BuildDrawList(FUiDrawList& List)
{
	auto& State = *m_pState;
	State.Tree.FlushDestroyed(State);
	const Toolbox::size_t Before = List.GetItems().Size();
	State.bFrozen = true;
	try
	{
		Detail::FUiDrawBuilder::Build(State, List);
	}
	catch (const Toolbox::FException& Error)
	{
		State.bFrozen = false;
		return TResult<void>::Failure(EErrorCode::UserException, Error.What());
	}
	State.bFrozen = false;
	State.LastDrawItems = List.GetItems().Size() - Before;
	return {};
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
	return m_pState->Focus.SetFocus(*m_pState, Target);
}
// フォーカスのある要素。
DUiElement* FUiRoot::GetFocused() const noexcept
{
	return m_pState->Focus.GetFocused();
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
	for (Toolbox::size_t Index = m_pState->Modals.Size(); Index > 0; --Index)
	{
		DUiElement* Modal = m_pState->Modals[Index - 1].Get();
		if (Detail::FUiRootState::IsLive(Modal) && Modal->IsVisibleInTree())
		{
			return Modal;
		}
	}
	return nullptr;
}
// Modalを登録する。
void FUiRoot::PushModal_Internal(const TUiRef<DUiElement>& Element)
{
	auto& State = *m_pState;
	DUiElement* Focused = State.Focus.GetFocused();
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
	auto& State = *m_pState;
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
	auto& State = *m_pState;
	State.Styles = Toolbox::Move(Styles);
	// 見た目だけ解決し直す（状態・フォーカス・スクロール位置は要素が保つ）。
	for (DUiElement* Layer : State.Layers)
	{
		Toolbox::TVector<DUiElement*> Stack;
		Stack.PushBack(Layer);
		while (!Stack.IsEmpty())
		{
			DUiElement* Element = Stack.Back();
			Stack.PopBack();
			Element->InvalidateStyle();
			for (DUiElement* Child : Element->m_Children)
			{
				Stack.PushBack(Child);
			}
		}
	}
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
	const auto& State = *m_pState;
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
