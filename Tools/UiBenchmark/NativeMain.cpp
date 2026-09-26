// SPDX-License-Identifier: NOASSERTION
// 実DxLib・実字体によるUIのCPU時間と割当の測定。固定幅の文字の代替による系列（dxf_ui_benchmark）とは別の系列で、
// 両者の値を割って改善率にしない。時間はCPUでの呼出しの時間で、GPUの処理時間ではない（native列は提示の待ちを含む）。
#include "BenchmarkScene.h"
#include "TimedTextService.h"
#include "Dxf/AssetService.h"
#include "Dxf/NativeBackends.h"
#include "Dxf/RenderSystem.h"
#include "Dxf/UiAssetTextService.h"
#include "Dxf/UiRenderer.h"
#include "Dxf/UiSceneHost.h"
#include "Toolbox/Algorithm.h"
#include "AllocationFault.h"
#include <stdio.h>
namespace
{
using namespace Dxf;
using namespace Dxf::UiBenchmark;
constexpr Toolbox::int32 Warmup = 30;
constexpr Toolbox::int32 Frames = 60;
constexpr Toolbox::int32 Repeats = 3;
// 区間：変更・配置（文字の計測を含む）・命令生成・キュー受付・Native（実行と提示）。
constexpr Toolbox::size_t Segments = 5;

struct FSample
{
	Toolbox::f64 Nanoseconds[Segments] = {};
	Toolbox::uint64 Allocations[Segments] = {};
	Toolbox::uint64 TextMeasures = 0;
	Toolbox::f64 TextNanoseconds = 0;
	Toolbox::uint64 Commands = 0;
	Toolbox::f64 Total() const noexcept
	{
		Toolbox::f64 Sum = 0;
		for (const Toolbox::f64 Value : Nanoseconds)
		{
			Sum += Value;
		}
		return Sum;
	}
};

void Check(TResult<void> Result)
{
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
}

struct FClock
{
	Toolbox::uint64 Time = Toolbox::MonotonicNanoseconds();
	Toolbox::uint64 Allocations = Toolbox::Testing::GetTotalTestAllocations();
	void Lap(FSample& Sample, Toolbox::size_t Segment, bool bRecord)
	{
		const Toolbox::uint64 NowTime = Toolbox::MonotonicNanoseconds();
		const Toolbox::uint64 NowAllocations = Toolbox::Testing::GetTotalTestAllocations();
		if (bRecord)
		{
			Sample.Nanoseconds[Segment] += static_cast<Toolbox::f64>(NowTime - Time);
			Sample.Allocations[Segment] += NowAllocations - Allocations;
		}
		Time = NowTime;
		Allocations = NowAllocations;
	}
};

// 文字の種類。
struct FTextKind
{
	const char* Name;
	FBenchmarkLabel Label;
};

// 画面のUIの系列（ラベルの格子・一覧）。bScale は毎フレーム倍率を1.0／1.25で切り替える。
FSample RunScreen(FTimedTextService& Text, FRenderSystem& Renderer, EBenchmarkMode Mode, Toolbox::size_t Count,
                  Toolbox::int32 Views, const FBenchmarkLabel& Label, bool bScale)
{
	FBenchmarkScene Scene(Text, Mode, Count, Label);
	FUiDrawList Draw;
	FSample Result;
	for (Toolbox::int32 Frame = -Warmup; Frame < Frames; ++Frame)
	{
		const bool bRecord = Frame >= 0;
		Check(Renderer.BeginFrame(1280, 720, {}));
		const auto TextBefore = Text.GetMeasures();
		const auto TextTimeBefore = Text.GetNanoseconds();
		FClock Clock;
		Scene.Mutate(Frame);
		Clock.Lap(Result, 0, bRecord);
		FUiRoot& Root = Scene.GetRoot();
		for (Toolbox::int32 View = 0; View < Views; ++View)
		{
			FUiScaleSettings Scale;
			Scale.UserScale = bScale && (Frame & 1) ? 1.25f : 1.0f;
			const FUiSurface Surface({View * 1280 / Views, 0, (View + 1) * 1280 / Views, 720}, Scale);
			Root.SetSurface(Surface);
			Check(Root.Layout());
			Clock.Lap(Result, 1, bRecord);
			Draw.Clear();
			Check(Root.BuildDrawList(Draw));
			Clock.Lap(Result, 2, bRecord);
			auto Submitted = SubmitUiDrawList(Renderer.GetContext().Get2D(), Draw, Surface, {1000, 0});
			if (!Submitted)
			{
				throw Toolbox::FException(Submitted.Error().Message);
			}
			Clock.Lap(Result, 3, bRecord);
			if (bRecord)
			{
				Result.Commands += Submitted.Value().Commands;
			}
		}
		Check(Renderer.EndFrame());
		Clock.Lap(Result, 4, bRecord);
		if (bRecord)
		{
			Result.TextMeasures += Text.GetMeasures() - TextBefore;
			Result.TextNanoseconds += static_cast<Toolbox::f64>(Text.GetNanoseconds() - TextTimeBefore);
		}
	}
	return Result;
}

// 3Dのパネルの系列。bDraw3D は中間画像への実描画と3Dの貼付、falseは配置と命令生成だけ（CPUへ寸法を渡しただけ）。
FSample RunPanel(FTimedTextService& Text, FRenderSystem& Renderer, FAssetService& Assets, Toolbox::size_t Count,
                 bool bDraw3D, bool bTransparent)
{
	FBenchmarkScene Scene(Text, EBenchmarkMode::Static, Count);
	FUiSceneHost Host;
	FUiWorldPanel3D Panel;
	Panel.TopLeft = {-2, 1.5f, 0};
	Panel.TopRight = {2, 1.5f, 0};
	Panel.BottomLeft = {-2, -1.5f, 0};
	Panel.TextureWidth = 1024;
	Panel.TextureHeight = 512;
	Panel.Composition = bTransparent ? EUiPanelComposition::Transparent : EUiPanelComposition::Opaque;
	FUiDisplayOptions Options;
	Options.Scale.Mode = EUiScaleMode::FixedPixel;
	Host.AddWorldPanel3D(Scene.GetRoot(), Panel, Assets, Options);
	FRenderView3D View;
	View.Id = 1;
	View.Eye = {0, 0, -6};
	FUiDrawList Draw;
	FSample Result;
	for (Toolbox::int32 Frame = -Warmup; Frame < Frames; ++Frame)
	{
		const bool bRecord = Frame >= 0;
		Check(Renderer.BeginFrame(1280, 720, {}));
		const auto TextBefore = Text.GetMeasures();
		FClock Clock;
		Clock.Lap(Result, 0, bRecord);
		if (bDraw3D)
		{
			// 中間画像への描画（配置・命令生成・受付・実行を含む）と3Dの貼付。
			Check(Host.RenderWorldPanelTextures(Renderer.GetContext()));
			Clock.Lap(Result, 1, bRecord);
			Check(Renderer.GetContext().Get3D().SetView(View));
			Check(Host.DrawWorldPanels3D(Renderer.GetContext(), View));
			Clock.Lap(Result, 3, bRecord);
		}
		else
		{
			FUiRoot& Root = Scene.GetRoot();
			FUiScaleSettings Scale;
			Scale.Mode = EUiScaleMode::FixedPixel;
			Root.SetSurface(FUiSurface({0, 0, 1024, 512}, Scale));
			Check(Root.Layout());
			Clock.Lap(Result, 1, bRecord);
			Draw.Clear();
			Check(Root.BuildDrawList(Draw));
			Clock.Lap(Result, 2, bRecord);
		}
		Check(Renderer.EndFrame());
		Clock.Lap(Result, 4, bRecord);
		if (bRecord)
		{
			Result.TextMeasures += Text.GetMeasures() - TextBefore;
		}
	}
	return Result;
}

void Print(const char* Surface, const char* Text, const char* Mode, Toolbox::size_t Count, Toolbox::int32 Views,
           FSample* Samples, Toolbox::f64 Seconds)
{
	Toolbox::Sort(Samples, Samples + Repeats,
	              [](const FSample& A, const FSample& B)
	              {
		              return A.Total() < B.Total();
	              });
	const FSample& S = Samples[Repeats / 2];
	auto Us = [](Toolbox::f64 Value)
	{
		return Value / Frames / 1000.0;
	};
	auto PerFrame = [](Toolbox::uint64 Value)
	{
		return static_cast<Toolbox::f64>(Value) / Frames;
	};
	printf(
	    "%s,%s,%s,%llu,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
	    Surface, Text, Mode, static_cast<unsigned long long>(Count), Views, Us(S.Total()), Us(Samples[0].Total()),
	    Us(Samples[Repeats - 1].Total()), Us(S.Nanoseconds[0]), Us(S.Nanoseconds[1]), Us(S.TextNanoseconds),
	    Us(S.Nanoseconds[2]), Us(S.Nanoseconds[3]), Us(S.Nanoseconds[4]), PerFrame(S.Allocations[0]),
	    PerFrame(S.Allocations[1]), PerFrame(S.Allocations[2]), PerFrame(S.Allocations[3]), PerFrame(S.Allocations[4]),
	    PerFrame(S.Allocations[2] + S.Allocations[3]), PerFrame(S.TextMeasures), PerFrame(S.Commands), Seconds);
	fflush(stdout);
}
} // namespace

