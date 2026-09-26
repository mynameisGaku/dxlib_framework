// SPDX-License-Identifier: NOASSERTION
// UIの確保の寿命の測定。段階ごとの未解放の確保の数と、作成・破棄・拡縮・DPI・再読込の繰返しの増加を出す。
// 固定幅の文字の代替・代替の描画先を使うCPUの測定（実字体・実GPUは含めない）。
#include "BenchmarkScene.h"
#include "Dxf/AssetService.h"
#include "Dxf/RenderSystem.h"
#include "Dxf/UiDefaultStyles.h"
#include "Dxf/UiSceneHost.h"
#include "Support/FakeBackend.h"
#include "AllocationFault.h"
#include <stdio.h>
namespace
{
using namespace Dxf;
using namespace Dxf::UiBenchmark;
constexpr Toolbox::int32 Cycles = 50;

// 何も記録しない描画先（代替の描画先の記録の増加を、UIの確保から分ける）。
class FNullRenderer final : public IRenderBackend
{
public:
	TResult<void> SetTarget(Toolbox::int32, Toolbox::int32, Toolbox::int32) override
	{
		return {};
	}
	TResult<void> Clear(FColor) override
	{
		return {};
	}
	TResult<void> ResetState(Toolbox::int32, Toolbox::int32) override
	{
		return {};
	}
	TResult<void> DrawSprite(const FSpriteCommand&) override
	{
		return {};
	}
	TResult<void> DrawText(const FTextCommand&) override
	{
		return {};
	}
	TResult<void> DrawRectangle(const FRectangleCommand&) override
	{
		return {};
	}
	bool SupportsClip2D() const noexcept override
	{
		return true;
	}
	TResult<void> SetClip2D(bool, FIntRect) override
	{
		return {};
	}
	TResult<void> Present() override
	{
		return {};
	}
};

Toolbox::int64 Outstanding()
{
	return static_cast<Toolbox::int64>(Toolbox::Testing::GetOutstandingTestAllocations());
}

void Check(TResult<void> Result)
{
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
}

// Hostを通して1フレームを描く（入力・時間は1回、描画は代替の描画先で実行する）。
void Frame(FUiSceneHost& Host, FRenderSystem& Renderer, Toolbox::uint64& Index, Toolbox::int32 Width,
           Toolbox::int32 Height, Toolbox::int32 Dpi)
{
	FInputStateTracker Tracker;
	FRawInput Raw;
	Tracker.Advance(Raw);
	FFrameTime Time;
	Time.FrameIndex = ++Index;
	Time.UnscaledDeltaSeconds = 1.0 / 60.0;
	FTickContext Context{Tracker.GetSnapshot(), Time};
	Context.Window.bKnown = true;
	Context.Window.RenderWidth = Width;
	Context.Window.RenderHeight = Height;
	Context.Window.ClientWidth = Width;
	Context.Window.ClientHeight = Height;
	Context.Window.Dpi = Dpi;
	(void)Host.RouteInput(Context);
	Check(Renderer.BeginFrame(Width, Height, {}));
	Check(Host.Draw(Renderer.GetContext()));
	Check(Renderer.EndFrame());
}

// 繰返しの間の未解放の数の最小・最大・最初・最後（増え続けないことの確認）。
struct FTrend
{
	Toolbox::int64 First = 0;
	Toolbox::int64 Last = 0;
	Toolbox::int64 Min = 0;
	Toolbox::int64 Max = 0;
	void Add(Toolbox::int32 Cycle, Toolbox::int64 Value)
	{
		if (Cycle == 0)
		{
			First = Min = Max = Value;
		}
		Last = Value;
		Min = Value < Min ? Value : Min;
		Max = Value > Max ? Value : Max;
	}
};

void PrintTrend(const char* Name, const FTrend& Trend, Toolbox::int64 Base)
{
	printf("%s,%d,%lld,%lld,%lld,%lld\n", Name, Cycles, static_cast<long long>(Trend.First - Base),
	       static_cast<long long>(Trend.Last - Base), static_cast<long long>(Trend.Min - Base),
	       static_cast<long long>(Trend.Max - Base));
}
} // namespace

