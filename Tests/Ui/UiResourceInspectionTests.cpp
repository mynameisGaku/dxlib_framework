// SPDX-License-Identifier: NOASSERTION
#include "UiTestSupport.h"
#include "Dxf/UiStyleResource.h"
#include "Dxf/UiInspection.h"
#include "Dxf/UiBindProperty.h"
#include "Dxf/UiLabel.h"
#include "Dxf/UiPanel.h"
#include "Dxf/UiImage.h"
#include "Dxf/UiScrollView.h"
#include "Dxf/UiButton.h"
#include "Support/FakeBackend.h"
#include "Dxf/AssetService.h"
using namespace Dxf;
using namespace UiTest;
TEST("UI style reload is transactional across roots and preserves interaction state")
{
	FUiRoot A;
	FUiRoot B;
	FUiStyleResource Resource;
	Resource.Attach(A);
	Resource.Attach(B);
	auto Button = A.Create<DUiButton>("A");
	Button.Get()->SetWidth(FUiLength::Fixed(100));
	Button.Get()->SetHeight(FUiLength::Fixed(40));
	REQUIRE(A.AddToLayer(EUiLayer::Normal, Button.Cast<DUiElement>()));
	LayoutRoot(A);
	REQUIRE(A.SetFocus(Button.Cast<DUiElement>()));
	REQUIRE(Resource.Reload(
	    {{"color.dxfui", "dxfui-style 1\ntoken accent = #aabbcc\nstyle Button {\n background = @accent\n}\n"}}));
	REQUIRE(Resource.GetRevision() == 1);
	LayoutRoot(A);
	LayoutRoot(B);
	REQUIRE(Button.Get()->GetStyle().Background.R == 0xaa);
	const auto* Old = Resource.Get().Get();
	REQUIRE(!Resource.Reload({{"bad.dxfui", "dxfui-style 1\nstyle Button {\n background = @missing\n}\n"}}));
	REQUIRE(Resource.Get().Get() == Old && Resource.GetRevision() == 1);
	LayoutRoot(A);
	REQUIRE(Button.Get()->GetStyle().Background.R == 0xaa);
	REQUIRE(A.GetFocused() == Button.Get());
	auto Temp = Toolbox::MakeUnique<FUiRoot>();
	Resource.Attach(*Temp);
	Temp.Reset();
	REQUIRE(Resource.Reload({{"valid", "dxfui-style 1\ntoken size = 25\n"}}));
	REQUIRE(Resource.GetRevision() == 2);
}

TEST("UI property binding applies current value and unsubscribes at detach")
{
	class DValueLabel final : public DUiLabel
	{
	public:
		explicit DValueLabel(TUiProperty<Toolbox::int32>& Model) : m_pModel(&Model)
		{
		}

	protected:
		void OnAttach() override
		{
			BindUiProperty(GetRef<DValueLabel>(), *m_pModel,
			               [](DUiElement& Element, const Toolbox::int32& Value)
			               {
				               static_cast<DUiLabel&>(Element).SetText(Toolbox::ToString(Value));
			               });
		}

	private:
		TUiProperty<Toolbox::int32>* m_pModel;
	};
	TUiProperty<Toolbox::int32> Model(7);
	FUiRoot Root;
	auto Label = Root.Create<DValueLabel>(Model);
	REQUIRE(Root.AddToLayer(EUiLayer::Normal, Label.Cast<DUiElement>()));
	REQUIRE(Label.Get()->GetText() == "7");
	for (Toolbox::int32 I = 0; I < 100; ++I)
	{
		REQUIRE(Root.Remove(Label.Cast<DUiElement>()));
		REQUIRE(Model.GetSubscriberCount() == 0);
		Model.Set(8 + I);
		REQUIRE(Root.AddToLayer(EUiLayer::Normal, Label.Cast<DUiElement>()));
		REQUIRE(Label.Get()->GetText() == Toolbox::ToString(8 + I));
		REQUIRE(Model.GetSubscriberCount() == 1);
	}
}

