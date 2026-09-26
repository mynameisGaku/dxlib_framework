// SPDX-License-Identifier: NOASSERTION
// 表示先ごとの実画素の検査。期待値は定数と投影から独立に求め、許容差は検査の前に固定する（緩めない）。
#include "DisplayScenario.h"
#include "DisplayScene.h"
#include "FixedInput.h"
#include "ForwardRenderer.h"
#include "ScreenCapture.h"
#include "Dxf/Application.h"
#include "Dxf/NativeBackends.h"
#include "Dxf/ViewCoordinates.h"
#include "DxLib.h"
namespace Dxf::UiSmoke
{
namespace
{
// 2DのUIの不透明な色（テクスチャを経ない）は完全一致。
constexpr Toolbox::int32 ExactTolerance = 0;
// 3Dのパネルの不透明な画素（中間画像のサンプリング）。
constexpr Toolbox::int32 SampledTolerance = 2;
// 一層の半透明の合成 C = a*F + (1-a)*B（8bitの丸めを二回まで）。
constexpr Toolbox::int32 BlendTolerance = 3;
// 二層の半透明の合成（丸めを四回まで）。
constexpr Toolbox::int32 LayerTolerance = 4;
// 文字の縁が背景より暗くならないことの許容差（黒い縁の検出）。
constexpr Toolbox::int32 EdgeTolerance = 3;

void Require_Internal(bool Value, const char* Error)
{
	if (!Value)
	{
		throw Toolbox::FException(Error);
	}
}

// 赤・緑・青が同じか。
bool SameRgb_Internal(FColor A, FColor B)
{
	return A.R == B.R && A.G == B.G && A.B == B.B;
}

// 期待値を併記した失敗。
void RequireColor_Internal(FColor Actual, Toolbox::f64 R, Toolbox::f64 G, Toolbox::f64 B, Toolbox::int32 Tolerance,
                           const char* Name)
{
	const Toolbox::f64 Expected[3] = {R, G, B};
	const Toolbox::int32 Values[3] = {Actual.R, Actual.G, Actual.B};
	for (Toolbox::size_t I = 0; I < 3; ++I)
	{
		if (Toolbox::Abs(static_cast<Toolbox::f64>(Values[I]) - Expected[I]) > Tolerance + 0.5)
		{
			throw Toolbox::FException(Toolbox::FString(Name) + ": actual (" + Toolbox::ToString(Actual.R) + "," +
			                          Toolbox::ToString(Actual.G) + "," + Toolbox::ToString(Actual.B) + ") expected (" +
			                          Toolbox::ToString(R) + "," + Toolbox::ToString(G) + "," + Toolbox::ToString(B) +
			                          ") tolerance " + Toolbox::ToString(Tolerance));
		}
	}
}

void RequireColor_Internal(FColor Actual, FColor Expected, Toolbox::int32 Tolerance, const char* Name)
{
	RequireColor_Internal(Actual, Expected.R, Expected.G, Expected.B, Tolerance, Name);
}

// 乗算前の色Fと不透明度Fの.Aを、背景Bへ C = a*F + (1-a)*B で重ねた値（実数）。
struct FLinearColor
{
	Toolbox::f64 R = 0;
	Toolbox::f64 G = 0;
	Toolbox::f64 B = 0;
};

FLinearColor Over_Internal(FColor Front, FLinearColor Back)
{
	const Toolbox::f64 Alpha = Front.A / 255.0;
	return {Alpha * Front.R + (1 - Alpha) * Back.R, Alpha * Front.G + (1 - Alpha) * Back.G,
	        Alpha * Front.B + (1 - Alpha) * Back.B};
}

FLinearColor Linear_Internal(FColor Color)
{
	return {static_cast<Toolbox::f64>(Color.R), static_cast<Toolbox::f64>(Color.G), static_cast<Toolbox::f64>(Color.B)};
}

// 世界の点が映る画素。
struct FPixel
{
	Toolbox::int32 X = 0;
	Toolbox::int32 Y = 0;
};

FPixel Project_Internal(Toolbox::FVector3 World)
{
	auto Projected = ProjectWorldToScreen(DDisplayScene::MakeView(), 1280, 720, World);
	Require_Internal(static_cast<bool>(Projected) && Projected.Value().bInsideView, "display projection");
	return {static_cast<Toolbox::int32>(Toolbox::Floor(Projected.Value().Screen.X)),
	        static_cast<Toolbox::int32>(Toolbox::Floor(Projected.Value().Screen.Y))};
}

// 奥のパネルの平面(z=0)上の点に対応する、手前のパネルの平面(z=-2)上の点（視点を通る同じ視線）。
Toolbox::FVector3 NearPointFor_Internal(Toolbox::f32 FarX, Toolbox::f32 FarY)
{
	const Toolbox::FVector3 Eye = DDisplayScene::MakeView().Eye;
	const Toolbox::FVector3 Far{FarX, FarY, 0};
	return Eye + (Far - Eye) * 0.8f;
}

void Step_Internal(FApplication& App, Toolbox::uint64& Frame)
{
	const auto Result = App.Step(static_cast<Toolbox::f64>(Frame++) / 60.0);
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
	Require_Internal(Result.Value(), "display scene window ended before scenario completed");
}

void Click_Internal(FApplication& App, FFixedInput& Input, Toolbox::uint64& Frame, FPixel Pixel)
{
	Input.Raw.MouseX = Pixel.X;
	Input.Raw.MouseY = Pixel.Y;
	Step_Internal(App, Frame);
	Input.Raw.MouseButtons[0] = true;
	Step_Internal(App, Frame);
	Input.Raw.MouseButtons[0] = false;
	Step_Internal(App, Frame);
}

// 2DのUI（全画面・Viewport・2Dワールド）の画素。
void VerifyScreenDisplays_Internal(const FScreenCapture& Actual)
{
	using S = DDisplayScene;
	// 左右の境界（左は幅641、右は幅639）。
	RequireColor_Internal(Actual.Pixel(640, 640), S::LeftColor, ExactTolerance, "left viewport last column");
	RequireColor_Internal(Actual.Pixel(641, 640), S::RightColor, ExactTolerance, "right viewport first column");
	RequireColor_Internal(Actual.Pixel(5, 715), S::LeftColor, ExactTolerance, "left viewport corner");
	RequireColor_Internal(Actual.Pixel(1275, 715), S::RightColor, ExactTolerance, "right viewport corner");
	RequireColor_Internal(Actual.Pixel(320, 555), S::ClearColor, ExactTolerance, "above viewport");
	// 論理(10,10)〜(30,30)の目印。左は倍率1、右は倍率2。
	RequireColor_Internal(Actual.Pixel(20, 580), S::MarkerColor, ExactTolerance, "left marker");
	RequireColor_Internal(Actual.Pixel(29, 589), S::MarkerColor, ExactTolerance, "left marker last pixel");
	RequireColor_Internal(Actual.Pixel(31, 580), S::LeftColor, ExactTolerance, "left marker right edge");
	RequireColor_Internal(Actual.Pixel(20, 591), S::LeftColor, ExactTolerance, "left marker bottom edge");
	RequireColor_Internal(Actual.Pixel(662, 581), S::MarkerColor, ExactTolerance, "right marker first pixel");
	RequireColor_Internal(Actual.Pixel(700, 619), S::MarkerColor, ExactTolerance, "right marker last pixel");
	RequireColor_Internal(Actual.Pixel(659, 600), S::RightColor, ExactTolerance, "right marker left edge");
	RequireColor_Internal(Actual.Pixel(703, 600), S::RightColor, ExactTolerance, "right marker right edge");
	// 全画面は3D・Viewportの後に重なる。
	RequireColor_Internal(Actual.Pixel(1220, 40), S::OverlayColor, ExactTolerance, "overlay corner");
	RequireColor_Internal(Actual.Pixel(620, 695), S::OverlayColor, ExactTolerance, "overlay over left viewport");
	RequireColor_Internal(Actual.Pixel(680, 695), S::OverlayColor, ExactTolerance, "overlay over right viewport");
	RequireColor_Internal(Actual.Pixel(220, 270), S::OverlayColor, ExactTolerance, "overlay over 3D panel");
	// 2Dワールドのパネルは(740,20)〜(900,80)。
	RequireColor_Internal(Actual.Pixel(741, 21), S::Panel2DColor, ExactTolerance, "2D panel first pixel");
	RequireColor_Internal(Actual.Pixel(899, 79), S::Panel2DColor, ExactTolerance, "2D panel last pixel");
	RequireColor_Internal(Actual.Pixel(737, 50), S::ClearColor, ExactTolerance, "2D panel left outside");
	RequireColor_Internal(Actual.Pixel(820, 83), S::ClearColor, ExactTolerance, "2D panel bottom outside");
}

// 実画像の画素。元画像をCPUで読み、3x3が同じ色の点だけを2倍の位置で比べる（拡大の補間方式に依らない）。
void VerifyImage_Internal(const FScreenCapture& Actual, const char* ProjectRoot)
{
	using S = DDisplayScene;
	const Toolbox::FPath Path = Toolbox::FPath(ProjectRoot) / S::ImagePath;
	const Toolbox::int32 Source = DxLib::LoadSoftImage(Path.ToUtf8().CStr());
	Require_Internal(Source >= 0, "source image load");
	int Width = 0;
	int Height = 0;
	Require_Internal(DxLib::GetSoftImageSize(Source, &Width, &Height) == 0 && Width == 64 && Height == 64,
	                 "source image size");
	auto SourcePixel = [&](Toolbox::int32 X, Toolbox::int32 Y)
	{
		int R = 0;
		int G = 0;
		int B = 0;
		int A = 0;
		(void)DxLib::GetPixelSoftImage(Source, X, Y, &R, &G, &B, &A);
		return FColor{static_cast<Toolbox::uint8>(R), static_cast<Toolbox::uint8>(G), static_cast<Toolbox::uint8>(B),
		              255};
	};
	Toolbox::int32 Checked = 0;
	Toolbox::int32 Distinct = 0;
	FColor Previous{0, 0, 0, 0};
	for (Toolbox::int32 Y = 2; Y < 62; Y += 5)
	{
		for (Toolbox::int32 X = 2; X < 62; X += 5)
		{
			const FColor Center = SourcePixel(X, Y);
			bool bUniform = true;
			for (Toolbox::int32 DY = -1; DY <= 1 && bUniform; ++DY)
			{
				for (Toolbox::int32 DX = -1; DX <= 1 && bUniform; ++DX)
				{
					bUniform = SameRgb_Internal(SourcePixel(X + DX, Y + DY), Center);
				}
			}
			if (!bUniform)
			{
				continue;
			}
			// DxLibの既定の透過色（黒）の画素は描かれず、背景が見える。
			const bool bKey = SameRgb_Internal(Center, FColor{0, 0, 0, 255});
			RequireColor_Internal(Actual.Pixel(S::ImageRect.Left + X * 2 + 1, S::ImageRect.Top + Y * 2 + 1),
			                      bKey ? S::ClearColor : Center, SampledTolerance, "stretched real image pixel");
			++Checked;
			if (!SameRgb_Internal(Center, Previous))
			{
				++Distinct;
			}
			Previous = Center;
		}
	}
	(void)DxLib::DeleteSoftImage(Source);
	Require_Internal(Checked >= 20 && Distinct >= 3, "real image samples cover several colors");
}

// 3Dのパネルの画素。Referenceはパネルを描かないフレーム。
void VerifyPanels3D_Internal(const FScreenCapture& Actual, const FScreenCapture& Reference)
{
	using S = DDisplayScene;
	// 不透明なパネルと、手前の箱による隠れ。
	const FPixel Opaque = Project_Internal({-7, 2.5f, 0});
	RequireColor_Internal(Actual.Pixel(Opaque.X, Opaque.Y), S::OpaqueColor, SampledTolerance, "opaque 3D panel");
	const FPixel Front = Project_Internal({-5, 0.5f, -2.6f});
	RequireColor_Internal(Actual.Pixel(Front.X, Front.Y), Reference.Pixel(Front.X, Front.Y), ExactTolerance,
	                      "front box hides opaque panel");
	Require_Internal(!SameRgb_Internal(Reference.Pixel(Front.X, Front.Y), S::OpaqueColor),
	                 "front box reference differs from panel");
	// 透明（α=0）の画素は背景と完全に同じで、後に描いた箱を隠さない。
	const FPixel Late = Project_Internal({1.25f, 2.5f, 1.7f});
	RequireColor_Internal(Actual.Pixel(Late.X, Late.Y), Reference.Pixel(Late.X, Late.Y), ExactTolerance,
	                      "transparent pixel keeps later box visible");
	Require_Internal(!SameRgb_Internal(Reference.Pixel(Late.X, Late.Y), S::ClearColor), "later box is drawn");
	for (const Toolbox::FVector3 Point :
	     Toolbox::TArray<Toolbox::FVector3, 2>{Toolbox::FVector3{0.2f, 2.5f, 0}, Toolbox::FVector3{3.6f, 2.8f, 0}})
	{
		const FPixel Pixel = Project_Internal(Point);
		RequireColor_Internal(Actual.Pixel(Pixel.X, Pixel.Y), Reference.Pixel(Pixel.X, Pixel.Y), ExactTolerance,
		                      "transparent pixel equals background");
	}
	// 一層の半透明を、色の異なる三つの背景で確かめる。
	Toolbox::int32 DistinctBackgrounds = 0;
	FColor Previous{0, 0, 0, 0};
	for (const Toolbox::f32 X : Toolbox::TArray<Toolbox::f32, 3>{1.0f, 3.2f, 4.6f})
	{
		const FPixel Pixel = Project_Internal({X, 1.6f, 0});
		const FColor Back = Reference.Pixel(Pixel.X, Pixel.Y);
		const FLinearColor Expected = Over_Internal(S::FarSemiColor, Linear_Internal(Back));
		RequireColor_Internal(Actual.Pixel(Pixel.X, Pixel.Y), Expected.R, Expected.G, Expected.B, BlendTolerance,
		                      "semi-transparent panel over background");
		if (!SameRgb_Internal(Back, Previous))
		{
			++DistinctBackgrounds;
		}
		Previous = Back;
	}
	Require_Internal(DistinctBackgrounds == 3, "semi-transparent checks cover three backgrounds");
	// 透明な中間画像の不透明な画素。
	const FPixel Solid = Project_Internal({1.0f, 0.25f, 0});
	RequireColor_Internal(Actual.Pixel(Solid.X, Solid.Y), S::FarOpaqueColor, SampledTolerance,
	                      "opaque pixel in transparent panel");
	// 二層：手前のパネルは奥のパネルの後に描かれる（登録は逆順）。
	{
		const FPixel Pixel = Project_Internal(NearPointFor_Internal(6.0f, 1.0f));
		const FLinearColor Far = Over_Internal(S::FarSemiColor, Linear_Internal(Reference.Pixel(Pixel.X, Pixel.Y)));
		const FLinearColor Expected = Over_Internal(S::NearColor, Far);
		RequireColor_Internal(Actual.Pixel(Pixel.X, Pixel.Y), Expected.R, Expected.G, Expected.B, LayerTolerance,
		                      "near panel over far semi-transparent panel");
	}
	{
		const FPixel Pixel = Project_Internal(NearPointFor_Internal(6.5f, 0.25f));
		const FLinearColor Expected = Over_Internal(S::NearColor, Linear_Internal(S::FarOpaqueColor));
		RequireColor_Internal(Actual.Pixel(Pixel.X, Pixel.Y), Expected.R, Expected.G, Expected.B, LayerTolerance,
		                      "near panel over far opaque pixel");
	}
	{
		const FPixel Pixel = Project_Internal({6.5f, -1.2f, -2});
		const FLinearColor Expected = Over_Internal(S::NearColor, Linear_Internal(Reference.Pixel(Pixel.X, Pixel.Y)));
		RequireColor_Internal(Actual.Pixel(Pixel.X, Pixel.Y), Expected.R, Expected.G, Expected.B, BlendTolerance,
		                      "near panel alone");
	}
	// 文字の縁：透明な行の白い文字は、どの画素も背景より暗くならない（黒い縁・二重の不透明度がない）。
	const FPixel TopLeft = Project_Internal({272.0f / 64.0f, 3.0f - 6.0f / 64.0f, 0});
	const FPixel BottomRight = Project_Internal({502.0f / 64.0f, 3.0f - 58.0f / 64.0f, 0});
	Toolbox::int32 TextPixels = 0;
	for (Toolbox::int32 Y = TopLeft.Y + 1; Y < BottomRight.Y; ++Y)
	{
		for (Toolbox::int32 X = TopLeft.X + 1; X < BottomRight.X; ++X)
		{
			const FColor C = Actual.Pixel(X, Y);
			const FColor B = Reference.Pixel(X, Y);
			if (C.R + EdgeTolerance < B.R || C.G + EdgeTolerance < B.G || C.B + EdgeTolerance < B.B)
			{
				throw Toolbox::FException(Toolbox::FString("text edge darker than background at ") +
				                          Toolbox::ToString(X) + "," + Toolbox::ToString(Y));
			}
			if (C.R > B.R + 40 && C.G > B.G + 40 && C.B > B.B + 40)
			{
				++TextPixels;
			}
		}
	}
	Require_Internal(TextPixels > 20, "transparent panel text pixels");
}
} // namespace

void RunDisplayScenario(const char* ProjectRoot, const Toolbox::FPath& Out)
{
	FDxLibBackends Native;
	const auto Original = Native.GetServices();
	FFixedInput Input;
	FForwardRenderer Render(Original.Renderer);
	FBackendServices Services{Original.Platform, Input,  Original.Textures, Original.Sounds,
	                          Original.Fonts,    Render, Original.pModels};
	FApplicationSettings Settings;
	Settings.Window.Width = 1280;
	Settings.Window.Height = 720;
	Settings.Window.bVSync = false;
	Settings.ExecutionThreadCount = 1;
	Settings.ProjectRoot = ProjectRoot;
	Settings.ClearColor = DDisplayScene::ClearColor;
	FApplication App(Services, Settings);
	auto Owned = Toolbox::MakeUnique<DDisplayScene>();
	DDisplayScene* Scene = Owned.Get();
	Require_Internal(static_cast<bool>(App.Start(Toolbox::Move(Owned))), "display scene start");
	Toolbox::uint64 Frame = 0;
	Step_Internal(App, Frame);
	// 基準（3Dのパネルなし）と実際のフレームを、同じ内容の連続したフレームで読む。
	FScreenCapture Reference;
	FScreenCapture Actual;
	Scene->bDrawPanels3D = false;
	Render.BeforePresent = [&]()
	{
		Reference.Capture();
	};
	Step_Internal(App, Frame);
	Scene->bDrawPanels3D = true;
	Render.BeforePresent = [&]()
	{
		Actual.Capture();
	};
	Step_Internal(App, Frame);
	Render.BeforePresent = {};
	Reference.Save(Out / "ui-display-reference.png");
	Actual.Save(Out / "ui-display-actual.png");
	VerifyScreenDisplays_Internal(Actual);
	VerifyImage_Internal(Actual, ProjectRoot);
	VerifyPanels3D_Internal(Actual, Reference);
	// 既知の画素のクリックは、その表示先のルートだけへ届く。
	struct FClickCase
	{
		FPixel Pixel;
		EDisplayButton Button;
	};
	const Toolbox::TArray<FClickCase, 5> Clicks{
	    FClickCase{{320, 640}, EDisplayButton::Left}, FClickCase{{960, 640}, EDisplayButton::Right},
	    FClickCase{{640, 600}, EDisplayButton::Left}, FClickCase{{820, 50}, EDisplayButton::Panel2D},
	    FClickCase{Project_Internal({-7.5f, 0.5f, 0}), EDisplayButton::Panel3D}};
	Toolbox::int32 Expected[static_cast<Toolbox::size_t>(EDisplayButton::Count)] = {};
	for (const FClickCase& Case : Clicks)
	{
		Click_Internal(App, Input, Frame, Case.Pixel);
		++Expected[static_cast<Toolbox::size_t>(Case.Button)];
		for (Toolbox::size_t I = 0; I < static_cast<Toolbox::size_t>(EDisplayButton::Count); ++I)
		{
			Require_Internal(Scene->GetClicks(static_cast<EDisplayButton>(I)) == Expected[I], "display click routing");
		}
	}
	App.Shutdown();
}
} // namespace Dxf::UiSmoke