int main()
{
	try
	{
		printf("# UI allocation lifetime; outstanding allocation counts relative to the previous phase or base\n");
		// 1. UIの静的な初期化の前。
		const Toolbox::int64 Start = Outstanding();
		const auto StartTotal = Toolbox::Testing::GetTotalTestAllocations();
		// 2. 組込みスタイルの初回の初期化（プロセスの間保持する唯一の静的な資源）。
		(void)GetBuiltInUiStyleSheet();
		const Toolbox::int64 AfterStatic = Outstanding();
		const auto StaticTotal = Toolbox::Testing::GetTotalTestAllocations() - StartTotal;
		(void)GetBuiltInUiStyleSheet();
		const Toolbox::int64 AfterSecondStatic = Outstanding();
		printf("phase,outstanding_delta,note\n");
		printf("static-builtin-style-first,%lld,total_allocations=%llu\n", static_cast<long long>(AfterStatic - Start),
		       static_cast<unsigned long long>(StaticTotal));
		printf("static-builtin-style-second,%lld,\n", static_cast<long long>(AfterSecondStatic - AfterStatic));
		Toolbox::int64 AfterServices = 0;
		Toolbox::int64 AfterDestroy = 0;
		{
			Testing::FFakeBackend Backend;
			FAssetService Assets(Backend, Backend, Backend);
			auto Font = Assets.LoadFont();
			if (!Font)
			{
				throw Toolbox::FException(Font.Error().Message);
			}
			FBenchmarkText Text(Font.Value());
			FNullRenderer Null;
			FRenderSystem Renderer(Null);
			AfterServices = Outstanding();
			Toolbox::uint64 Index = 0;
			{
				// 3. Root／Host／テキストサービスの作成後（1000要素、1表示）。
				FBenchmarkScene Scene(Text, EBenchmarkMode::Static, 1000);
				FUiSceneHost Host;
				FUiDisplayOptions Options;
				Options.Scale.Mode = EUiScaleMode::FixedPixel;
				Host.AddScreen(Scene.GetRoot(), Options);
				const Toolbox::int64 AfterCreate = Outstanding();
				// 4. 慣らしの後の定常フレーム。
				for (Toolbox::int32 I = 0; I < 30; ++I)
				{
					Frame(Host, Renderer, Index, 1280, 720, 96);
				}
				const Toolbox::int64 AfterWarmup = Outstanding();
				for (Toolbox::int32 I = 0; I < 120; ++I)
				{
					Frame(Host, Renderer, Index, 1280, 720, 96);
				}
				const Toolbox::int64 AfterSteady = Outstanding();
				printf("create-root-host-1000,%lld,\n", static_cast<long long>(AfterCreate - AfterServices));
				printf("warmup-30-frames,%lld,\n", static_cast<long long>(AfterWarmup - AfterCreate));
				printf("steady-120-frames,%lld,\n", static_cast<long long>(AfterSteady - AfterWarmup));
			}
			// 5. Root／Hostの破棄の後。
			AfterDestroy = Outstanding();
			printf("destroy-root-host,%lld,relative to services\n",
			       static_cast<long long>(AfterDestroy - AfterServices));
			printf("cycle,count,first,last,min,max\n");
			// 作成・破棄の繰返し。
			FTrend Recreate;
			for (Toolbox::int32 Cycle = 0; Cycle < Cycles; ++Cycle)
			{
				{
					FBenchmarkScene Scene(Text, EBenchmarkMode::Static, 100);
					FUiSceneHost Host;
					Host.AddScreen(Scene.GetRoot());
					for (Toolbox::int32 I = 0; I < 5; ++I)
					{
						Frame(Host, Renderer, Index, 1280, 720, 96);
					}
				}
				Recreate.Add(Cycle, Outstanding());
			}
			PrintTrend("root-host-recreate", Recreate, AfterServices);
			// 寸法・DPI・スタイル再読込の繰返し（同じRoot）。
			FBenchmarkScene Scene(Text, EBenchmarkMode::StyleReload, 100);
			FUiSceneHost Host;
			FUiDisplayOptions Dpi;
			Dpi.Scale.Mode = EUiScaleMode::Dpi;
			Host.AddScreen(Scene.GetRoot(), Dpi);
			Frame(Host, Renderer, Index, 1280, 720, 96);
			const Toolbox::int64 SceneBase = Outstanding();
			FTrend Resize;
			for (Toolbox::int32 Cycle = 0; Cycle < Cycles; ++Cycle)
			{
				Frame(Host, Renderer, Index, (Cycle & 1) ? 1001 : 1280, (Cycle & 1) ? 501 : 720, 96);
				Resize.Add(Cycle, Outstanding());
			}
			PrintTrend("resize-1280x720-1001x501", Resize, SceneBase);
			FTrend DpiChange;
			for (Toolbox::int32 Cycle = 0; Cycle < Cycles; ++Cycle)
			{
				Frame(Host, Renderer, Index, 1280, 720, (Cycle & 1) ? 144 : 96);
				DpiChange.Add(Cycle, Outstanding());
			}
			PrintTrend("dpi-96-144", DpiChange, SceneBase);
			FTrend Reload;
			for (Toolbox::int32 Cycle = 0; Cycle < Cycles; ++Cycle)
			{
				Scene.Mutate(Cycle);
				Frame(Host, Renderer, Index, 1280, 720, 96);
				Reload.Add(Cycle, Outstanding());
			}
			PrintTrend("style-reload", Reload, SceneBase);
		}
		// 6. 資源管理・代替の描画先の終了の後（残るのはプロセスの間保持する静的な資源だけのはず）。
		const Toolbox::int64 End = Outstanding();
		printf("phase,outstanding_delta,note\n");
		printf("after-services-end,%lld,relative to process start (static-builtin-style-first=%lld)\n",
		       static_cast<long long>(End - Start), static_cast<long long>(AfterStatic - Start));
		return 0;
	}
	catch (const Toolbox::FException& Error)
	{
		Toolbox::Err << Error.What() << "\n";
		return 1;
	}
}