TEST("UI inspection identifies intentional and accidental layout and text failures")
{
	FFixedWidthTextService Text;
	FUiRootSettings Settings;
	Settings.Text = &Text;
	FUiRoot Root(Settings);
	auto Panel = Root.Create<DUiPanel>(EUiStackMode::Overlay);
	Panel.Get()->SetWidth(FUiLength::Fixed(150));
	Panel.Get()->SetHeight(FUiLength::Fixed(100));
	auto A = Root.Create<DUiLabel>("long text exceeds width");
	A.Get()->SetWidth(FUiLength::Fixed(30));
	A.Get()->SetHeight(FUiLength::Fixed(10));
	auto B = Root.Create<DProbe>();
	B.Get()->SetAbsolutePosition({130, 80});
	B.Get()->SetWidth(FUiLength::Fixed(50));
	B.Get()->SetHeight(FUiLength::Fixed(50));
	auto C = Root.Create<DProbe>();
	C.Get()->SetWidth(FUiLength::Fixed(30));
	C.Get()->SetHeight(FUiLength::Fixed(10));
	REQUIRE(Root.AddChild(Panel.Cast<DUiElement>(), A.Cast<DUiElement>()));
	REQUIRE(Root.AddChild(Panel.Cast<DUiElement>(), B.Cast<DUiElement>()));
	REQUIRE(Root.AddChild(Panel.Cast<DUiElement>(), C.Cast<DUiElement>()));
	REQUIRE(Root.AddToLayer(EUiLayer::Normal, Panel.Cast<DUiElement>()));
	LayoutRoot(Root);
	const auto Issues = InspectUiLayout(Root);
	bool TextFound = false;
	bool LayoutFound = false;
	bool OverlapFound = false;
	for (const auto& Issue : Issues.Issues)
	{
		TextFound = TextFound || Issue.Kind == EUiInspectionKind::TextFit;
		LayoutFound = LayoutFound || Issue.Kind == EUiInspectionKind::LayoutFit;
		OverlapFound = OverlapFound || Issue.Kind == EUiInspectionKind::Overlap;
		REQUIRE(!Issue.Path.IsEmpty());
	}
	REQUIRE(TextFound && LayoutFound && OverlapFound);
	A.Get()->DeclareCheckAllowance(EUiCheckAllowance::TextOverflow, "intentional overflow fixture");
	B.Get()->DeclareCheckAllowance(EUiCheckAllowance::LayoutOverflow, "decoration");
	C.Get()->DeclareCheckAllowance(EUiCheckAllowance::Overlap, "overlay");
	const auto Declared = InspectUiLayout(Root);
	REQUIRE(Declared.DeclaredExemptions >= 3);
	REQUIRE(Declared.Issues.IsEmpty());
	bool Failed = false;
	try
	{
		(void)InspectUiLayout(Root, nullptr, 0);
	}
	catch (const Toolbox::FException&)
	{
		Failed = true;
	}
	REQUIRE(Failed);
}

TEST("UI image peak resolution and recorded clip diagnostics do not call rendering")
{
	Testing::FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	const auto Texture = Assets.LoadTexture("test");
	REQUIRE(Texture);
	FUiRoot Root;
	auto Image = Root.Create<DUiImage>(Texture.Value());
	Image.Get()->SetWidth(FUiLength::Fixed(128));
	Image.Get()->SetHeight(FUiLength::Fixed(128));
	Image.Get()->SetInspectionPeakScale(1.2f);
	REQUIRE(Root.AddToLayer(EUiLayer::Normal, Image.Cast<DUiElement>()));
	LayoutRoot(Root);
	FUiDrawList Recorded;
	FUiDrawItem Bad;
	Bad.Clip = {-10, -10, 1000, 1000};
	Recorded.EditItems().PushBack(Bad);
	const auto Issues = InspectUiLayout(Root, &Recorded);
	bool Resolution = false;
	bool Clip = false;
	for (const auto& Issue : Issues.Issues)
	{
		Resolution = Resolution || Issue.Kind == EUiInspectionKind::ImageResolution;
		Clip = Clip || Issue.Kind == EUiInspectionKind::ClipFit;
	}
	REQUIRE(Resolution && Clip);
	REQUIRE(Backend.GetTrace().Presentations == 0);
}

