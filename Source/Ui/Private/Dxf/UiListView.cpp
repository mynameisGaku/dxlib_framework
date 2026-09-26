// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiListView.h"
#include "Dxf/UiRoot.h"
#include "Toolbox/Algorithm.h"
namespace Dxf
{
DUiListView::DUiListView()
{
	SetWantsUpdate(true);
}

void DUiListView::SetItems(Toolbox::TVector<FUiListItem> Items)
{
	Toolbox::TVector<Toolbox::uint64> Keys;
	Keys.Reserve(Items.Size());
	for (const auto& Item : Items)
	{
		if (Item.Key == 0)
		{
			throw Toolbox::FException("UI list key must be nonzero");
		}
		Keys.PushBack(Item.Key);
	}
	Toolbox::Sort(Keys.Begin(), Keys.End());
	for (Toolbox::size_t Index = 1; Index < Keys.Size(); ++Index)
	{
		if (Keys[Index] == Keys[Index - 1])
		{
			throw Toolbox::FException("Duplicate UI list key");
		}
	}
	bool bKeepSelection = false;
	for (const auto& Item : Items)
	{
		bKeepSelection = bKeepSelection || (m_Selected && Item.Key == *m_Selected);
	}
	m_Items = Toolbox::Move(Items);
	if (!bKeepSelection)
	{
		m_Selected.Reset();
	}
	SetContentHeight_Internal(static_cast<Toolbox::f64>(m_Items.Size()) * m_RowHeight);
	InvalidateMeasure();
}

void DUiListView::SetRowHeight(Toolbox::f32 Height)
{
	if (!Toolbox::IsFinite(Height) || !(Height > 0))
	{
		throw Toolbox::FException("Invalid UI list row height");
	}
	m_RowHeight = Height;
	SetContentHeight_Internal(static_cast<Toolbox::f64>(m_Items.Size()) * Height);
	InvalidateMeasure();
}

void DUiListView::SetSelectedKey(Toolbox::TOptional<Toolbox::uint64> Key)
{
	if (Key)
	{
		bool bFound = false;
		for (const auto& Item : m_Items)
		{
			bFound = bFound || Item.Key == *Key;
		}
		if (!bFound)
		{
			throw Toolbox::FException("Unknown UI list selection key");
		}
	}
	m_Selected = Key;
	RefreshStyles_Internal();
}

void DUiListView::RefreshStyles_Internal()
{
	for (const auto& Row : m_Rows)
	{
		if (auto* Element = Row.Element.Get())
		{
			Element->SetStyleId(m_Selected && Row.Key == *m_Selected ? "ListRow.Selected" : "ListRow");
		}
	}
}

void DUiListView::OnPrepareLayout()
{
	FUiRoot* Root = GetRoot();
	if (Root == nullptr || !IsAttached())
	{
		return;
	}
	// 最初は設定値、その後は実配置を使用。Rootは測定の前後で準備を行う。
	Toolbox::f64 ViewHeight = GetContentRect().Height;
	if (!(ViewHeight > 0))
	{
		ViewHeight = GetLayout().Height.Mode == EUiSizeMode::Fixed ? GetLayout().Height.Value
		                                                           : Root->GetSurface().GetLogicalSize().Height;
	}
	ViewHeight = Toolbox::Max(0.0, ViewHeight);
	const Toolbox::f64 Required = Toolbox::Ceil(ViewHeight / m_RowHeight) + 2;
	if (Required > 4096)
	{
		throw Toolbox::FException("UI list visible row limit exceeded");
	}
	const Toolbox::size_t Count = Toolbox::Min(m_Items.Size(), static_cast<Toolbox::size_t>(Required));
	const Toolbox::f64 Maximum =
	    Toolbox::Max(0.0, static_cast<Toolbox::f64>(m_Items.Size()) * m_RowHeight - ViewHeight);
	const Toolbox::f64 Offset = Toolbox::Min(GetScrollOffset(), Maximum);
	const Toolbox::size_t FirstVisible = static_cast<Toolbox::size_t>(Toolbox::Floor(Offset / m_RowHeight));
	m_First = FirstVisible > 0 ? FirstVisible - 1 : 0;
	while (m_Rows.Size() > Count)
	{
		const auto Row = m_Rows.Back().Element;
		m_Rows.Back().Click.Reset();
		m_Rows.PopBack();
		Root->Destroy(Row.Cast<DUiElement>());
	}
	while (m_Rows.Size() < Count)
	{
		FRow Row;
		Row.Element = CreateChild<DUiButton>();
		const auto NewElement = Row.Element;
		const auto Owner = Root->GetHandle();
		try
		{
			Row.Element.Get()->SetHitTest(EUiHitTest::Self);
			Row.Element.Get()->SetAlign(EUiAlign::Stretch, EUiAlign::Start);
			Row.Element.Get()->SetPadding(FUiThickness::All(2));
			const auto Self = GetRef<DUiListView>();
			const auto Id = Row.Element.Get()->GetId();
			Row.Click = Row.Element.Get()->OnClicked().Subscribe(
			    [Self, Id]()
			    {
				    if (DUiListView* List = Self.Get())
				    {
					    List->ActivateRow_Internal(Id);
				    }
			    });
			m_Rows.PushBack(Toolbox::Move(Row));
		}
		catch (...)
		{
			// 購読や行一覧の確保に失敗したときは、作成した行と子ラベルを回収する。
			if (auto* LiveRoot = Owner.Get())
			{
				LiveRoot->Destroy(NewElement.Cast<DUiElement>());
			}
			throw;
		}
	}
	for (Toolbox::size_t Index = 0; Index < m_Rows.Size(); ++Index)
	{
		FRow& Row = m_Rows[Index];
		DUiButton* Button = Row.Element.Get();
		const Toolbox::size_t ItemIndex = m_First + Index;
		if (ItemIndex >= m_Items.Size())
		{
			Button->SetVisibility(EUiVisibility::Collapsed);
			continue;
		}
		const FUiListItem& Item = m_Items[ItemIndex];
		if (Row.Key != Item.Key)
		{
			// 再利用で、旧項目の押下・フォーカス・Tooltipを残さない。
			Button->SetVisibility(EUiVisibility::Collapsed);
			Row.Key = Item.Key;
		}
		Button->SetVisibility(EUiVisibility::Visible);
		Button->SetHeight(FUiLength::Fixed(m_RowHeight));
		Button->SetText(Item.Text);
		Button->SetTooltip(Item.Tooltip);
	}
	RefreshStyles_Internal();
}

FUiSize DUiListView::MeasureScrollable(FUiLayoutContext& Context, FUiSize Available)
{
	for (auto& Row : m_Rows)
	{
		if (DUiButton* Button = Row.Element.Get())
		{
			(void)Context.MeasureChild(*Button, {Available.Width, m_RowHeight});
		}
	}
	const Toolbox::f64 Height = static_cast<Toolbox::f64>(m_Items.Size()) * m_RowHeight;
	if (Height > static_cast<Toolbox::f64>(UiUnbounded))
	{
		throw Toolbox::FException("UI list extent cannot be represented");
	}
	return {Available.Width, static_cast<Toolbox::f32>(Height)};
}

void DUiListView::ArrangeScrollable(FUiLayoutContext& Context, const FUiRect& View)
{
	for (Toolbox::size_t Index = 0; Index < m_Rows.Size(); ++Index)
	{
		if (DUiButton* Button = m_Rows[Index].Element.Get())
		{
			const Toolbox::f64 Y =
			    View.Y + static_cast<Toolbox::f64>(m_First + Index) * m_RowHeight - GetScrollOffset();
			Context.ArrangeChild(*Button, {View.X, static_cast<Toolbox::f32>(Y), View.Width, m_RowHeight});
		}
	}
}

void DUiListView::ActivateRow_Internal(Toolbox::uint64 Id)
{
	for (const auto& Row : m_Rows)
	{
		if (Row.Element.Get() != nullptr && Row.Element.Get()->GetId() == Id)
		{
			const Toolbox::uint64 Key = Row.Key;
			SetSelectedKey(Toolbox::TOptional<Toolbox::uint64>(Key));
			m_SelectionChanged.Emit(Key);
			return;
		}
	}
}

void DUiListView::OnNavigationEvent(FUiNavigationEvent& Event)
{
	if (Event.Command != EUiNavigationCommand::Down && Event.Command != EUiNavigationCommand::Up)
	{
		return;
	}
	Event.bHandled = true;
	if (m_Items.IsEmpty())
	{
		return;
	}
	Toolbox::size_t Index = 0;
	for (Toolbox::size_t Candidate = 0; Candidate < m_Items.Size(); ++Candidate)
	{
		if (m_Selected && m_Items[Candidate].Key == *m_Selected)
		{
			Index = Event.Command == EUiNavigationCommand::Down ? Toolbox::Min(Candidate + 1, m_Items.Size() - 1)
			                                                    : (Candidate > 0 ? Candidate - 1 : 0);
			break;
		}
	}
	const Toolbox::uint64 Key = m_Items[Index].Key;
	SetSelectedKey(Toolbox::TOptional<Toolbox::uint64>(Key));
	const Toolbox::f64 Top = static_cast<Toolbox::f64>(Index) * m_RowHeight;
	const Toolbox::f64 Height = GetContentRect().Height;
	if (Top < GetScrollOffset())
	{
		SetScrollOffset(Top);
	}
	else if (Top + m_RowHeight > GetScrollOffset() + Height)
	{
		SetScrollOffset(Top + m_RowHeight - Height);
	}
	m_SelectionChanged.Emit(Key);
}
} // namespace Dxf
// namespace Dxf
