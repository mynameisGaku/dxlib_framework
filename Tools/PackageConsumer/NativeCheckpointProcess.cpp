// SPDX-License-Identifier: NOASSERTION
#include "NativeCheckpointProcess.h"
#include "CheckpointProcess.h"
#include "Dxf/CheckpointScene2D.h"
#include "Dxf/CheckpointScene3D.h"
#include "Dxf/ViewCoordinates.h"
#include "Dxf/NativeHandle.h"
#include "DxLib.h"
namespace
{
using namespace Dxf;
using namespace Toolbox;
// 本物の描画の不一致を外部子プロセスの失敗へする。
void Check(bool Value, const char* Reason)
{
	if (!Value)
	{
		throw FException(Reason);
	}
}
// 同じ位置をWorldへ書き戻さず、表示だけを現在の矩形へ合わせる。
FContentView2D View2D(int32 Width, int32 Height, bool Split, int32 Side)
{
	FContentView2D Value;
	const int32 Left = Split && Side == 1 ? Width / 2 : 0;
	const int32 Right = Split && Side == 0 ? Width / 2 : Width;
	Value.Origin = {Left + (Right - Left) * .25f, Height * .6f};
	Value.PixelsPerMeter = Min(45.0f, (Right - Left) / 22.0f);
	Value.bClip = Split;
	Value.ClipRect = {Left, 0, Right, Height};
	return Value;
}
FRenderView3D View3D(int32 Width, int32 Height, bool Split, int32 Side)
{
	FRenderView3D Value;
	Value.Eye = {4, 6, -20};
	Value.Target = {4, 1, 0};
	Value.bOrthographic = true;
	Value.OrthographicHeight = 20;
	Value.Debug.Lighting = ELightingMode3D::Unlit;
	Value.bViewport = Split;
	Value.Viewport = {Split && Side == 1 ? Width / 2 : 0, 0, Split && Side == 0 ? Width / 2 : Width, Height};
	return Value;
}
// GPUから一括で読み戻した一時画像だけを解放する。
void Release(void*, int32 Image) noexcept
{
	(void)DxLib::DeleteSoftImage(Image);
}
template <typename TScene>
void Pixels(TScene& Scene, const FPath& SaveRoot, const char* Stage, bool Split, int32 Width, int32 Height, const TFunction<void()>& Step)
{
	auto View = [&]()
	{
		if constexpr (IsSame<TScene, ACheckpointScene2D>)
		{
			return View2D(Width, Height, Split, 0);
		}
		else
		{
			return View3D(Width, Height, Split, 0);
		}
	}();
	const auto Second = [&]()
	{
		if constexpr (IsSame<TScene, ACheckpointScene2D>)
		{
			return View2D(Width, Height, Split, 1);
		}
		else
		{
			return View3D(Width, Height, Split, 1);
		}
	}();
	Scene.SetViews(View, Split ? TOptional<decltype(View)>(Second) : TOptional<decltype(View)>{});
	const auto BeforeStep = Scene.GetCompletedPhysicsStepCount();
	Step();
	Check(Scene.GetCompletedPhysicsStepCount() == BeforeStep + 1, "External view count added a physics Step");
	int ActualWidth = 0;
	int ActualHeight = 0;
	Check(DxLib::GetDrawScreenSize(&ActualWidth, &ActualHeight) == 0 && ActualWidth == Width && ActualHeight == Height, "External native dimensions differ");
	FNativeHandle Image(DxLib::MakeARGB8ColorSoftImage(Width, Height), nullptr, &Release);
	Check(Image.Get() >= 0 && DxLib::SetDrawScreen(DX_SCREEN_FRONT) == 0, "External save image allocation failed");
	const auto Read = DxLib::GetDrawScreenSoftImage(0, 0, Width, Height, Image.Get());
	Check(DxLib::SetDrawScreen(DX_SCREEN_BACK) == 0 && Read == 0, "External save image readback failed");
	const FString Names[] = {"left", "right", "key", "cargo", "cargoChanged"};
	const bool Before = FStringView(Stage) == "before";
	const bool Changed = FStringView(Stage) == "changed";
	const f32 Xs[] = {0, 4, 12, 8, -4};
	for (int32 Side = 0; Side < (Split ? 2 : 1); ++Side)
	{
		for (int32 Item = 0; Item < 5; ++Item)
		{
			const bool Expected = Item < 2 || (Item == 2 && Before) || (Item == 3 && !Before) || (Item == 4 && Changed);
			Dxf::FVector2 Point;
			if constexpr (IsSame<TScene, ACheckpointScene2D>)
			{
				const auto V = View2D(Width, Height, Split, Side);
				Point = {V.Origin.X + Xs[Item] * V.PixelsPerMeter, V.Origin.Y - V.PixelsPerMeter};
			}
			else
			{
				const auto Projected = ProjectWorldToScreen(View3D(Width, Height, Split, Side), Width, Height, {Xs[Item], 1, 0});
				Check(Projected && Projected.Value().bInsideView, "External saved body projection outside view");
				Point = Projected.Value().Screen;
			}
			int32 Matches = 0;
			for (int32 Y = -3; Y <= 3; ++Y)
			{
				for (int32 X = -3; X <= 3; ++X)
				{
					int R = 0;
					int G = 0;
					int B = 0;
					int A = 0;
					Check(DxLib::GetPixelSoftImage(Image.Get(), static_cast<int>(Point.X) + X, static_cast<int>(Point.Y) + Y, &R, &G, &B, &A) == 0, "External save pixel read failed");
					if (Abs(R - 80) <= 12 && Abs(G - 180) <= 12 && Abs(B - 240) <= 12)
					{
						++Matches;
					}
				}
			}
			Check(Expected ? Matches >= 35 : Matches == 0, "External saved composition pixel mismatch");
			Out << "EXTERNAL_SAVE_PIXEL stage=" << Stage << " side=" << Side << " item=" << Item << " present=" << Expected << " matches=" << Matches << '\n';
		}
	}
	// 仕掛けとは別の領域で、実画像または実モデルの存在も確認する。
	for (int32 Side = 0; Side < (Split ? 2 : 1); ++Side)
	{
		Dxf::FVector2 Point;
		if constexpr (IsSame<TScene, ACheckpointScene2D>)
		{
			const auto V = View2D(Width, Height, Split, Side);
			Point = {V.Origin.X, V.Origin.Y - 3 * V.PixelsPerMeter};
		}
		else
		{
			const auto Projected = ProjectWorldToScreen(View3D(Width, Height, Split, Side), Width, Height, {0, 3.8f, 0});
			Check(Projected && Projected.Value().bInsideView, "External appearance outside view");
			Point = Projected.Value().Screen;
		}
		int32 Matches = 0;
		for (int32 Y = -15; Y <= 15; ++Y)
		{
			for (int32 X = -15; X <= 15; ++X)
			{
				int R = 0;
				int G = 0;
				int B = 0;
				int A = 0;
				Check(DxLib::GetPixelSoftImage(Image.Get(), static_cast<int>(Point.X) + X, static_cast<int>(Point.Y) + Y, &R, &G, &B, &A) == 0, "External appearance read failed");
				if (G > 40 && B > 40 && Abs(R - B) > 8)
				{
					++Matches;
				}
			}
		}
		Out << "EXTERNAL_SAVE_ASSET stage=" << Stage << " side=" << Side << " matches=" << Matches << '\n';
		Check(Matches > (IsSame<TScene, ACheckpointScene2D> ? 200 : 30), "External Texture or Model pixels missing");
	}
	const FString File = FString(Stage) + (Split ? Width == 1001 ? "-odd.png" : "-split.png" : "-single.png");
	const auto Path = SaveRoot / FPath(File);
	Check(DxLib::SaveSoftImageToPng(Path.ToUtf8().CStr(), Image.Get(), 6) == 0, "External save image output failed");
}
} // namespace
void RunNativeCheckpointProcess(FApplication& App, const FPath& Root, const FPath& SaveRoot, FStringView Mode, uint32 Dimension, const TFunction<void()>& Step)
{
	const auto Observe = [&](DScene& Base, const char* Stage)
	{
		const auto Draw = [&]<typename TScene>(TScene& Scene)
		{
			Pixels(Scene, SaveRoot, Stage, false, 1280, 720, Step);
			Pixels(Scene, SaveRoot, Stage, true, 1280, 720, Step);
			Check(DxLib::SetWindowSize(1001, 501) == 0, "External save odd resize failed");
			Pixels(Scene, SaveRoot, Stage, true, 1001, 501, Step);
			Check(DxLib::SetWindowSize(1280, 720) == 0, "External save restore resize failed");
			Step();
		};
		if (Dimension == 2)
		{
			Draw(*Base.TryCast<ACheckpointScene2D>());
		}
		else
		{
			Draw(*Base.TryCast<ACheckpointScene3D>());
		}
	};
	RunCheckpointProcess(App.GetAssets(), App.GetScenes(), Step, Observe, Root, SaveRoot, Mode, Dimension, true);
}
