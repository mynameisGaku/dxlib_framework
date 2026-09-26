// SPDX-License-Identifier: NOASSERTION
#include "UiItemBrowser.h"
#include "UiSampleWidgets.h"
#include "Dxf/UiBindProperty.h"
namespace Dxf::UiSample
{
DUiItemBrowser::DUiItemBrowser(Toolbox::TSharedPtr<FUiSampleState> State)
    : DUiPanel(EUiStackMode::Vertical, 8), m_pState(Toolbox::Move(State))
{
	SetWidth(FUiLength::Fixed(270));
	SetHeight(FUiLength::Fixed(430));
	SetPadding(FUiThickness::All(12));
	SetAbsolutePosition({990, 120});
}

Toolbox::size_t DUiItemBrowser::GetVisibleRowCount() const noexcept
{
	return m_List.Get() != nullptr ? m_List.Get()->GetMaterializedRowCount() : 0;
}

void DUiItemBrowser::OnFirstAttach()
{
	auto Title = CreateChild<DUiLabel>("10,000項目 / 仮想化");
	SetupSampleLabel(*Title.Get(), "ListTitle");
	m_List = CreateChild<DUiListView>();
	m_List.Get()->SetName("Items");
	m_List.Get()->SetWidth(FUiLength::Fill());
	m_List.Get()->SetHeight(FUiLength::Fill());
	Toolbox::TVector<FUiListItem> Items;
	Items.Reserve(10000);
	for (Toolbox::uint64 I = 1; I <= 10000; ++I)
	{
		Items.PushBack({I, "Item " + Toolbox::ToString(I), "説明 " + Toolbox::ToString(I)});
	}
	m_List.Get()->SetItems(Toolbox::Move(Items));
	m_Detail = CreateChild<DUiLabel>();
	SetupSampleLabel(*m_Detail.Get(), "ItemDetail", 60);
	m_Detail.Get()->SetTextLayout(EUiTextWrap::Wrap, EUiTextOverflow::Ellipsis, 2);
}

void DUiItemBrowser::OnAttach()
{
	const auto State = m_pState;
	BindUiProperty(m_Detail, State->Detail,
	               [](DUiLabel& View, const Toolbox::FString& Text)
	               {
		               View.SetText(Text);
	               });
	GetAttachScope().Add(m_List.Get()->OnSelectionChanged().Subscribe(
	    [State](Toolbox::uint64 Key)
	    {
		    State->Detail.Set("選択キー: " + Toolbox::ToString(Key));
	    }));
}
} // namespace Dxf::UiSample
