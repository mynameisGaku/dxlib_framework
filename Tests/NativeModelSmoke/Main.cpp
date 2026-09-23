// SPDX-License-Identifier: NOASSERTION
// 実SDKのモデル試験。フレームワークのAPIだけで.fbxを読み、Get3Dで描き、描画結果を読み戻して確かめる。
// 2体の独立したインスタンス・一時停止・速度・再読込・受付後の破棄・日本語パス・描画失敗・終了順序を扱う。
// 使い方: NativeModelSmoke <ProjectRoot> <画像の出力ディレクトリ>
#include "Dxf/AssetService.h"
#include "../Support/ModelSmokeStep.h"
#include "Toolbox/Log.h"
#include "Dxf/ViewCoordinates.h"
#include "Dxf/Application.h"
#include "Dxf/GameScene.h"
#include "Dxf/ModelImport.h"
#include "Dxf/DxLibSession.h"
#include "Dxf/NativeBackends.h"
#include "Dxf/RenderSystem.h"
#include "Toolbox/Platform.h"
#ifndef DX_NON_USING_NAMESPACE_DXLIB
#define DX_NON_USING_NAMESPACE_DXLIB
#endif
#include "DxLib.h"
// Windowsの文字種選択マクロが、Toolboxの同名関数を書き換えるのを防ぐ。
#ifdef CreateDirectory
#undef CreateDirectory
#endif
#ifdef DrawText
#undef DrawText
#endif
#ifdef CopyFile
#undef CopyFile
#endif
#include <stdio.h>
void RunPickingExample(const Toolbox::FPath& Root, const Toolbox::FPath& Output, Toolbox::int32 Sequence);
namespace
{
using namespace Dxf;

constexpr Toolbox::int32 Width = 640;
constexpr Toolbox::int32 Height = 480;
constexpr FColor Background{12, 12, 12, 255};
Toolbox::int32 GFailures = 0;
// 同一プロセス内のApplication通し番号。
Toolbox::int32 GApplicationSequence = 0;

// 1項目の結果を出力する。
void Check_Internal(bool bOk, const char* Name, const Toolbox::FString& Detail = {})
{
	if (Detail.IsEmpty())
	{
		printf("%s %s\n", bOk ? "PASS" : "FAIL", Name);
	}
	else
	{
		printf("%s %s %s\n", bOk ? "PASS" : "FAIL", Name, Detail.CStr());
	}
	fflush(stdout);
	GFailures += bOk ? 0 : 1;
}

void RequireSuccess_Internal(TResult<void> Result)
{
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
}

template <typename T> T TakeOrThrow_Internal(TResult<T> Result)
{
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
	return Toolbox::Move(Result).Value();
}

/**
 * 画面の左半分・右半分に描かれた形の要約。
 */
struct FSignature
{
	Toolbox::uint64 Hash[2] = {1469598103934665603ull, 1469598103934665603ull};
	Toolbox::uint32 Pixels[2] = {0, 0};
	Toolbox::uint32 Colored[2] = {0, 0};
	// 頂点色の各成分の優勢画素と最大値。
	Toolbox::uint32 Dominant[3] = {0, 0, 0};
	Toolbox::int32 MaxChannel = 0;
	// 照明の変化を輪郭とは別に比較する。
	Toolbox::uint64 Brightness[2] = {0, 0};
	Toolbox::uint64 ColorHash[2] = {1469598103934665603ull, 1469598103934665603ull};
	Toolbox::uint32 OverlayRgb = 0;
	Toolbox::uint32 CenterRgb = 0;
	// 重複矩形の内部を確認する点。
	Toolbox::uint32 OverlapRgb = 0;
	// 境界をまたぐUI文字の白い画素。
	Toolbox::uint32 TextPixels[2] = {0, 0};
};

// CPU側へ読み戻した画像を解放する。Contextは使用しない。
void ReleaseImage_Internal(void*, Toolbox::int32 Handle) noexcept
{
	DxLib::DeleteSoftImage(Handle);
}

// 裏画面を一括で読み戻し、背景と異なる画素の配置を左右別に要約する。
FSignature Read_Internal()
{
	// 画素ごとのGPU転送を避け、比較する画像を1回の転送で確定する。
	FNativeHandle Image(DxLib::MakeARGB8ColorSoftImage(Width, Height), nullptr, &ReleaseImage_Internal);
	if (Image.Get() < 0 || DxLib::GetDrawScreenSoftImage(0, 0, Width, Height, Image.Get()) < 0)
	{
		throw Toolbox::FException("Model test image readback failed");
	}
	// 左右それぞれの輪郭と色の集計。
	FSignature Result;
	for (Toolbox::int32 Y = 0; Y < Height; Y += 2)
	{
		for (Toolbox::int32 X = 0; X < Width; X += 2)
		{
			int Red = 0;
			int Green = 0;
			int Blue = 0;
			// 不透明度もAPIの出力先として受け取る。
			int Alpha = 0;
			if (DxLib::GetPixelSoftImage(Image.Get(), X, Y, &Red, &Green, &Blue, &Alpha) < 0)
			{
				throw Toolbox::FException("Model test pixel read failed");
			}
			if (X == Width / 2 && Y == Height / 2)
				Result.CenterRgb = static_cast<Toolbox::uint32>((Red << 16) | (Green << 8) | Blue);
			if (X == 240 && Y == 240)
			{
				Result.OverlapRgb = static_cast<Toolbox::uint32>((Red << 16) | (Green << 8) | Blue);
			}
			if (X == 20 && Y == 20)
			{
				Result.OverlayRgb = static_cast<Toolbox::uint32>((Red << 16) | (Green << 8) | Blue);
			}
			const int Distance = Toolbox::Abs(Red - Background.R) + Toolbox::Abs(Green - Background.G) +
			                     Toolbox::Abs(Blue - Background.B);
			const int Side = X < Width / 2 ? 0 : 1;
			if (Y >= 30 && Y < 60 && X >= 220 && X < 500 && Red > 200 && Green > 200 && Blue > 200)
			{
				++Result.TextPixels[Side];
			}
			Result.Brightness[Side] += static_cast<Toolbox::uint64>(Red + Green + Blue);
			Result.ColorHash[Side] =
			    (Result.ColorHash[Side] ^ static_cast<Toolbox::uint64>((Red << 16) | (Green << 8) | Blue)) *
			    1099511628211ull;
			if (Distance > 40)
			{
				++Result.Pixels[Side];
				Result.Hash[Side] =
				    (Result.Hash[Side] ^ static_cast<Toolbox::uint64>(Y * Width + X)) * 1099511628211ull;
				const int High = Toolbox::Max(Red, Toolbox::Max(Green, Blue));
				const int Low = Toolbox::Min(Red, Toolbox::Min(Green, Blue));
				Result.Dominant[0] += Red > Green + 25 && Red > Blue + 25 ? 1 : 0;
				Result.Dominant[1] += Green > Red + 25 && Green > Blue + 25 ? 1 : 0;
				Result.Dominant[2] += Blue > Red + 25 && Blue > Green + 25 ? 1 : 0;
				Result.MaxChannel = Toolbox::Max(Result.MaxChannel, High);
				// テクスチャのチェッカー（茶・青緑）は成分差が大きい。
				Result.Colored[Side] += High - Low > 60 ? 1 : 0;
			}
		}
	}
	return Result;
}

Toolbox::FString Describe_Internal(const FSignature& Signature)
{
	return "left=" + Toolbox::ToString(Signature.Pixels[0]) + "/" + Toolbox::ToString(Signature.Colored[0]) +
	       " right=" + Toolbox::ToString(Signature.Pixels[1]) + "/" + Toolbox::ToString(Signature.Colored[1]);
}

/**
 * 試験全体で共有する状態。
 */
struct FSmoke
{
	FDxLibBackends Backends;
	FBackendServices Services = Backends.GetServices();
	FDxLibSession Session{Services.Platform};
	Toolbox::FPath OutDir;
	FRenderView3D View;
	FSmoke(const Toolbox::FPath& Output) : OutDir(Output)
	{
		View.Eye = {0, 150, -450};
		View.Target = {0, 100, 0};
		View.NearPlane = 1.0f;
		View.FarPlane = 2000.0f;
	}