int main()
{
	try
	{
		FDxLibBackends Backends;
		auto Services = Backends.GetServices();
		FWindowSettings Window;
		Window.Title = "dxf native UI benchmark";
		Window.bVSync = false;
		Check(Services.Platform.Initialize(Window));
		{
			FAssetService Assets(Services.Textures, Services.Sounds, Services.Fonts);
			FUiAssetTextService RealText(Assets);
			FTimedTextService Text(RealText);
			FRenderSystem Renderer(Services.Renderer);
			printf(
			    "# real DxLib and real fonts; warmup=%d frames=%d repeats=%d; us/frame (median run); allocations per "
			    "frame; native=queue execution and present on the CPU (not GPU time); text=real font measure time\n",
			    Warmup, Frames, Repeats);
			printf("surface,text,mode,count,views,total_median,total_min,total_max,mutation,layout,text_measure,build,"
			       "submit,native,alloc_mutation,alloc_layout,alloc_build,alloc_submit,alloc_native,alloc_gen_to_queue,"
			       "text_measures,commands,series_seconds\n");
			FBenchmarkLabel Japanese;
			Japanese.Initial = "数値 0";
			Japanese.Alternate = "数値 1";
			Japanese.Width = 60;
			Japanese.Height = 22;
			FBenchmarkLabel Wrap;
			Wrap.Initial = "日本語の長い説明文を折り返して表示する 0";
			Wrap.Alternate = "日本語の長い説明文を折り返して表示する 1";
			Wrap.Width = 120;
			Wrap.Height = 48;
			Wrap.Wrap = EUiTextWrap::Wrap;
			FBenchmarkLabel Ellipsis;
			Ellipsis.Initial = "A long ASCII label value that ellipsizes 0";
			Ellipsis.Alternate = "A long ASCII label value that ellipsizes 1";
			Ellipsis.Width = 80;
			Ellipsis.Overflow = EUiTextOverflow::Ellipsis;
			const FTextKind Kinds[] = {{"ascii-short", FBenchmarkLabel{}},
			                           {"japanese-short", Japanese},
			                           {"japanese-wrap", Wrap},
			                           {"ascii-ellipsis", Ellipsis}};
			struct FMode
			{
				const char* Name;
				EBenchmarkMode Mode;
				bool bScale;
			};
			const FMode Modes[] = {{"static", EBenchmarkMode::Static, false},
			                       {"value", EBenchmarkMode::Value, false},
			                       {"scale", EBenchmarkMode::Static, true},
			                       {"style-reload", EBenchmarkMode::StyleReload, false}};
			const Toolbox::TArray<Toolbox::size_t, 2> SmallLarge{100, 1000};
			const Toolbox::TArray<Toolbox::size_t, 2> ListCounts{1000, 10000};
			for (const FTextKind& Kind : Kinds)
			{
				for (const FMode& Mode : Modes)
				{
					for (const Toolbox::size_t Count : SmallLarge)
					{
						for (Toolbox::int32 Views = 1; Views <= 2; ++Views)
						{
							const Toolbox::uint64 Start = Toolbox::MonotonicNanoseconds();
							FSample Samples[Repeats];
							for (Toolbox::int32 R = 0; R < Repeats; ++R)
							{
								Samples[R] =
								    RunScreen(Text, Renderer, Mode.Mode, Count, Views, Kind.Label, Mode.bScale);
							}
							Print("screen", Kind.Name, Mode.Name, Count, Views, Samples,
							      static_cast<Toolbox::f64>(Toolbox::MonotonicNanoseconds() - Start) / 1.0e9);
						}
					}
				}
			}
			for (const Toolbox::size_t Count : ListCounts)
			{
				for (Toolbox::int32 Views = 1; Views <= 2; ++Views)
				{
					const Toolbox::uint64 Start = Toolbox::MonotonicNanoseconds();
					FSample Samples[Repeats];
					for (Toolbox::int32 R = 0; R < Repeats; ++R)
					{
						Samples[R] = RunScreen(Text, Renderer, EBenchmarkMode::ListScroll, Count, Views, {}, false);
					}
					Print("screen", "ascii-list", "list-scroll", Count, Views, Samples,
					      static_cast<Toolbox::f64>(Toolbox::MonotonicNanoseconds() - Start) / 1.0e9);
				}
			}
			for (const Toolbox::size_t Count : SmallLarge)
			{
				for (Toolbox::int32 Variant = 0; Variant < 3; ++Variant)
				{
					const bool bDraw3D = Variant > 0;
					const bool bTransparent = Variant == 2;
					const Toolbox::uint64 Start = Toolbox::MonotonicNanoseconds();
					FSample Samples[Repeats];
					for (Toolbox::int32 R = 0; R < Repeats; ++R)
					{
						Samples[R] = RunPanel(Text, Renderer, Assets, Count, bDraw3D, bTransparent);
					}
					Print(bDraw3D ? (bTransparent ? "panel-3d-transparent-drawn" : "panel-3d-opaque-drawn")
					              : "panel-cpu-layout-only",
					      "ascii-short", "static", Count, 1, Samples,
					      static_cast<Toolbox::f64>(Toolbox::MonotonicNanoseconds() - Start) / 1.0e9);
				}
			}
			Assets.Shutdown();
		}
		Services.Platform.Shutdown();
		return 0;
	}
	catch (const Toolbox::FException& Error)
	{
		Toolbox::Err << Error.What() << "\n";
		return 1;
	}
}
