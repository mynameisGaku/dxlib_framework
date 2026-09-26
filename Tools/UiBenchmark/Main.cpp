// SPDX-License-Identifier: NOASSERTION
// CPUのUI更新・配置・入力・命令生成。実字体・実GPU・Physicsの費用は含めない。
#include "BenchmarkText.h"
#include "Dxf/UiRoot.h"
#include "Dxf/UiLabel.h"
#include "Dxf/UiPanel.h"
#include "Dxf/UiListView.h"
#include "Toolbox/Platform.h"
#include "Toolbox/Algorithm.h"
#include "AllocationFault.h"
#include <stdio.h>
namespace
{
using namespace Dxf;
constexpr Toolbox::int32 Frames = 120;
constexpr Toolbox::int32 Repeats = 5;
struct FSample
{
	Toolbox::f64 Layout = 0;
	Toolbox::f64 Input = 0;
	Toolbox::f64 Draw = 0;
	Toolbox::f64 Mutation = 0;
	Toolbox::uint64 Allocations = 0;
	Toolbox::uint64 Measures = 0;
	Toolbox::size_t Elements = 0;
	Toolbox::size_t Rows = 0;
};
void Check(TResult<void> Result)
{
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
}

FSample Run(Toolbox::size_t Count, Toolbox::int32 Mode, Toolbox::int32 Views, bool bTexture)
{
	UiBenchmark::FBenchmarkText Text;
	FUiRootSettings Settings;
	Settings.Text = &Text;
	FUiRoot Root(Settings);
	Toolbox::TVector<TUiRef<DUiLabel>> Labels;
	TUiRef<DUiListView> List;
	if (Mode == 3)
	{
		List = Root.Create<DUiListView>();
		Toolbox::TVector<FUiListItem> Items;
		for (Toolbox::size_t I = 0; I < Count; ++I)
		{
			Items.PushBack({I + 1, "row value", "tooltip"});
		}
		List.Get()->SetItems(Toolbox::Move(Items));
		Check(Root.AddToLayer(EUiLayer::Normal, List.Cast<DUiElement>()));
	}
	else
	{
		auto Panel = Root.Create<DUiPanel>();
		Panel.Get()->SetStack(EUiStackMode::Overlay);
		Check(Root.AddToLayer(EUiLayer::Normal, Panel.Cast<DUiElement>()));
		for (Toolbox::size_t I = 0; I < Count; ++I)
		{
			auto Label = Root.Create<DUiLabel>("value 0");
			Label.Get()->SetAbsolutePosition(
			    {static_cast<Toolbox::f32>((I % 20) * 55), static_cast<Toolbox::f32>((I / 20) * 22)});
			Label.Get()->SetWidth(FUiLength::Fixed(52));
			Label.Get()->SetHeight(FUiLength::Fixed(20));
			Check(Root.AddChild(Panel.Cast<DUiElement>(), Label.Cast<DUiElement>()));
			Labels.PushBack(Label);
		}
	}
	const Toolbox::int32 Width = bTexture ? 512 : 1280;
	const Toolbox::int32 Height = bTexture ? 256 : 720;
	FUiDrawList Draw;
	FSample Result;
	for (Toolbox::int32 Frame = -30; Frame < Frames; ++Frame)
	{
		const auto BeforeMeasures = Root.GetStats().Layout.Measures;
		const auto BeforeAlloc = Toolbox::Testing::GetTotalTestAllocations();
		Toolbox::uint64 T = Toolbox::MonotonicNanoseconds();
		if (Mode == 1 && !Labels.IsEmpty())
		{
			Labels[0].Get()->SetText((Frame & 1) ? "value 1" : "value 0");
		}
		if (Mode == 2 && !Labels.IsEmpty())
		{
			Labels[0].Get()->SetWidth(FUiLength::Fixed((Frame & 1) ? 48.0f : 52.0f));
		}
		if (Mode == 3)
		{
			List.Get()->SetScrollOffset(static_cast<Toolbox::f32>((Frame + 30) * 20));
		}
		const auto AfterMutation = Toolbox::MonotonicNanoseconds();
		Toolbox::uint64 Layout = 0;
		Toolbox::uint64 Drawing = 0;
		for (Toolbox::int32 View = 0; View < Views; ++View)
		{
			Root.SetSurface(FUiSurface({View * Width / Views, 0, (View + 1) * Width / Views, Height}, {}));
			const auto Begin = Toolbox::MonotonicNanoseconds();
			Check(Root.Layout());
			const auto Middle = Toolbox::MonotonicNanoseconds();
			Draw.Clear();
			Check(Root.BuildDrawList(Draw));
			Layout += Middle - Begin;
			Drawing += Toolbox::MonotonicNanoseconds() - Middle;
		}
		const auto InputStart = Toolbox::MonotonicNanoseconds();
		FUiInputFrame Input;
		Input.DeltaSeconds = 1.0 / 60.0;
		Input.Pointer.bPresent = true;
		Input.Pointer.Position = {60, 60};
		if (!Root.ProcessInput(Input))
		{
			throw Toolbox::FException("input failed");
		}
		Check(Root.Update(1.0 / 60.0));
		const auto End = Toolbox::MonotonicNanoseconds();
		if (Frame >= 0)
		{
			Result.Mutation += static_cast<Toolbox::f64>(AfterMutation - T);
			Result.Layout += static_cast<Toolbox::f64>(Layout);
			Result.Draw += static_cast<Toolbox::f64>(Drawing);
			Result.Input += static_cast<Toolbox::f64>(End - InputStart);
			Result.Allocations += Toolbox::Testing::GetTotalTestAllocations() - BeforeAlloc;
			Result.Measures += Root.GetStats().Layout.Measures - BeforeMeasures;
		}
	}
	Result.Layout /= Frames * 1000;
	Result.Draw /= Frames * 1000;
	Result.Input /= Frames * 1000;
	Result.Mutation /= Frames * 1000;
	Result.Elements = Root.GetStats().Elements;
	if (List)
	{
		Result.Rows = List.Get()->GetMaterializedRowCount();
	}
	return Result;
}
} // namespace