	// 指定のインスタンスを1フレーム描き、Flush後の裏画面を要約する。Nameがあれば画像を保存する。
	FSignature Frame(FRenderSystem& Renderer, const FModelInstance* A, const FModelInstance* B,
	                 const char* Name = nullptr, bool bOverlay = false)
	{
		if (!TakeOrThrow_Internal(Services.Platform.PumpEvents()))
		{
			throw Toolbox::FException("The window was closed");
		}
		RequireSuccess_Internal(Renderer.BeginFrame(Width, Height, Background));
		auto& Render = Renderer.GetContext();
		RequireSuccess_Internal(Render.Get3D().SetView(View));
		if (A != nullptr)
		{
			RequireSuccess_Internal(Render.Get3D().DrawModel(*A));
		}
		if (B != nullptr)
		{
			RequireSuccess_Internal(Render.Get3D().DrawModel(*B));
		}
		if (bOverlay)
		{
			FDrawStyle Style;
			Style.Color = {255, 64, 32, 255};
			RequireSuccess_Internal(Render.Get2D().FillRectangle({10, 10, 40, 40}, Style));
		}
		FSignature Signature;
		const Toolbox::FString Path =
		    Name != nullptr ? (OutDir / (Toolbox::FString(Name) + ".png")).ToUtf8() : Toolbox::FString();
		RequireSuccess_Internal(Render.Native(
		    [&]
		    {
			    Signature = Read_Internal();
			    if (!Path.IsEmpty() && DxLib::SaveDrawScreenToPNG(0, 0, Width, Height, Path.CStr()) < 0)
			    {
				    return TResult<void>::Failure(EErrorCode::BackendFailure, "SaveDrawScreenToPNG failed");
			    }
			    return TResult<void>::Success();
		    }));
		RequireSuccess_Internal(Renderer.EndFrame());
		return Signature;
	}
};

bool Same_Internal(const FSignature& A, const FSignature& B, int Side)
{
	return A.Hash[Side] == B.Hash[Side] && A.Pixels[Side] == B.Pixels[Side];
}

FModelInstance Place_Internal(FAssetService& Assets, const FModel& Model, Toolbox::f32 X)
{
	FModelInstance Instance = TakeOrThrow_Internal(Assets.CreateModelInstance(Model));
	RequireSuccess_Internal(Instance.SetTransform(Toolbox::FMatrix4::Translation({X, 0, 0})));
	RequireSuccess_Internal(Instance.Play("Bend"));
	return Instance;
}

// 奇数寸法の別描画先で、理論上の正方形の位置・画素寸法を比較する。
void ViewportProjection_Internal(FSmoke& Smoke, FRenderSystem& Renderer, FAssetService& Assets)
{
	const auto Target = TakeOrThrow_Internal(Assets.CreateRenderTarget(641, 481));
	Toolbox::TArray<FRenderView3D, 2> Views;
	for (Toolbox::int32 Ortho = 0; Ortho < 2; ++Ortho)
	{
		RequireSuccess_Internal(Renderer.BeginFrame(Width, Height, Background));
		auto& Render = Renderer.GetContext();
		RequireSuccess_Internal(Render.SetRenderTarget(Target));
		RequireSuccess_Internal(Render.ClearTarget(Background));
		for (Toolbox::int32 Side = 0; Side < 2; ++Side)
		{
			FRenderView3D View;
			View.bViewport = true;
			View.Viewport = Side == 0 ? FIntRect{0, 0, 320, 240} : FIntRect{320, 0, 641, 481};
			View.Eye = {0, 0, -4};
			View.bOrthographic = Ortho != 0;
			View.OrthographicHeight = 8;
			View.VerticalFov = 1.5707963268f;
			Views[Side] = View;
			View.Debug.Lighting = ELightingMode3D::Unlit;
			RequireSuccess_Internal(Render.Get3D().SetView(View));
			RequireSuccess_Internal(Render.Get3D().DrawTriangle({-1, -1, 0}, {1, -1, 0}, {1, 1, 0}));
			RequireSuccess_Internal(Render.Get3D().DrawTriangle({-1, -1, 0}, {1, 1, 0}, {-1, 1, 0}));
		}
		RequireSuccess_Internal(Render.Native(
		    [&]() -> TResult<void>
		    {
			    FNativeHandle Image(DxLib::MakeARGB8ColorSoftImage(641, 481), nullptr, &ReleaseImage_Internal);
			    if (Image.Get() < 0 || DxLib::GetDrawScreenSoftImage(0, 0, 641, 481, Image.Get()) < 0)
			    {
				    return TResult<void>::Failure(EErrorCode::BackendFailure, "viewport projection readback failed");
			    }
			    Toolbox::int32 MinX[2] = {641, 641};
			    Toolbox::int32 MinY[2] = {481, 481};
			    Toolbox::int32 MaxX[2] = {-1, -1};
			    Toolbox::int32 MaxY[2] = {-1, -1};
			    for (Toolbox::int32 Y = 0; Y < 481; ++Y)
			    {
				    for (Toolbox::int32 X = 0; X < 641; ++X)
				    {
					    Toolbox::int32 R = 0;
					    Toolbox::int32 G = 0;
					    Toolbox::int32 B = 0;
					    Toolbox::int32 A = 0;
					    if (DxLib::GetPixelSoftImage(Image.Get(), X, Y, &R, &G, &B, &A) < 0)
					    {
						    return TResult<void>::Failure(EErrorCode::BackendFailure,
						                                  "viewport projection pixel failed");
					    }
					    if (R > 200)
					    {
						    const Toolbox::int32 Side = X < 320 ? 0 : 1;
						    MinX[Side] = Toolbox::Min(MinX[Side], X);
						    MinY[Side] = Toolbox::Min(MinY[Side], Y);
						    MaxX[Side] = Toolbox::Max(MaxX[Side], X);
						    MaxY[Side] = Toolbox::Max(MaxY[Side], Y);
					    }
				    }
			    }
			    for (Toolbox::int32 Side = 0; Side < 2; ++Side)
			    {
				    // 描画時の値を保存し、Nativeの最後のカメラとは独立に四隅を求める。
				    const auto TopLeft = TakeOrThrow_Internal(ProjectWorldToScreen(Views[Side], 641, 481, {-1, 1, 0}));
				    const auto BottomRight = TakeOrThrow_Internal(ProjectWorldToScreen(Views[Side], 641, 481, {1, -1, 0}));
				    Check_Internal(Toolbox::Abs(TopLeft.Screen.X - MinX[Side]) <= 1 && Toolbox::Abs(TopLeft.Screen.Y - MinY[Side]) <= 1 && Toolbox::Abs(BottomRight.Screen.X - (MaxX[Side] + 1)) <= 1 && Toolbox::Abs(BottomRight.Screen.Y - (MaxY[Side] + 1)) <= 1, "projection API matches off-center rendered corners on unequal odd target views");
				    const Toolbox::int32 ExpectedSize = Side == 0 ? 60 : 120;
				    const Toolbox::int32 CenterX = Side == 0 ? 320 : 961;
				    const Toolbox::int32 CenterY = Side == 0 ? 240 : 481;
				    Check_Internal(Toolbox::Abs(MaxX[Side] - MinX[Side] + 1 - ExpectedSize) <= 1 &&
				                       Toolbox::Abs(MaxY[Side] - MinY[Side] + 1 - ExpectedSize) <= 1 &&
				                       Toolbox::Abs(MaxX[Side] + MinX[Side] + 1 - CenterX) <= 1 &&
				                       Toolbox::Abs(MaxY[Side] + MinY[Side] + 1 - CenterY) <= 1,
				                   "viewport analytic square size and center on odd offscreen target");
			    }
			    return {};
		    }));
		// 別寸法の描画先へ戻ったあと、保持された大きすぎるビューを受付しない。
		RequireSuccess_Internal(Render.SetBackBuffer());
		Check_Internal(!Render.Get3D().DrawLine({0, 0, 0}, {1, 0, 0}),
		               "viewport revalidates retained view after target switch");
		RequireSuccess_Internal(Render.Get3D().SetView(FRenderView3D{}));
		RequireSuccess_Internal(Render.Get2D().FillRectangle({0, 0, 640, 480}));
		RequireSuccess_Internal(Renderer.EndFrame());
	}
}

// 実D3D11の左右ビュー、投影、領域外深度、全画面2Dを同じ読戻しで比較する。
void ViewportSmoke_Internal(FSmoke& Smoke, FRenderSystem& Renderer, FAssetService& Assets, const FModel& Model)
{
	const FFont Font = TakeOrThrow_Internal(Assets.LoadFont());
	FModelInstance A = TakeOrThrow_Internal(Assets.CreateModelInstance(Model));
	FModelInstance B = TakeOrThrow_Internal(Assets.CreateModelInstance(Model));
	RequireSuccess_Internal(A.SetTransform(Toolbox::FMatrix4::Translation({-55, 0, 0})));
	RequireSuccess_Internal(B.SetTransform(Toolbox::FMatrix4::Translation({55, 0, 0})));
	RequireSuccess_Internal(A.Play("Widen", false));
	RequireSuccess_Internal(A.SetTime(0.5));
	RequireSuccess_Internal(A.SetMorphWeight(1, 0.5f));
	RequireSuccess_Internal(B.SetMaterial(FModelMaterial3D{}));
	auto Frame = [&](bool Right, bool DepthProbe, bool Overlay, bool Ortho, const char* Name)
	{
		RequireSuccess_Internal(Renderer.BeginFrame(Width, Height, Background));
		auto& Render = Renderer.GetContext();
		for (Toolbox::int32 Side = 0; Side < (Right ? 2 : 1); ++Side)
		{
			FRenderView3D View = Smoke.View;
			View.bViewport = true;
			View.Viewport = {Side * 320, 0, (Side + 1) * 320, 480};
			View.bOrthographic = Ortho;
			View.OrthographicHeight = 360;
			if (Side == 1)
			{
				View.Eye = {250, 150, -400};
				View.LightDirection = {1, -1, 0};
			}
			RequireSuccess_Internal(Render.Get3D().SetView(View));
			RequireSuccess_Internal(Render.Get3D().DrawModel(A));
			RequireSuccess_Internal(Render.Get3D().DrawModel(B));
			FDrawStyle3D Style;
			Style.Color = {200, 60, 30, 255};
			RequireSuccess_Internal(Render.Get3D().DrawTriangle({-500, -40, 0}, {0, -20, 0}, {500, -40, 0}, Style));
		}
		if (Overlay)
		{
			FDrawStyle Style;
			Style.Color = {255, 64, 32, 255};
			RequireSuccess_Internal(Render.Get2D().FillRectangle({0, 230, 640, 250}, Style));
			RequireSuccess_Internal(Render.Get2D().DrawText(Font, "LEFT VIEW  /  RIGHT VIEW", {220, 30}));
		}
		FSignature Result;
		RequireSuccess_Internal(Render.Native(
		    [&]() -> TResult<void>
		    {
			    if (DepthProbe)
			    {
				    // 左の深度が右開始時に消えていたら、この遠方面の赤がモデルを覆う。
				    DxLib::SetUseZBufferFlag(TRUE);
				    DxLib::SetWriteZBufferFlag(FALSE);
				    DxLib::SetZBufferCmpType(DX_CMP_LESSEQUAL);
				    DxLib::SetDrawZ(0.9999f);
				    DxLib::SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 255);
				    DxLib::DrawBox(0, 0, 320, 480, DxLib::GetColor(255, 0, 0), TRUE);
				    DxLib::SetDrawZ(0.2f);
			    }
			    Result = Read_Internal();
			    if (Name != nullptr)
			    {
				    const auto Path = (Smoke.OutDir / Name).ToUtf8();
				    if (DxLib::SaveDrawScreenToPNG(0, 0, Width, Height, Path.CStr()) < 0)
				    {
					    return TResult<void>::Failure(EErrorCode::BackendFailure, "viewport capture failed");
				    }
			    }
			    return {};
		    }));
		RequireSuccess_Internal(Renderer.EndFrame());
		return Result;
	};
	for (Toolbox::int32 Ortho = 0; Ortho < 2; ++Ortho)
	{
		const auto Left = Frame(false, false, false, Ortho != 0, nullptr);
		const auto Both =
		    Frame(true, false, false, Ortho != 0, Ortho ? "viewport-ortho.png" : "viewport-perspective.png");
		Check_Internal(Left.ColorHash[0] == Both.ColorHash[0] && Left.Pixels[1] == 0 && Both.Pixels[1] > 100,
		               "viewport preserves other color and draws distinct cameras");
		const auto DepthLeft = Frame(false, true, false, Ortho != 0, nullptr);
		const auto DepthBoth = Frame(true, true, false, Ortho != 0, nullptr);
		Check_Internal(DepthLeft.ColorHash[0] == DepthBoth.ColorHash[0] && DepthBoth.Dominant[0] < 38000,
		               "viewport preserves outside depth with occlusion probe");
		const auto Overlay = Frame(true, false, true, Ortho != 0, "viewport-overlay.png");
		Check_Internal(Overlay.CenterRgb == 0xff4020 && Overlay.TextPixels[0] > 10 && Overlay.TextPixels[1] > 10,
		               "viewport restores full screen 2D text and rectangle across split");
	}
}