TEST("UI frozen draw destruction of child and parent frees each exactly once")
{
	class DDestroyDraw final : public DUiElement
	{
	public:
		mutable TUiRef<DUiElement> Child;
		Toolbox::int32* Deaths = nullptr;
		~DDestroyDraw() override
		{
			++*Deaths;
		}

	protected:
		void OnDraw(FUiDrawContext&) const override
		{
			auto* Root = GetRoot();
			Root->Destroy(Child);
			Root->Destroy(GetRef());
		}
	};
	Toolbox::int32 Deaths = 0;
	FUiRoot Root;
	auto Parent = Root.Create<DDestroyDraw>();
	Parent.Get()->Deaths = &Deaths;
	auto Child = Root.Create<DProbe>();
	Parent.Get()->Child = Child.Cast<DUiElement>();
	REQUIRE(Root.AddChild(Parent.Cast<DUiElement>(), Child.Cast<DUiElement>()));
	REQUIRE(Root.AddToLayer(EUiLayer::Normal, Parent.Cast<DUiElement>()));
	LayoutRoot(Root);
	FUiDrawList List;
	REQUIRE(Root.BuildDrawList(List));
	REQUIRE(Root.Layout());
	REQUIRE(!Parent && !Child && Deaths == 1);
}

TEST("UI navigation repeat counts do not depend on tested frame interval")
{
	class DNavigationCounter final : public DUiElement
	{
	public:
		Toolbox::int32 Count = 0;
		DNavigationCounter()
		{
			SetFocusable(true);
		}

	protected:
		void OnNavigationEvent(FUiNavigationEvent& Event) override
		{
			if (Event.Command == EUiNavigationCommand::Right)
			{
				++Count;
				Event.bHandled = true;
			}
		}
	};
	Toolbox::int32 Counts[2] = {0, 0};
	for (Toolbox::int32 Variant = 0; Variant < 2; ++Variant)
	{
		FUiRoot Root;
		auto Item = Root.Create<DNavigationCounter>();
		REQUIRE(Root.AddToLayer(EUiLayer::Normal, Item.Cast<DUiElement>()));
		LayoutRoot(Root);
		REQUIRE(Root.SetFocus(Item.Cast<DUiElement>()));
		auto Input = NavigationFrame(EUiNavigationCommand::Right, true, true, 0);
		REQUIRE(Root.ProcessInput(Input));
		Input.Navigation.Pressed[static_cast<Toolbox::size_t>(EUiNavigationCommand::Right)] = false;
		const Toolbox::int32 Count = Variant == 0 ? 10 : 100;
		Input.DeltaSeconds = 1.0 / Count;
		for (Toolbox::int32 I = 0; I < Count; ++I)
		{
			REQUIRE(Root.ProcessInput(Input));
		}
		Counts[Variant] = Item.Get()->Count;
	}
	REQUIRE(Counts[0] == Counts[1] && Counts[0] > 5);
}

TEST("UI same-size surface translation preserves cached logical layout")
{
	FUiRoot Root;
	auto Panel = Root.Create<DUiPanel>();
	REQUIRE(Root.AddToLayer(EUiLayer::Normal, Panel.Cast<DUiElement>()));
	FUiScaleSettings Scale;
	Scale.Mode = EUiScaleMode::FixedPixel;
	Root.SetSurface(FUiSurface({0, 0, 200, 100}, Scale));
	REQUIRE(Root.Layout());
	const auto Before = Root.GetStats().Layout.Measures;
	for (Toolbox::int32 Index = 0; Index < 20; ++Index)
	{
		Root.SetSurface(FUiSurface({Index * 200, 0, (Index + 1) * 200, 100}, Scale));
		REQUIRE(Root.Layout());
		REQUIRE(Root.GetStats().Layout.Measures == Before);
	}
	Root.SetSurface(FUiSurface({0, 0, 201, 100}, Scale));
	REQUIRE(Root.Layout());
	REQUIRE(Root.GetStats().Layout.Measures > Before);
}
