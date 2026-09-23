// SPDX-License-Identifier: NOASSERTION
// 実SDKのモデル試験。フレームワークのAPIだけで.fbxを読み、Get3Dで描き、描画結果を読み戻して確かめる。
// 2体の独立したインスタンス・一時停止・速度・再読込・受付後の破棄・日本語パス・描画失敗・終了順序を扱う。
// 使い方: NativeModelSmoke <ProjectRoot> <画像の出力ディレクトリ>
#include "Dxf/AssetService.h"
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
#ifdef CopyFile
#undef CopyFile
#endif
#include <stdio.h>
namespace
{
using namespace Dxf;

constexpr Toolbox::int32 Width = 640;
constexpr Toolbox::int32 Height = 480;
constexpr FColor Background{12, 12, 12, 255};
Toolbox::int32 GFailures = 0;

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
			if (X == 20 && Y == 20)
			{
				Result.OverlayRgb = static_cast<Toolbox::uint32>((Red << 16) | (Green << 8) | Blue);
			}
			const int Distance = Toolbox::Abs(Red - Background.R) + Toolbox::Abs(Green - Background.G) +
			                     Toolbox::Abs(Blue - Background.B);
			const int Side = X < Width / 2 ? 0 : 1;
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

void Run_Internal(const Toolbox::FPath& Root, const Toolbox::FPath& OutDir)
{
	FSmoke Smoke(OutDir);
	FWindowSettings Settings;
	Settings.Title = "dxlib_framework - NativeModelSmoke";
	Settings.Width = Width;
	Settings.Height = Height;
	Settings.bVSync = false;
	RequireSuccess_Internal(Smoke.Session.Initialize(Settings));
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

	// 受付後にインスタンスを破棄しても（Sceneの退役に相当）、記録済みの描画は完了する。
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
	// 描画系を終えてからセッションを閉じる。
	Smoke.Session.Shutdown();
}
} // namespace

Toolbox::int32 main(Toolbox::int32 ArgCount, char** Args)
{
	if (ArgCount != 3)
	{
		Toolbox::Err << "Usage: NativeModelSmoke <ProjectRoot> <output directory>\n";
		return 2;
	}
	try
	{
		const Toolbox::FPath OutDir(Args[2]);
		Toolbox::CreateDirectory(OutDir);
		Run_Internal(Toolbox::FPath(Args[1]), OutDir);
		// 同じプロセスでGPU資源を作り直し、古いハンドルを再利用しない。
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