// 重複または再訪でも新しい区間内は深度を初期化する契約を確認する。
void ViewportOverlap_Internal(FRenderSystem& Renderer)
{
	RequireSuccess_Internal(Renderer.BeginFrame(Width, Height, Background));
	auto& Render = Renderer.GetContext();
	for (Toolbox::int32 Pass = 0; Pass < 3; ++Pass)
	{
		FRenderView3D View;
		View.bViewport = true;
		View.Viewport = Pass == 1 ? FIntRect{160, 0, 480, 480} : FIntRect{0, 0, 320, 480};
		View.bOrthographic = true;
		View.Debug.Lighting = ELightingMode3D::Unlit;
		RequireSuccess_Internal(Render.Get3D().SetView(View));
		FDrawStyle3D Style;
		Style.Color = Pass == 0 ? FColor{255, 255, 255, 255} : FColor{255, 64, 32, 255};
		const Toolbox::f32 Z = static_cast<Toolbox::f32>(Pass * 10);
		RequireSuccess_Internal(Render.Get3D().DrawTriangle({-100, -100, Z}, {0, 100, Z}, {100, -100, Z}, Style));
		if (Pass > 0)
		{
			RequireSuccess_Internal(Render.Native(
			    [&]() -> TResult<void>
			    {
				    const auto Pixels = Read_Internal();
				    Check_Internal(Pixels.OverlapRgb == 0xff4020 &&
				                       Pixels.OverlayRgb == (Pass == 1 ? 0xffffffu : 0xff4020u),
				                   "viewport overlap and revisit reset only the new rectangle depth");
				    return {};
			    }));
		}
	}
	RequireSuccess_Internal(Renderer.EndFrame());
}

