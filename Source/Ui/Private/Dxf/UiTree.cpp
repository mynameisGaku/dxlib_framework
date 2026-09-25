// SPDX-License-Identifier: NOASSERTION
#include "UiTree.h"
#include "UiRootState.h"
namespace Dxf::Detail
{
namespace
{
// 失敗の結果を作る。
TResult<void> Fail_Internal(EErrorCode Code, const char* Message)
{
	return TResult<void>::Failure(Code, Message);
}
// 子の一覧の写し（フックの中での変更に備える）。
Toolbox::TVector<DUiElement*> SnapshotChildren_Internal(const DUiElement& Element)
{
	Toolbox::TVector<DUiElement*> Children;
	Children.Reserve(Element.GetChildCount());
	for (Toolbox::size_t Index = 0; Index < Element.GetChildCount(); ++Index)
	{
		Children.PushBack(Element.GetChild(Index));
	}
	return Children;
}
} // namespace

// 要素の深さ。
Toolbox::size_t FUiTree::Depth(const DUiElement& Element) noexcept
{
	Toolbox::size_t Result = 0;
	for (const DUiElement* Parent = Element.GetParent(); Parent != nullptr; Parent = Parent->GetParent())
	{
		++Result;
	}
	return Result;
}
// 子孫の最大の深さ。
Toolbox::size_t FUiTree::SubtreeHeight(const DUiElement& Element) noexcept
{
	Toolbox::size_t Height = 0;
	for (Toolbox::size_t Index = 0; Index < Element.GetChildCount(); ++Index)
	{
		Height = Toolbox::Max(Height, SubtreeHeight(*Element.GetChild(Index)) + 1);
	}
	return Height;
}
// 親の子の一覧から外す。
void FUiTree::Unlink_Internal(DUiElement& Element) noexcept
{
	DUiElement* Parent = Element.m_pParent;
	if (Parent == nullptr)
	{
		return;
	}
	for (Toolbox::size_t Index = 0; Index < Parent->m_Children.Size(); ++Index)
	{
		if (Parent->m_Children[Index] == &Element)
		{
			Parent->m_Children.Erase(Parent->m_Children.Begin() + Index);
			break;
		}
	}
	Element.m_pParent = nullptr;
	Parent->InvalidateMeasure();
}
// 子として加える。
TResult<void> FUiTree::AddChild(FUiRootState& State, DUiElement& Parent, DUiElement& Child, Toolbox::size_t Index)
{
	if (State.bFrozen || State.bShuttingDown)
	{
		return Fail_Internal(EErrorCode::InvalidState, "UI tree cannot change during layout, drawing or shutdown");
	}
	if (!State.Owns(&Parent) || !State.Owns(&Child))
	{
		return Fail_Internal(EErrorCode::InvalidArgument, "UI element belongs to another root");
	}
	if (Parent.m_bDestroyRequested || Child.m_bDestroyRequested)
	{
		return Fail_Internal(EErrorCode::InvalidState, "UI element is destroyed");
	}
	if (&Parent == &Child)
	{
		return Fail_Internal(EErrorCode::InvalidArgument, "UI element cannot be its own child");
	}
	if (Child.m_pParent != nullptr)
	{
		return Fail_Internal(EErrorCode::InvalidArgument, "UI element already has a parent");
	}
	for (Toolbox::size_t Layer = 0; Layer < State.Layers.Size(); ++Layer)
	{
		if (State.Layers[Layer] == &Child)
		{
			return Fail_Internal(EErrorCode::InvalidArgument, "UI layer cannot be a child");
		}
	}
	for (const DUiElement* Ancestor = &Parent; Ancestor != nullptr; Ancestor = Ancestor->m_pParent)
	{
		if (Ancestor == &Child)
		{
			return Fail_Internal(EErrorCode::InvalidArgument, "UI parent cycle");
		}
	}
	if (Child.m_AttachState != EUiAttachState::Detached)
	{
		return Fail_Internal(EErrorCode::InvalidState, "UI element is already attached");
	}
	if (Parent.m_AttachState == EUiAttachState::Detaching)
	{
		return Fail_Internal(EErrorCode::InvalidState, "UI parent is detaching");
	}
	if (Depth(Parent) + 1 + SubtreeHeight(Child) > State.Settings.Limits.MaxDepth)
	{
		return Fail_Internal(EErrorCode::InvalidArgument, "UI tree depth limit exceeded");
	}
	// 子の一覧へ入れる（確保に失敗しても木は変わらない）。
	const Toolbox::size_t Position = Toolbox::Min(Index, Parent.m_Children.Size());
	Parent.m_Children.PushBack(&Child);
	for (Toolbox::size_t Move = Parent.m_Children.Size() - 1; Move > Position; --Move)
	{
		Parent.m_Children[Move] = Parent.m_Children[Move - 1];
	}
	Parent.m_Children[Position] = &Child;
	Child.m_pParent = &Parent;
	Parent.InvalidateMeasure();
	if (Parent.m_AttachState == EUiAttachState::Attached)
	{
		auto Attached = AttachSubtree_Internal(State, Child);
		if (!Attached)
		{
			Unlink_Internal(Child);
			return Attached;
		}
	}
	return {};
}
// 親から取り外す。
TResult<void> FUiTree::Remove(FUiRootState& State, DUiElement& Element)
{
	if (State.bFrozen || State.bShuttingDown)
	{
		return Fail_Internal(EErrorCode::InvalidState, "UI tree cannot change during layout, drawing or shutdown");
	}
	if (!State.Owns(&Element) || Element.m_bDestroyRequested)
	{
		return Fail_Internal(EErrorCode::InvalidArgument, "Invalid UI element");
	}
	if (Element.m_pParent == nullptr)
	{
		return Fail_Internal(EErrorCode::InvalidState, "UI element has no parent");
	}
	if (Element.m_AttachState == EUiAttachState::Attaching || Element.m_AttachState == EUiAttachState::Detaching)
	{
		return Fail_Internal(EErrorCode::InvalidState, "UI element is changing its attachment");
	}
	if (Element.m_AttachState == EUiAttachState::Attached)
	{
		DetachSubtree_Internal(State, Element);
	}
	Unlink_Internal(Element);
	return {};
}
// 破棄の印を子孫へ付ける。
void FUiTree::MarkDestroyed_Internal(DUiElement& Element) noexcept
{
	Element.m_bDestroyRequested = true;
	for (DUiElement* Child : Element.m_Children)
	{
		MarkDestroyed_Internal(*Child);
	}
}
// 破棄を要求する。
void FUiTree::Destroy(FUiRootState& State, DUiElement& Element) noexcept
{
	if (!State.Owns(&Element) || Element.m_bDestroyRequested)
	{
		return;
	}
	for (Toolbox::size_t Layer = 0; Layer < State.Layers.Size(); ++Layer)
	{
		if (State.Layers[Layer] == &Element)
		{
			return;
		}
	}
	MarkDestroyed_Internal(Element);
	try
	{
		m_PendingDestroy.PushBack(&Element);
	}
	catch (...)
	{
		// 一覧へ入れられなくても参照は解決しない。窓口の破棄時にまとめて解放する。
	}
	// レイアウト・描画中や、フックの途中の要素は、切断と取り外しを境界まで遅らせる。
	if (State.bFrozen || Element.m_AttachState == EUiAttachState::Attaching ||
	    Element.m_AttachState == EUiAttachState::Detaching)
	{
		return;
	}
	if (Element.m_AttachState == EUiAttachState::Attached)
	{
		DetachSubtree_Internal(State, Element);
	}
	Unlink_Internal(Element);
}
// 境界で解放する。
void FUiTree::FlushDestroyed(FUiRootState& State) noexcept
{
	if (State.DispatchDepth > 0 || State.bFrozen || m_WalkDepth > 0 || m_PendingDestroy.IsEmpty())
	{
		return;
	}
	auto Pending = Toolbox::Move(m_PendingDestroy);
	m_PendingDestroy = {};
	// 取り外しがまだの要素を切断して外す。
	for (DUiElement* Element : Pending)
	{
		if (Element->m_AttachState == EUiAttachState::Attached)
		{
			DetachSubtree_Internal(State, *Element);
		}
		Unlink_Internal(*Element);
	}
	// 子孫から順に格納領域から外して解放する。
	Toolbox::TVector<DUiElement*> Order;
	for (DUiElement* Element : Pending)
	{
		// 前順に集めて、逆順に解放する。
		Toolbox::size_t Begin = Order.Size();
		try
		{
			Order.PushBack(Element);
		}
		catch (...)
		{
			continue;
		}
		for (Toolbox::size_t Index = Begin; Index < Order.Size(); ++Index)
		{
			for (DUiElement* Child : Order[Index]->m_Children)
			{
				try
				{
					Order.PushBack(Child);
				}
				catch (...)
				{
				}
			}
		}
	}
	for (Toolbox::size_t Index = Order.Size(); Index > 0; --Index)
	{
		DUiElement* Element = Order[Index - 1];
		Element->m_Children.Clear();
		Element->m_pParent = nullptr;
		const TUiRef<DUiElement> Handle = Element->m_Self;
		State.Elements.Remove(Handle);
	}
}
// 全要素を切断する。
void FUiTree::DetachAll(FUiRootState& State) noexcept
{
	for (Toolbox::size_t Layer = State.Layers.Size(); Layer > 0; --Layer)
	{
		DUiElement* Element = State.Layers[Layer - 1];
		if (Element != nullptr && Element->m_AttachState == EUiAttachState::Attached)
		{
			DetachSubtree_Internal(State, *Element);
		}
	}
}
// 子の木を接続する。
TResult<void> FUiTree::AttachSubtree_Internal(FUiRootState& State, DUiElement& Element)
{
	// この接続で接続を終えた要素（失敗時に逆順で切断する）。
	Toolbox::TVector<DUiElement*> Attached;
	// 前順に辿る要素。
	Toolbox::TVector<DUiElement*> Stack;
	Stack.PushBack(&Element);
	++m_WalkDepth;
	TResult<void> Result;
	while (!Stack.IsEmpty() && Result)
	{
		DUiElement* Current = Stack.Back();
		Stack.PopBack();
		// 途中で破棄・取り外し・別の接続が起きた要素は飛ばす。
		if (Current->m_bDestroyRequested || Current->m_AttachState != EUiAttachState::Detached)
		{
			continue;
		}
		if (Current != &Element &&
		    (Current->m_pParent == nullptr || Current->m_pParent->m_AttachState != EUiAttachState::Attached))
		{
			continue;
		}
		Current->m_AttachState = EUiAttachState::Attaching;
		try
		{
			if (!Current->m_bAttachedOnce)
			{
				const Toolbox::size_t ChildCount = Current->m_Children.Size();
				try
				{
					Current->OnFirstAttach();
				}
				catch (...)
				{
					// 失敗した初回のフックが作った子を破棄する（次の接続で重複して作らないため）。
					while (Current->m_Children.Size() > ChildCount)
					{
						DUiElement* Created = Current->m_Children.Back();
						Destroy(State, *Created);
						if (!Current->m_Children.IsEmpty() && Current->m_Children.Back() == Created)
						{
							Unlink_Internal(*Created);
						}
					}
					throw;
				}
				Current->m_bAttachedOnce = true;
			}
			Current->OnAttach();
		}
		catch (const Toolbox::FException& Error)
		{
			Current->m_AttachScope.Clear();
			Current->m_AttachState = EUiAttachState::Detached;
			Result = TResult<void>::Failure(EErrorCode::UserException, Error.What());
			break;
		}
		catch (...)
		{
			Current->m_AttachScope.Clear();
			Current->m_AttachState = EUiAttachState::Detached;
			Result = TResult<void>::Failure(EErrorCode::UserException, "UI attach hook failed");
			break;
		}
		Current->m_AttachState = EUiAttachState::Attached;
		Attached.PushBack(Current);
		if (Current->m_bDestroyRequested)
		{
			// フックの中で自身の破棄を要求した: 切断して外す（解放は境界で）。
			continue;
		}
		Current->m_bStyleDirty = true;
		Current->InvalidateMeasure();
		if (Current->m_bWantsUpdate)
		{
			State.Updating.PushBack(Current->m_Self);
		}
		// 子を前順で辿るため、逆順に積む。
		for (Toolbox::size_t Index = Current->m_Children.Size(); Index > 0; --Index)
		{
			Stack.PushBack(Current->m_Children[Index - 1]);
		}
	}
	--m_WalkDepth;
	if (!Result)
	{
		for (Toolbox::size_t Index = Attached.Size(); Index > 0; --Index)
		{
			DUiElement* Current = Attached[Index - 1];
			if (Current->m_AttachState == EUiAttachState::Attached)
			{
				DetachOne_Internal(State, *Current);
			}
		}
		return Result;
	}
	// フックの中で破棄を要求した要素を外す。
	for (DUiElement* Current : Attached)
	{
		if (Current->m_bDestroyRequested && Current->m_AttachState == EUiAttachState::Attached)
		{
			DetachSubtree_Internal(State, *Current);
			Unlink_Internal(*Current);
		}
	}
	return Result;
}
// 子の木を切断する（後順）。
void FUiTree::DetachSubtree_Internal(FUiRootState& State, DUiElement& Element) noexcept
{
	if (Element.m_AttachState != EUiAttachState::Attached)
	{
		return;
	}
	Element.m_AttachState = EUiAttachState::Detaching;
	++m_WalkDepth;
	Toolbox::TVector<DUiElement*> Children;
	try
	{
		Children = SnapshotChildren_Internal(Element);
	}
	catch (...)
	{
		Children = {};
	}
	for (DUiElement* Child : Children)
	{
		if (Child->m_pParent == &Element)
		{
			DetachSubtree_Internal(State, *Child);
		}
	}
	--m_WalkDepth;
	Element.m_AttachState = EUiAttachState::Attached;
	DetachOne_Internal(State, Element);
}
// 一つの要素を切断する。
void FUiTree::DetachOne_Internal(FUiRootState& State, DUiElement& Element) noexcept
{
	Element.m_AttachState = EUiAttachState::Detaching;
	++m_WalkDepth;
	try
	{
		Element.OnDetach();
	}
	catch (...)
	{
	}
	--m_WalkDepth;
	Element.m_AttachScope.Clear();
	State.Input.OnElementDetached(State, Element);
	State.Focus.OnElementDetached(State, Element);
	State.Tooltip.OnElementDetached(State, Element);
	for (Toolbox::size_t Index = State.Modals.Size(); Index > 0; --Index)
	{
		if (State.Modals[Index - 1].GetId() == Element.m_Self.GetId())
		{
			State.pOwner->PopModal_Internal(Element.m_Self);
		}
	}
	Element.m_bHovered = false;
	Element.m_bPressed = false;
	Element.m_bFocused = false;
	Element.m_AttachState = EUiAttachState::Detached;
}
} // namespace Dxf::Detail