int main()
{
	try
	{
		printf("# CPU only; fixed-width text double; warmup=30 frames=120 repeats=5; units=us/frame; allocation calls "
		       "include draw strings and traversal\n");
		printf("surface,mode,count,views,total_median,total_min,total_max,layout,input,draw,mutation,alloc_per_frame,"
		       "measures_per_frame,elements,rows\n");
		const char* Modes[] = {"static", "value", "layout", "list-scroll"};
		for (Toolbox::int32 Texture = 0; Texture < 2; ++Texture)
			for (Toolbox::int32 Views = 1; Views <= 2; ++Views)
				for (Toolbox::int32 Mode = 0; Mode < 4; ++Mode)
					for (Toolbox::int32 Large = 0; Large < 2; ++Large)
					{
						const Toolbox::size_t Count = Mode == 3 ? (Large ? 10000 : 1000) : (Large ? 1000 : 100);
						FSample Samples[Repeats];
						for (Toolbox::int32 R = 0; R < Repeats; ++R)
						{
							Samples[R] = Run(Count, Mode, Views, Texture != 0);
						}
						Toolbox::Sort(Samples, Samples + Repeats,
						              [](const FSample& A, const FSample& B)
						              {
							              return A.Layout + A.Draw + A.Input + A.Mutation <
							                     B.Layout + B.Draw + B.Input + B.Mutation;
						              });
						const auto& S = Samples[Repeats / 2];
						auto Total = [](const FSample& X)
						{
							return X.Layout + X.Draw + X.Input + X.Mutation;
						};
						printf("%s,%s,%llu,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f,%.2f,%llu,%llu\n",
						       Texture ? "world-texture-layout" : "screen-layout", Modes[Mode],
						       static_cast<unsigned long long>(Count), Views, Total(S), Total(Samples[0]),
						       Total(Samples[Repeats - 1]), S.Layout, S.Input, S.Draw, S.Mutation,
						       static_cast<Toolbox::f64>(S.Allocations) / Frames,
						       static_cast<Toolbox::f64>(S.Measures) / Frames,
						       static_cast<unsigned long long>(S.Elements), static_cast<unsigned long long>(S.Rows));
						fflush(stdout);
					}
		return 0;
	}
	catch (const Toolbox::FException& Error)
	{
		Toolbox::Err << Error.What() << "\n";
		return 1;
	}
}