void Run_Internal(const Toolbox::FPath& Root, const Toolbox::FPath& OutDir)
{
	FSmoke Smoke(OutDir);
	FWindowSettings Settings;
	Settings.Title = "dxlib_framework - NativeModelSmoke";
	Settings.Width = Width;
	Settings.Height = Height;
	Settings.bVSync = false;
	RequireSuccess_Internal(Smoke.Session.Initialize(Settings));
	Check_Internal(DxLib::GetHandleNum(DX_HANDLETYPE_SHADER) == 0 && DxLib::GetHandleNum(DX_HANDLETYPE_MODEL) == 0,
	               "new session has no retained model or PBR shader handles");
	Check_Internal(Smoke.Services.pModels != nullptr, "model backend available in this DxLib build");
	FAssetService Assets(Smoke.Services.Textures, Smoke.Services.Sounds, Smoke.Services.Fonts, Smoke.Services.pModels);
	Check_Internal(Assets.SetProjectRoot(Root), "project root");
	FRenderSystem Renderer(Smoke.Services.Renderer);

	// モーフの変形量と再生を複製インスタンス間で分離する。
	FModel MorphModel = TakeOrThrow_Internal(Assets.LoadModel("Tests/Assets/MorphTriangle.fbx"));
	Check_Internal(MorphModel.GetMorphCount() == 2, "multiple morph targets and skin imported");
	FModelInstance MorphA = TakeOrThrow_Internal(Assets.CreateModelInstance(MorphModel));
	FModelInstance MorphB = TakeOrThrow_Internal(Assets.CreateModelInstance(MorphModel));
	RequireSuccess_Internal(MorphA.SetTransform(Toolbox::FMatrix4::Translation({-120, 0, 0})));
	RequireSuccess_Internal(MorphB.SetTransform(Toolbox::FMatrix4::Translation({120, 0, 0})));
	const FSignature MorphStart = Smoke.Frame(Renderer, &MorphA, &MorphB, "morph-start");
	RequireSuccess_Internal(MorphB.SetMorphWeight(0, 1));
	const FSignature MorphWide = Smoke.Frame(Renderer, &MorphA, &MorphB, "morph-right-wide");
	Check_Internal(MorphWide.Pixels[1] > MorphStart.Pixels[1] + 500,
	               "manual morph visibly widens the selected instance");
	Check_Internal(MorphWide.ColorHash[0] == MorphStart.ColorHash[0], "morph leaves the other instance unchanged");
	RequireSuccess_Internal(MorphB.ResetMorphWeight(0));
	RequireSuccess_Internal(MorphA.Play("Widen", false));
	RequireSuccess_Internal(MorphA.SetTime(0.5));
	const FSignature MorphAnimated = Smoke.Frame(Renderer, &MorphA, &MorphB, "morph-left-animated");
	Check_Internal(MorphAnimated.Pixels[0] > MorphStart.Pixels[0] + 200 &&
	                   MorphAnimated.ColorHash[1] == MorphStart.ColorHash[1],
	               "morph clip plays independently and resetting override restores base");
	MorphA.Pause();
	RequireSuccess_Internal(MorphA.Advance(0.2));
	const FSignature MorphPaused = Smoke.Frame(Renderer, &MorphA, &MorphB);
	Check_Internal(MorphPaused.ColorHash[0] == MorphAnimated.ColorHash[0],
	               "paused morph retains the same rendered pose");

	// 実DxLibの最終頂点から、追加UVが落ちずに保持されていることを確認する。
	Toolbox::TVector<Toolbox::uint8> UvBytes;
	Check_Internal(Toolbox::ReadFileBytes(Root / "Tests/Assets/AdditionalUv.fbx", UvBytes, 100000),
	               "read additional UV fixture");
	auto UvData = TakeOrThrow_Internal(ImportFbxModel(UvBytes.Data(), UvBytes.Size()));
	auto UvAllocation = TakeOrThrow_Internal(Smoke.Services.pModels->LoadModel(UvData, {}));
	const Toolbox::int32 UvHandle = UvAllocation.NativeHandle;
	Check_Internal(DxLib::MV1SetupReferenceMesh(UvHandle, -1, FALSE) >= 0, "setup native UV reference");
	const auto UvReference = DxLib::MV1GetReferenceMesh(UvHandle, -1, FALSE);
	bool UvsMatch = UvReference.VertexNum == 3;
	for (Toolbox::int32 Index = 0; Index < UvReference.VertexNum; ++Index)
	{
		const auto& Vertex = UvReference.Vertexs[Index];
		UvsMatch = UvsMatch && Toolbox::Abs(Vertex.TexCoord[1].u - (Vertex.TexCoord[0].u * 0.5f + 0.2f)) < 0.0001f;
		UvsMatch = UvsMatch && Toolbox::Abs(Vertex.TexCoord[1].v - (Vertex.TexCoord[0].v * 0.5f + 0.2f)) < 0.0001f;
	}
	Check_Internal(UvsMatch, "second UV set survives native model conversion");
	Smoke.Services.pModels->DeleteModel(UvHandle);
	FModel UvModel = TakeOrThrow_Internal(Assets.LoadModel("Tests/Assets/AdditionalUv.fbx"));
	FModelInstance UvInstance = TakeOrThrow_Internal(Assets.CreateModelInstance(UvModel));
	const FSignature UvFrame = Smoke.Frame(Renderer, &UvInstance, nullptr, "additional-uv");
	Check_Internal(UvFrame.Pixels[0] + UvFrame.Pixels[1] > 1000, "model with two UV sets draws");
	// 頂点色を補間し、材質の拡散色を乗算した三角形を実画面で確認する。
	FModel Colors = TakeOrThrow_Internal(Assets.LoadModel("Tests/Assets/VertexColors.fbx"));
	FModelInstance Colored = TakeOrThrow_Internal(Assets.CreateModelInstance(Colors));
	const FSignature ColorFrame = Smoke.Frame(Renderer, &Colored, nullptr, "vertex-colors");
	Check_Internal(ColorFrame.Dominant[0] > 50 && ColorFrame.Dominant[1] > 50 && ColorFrame.Dominant[2] > 50,
	               "vertex RGB colors interpolate in real rendering", Describe_Internal(ColorFrame));
	Check_Internal(ColorFrame.MaxChannel >= 100 && ColorFrame.MaxChannel <= 129,
	               "vertex colors multiply material diffuse once", Toolbox::ToString(ColorFrame.MaxChannel));
	// 正面からの単一光と正射影で、GGXの解析値と実画素を比較する。
	const FRenderView3D OriginalView = Smoke.View;
	Smoke.View.Eye = {0, 100, -600};
	Smoke.View.Target = {0, 100, 0};
	Smoke.View.bOrthographic = true;
	Smoke.View.OrthographicHeight = 400;
	Smoke.View.LightDirection = {0, 0, 1};
	Smoke.View.LightColor = {255, 255, 255, 255};
	Smoke.View.AmbientColor = {0, 0, 0, 255};
	FModel PbrModel = TakeOrThrow_Internal(Assets.LoadModel("Tests/Assets/PbrTriangle.fbx"));
	FModelInstance PbrInstance = TakeOrThrow_Internal(Assets.CreateModelInstance(PbrModel));
	Check_Internal(PbrInstance.GetMaterial().bPbr, "imported PBR selects shader automatically");
	const FSignature ImportedPbr = Smoke.Frame(Renderer, &PbrInstance, nullptr, "pbr-imported-reference");
	// N=V=L、金属度0.75、粗さ0.7、線形色(0.8,0.3,0.1)。独立に算出したsRGB値。
	const Toolbox::int32 Expected[3] = {131, 87, 53};
	for (Toolbox::int32 Channel = 0; Channel < 3; ++Channel)
	{
		const Toolbox::int32 Actual = static_cast<Toolbox::int32>((ImportedPbr.CenterRgb >> ((2 - Channel) * 8)) & 255);
		Check_Internal(Toolbox::Abs(Actual - Expected[Channel]) <= 3,
		               "PBR real pixel agrees with front-facing GGX reference", Toolbox::ToString(Actual));
	}
	FModelMaterial3D ImportedOverride = PbrInstance.GetMaterial();
	ImportedOverride.Metallic = 0.75f;
	ImportedOverride.Roughness = 0.7f;
	RequireSuccess_Internal(PbrInstance.SetMaterial(ImportedOverride));
	const FSignature ExplicitPbr = Smoke.Frame(Renderer, &PbrInstance, nullptr);
	Check_Internal(ImportedPbr.ColorHash[0] == ExplicitPbr.ColorHash[0] &&
	                   ImportedPbr.ColorHash[1] == ExplicitPbr.ColorHash[1],
	               "imported PBR factors equal explicit factors");
	FModel PbrUv = TakeOrThrow_Internal(Assets.LoadModel("Tests/Assets/PbrUv.fbx"));
	FModelInstance PbrUvInstance = TakeOrThrow_Internal(Assets.CreateModelInstance(PbrUv));
	const FSignature AutoUv = Smoke.Frame(Renderer, &PbrUvInstance, nullptr, "pbr-uv1");
	FModelMaterial3D UvMaterial = PbrUvInstance.GetMaterial();
	UvMaterial.BaseColorUv = 1;
	RequireSuccess_Internal(PbrUvInstance.SetMaterial(UvMaterial));
	const FSignature ExplicitUv = Smoke.Frame(Renderer, &PbrUvInstance, nullptr);
	UvMaterial.BaseColorUv = 0;
	RequireSuccess_Internal(PbrUvInstance.SetMaterial(UvMaterial));
	const FSignature FirstUv = Smoke.Frame(Renderer, &PbrUvInstance, nullptr, "pbr-uv0");
	Check_Internal(AutoUv.ColorHash[0] == ExplicitUv.ColorHash[0] && AutoUv.ColorHash[0] != FirstUv.ColorHash[0],
	               "PBR material selects UV1 and explicit UV0 changes pixels");

	// 同じ手作成データで骨・2モーフ・UV1・頂点色・PBRを同時に使用する。
	FModel Combined = TakeOrThrow_Internal(Assets.LoadModel("Tests/Assets/CombinedModel.fbx"));
	ViewportOverlap_Internal(Renderer);
	ViewportProjection_Internal(Smoke, Renderer, Assets);
	ViewportSmoke_Internal(Smoke, Renderer, Assets, Combined);
	FModelInstance CombinedA = TakeOrThrow_Internal(Assets.CreateModelInstance(Combined));
	FModelInstance CombinedB = TakeOrThrow_Internal(Assets.CreateModelInstance(Combined));
	RequireSuccess_Internal(CombinedA.SetTransform(Toolbox::FMatrix4::Translation({-120, 0, 0})));
	RequireSuccess_Internal(CombinedB.SetTransform(Toolbox::FMatrix4::Translation({120, 0, 0})));
	RequireSuccess_Internal(CombinedB.SetMaterial(FModelMaterial3D{}));
	RequireSuccess_Internal(CombinedA.Play("Widen", false));
	RequireSuccess_Internal(CombinedA.SetMorphWeight(0, 0));
	const FSignature CombinedStart = Smoke.Frame(Renderer, &CombinedA, &CombinedB);
	// 同形状・同じPBRとUVを持つ無頂点色データと比較し、併用時の色の寄与を確かめる。
	RequireSuccess_Internal(PbrUvInstance.SetTransform(Toolbox::FMatrix4::Translation({-120, 0, 0})));
	UvMaterial.BaseColorUv = 1;
	RequireSuccess_Internal(PbrUvInstance.SetMaterial(UvMaterial));
	const FSignature WithoutColors = Smoke.Frame(Renderer, &PbrUvInstance, &CombinedB);
	Check_Internal(CombinedStart.Hash[0] == WithoutColors.Hash[0] &&
	                   CombinedStart.Brightness[0] < WithoutColors.Brightness[0] &&
	                   CombinedStart.ColorHash[1] == WithoutColors.ColorHash[1],
	               "vertex colors modulate combined PBR UV1 pixels without changing geometry");
	RequireSuccess_Internal(CombinedA.SetTime(0.5));
	const FSignature BoneOnly = Smoke.Frame(Renderer, &CombinedA, &CombinedB);
	Check_Internal(BoneOnly.Hash[0] != CombinedStart.Hash[0] && BoneOnly.ColorHash[1] == CombinedStart.ColorHash[1],
	               "combined bone animation moves only the first instance");
	RequireSuccess_Internal(CombinedA.ResetMorphWeight(0));
	const FSignature OneMorph = Smoke.Frame(Renderer, &CombinedA, &CombinedB);
	RequireSuccess_Internal(CombinedA.SetMorphWeight(1, 1));
	const FSignature BothMorphs = Smoke.Frame(Renderer, &CombinedA, &CombinedB, "combined-bone-two-morphs");
	Check_Internal(OneMorph.Pixels[0] > BoneOnly.Pixels[0] + 100 && BothMorphs.Pixels[0] + 100 < OneMorph.Pixels[0],
	               "both morph channels affect the animated bone mesh");
	Check_Internal(BothMorphs.ColorHash[1] == CombinedStart.ColorHash[1],
	               "combined PBR deformation leaves ordinary clone unchanged");
	// UVの指定と頂点色を含む描画が通常材質の次へ漏れない。
	FModelMaterial3D CombinedMaterial = CombinedA.GetMaterial();
	CombinedMaterial.BaseColorUv = 0;
	RequireSuccess_Internal(CombinedA.SetMaterial(CombinedMaterial));
	const FSignature CombinedUv0 = Smoke.Frame(Renderer, &CombinedA, &CombinedB);
	Check_Internal(CombinedUv0.ColorHash[0] != BothMorphs.ColorHash[0] &&
	                   CombinedUv0.ColorHash[1] == BothMorphs.ColorHash[1],
	               "combined UV selection affects PBR colors only");
	CombinedMaterial.BaseColorUv = 1;
	RequireSuccess_Internal(CombinedA.SetMaterial(CombinedMaterial));
	const FSignature AcceptedReference = Smoke.Frame(Renderer, &CombinedA, &CombinedB, "combined-reference");
	// 描画受付後に材質と両モーフ、再生時刻を変更しても受付済みの画素は変わらない。
	RequireSuccess_Internal(Renderer.BeginFrame(Width, Height, Background));
	RequireSuccess_Internal(Renderer.GetContext().Get3D().SetView(Smoke.View));
	RequireSuccess_Internal(Renderer.GetContext().Get3D().DrawModel(CombinedA));
	RequireSuccess_Internal(Renderer.GetContext().Get3D().DrawModel(CombinedB));
	CombinedMaterial.Tint = {32, 200, 64, 255};
	CombinedMaterial.Metallic = 0;
	RequireSuccess_Internal(CombinedA.SetMaterial(CombinedMaterial));
	RequireSuccess_Internal(CombinedA.SetMorphWeight(0, 0));
	RequireSuccess_Internal(CombinedA.SetMorphWeight(1, 0));
	RequireSuccess_Internal(CombinedA.SetTime(0));
	FSignature Accepted;
	RequireSuccess_Internal(Renderer.GetContext().Native(
	    [&]
	    {
		    Accepted = Read_Internal();
		    return TResult<void>::Success();
	    }));
	RequireSuccess_Internal(Renderer.EndFrame());
	Check_Internal(Accepted.ColorHash[0] == AcceptedReference.ColorHash[0] &&
	                   Accepted.ColorHash[1] == AcceptedReference.ColorHash[1],
	               "accepted material morph and animation snapshots match reference pixels");
	const FSignature Changed = Smoke.Frame(Renderer, &CombinedA, &CombinedB);
	Check_Internal(Changed.ColorHash[0] != Accepted.ColorHash[0] && Changed.ColorHash[1] == Accepted.ColorHash[1],
	               "post-acceptance changes appear only in the next draw");
	Smoke.View = OriginalView;
	// 2体の独立したインスタンス。
	FModel Column = TakeOrThrow_Internal(Assets.LoadModel("Assets/Models/SkinnedColumn.fbx"));
	Check_Internal(Column.GetClipCount() == 2 && Column.FindClip("Bend") == 0 && Column.GetClip(0)->NativeDuration > 0,
	               "clips by name with measured native length",
	               "bend.native=" + Toolbox::ToString(static_cast<Toolbox::int64>(Column.GetClip(0)->NativeDuration)));
	FModelInstance A = Place_Internal(Assets, Column, -120);
	FModelInstance B = Place_Internal(Assets, Column, 120);
	const FSignature Start = Smoke.Frame(Renderer, &A, &B, "models-start");
	Check_Internal(Start.Pixels[0] > 200 && Start.Pixels[1] > 200 && Start.Colored[0] > 50 && Start.Colored[1] > 50,
	               "two textured instances drawn", Describe_Internal(Start));

	// GPUモデル照明の方向を反転し、同じビュー内の非照明モデルが変わらないことを確認する。
	FModelMaterial3D LitMaterial;
	LitMaterial.bLit = true;
	RequireSuccess_Internal(A.SetMaterial(LitMaterial));
	Smoke.View.LightDirection = {0, 0, 1};
	Smoke.View.AmbientColor = {0, 0, 0, 255};
	const FSignature LitFront = Smoke.Frame(Renderer, &A, &B, "model-lit-front");
	Smoke.View.LightDirection = {0, 0, -1};
	const FSignature LitBack = Smoke.Frame(Renderer, &A, &B, "model-lit-back");
	Check_Internal(LitFront.Brightness[0] > LitBack.Brightness[0] + 10000,
	               "GPU model light direction changes brightness");
	Check_Internal(LitFront.ColorHash[1] == Start.ColorHash[1] && LitBack.ColorHash[1] == Start.ColorHash[1],
	               "unlit model is independent of lit neighbor");
	// 外部所有の有効ライトと無効な既定ライトが、ビュー終了後に元の状態へ戻る。
	const Toolbox::int32 Foreign = DxLib::CreateDirLightHandle(DxLib::VGet(0, 0, -1));
	Check_Internal(Foreign >= 0, "create external light for restoration check");
	DxLib::SetLightEnable(FALSE);
	DxLib::SetLightEnableHandle(Foreign, TRUE);
	const FSignature WithForeign = Smoke.Frame(Renderer, &A, &B);
	Check_Internal(WithForeign.ColorHash[0] == LitBack.ColorHash[0], "external light does not affect model view");
	Check_Internal(DxLib::GetLightEnable() == FALSE && DxLib::GetLightEnableHandle(Foreign) == TRUE &&
	                   DxLib::GetEnableLightHandleNum() == 1,
	               "external light enable states restored and owned light released");
	DxLib::DeleteLightHandle(Foreign);
	DxLib::SetLightEnable(TRUE);
	const FSignature Overlay = Smoke.Frame(Renderer, &A, &B, "model-light-2d", true);
	Check_Internal(Overlay.OverlayRgb == 0xff4020u, "2D color restored after lit model");
	// ファイルのカメラ・ライトを明示選択して描画する。
	const FRenderView3D PreviousView = Smoke.View;
	FModel SceneObjects = TakeOrThrow_Internal(Assets.LoadModel("Tests/Assets/SceneObjects.fbx"));
	RequireSuccess_Internal(SceneObjects.GetCamera(0)->ApplyTo(Smoke.View));
	RequireSuccess_Internal(SceneObjects.GetLight(1)->ApplyTo(Smoke.View));
	const FSignature Point = Smoke.Frame(Renderer, &A, &B, "file-camera-point-light", true);
	Check_Internal(Point.Pixels[0] > 200 && Point.Pixels[1] > 200 && Point.OverlayRgb == 0xff4020u,
	               "file perspective camera and point light render with 2D restoration");
	// 点光源をモデルの背後へ動かすと、照明を使う個体だけ暗くなる。
	Smoke.View.ModelLightPosition.Z = 400;
	const FSignature PointBack = Smoke.Frame(Renderer, &A, &B);
	Check_Internal(Point.Brightness[0] > PointBack.Brightness[0] + 10000 &&
	                   Point.ColorHash[1] == PointBack.ColorHash[1],
	               "point light position affects only lit model");
	RequireSuccess_Internal(SceneObjects.GetLight(2)->ApplyTo(Smoke.View));
	const FSignature Spot = Smoke.Frame(Renderer, &A, &B, "file-spot-light");
	Smoke.View.ModelLightDirection = {0, 0, -1};
	const FSignature SpotAway = Smoke.Frame(Renderer, &A, &B);
	Check_Internal(Spot.Brightness[0] > SpotAway.Brightness[0] + 10000 && Spot.ColorHash[1] == SpotAway.ColorHash[1],
	               "spot cone excludes model when turned away");
	RequireSuccess_Internal(SceneObjects.GetCamera(1)->ApplyTo(Smoke.View));
	const FSignature Ortho = Smoke.Frame(Renderer, &A, &B, "file-orthographic-camera");
	Smoke.View.Eye.Z -= 100;
	Smoke.View.Target.Z -= 100;
	const FSignature OrthoMoved = Smoke.Frame(Renderer, &A, &B);
	Check_Internal(Ortho.Pixels[1] > 200 && Ortho.ColorHash[1] == OrthoMoved.ColorHash[1],
	               "file orthographic camera keeps size after depth movement");

	// 同一フレーム内の異なるビューで、ファイル内の全ライトと両カメラを切り替える。
	for (Toolbox::int32 Light = 0; Light < 3; ++Light)
	{
		RequireSuccess_Internal(SceneObjects.GetCamera(0)->ApplyTo(Smoke.View));
		RequireSuccess_Internal(SceneObjects.GetLight(Light)->ApplyTo(Smoke.View));
		Smoke.View.Id = 10;
		const FRenderView3D LeftView = Smoke.View;
		const FSignature LeftReference = Smoke.Frame(Renderer, &A, nullptr, nullptr, true);
		RequireSuccess_Internal(SceneObjects.GetCamera(1)->ApplyTo(Smoke.View));
		RequireSuccess_Internal(SceneObjects.GetLight((Light + 1) % 3)->ApplyTo(Smoke.View));
		// 右個体へ位置を合わせ、スポットの照射範囲内で切替を比較する。
		Smoke.View.ModelLightPosition.X = 120;
		Smoke.View.Id = 11;
		// Aを右へ移して同じ照明あり材質を使う。参照も同じ位置で取り直す。
		RequireSuccess_Internal(A.SetTransform(Toolbox::FMatrix4::Translation({120, 0, 0})));
		const FSignature RightPlaced = Smoke.Frame(Renderer, nullptr, &A);
		RequireSuccess_Internal(Renderer.BeginFrame(Width, Height, Background));
		RequireSuccess_Internal(Renderer.GetContext().Get3D().SetView(Smoke.View));
		RequireSuccess_Internal(Renderer.GetContext().Get3D().DrawModel(A));
		RequireSuccess_Internal(A.SetTransform(Toolbox::FMatrix4::Translation({-120, 0, 0})));
		RequireSuccess_Internal(Renderer.GetContext().Get3D().SetView(LeftView));
		RequireSuccess_Internal(Renderer.GetContext().Get3D().DrawModel(A));
		FDrawStyle OverlayStyle;
		OverlayStyle.Color = {255, 64, 32, 255};
		RequireSuccess_Internal(Renderer.GetContext().Get2D().FillRectangle({10, 10, 40, 40}, OverlayStyle));
		FSignature Switched;
		RequireSuccess_Internal(Renderer.GetContext().Native(
		    [&]
		    {
			    Switched = Read_Internal();
			    return TResult<void>::Success();
		    }));
		RequireSuccess_Internal(Renderer.EndFrame());
		Check_Internal(
		    Switched.ColorHash[0] == LeftReference.ColorHash[0] && Switched.ColorHash[1] == RightPlaced.ColorHash[1] &&
		        Switched.OverlayRgb == 0xff4020u && LeftReference.Pixels[0] > 200 && RightPlaced.Pixels[1] > 200,
		    "camera and light switches match separate view pixels and restore 2D",
		    "light=" + Toolbox::ToString(Light) +
		        " leftmatch=" + Toolbox::ToString(Switched.ColorHash[0] == LeftReference.ColorHash[0] ? 1 : 0) +
		        " rightmatch=" + Toolbox::ToString(Switched.ColorHash[1] == RightPlaced.ColorHash[1] ? 1 : 0) + " " +
		        Describe_Internal(Switched));
	}
	Smoke.View = PreviousView;
	// PBRの係数を個体ごとに変更し、既存のスキン描画と2D状態を保つ。
	const FRenderView3D BeforePbr = Smoke.View;
	Smoke.View.LightDirection = {0, 0, 1};
	FModelMaterial3D PbrMaterial;
	PbrMaterial.bPbr = true;
	PbrMaterial.Metallic = 0;
	PbrMaterial.Roughness = 0.2f;
	RequireSuccess_Internal(A.SetMaterial(PbrMaterial));
	const FSignature PbrSmooth = Smoke.Frame(Renderer, &A, &B, "pbr-smooth", true);
	PbrMaterial.Roughness = 0.9f;
	RequireSuccess_Internal(A.SetMaterial(PbrMaterial));
	const FSignature PbrRough = Smoke.Frame(Renderer, &A, &B, "pbr-rough");
	PbrMaterial.Metallic = 1;
	RequireSuccess_Internal(A.SetMaterial(PbrMaterial));
	const FSignature PbrMetal = Smoke.Frame(Renderer, &A, &B, "pbr-metal");
	Check_Internal(PbrSmooth.ColorHash[0] != PbrRough.ColorHash[0] && PbrRough.ColorHash[0] != PbrMetal.ColorHash[0],
	               "PBR roughness and metallic change real pixels");
	Check_Internal(PbrSmooth.ColorHash[1] == Start.ColorHash[1] && PbrMetal.ColorHash[1] == Start.ColorHash[1] &&
	                   PbrSmooth.OverlayRgb == 0xff4020u,
	               "PBR shader does not leak into neighbor or 2D");
	RequireSuccess_Internal(A.SetTime(0.5));
	const FSignature PbrAnimated = Smoke.Frame(Renderer, &A, &B, "pbr-skinned-animation");
	Check_Internal(!Same_Internal(PbrAnimated, PbrMetal, 0) && PbrAnimated.ColorHash[1] == PbrMetal.ColorHash[1],
	               "PBR keeps independent skinned animation");
	RequireSuccess_Internal(A.SetTime(0));
	RequireSuccess_Internal(A.SetMaterial(LitMaterial));
	Smoke.View = BeforePbr;
	// 色倍率も複製元の材質を変更せず、インスタンスごとに適用する。
	FModelMaterial3D RedMaterial;
	RedMaterial.Tint = {255, 0, 0, 255};
	RequireSuccess_Internal(A.SetMaterial(RedMaterial));
	const FSignature RedFrame = Smoke.Frame(Renderer, &A, &B, "model-tint");
	Check_Internal(RedFrame.ColorHash[0] != Start.ColorHash[0] && RedFrame.ColorHash[1] == Start.ColorHash[1],
	               "model tint is independent between instances");
	RequireSuccess_Internal(A.SetMaterial({}));
	Smoke.View.LightDirection = {0, -1, 1};
	Smoke.View.AmbientColor = {32, 32, 32, 255};

	// Aを一時停止し、Bだけ進める。
	A.Pause();
	RequireSuccess_Internal(A.Advance(0.5));
	RequireSuccess_Internal(B.Advance(0.5));
	const FSignature Bent = Smoke.Frame(Renderer, &A, &B, "models-b-bent");
	Check_Internal(Same_Internal(Bent, Start, 0), "paused instance keeps its pose", Describe_Internal(Bent));
	Check_Internal(!Same_Internal(Bent, Start, 1), "playing instance changes its pose");

	// Bを2倍速にする。0.25秒で0.5秒進み、1秒のクリップは先頭へ戻る。
	RequireSuccess_Internal(B.SetSpeed(2.0));
	RequireSuccess_Internal(B.Advance(0.25));
	const FSignature Wrapped = Smoke.Frame(Renderer, &A, &B);
	Check_Internal(B.GetTime() < 1e-6 && Same_Internal(Wrapped, Start, 1), "double speed loops back to the start pose",
	               "b.time=" + Toolbox::ToString(static_cast<Toolbox::int64>(B.GetTime() * 1000)) + "ms");
	Check_Internal(Same_Internal(Wrapped, Start, 0), "other instance unaffected by speed change");

	// Aを再開すると、Aの姿勢だけが変わる。
	A.Resume();
	RequireSuccess_Internal(A.Advance(0.5));
	const FSignature Resumed = Smoke.Frame(Renderer, &A, &B, "models-a-resumed");
	Check_Internal(!Same_Internal(Resumed, Start, 0) && Same_Internal(Resumed, Start, 1),
	               "resume advances only the resumed instance");

	// すべての参照を外して再読込し、同じ時刻なら同じ姿勢になる。
	A = FModelInstance();
	B = FModelInstance();
	Column = FModel();
	Assets.CollectUnused();
	Column = TakeOrThrow_Internal(Assets.LoadModel("Assets/Models/SkinnedColumn.fbx"));
	A = Place_Internal(Assets, Column, -120);
	B = Place_Internal(Assets, Column, 120);
	RequireSuccess_Internal(B.SetTime(0.5));
	const FSignature Reloaded = Smoke.Frame(Renderer, &A, &B);
	Check_Internal(Same_Internal(Reloaded, Start, 0) && Same_Internal(Reloaded, Bent, 1),
	               "reload reproduces the same poses", Describe_Internal(Reloaded));

	// 受付後のインスタンス破棄でも描画は完了する。実Scene切替は別のApplication試験で確認する。
	RequireSuccess_Internal(Renderer.BeginFrame(Width, Height, Background));
	RequireSuccess_Internal(Renderer.GetContext().Get3D().SetView(Smoke.View));
	RequireSuccess_Internal(Renderer.GetContext().Get3D().DrawModel(A));
	A = FModelInstance();
	FSignature Retired;
	RequireSuccess_Internal(Renderer.GetContext().Native(
	    [&]
	    {
		    Retired = Read_Internal();
		    return TResult<void>::Success();
	    }));
	RequireSuccess_Internal(Renderer.EndFrame());
	Check_Internal(Same_Internal(Retired, Start, 0) && Retired.Pixels[1] == 0,
	               "queued draw survives instance destruction");

	// 日本語を含むパス（絶対パス）。テクスチャはモデルと同じディレクトリから読む。
	const Toolbox::FPath Japanese = OutDir / Toolbox::FPath(u8"日本語モデル");
	Toolbox::CreateDirectory(Japanese);
	const Toolbox::FPath Box = Japanese / Toolbox::FPath(u8"箱.fbx");
	const Toolbox::FPath Checker = Japanese / "ModelChecker.bmp";
	if (!Toolbox::IsRegularFile(Box))
	{
		Toolbox::CopyFile(Root / "Assets/Models/StaticBox.fbx", Box);
	}
	if (!Toolbox::IsRegularFile(Checker))
	{
		Toolbox::CopyFile(Root / "Assets/Models/ModelChecker.bmp", Checker);
	}
	FModel BoxModel = TakeOrThrow_Internal(Assets.LoadModel(Box.ToUtf8()));
	FModelInstance BoxInstance = TakeOrThrow_Internal(Assets.CreateModelInstance(BoxModel));
	RequireSuccess_Internal(BoxInstance.SetTransform(Toolbox::FMatrix4::Translation({120, 50, 0})));
	const FSignature BoxFrame = Smoke.Frame(Renderer, nullptr, &BoxInstance, "japanese-path-box");
	Check_Internal(BoxModel.GetClipCount() == 0 && BoxFrame.Colored[1] > 100, "Japanese path model with its texture",
	               Describe_Internal(BoxFrame));
	auto Missing = Assets.LoadModel("Assets/Models/DoesNotExist.fbx");
	Check_Internal(!Missing && Missing.Error().Code == EErrorCode::NotFound, "missing file is NotFound");

	// ネイティブモデルが先に解放された描画は失敗し、そのフレームは表示しない。
	RequireSuccess_Internal(Renderer.BeginFrame(Width, Height, Background));
	RequireSuccess_Internal(Renderer.GetContext().Get3D().SetView(Smoke.View));
	RequireSuccess_Internal(Renderer.GetContext().Get3D().DrawModel(B));
	Assets.Shutdown();
	auto Failed = Renderer.EndFrame();
	Check_Internal(!Failed, "draw of a released model fails the frame",
	               Failed ? Toolbox::FString() : Failed.Error().Message);
	Renderer.CancelFrame();
	Check_Internal(!Column.IsValid() && !B.IsValid() && !BoxInstance.IsValid() && !BoxModel.IsValid(),
	               "shutdown invalidates models and instances");
	Check_Internal(!Assets.LoadModel("Assets/Models/SkinnedColumn.fbx"), "loading after shutdown fails");
	// モデル資源失効による失敗後にも、次フレームの2Dの画素が復元される。
	const FSignature AfterFailure = Smoke.Frame(Renderer, nullptr, nullptr, "combined-failure-2d", true);
	Check_Internal(AfterFailure.OverlayRgb == 0xff4020u && AfterFailure.Pixels[1] == 0,
	               "2D pixels recover after invalidated model draw failure");
	// 描画系を終えてからセッションを閉じる。
	Smoke.Session.Shutdown();
}

