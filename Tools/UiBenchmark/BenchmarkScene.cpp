// SPDX-License-Identifier: NOASSERTION
#include "BenchmarkScene.h"
#include "Dxf/UiPanel.h"
namespace Dxf::UiBenchmark
{
namespace
{
// 構築の失敗は測定の失敗にする。
void Check_Internal(TResult<void> Result)
{
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
}
} // namespace

FBenchmarkScene::FBenchmarkScene(FBenchmarkText& Text, EBenchmarkMode Mode, Toolbox::size_t Count)
    : m_Text(Text), m_Mode(Mode), m_Count(Count)
{
	Build_Internal();
}

void FBenchmarkScene::Build_Internal()
{
	m_Labels.Clear();
	m_List = {};
	m_pRoot.Reset();
	FUiRootSettings Settings;
	Settings.Text = &m_Text;
	m_pRoot = Toolbox::MakeUnique<FUiRoot>(Settings);
	if (m_Mode == EBenchmarkMode::StyleReload)
	{
		m_Styles.Attach(*m_pRoot);
	}
	if (m_Mode == EBenchmarkMode::ListScroll)
	{
		m_List = m_pRoot->Create<DUiListView>();
		Toolbox::TVector<FUiListItem> Items;
		Items.Reserve(m_Count);
		for (Toolbox::size_t I = 0; I < m_Count; ++I)
		{
			Items.PushBack({I + 1, "row value", "tooltip"});
		}
		m_List.Get()->SetItems(Toolbox::Move(Items));
		Check_Internal(m_pRoot->AddToLayer(EUiLayer::Normal, m_List.Cast<DUiElement>()));
		return;
	}
	auto Panel = m_pRoot->Create<DUiPanel>();
	Panel.Get()->SetStack(EUiStackMode::Overlay);
	Check_Internal(m_pRoot->AddToLayer(EUiLayer::Normal, Panel.Cast<DUiElement>()));
	m_Labels.Reserve(m_Count);
	for (Toolbox::size_t I = 0; I < m_Count; ++I)
	{
		auto Label = m_pRoot->Create<DUiLabel>("value 0");
		Label.Get()->SetAbsolutePosition(
		    {static_cast<Toolbox::f32>((I % 20) * 55), static_cast<Toolbox::f32>((I / 20) * 22)});
		Label.Get()->SetWidth(FUiLength::Fixed(52));
		Label.Get()->SetHeight(FUiLength::Fixed(20));
		Check_Internal(m_pRoot->AddChild(Panel.Cast<DUiElement>(), Label.Cast<DUiElement>()));
		m_Labels.PushBack(Label);
	}
}

void FBenchmarkScene::Mutate(Toolbox::int32 Frame)
{
	switch (m_Mode)
	{
	case EBenchmarkMode::Value:
		m_Labels[0].Get()->SetText((Frame & 1) ? "value 1" : "value 0");
		break;
	case EBenchmarkMode::Layout:
		m_Labels[0].Get()->SetWidth(FUiLength::Fixed((Frame & 1) ? 48.0f : 52.0f));
		break;
	case EBenchmarkMode::ListScroll:
		m_List.Get()->SetScrollOffset(static_cast<Toolbox::f32>((Frame + 30) * 20));
		break;
	case EBenchmarkMode::StyleReload:
		Check_Internal(m_Styles.Reload(
		    {{"bench.dxfui", (Frame & 1) ? "dxfui-style 1\nstyle Label {\n foreground = #ffffff\n}\n"
		                                 : "dxfui-style 1\nstyle Label {\n foreground = #eeeeee\n}\n"}}));
		break;
	case EBenchmarkMode::RootRecreate:
		Build_Internal();
		break;
	default:
		break;
	}
}

Toolbox::size_t FBenchmarkScene::GetRows() const noexcept
{
	return m_List.Get() != nullptr ? m_List.Get()->GetMaterializedRowCount() : 0;
}
} // namespace Dxf::UiBenchmark
