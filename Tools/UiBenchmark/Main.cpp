// SPDX-License-Identifier: NOASSERTION
// CPUのUI更新・配置・入力・描画命令の生成・通常の2Dキューの受付。実字体・実GPU・Physicsの費用は含めない。
// 割当は区間ごとに数え、目標の「命令生成〜キュー受付」はbuild＋submitの合計。
#include "BenchmarkScene.h"
#include "Dxf/AssetService.h"
#include "Dxf/RenderSystem.h"
#include "Dxf/UiRenderer.h"
#include "Support/FakeBackend.h"
#include "Toolbox/Algorithm.h"
#include "Toolbox/Platform.h"
#include "AllocationFault.h"
#include <stdio.h>
namespace
{
using namespace Dxf;
using namespace Dxf::UiBenchmark;
constexpr Toolbox::int32 Warmup = 30;
constexpr Toolbox::int32 Frames = 120;
constexpr Toolbox::int32 Repeats = 5;
// 区間：変更・配置・命令生成・キュー受付・入力と更新。
constexpr Toolbox::size_t Segments = 5;
struct FSample
{
	Toolbox::f64 Nanoseconds[Segments] = {};
	Toolbox::uint64 Allocations[Segments] = {};
	Toolbox::uint64 Measures = 0;
	Toolbox::uint64 TextMeasures = 0;
	Toolbox::uint64 Commands = 0;
	// 測定区間（変更〜入力）の前後で解放されずに残った割当の合計（代替の描画先の記録は含まない）。
	Toolbox::int64 Retained = 0;
	// Runの前後で解放されなかった割当（破棄後の漏れ）。
	Toolbox::int64 Leaked = 0;
	Toolbox::size_t Elements = 0;
	Toolbox::size_t Rows = 0;
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

// 区間の時間と割当を加える。
struct FSegmentClock
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

FSample Run(Toolbox::size_t Count, EBenchmarkMode Mode, Toolbox::int32 Views, bool bTexture)
{
	Testing::FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	auto Font = Assets.LoadFont();
	if (!Font)
	{
		throw Toolbox::FException(Font.Error().Message);
	}
	FBenchmarkText Text(Font.Value());
	FBenchmarkScene Scene(Text, Mode, Count);
	FRenderSystem Renderer(Backend);
	const Toolbox::int32 Width = bTexture ? 512 : 1280;
	const Toolbox::int32 Height = bTexture ? 256 : 720;
	FUiDrawList Draw;
	FSample Result;
	for (Toolbox::int32 Frame = -Warmup; Frame < Frames; ++Frame)
	{
		const bool bRecord = Frame >= 0;
		// フレームの開始（キューの準備）は測定の区間の外。
		Check(Renderer.BeginFrame(1280, 720, {}));
		FUiRoot& Root = Scene.GetRoot();
		const auto BeforeMeasures = Root.GetStats().Layout.Measures;
		const auto BeforeText = Text.GetMeasures();
		const auto OutstandingBefore = static_cast<Toolbox::int64>(Toolbox::Testing::GetOutstandingTestAllocations());
		FSegmentClock Clock;
		Scene.Mutate(Frame);
		Clock.Lap(Result, 0, bRecord);
		FUiRoot& Current = Scene.GetRoot();
		for (Toolbox::int32 View = 0; View < Views; ++View)
		{
			const FUiSurface Surface({View * Width / Views, 0, (View + 1) * Width / Views, Height}, {});
			Current.SetSurface(Surface);
			Check(Current.Layout());
			Clock.Lap(Result, 1, bRecord);
			Draw.Clear();
			Check(Current.BuildDrawList(Draw));
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
		FUiInputFrame Input;
		Input.DeltaSeconds = 1.0 / 60.0;
		Input.Pointer.bPresent = true;
		Input.Pointer.Position = {60, 60};
		if (!Current.ProcessInput(Input))
		{
			throw Toolbox::FException("input failed");
		}
		Check(Current.Update(1.0 / 60.0));
		Clock.Lap(Result, 4, bRecord);
		if (bRecord)
		{
			Result.Retained +=
			    static_cast<Toolbox::int64>(Toolbox::Testing::GetOutstandingTestAllocations()) - OutstandingBefore;
		}
		// 代替の描画先への実行は測定の区間の外。
		Check(Renderer.EndFrame());
		if (bRecord && Mode != EBenchmarkMode::RootRecreate)
		{
			Result.Measures += Current.GetStats().Layout.Measures - BeforeMeasures;
		}
		if (bRecord)
		{
			Result.TextMeasures += Text.GetMeasures() - BeforeText;
		}
	}
	Result.Elements = Scene.GetRoot().GetStats().Elements;
	Result.Rows = Scene.GetRows();
	return Result;
}
} // namespace

int main()
{
	try
	{
		printf("# CPU only; fixed-width text double; fake render backend; warmup=%d frames=%d repeats=%d; "
		       "times in us/frame (median run by total); allocations per frame; gen_to_queue=build+submit\n",
		       Warmup, Frames, Repeats);
		printf("surface,mode,count,views,total_median,total_min,total_max,mutation,layout,build,submit,input,"
		       "alloc_mutation,alloc_layout,alloc_build,alloc_submit,alloc_input,alloc_gen_to_queue,alloc_total,"
		       "layout_measures,text_measures,commands,retained_in_segments,leaked_after_run,elements,rows,series_"
		       "seconds\n");
		const char* Modes[] = {"static", "value", "layout", "list-scroll", "style-reload", "root-recreate"};
		for (Toolbox::int32 Texture = 0; Texture < 2; ++Texture)
			for (Toolbox::int32 Views = 1; Views <= 2; ++Views)
				for (Toolbox::int32 Mode = 0; Mode < static_cast<Toolbox::int32>(EBenchmarkMode::Count); ++Mode)
					for (Toolbox::int32 Large = 0; Large < 2; ++Large)
					{
						const auto Series = static_cast<EBenchmarkMode>(Mode);
						const Toolbox::size_t Count =
						    Series == EBenchmarkMode::ListScroll ? (Large ? 10000 : 1000) : (Large ? 1000 : 100);
						const Toolbox::uint64 SeriesStart = Toolbox::MonotonicNanoseconds();
						FSample Samples[Repeats];
						for (Toolbox::int32 R = 0; R < Repeats; ++R)
						{
							const auto Before =
							    static_cast<Toolbox::int64>(Toolbox::Testing::GetOutstandingTestAllocations());
							Samples[R] = Run(Count, Series, Views, Texture != 0);
							Samples[R].Leaked =
							    static_cast<Toolbox::int64>(Toolbox::Testing::GetOutstandingTestAllocations()) - Before;
						}
						const Toolbox::f64 SeriesSeconds =
						    static_cast<Toolbox::f64>(Toolbox::MonotonicNanoseconds() - SeriesStart) / 1.0e9;
						Toolbox::Sort(Samples, Samples + Repeats,
						              [](const FSample& A, const FSample& B)
						              {
							              return A.Total() < B.Total();
						              });
						const FSample& S = Samples[Repeats / 2];
						const auto PerFrameUs = [](Toolbox::f64 Value)
						{
							return Value / Frames / 1000.0;
						};
						const auto PerFrame = [](Toolbox::uint64 Value)
						{
							return static_cast<Toolbox::f64>(Value) / Frames;
						};
						Toolbox::uint64 AllAllocations = 0;
						for (const Toolbox::uint64 Value : S.Allocations)
						{
							AllAllocations += Value;
						}
						printf(
						    "%s,%s,%llu,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,"
						    "%.2f,%.2f,%.2f,%lld,%lld,%llu,%llu,%.2f\n",
						    Texture ? "panel-texture-layout" : "screen-layout", Modes[Mode],
						    static_cast<unsigned long long>(Count), Views, PerFrameUs(S.Total()),
						    PerFrameUs(Samples[0].Total()), PerFrameUs(Samples[Repeats - 1].Total()),
						    PerFrameUs(S.Nanoseconds[0]), PerFrameUs(S.Nanoseconds[1]), PerFrameUs(S.Nanoseconds[2]),
						    PerFrameUs(S.Nanoseconds[3]), PerFrameUs(S.Nanoseconds[4]), PerFrame(S.Allocations[0]),
						    PerFrame(S.Allocations[1]), PerFrame(S.Allocations[2]), PerFrame(S.Allocations[3]),
						    PerFrame(S.Allocations[4]), PerFrame(S.Allocations[2] + S.Allocations[3]),
						    PerFrame(AllAllocations), PerFrame(S.Measures), PerFrame(S.TextMeasures),
						    PerFrame(S.Commands), static_cast<long long>(S.Retained), static_cast<long long>(S.Leaked),
						    static_cast<unsigned long long>(S.Elements), static_cast<unsigned long long>(S.Rows),
						    SeriesSeconds);
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