// Applicationより長く生存する観察値。描画資源は所有しない。
struct FApplicationModelTrace
{
	// 最後に実画面を読み戻した値。
	FSignature Image;
	// オブジェクトの開始・終了・描画完了数。
	Toolbox::int32 Initialized = 0;
	Toolbox::int32 Deinitialized = 0;
	Toolbox::int32 Captured = 0;
	// 受付後の資源失効を起こしてApplicationの失敗終了を通す。
	bool bInvalidate = false;
	// 受付後の資源失効を実際に通ったか。
	bool bInjected = false;
	// 描画フックへ到達した回数。
	Toolbox::int32 Draws = 0;
	// 分割時の更新数と描画時刻を観察する。
	bool bSplit = false;
	bool bPicking = false;
	Toolbox::int32 Ticks = 0;
	Toolbox::f64 TimeBeforeDraw = 0;
	Toolbox::f64 TimeAfterDraw = 0;
};

// 実GameSceneが所有し、実Applicationから初期化・描画・終了されるモデルオブジェクト。
class ACombinedModelObject final : public DGameObject
{
public:
	explicit ACombinedModelObject(FApplicationModelTrace& Trace) : m_Trace(Trace)
	{
	}

protected:
	TResult<void> OnInitialize(const FInitContext& Context) override
	{
		m_pAssets = &Context.Assets;
		// Scene再入場ごとに同じファイルを読み、モデルと個体を作る。
		const FModel Model = TakeOrThrow_Internal(Context.Assets.LoadModel("Tests/Assets/CombinedModel.fbx"));
		m_Instance = TakeOrThrow_Internal(Context.Assets.CreateModelInstance(Model));
		RequireSuccess_Internal(m_Instance.Play("Widen", false));
		RequireSuccess_Internal(m_Instance.SetTime(0.5));
		RequireSuccess_Internal(m_Instance.SetMorphWeight(1, 1));
		++m_Trace.Initialized;
		DXF_LOG_INFO("ModelLifecycle", "Object initialized count=%d", m_Trace.Initialized);
		return {};
	}
	void OnTick(const FTickContext& Context) override
	{
		++m_Trace.Ticks;
		RequireSuccess_Internal(m_Instance.Advance(Context.Time.DeltaSeconds));
	}
	void OnDraw(FRenderContext& Render) const override
	{
		++m_Trace.Draws;
		m_Trace.TimeBeforeDraw = m_Instance.GetTime();
		// 正射影と正面光でPBR・UV1・頂点色・両モーフ・骨を描く。
		FRenderView3D View;
		View.Eye = {0, 100, -600};
		View.Target = {0, 100, 0};
		View.bOrthographic = true;
		View.OrthographicHeight = 400;
		View.LightDirection = {0, 0, 1};
		View.LightColor = {255, 255, 255, 255};
		for (Toolbox::int32 Side = 0; Side < (m_Trace.bSplit ? 2 : 1); ++Side)
		{
			View.bViewport = m_Trace.bSplit;
			View.Viewport = {Side * 320, 0, (Side + 1) * 320, 480};
			RequireSuccess_Internal(Render.Get3D().SetView(View));
			RequireSuccess_Internal(Render.Get3D().DrawModel(m_Instance));
			if (m_Trace.bPicking)
			{
				Check_Internal(static_cast<bool>(MakeViewPickSegment(View, Width, Height, {static_cast<Toolbox::f32>(Side * 320 + 160), 240})), "animation draw pick segment is independent of model time");
			}
		}
		m_Trace.TimeAfterDraw = m_Instance.GetTime();
		FDrawStyle Style;
		Style.Color = {255, 64, 32, 255};
		RequireSuccess_Internal(Render.Get2D().FillRectangle({10, 10, 40, 40}, Style));
		if (m_Trace.bInvalidate)
		{
			m_pAssets->Shutdown();
			m_Trace.bInjected = true;
			DXF_LOG_INFO("ModelLifecycle", "Injection executed after draw submission draws=%d", m_Trace.Draws);
		}
		RequireSuccess_Internal(Render.Native(
		    [this]
		    {
			    m_Trace.Image = Read_Internal();
			    ++m_Trace.Captured;
			    return TResult<void>::Success();
		    }));
	}
	void OnDeinitialize() noexcept override
	{
		m_Instance = FModelInstance();
		++m_Trace.Deinitialized;
		DXF_LOG_INFO("ModelLifecycle", "Object deinitialized count=%d injected=%d captured=%d", m_Trace.Deinitialized, m_Trace.bInjected, m_Trace.Captured);
	}

private:
	// Application外の観察先と、Applicationが所有するAssetService。
	FApplicationModelTrace& m_Trace;
	FAssetService* m_pAssets = nullptr;
	// オブジェクトが寿命を所有する描画個体。
	FModelInstance m_Instance;
};

// 実GameSceneのオブジェクト集合を通してモデルの寿命を管理する。
class ACombinedModelScene final : public DGameScene
{
public:
	explicit ACombinedModelScene(FApplicationModelTrace& Trace) : m_Trace(Trace)
	{
	}

protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		// 生成失敗はSceneの初期化失敗として伝播する。
		auto Spawned = Spawn<ACombinedModelObject>(m_Trace);
		return Spawned ? TResult<void>::Success() : TResult<void>::Failure(Spawned.Error());
	}

private:
	// Sceneより長く生存する観察先。
	FApplicationModelTrace& m_Trace;
};

// 低レベルRenderer試験とは別に、ApplicationとScene所有境界を実DxLibで通す。
void RunApplication_Internal(const Toolbox::FPath& Root, bool bFail, bool bSplit = false, bool bPicking = false)
{
	// 今回の新規Applicationとフレームを識別する通し番号。
	const Toolbox::int32 Sequence = ++GApplicationSequence;
	Toolbox::int32 Frame = 0;
	DXF_LOG_INFO("ModelLifecycle", "Application begin sequence=%d expected=%s split=%d picking=%d", Sequence, bFail ? "injected-error" : "continue", bSplit, bPicking);
	FDxLibBackends Backends;
	FApplicationModelTrace Trace;
	Trace.bInvalidate = bFail;
	Trace.bSplit = bSplit;
	Trace.bPicking = bPicking;
	FApplicationSettings Settings;
	Settings.Window.Width = Width;
	Settings.Window.Height = Height;
	Settings.Window.bVSync = false;
	Settings.ClearColor = Background;
	Settings.ProjectRoot = Root.ToUtf8();
	Settings.ExecutionThreadCount = 1;
	FApplication App(Backends.GetServices(), Settings);
	RequireSuccess_Internal(App.Start(Toolbox::MakeUnique<ACombinedModelScene>(Trace)));
	// 一度だけStepを呼び、結果とフック到達を後続操作より先に残す。
	auto Step = [&](Toolbox::f64 Time, bool bExpectedFailure = false)
	{
		DXF_LOG_INFO("ModelLifecycle", "Step begin sequence=%d frame=%d expected=%s", Sequence, Frame, bExpectedFailure ? "injected-error" : "continue");
		const auto Result = App.Step(Time);
		DXF_LOG_INFO("ModelLifecycle", "Step end sequence=%d frame=%d result=%s code=%d message=%s running=%d initialized=%d draws=%d injected=%d stopped=%d captured=%d", Sequence, Frame, Result ? (Result.Value() ? "true" : "false") : "error", Result ? 0 : static_cast<Toolbox::int32>(Result.Error().Code), Result ? "" : Result.Error().Message.CStr(), App.IsRunning(), Trace.Initialized, Trace.Draws, Trace.bInjected, Trace.Deinitialized, Trace.Captured);
		++Frame;
		if (!Testing::ModelSmokeStepMatches(Result, bExpectedFailure, Trace.bInjected))
		{
			throw Toolbox::FException("Unexpected model Step outcome; scenario aborted before further scene requests");
		}
		return Result;
	};
	const auto First = Step(0, bFail);
	if (bFail)
	{
		Check_Internal(!First && Trace.bInjected && Trace.Draws == 1 && !App.IsRunning() && Trace.Deinitialized == 1 && Trace.Captured == 0,
		               "real Application model draw failure shuts down scene before capture");
		return;
	}
	Check_Internal(First && First.Value() && Trace.Captured == 1 && Trace.Image.Pixels[0] > 200 &&
	                   Trace.Image.Pixels[1] > 200 && Trace.Image.OverlayRgb == 0xff4020u,
	               "real Application GameScene object renders combined model and 2D");
	Check_Internal(Trace.Ticks == 1 && Trace.TimeBeforeDraw == Trace.TimeAfterDraw,
	               "real Application updates object once regardless of viewport count");
	const FSignature Initial = Trace.Image;
	{
		Check_Internal(static_cast<bool>(Step(1.0 / 60.0)) && Trace.Ticks == 2 &&
		                   Toolbox::Abs(Trace.TimeBeforeDraw - (0.5 + 1.0 / 60.0)) < 0.000001 &&
		                   Trace.TimeBeforeDraw == Trace.TimeAfterDraw,
		               "split Application advances animation once before both views");
	}
	const Toolbox::int32 ShaderCount = DxLib::GetHandleNum(DX_HANDLETYPE_SHADER);
	Check_Internal(ShaderCount > 0, "PBR created a shader handle");
	RequireSuccess_Internal(App.GetScenes().RequestChange<DGameScene>());
	Check_Internal(static_cast<bool>(Step(1.0 / 60.0)), "real Application switches to empty scene");
	App.GetAssets().CollectUnused();
	Check_Internal(DxLib::GetHandleNum(DX_HANDLETYPE_MODEL) == 0 && DxLib::GetHandleNum(DX_HANDLETYPE_MODEL_BASE) == 0,
	               "scene retirement and collection release native model handles");
	Check_Internal(Trace.Deinitialized == 1 && Trace.Captured == 2,
	               "retired GameScene releases object and does not replay model draw");
	RequireSuccess_Internal(App.GetScenes().RequestChange<ACombinedModelScene>(Trace));
	Check_Internal(static_cast<bool>(Step(1.0 / 60.0)), "real Application reloads model scene");
	Check_Internal(Trace.Initialized == 2 && Trace.Captured == 3 &&
	                   Trace.Image.ColorHash[0] == Initial.ColorHash[0] &&
	                   Trace.Image.ColorHash[1] == Initial.ColorHash[1],
	               "scene reload reproduces combined model pixels");
	Check_Internal(DxLib::GetHandleNum(DX_HANDLETYPE_SHADER) == ShaderCount,
	               "scene reload reuses session PBR shader without accumulating handles");
	App.Shutdown();
	Check_Internal(Trace.Deinitialized == 2 && !App.IsRunning(),
	               "Application shutdown releases remaining model object");
}
} // namespace

Toolbox::int32 main(Toolbox::int32 ArgCount, char** Args)
{
	if (ArgCount != 3)
	{
		Toolbox::Err << "Usage: NativeModelSmoke <ProjectRoot> <output directory>\n";
		return 2;
	}
#ifdef NDEBUG
	DXF_LOG_INFO("ModelLifecycle", "Run begin config=Release");
#else
	DXF_LOG_INFO("ModelLifecycle", "Run begin config=Debug");
#endif
	try
	{
		const Toolbox::FPath OutDir(Args[2]);
		Toolbox::CreateDirectory(OutDir);
		DXF_LOG_INFO("ModelLifecycle", "Picking sequence boundary appsCompleted=%d", GApplicationSequence);
		RunPickingExample(Toolbox::FPath(Args[1]), OutDir, ++GApplicationSequence);
		DXF_LOG_INFO("ModelLifecycle", "Renderer sequence boundary appsCompleted=%d", GApplicationSequence);
		Run_Internal(Toolbox::FPath(Args[1]), OutDir);
		RunApplication_Internal(Toolbox::FPath(Args[1]), false);
		RunApplication_Internal(Toolbox::FPath(Args[1]), true);
		RunApplication_Internal(Toolbox::FPath(Args[1]), false, true);
		RunApplication_Internal(Toolbox::FPath(Args[1]), false, false, true);
		RunApplication_Internal(Toolbox::FPath(Args[1]), false, true, true);
		RunApplication_Internal(Toolbox::FPath(Args[1]), true, true);
		RunApplication_Internal(Toolbox::FPath(Args[1]), false);
		// 同じプロセスでGPU資源を作り直し、古いハンドルを再利用しない。
		DXF_LOG_INFO("ModelLifecycle", "Picking sequence boundary appsCompleted=%d", GApplicationSequence);
		RunPickingExample(Toolbox::FPath(Args[1]), OutDir, ++GApplicationSequence);
		DXF_LOG_INFO("ModelLifecycle", "Renderer sequence boundary appsCompleted=%d", GApplicationSequence);
		Run_Internal(Toolbox::FPath(Args[1]), OutDir);
	}
	catch (const Toolbox::FException& Error)
	{
		printf("FAIL exception %s\n", Error.What());
		++GFailures;
	}
	printf("RESULT %s failures=%d\n", GFailures == 0 ? "REAL_SDK_MODEL_SMOKE_PASSED" : "REAL_SDK_MODEL_SMOKE_FAILED",
	       GFailures);
	return GFailures == 0 ? 0 : 1;
}
