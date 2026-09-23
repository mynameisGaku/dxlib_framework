// SPDX-License-Identifier: NOASSERTION
// DxLibのモデル境界の変換を、手書きのMV1関数で確かめる。実SDKの読込・描画は実SDK試験で確認する。
#include "Support/Test.h"
#include "Dxf/AssetService.h"
#include "Dxf/NativeBackends.h"
#include "Dxf/RenderSystem.h"
#include "Dxf/ModelImport.h"
#include "Toolbox/Platform.h"
#include "DxLib.h"
namespace
{
using namespace Dxf;

// ProjectRootをリポジトリのルートにしたAssetService一式。
struct FNativeModelFixture
{
	FDxLibBackends Backends;
	FBackendServices Services = Backends.GetServices();
	FAssetService Assets{Services.Textures, Services.Sounds, Services.Fonts, Services.pModels};
	FNativeModelFixture()
	{
		DxLib::Trace = {};
		DxLib::ViewTrace = {};
		DxLib::ModelTrace = {};
		REQUIRE(Services.pModels != nullptr);
		REQUIRE(Assets.SetProjectRoot(Toolbox::FPath(DXF_TEST_ASSET_DIR).Parent()));
	}
};

// 1フレームでインスタンスを1体描画する。
TResult<void> DrawOnce_Internal(FRenderSystem& Renderer, const FModelInstance& Instance)
{
	auto Begin = Renderer.BeginFrame(640, 480);
	if (!Begin)
	{
		return Begin;
	}
	auto Draw = Renderer.GetContext().Get3D().DrawModel(Instance);
	if (!Draw)
	{
		Renderer.CancelFrame();
		return Draw;
	}
	return Renderer.EndFrame();
}
} // namespace

TEST("Native model load passes converted data and serves textures from the model directory")
{
	FNativeModelFixture Fixture;
	DxLib::ModelTrace.RequestTexture = "dxf_texture_0.bmp";
	auto Model = Fixture.Assets.LoadModel("Assets/Models/SkinnedColumn.fbx");
	REQUIRE(Model);
	REQUIRE(DxLib::ModelTrace.Loads == 1 && DxLib::ModelTrace.LoadedBytes > 1000);
	// ModelChecker.bmp（64×64、24ビット）を相対パスから読んで渡す。
	REQUIRE(DxLib::ModelTrace.TextureReads == 1 && DxLib::ModelTrace.TextureBytes > 64 * 64 * 3);
	REQUIRE(Model.Value().GetClipCount() == 2 && Model.Value().GetClip(0)->NativeDuration == 30.0);
}

TEST("Native model load tolerates a missing texture and rejects a clip count mismatch")
{
	FNativeModelFixture Fixture;
	DxLib::ModelTrace.RequestTexture = "not_converted.png";
	DxLib::ModelTrace.AnimCount = 0;
	// テクスチャが見つからなくてもモデルは読み込み、警告だけを残す（DxLibは既定の画像で代用する）。
	REQUIRE(Fixture.Assets.LoadModel("Assets/Models/StaticBox.fbx"));
	REQUIRE(DxLib::ModelTrace.TextureReads == 0);
	DxLib::ModelTrace = {};
	DxLib::ModelTrace.AnimCount = 5;
	auto Mismatch = Fixture.Assets.LoadModel("Assets/Models/SkinnedColumn.fbx");
	REQUIRE(!Mismatch && Mismatch.Error().Code == EErrorCode::BackendFailure);
	REQUIRE(DxLib::ModelTrace.Deleted == 1);
	DxLib::ModelTrace = {};
	DxLib::ModelTrace.bFailLoad = true;
	REQUIRE(!Fixture.Assets.LoadModel("Assets/Models/SkinnedColumn.fbx"));
}

TEST("Native model draw attaches clips once, sets native time and transposes the matrix")
{
	FNativeModelFixture Fixture;
	FRenderSystem Renderer(Fixture.Services.Renderer);
	auto Model = Fixture.Assets.LoadModel("Assets/Models/SkinnedColumn.fbx");
	REQUIRE(Model);
	auto Created = Fixture.Assets.CreateModelInstance(Model.Value());
	REQUIRE(Created && DxLib::ModelTrace.Duplicates == 1);
	FModelInstance Instance = Toolbox::Move(Created).Value();
	REQUIRE(Instance.SetTransform(Toolbox::FMatrix4::Translation({1, 2, 3})));
	REQUIRE(Instance.Play("Bend") && Instance.Advance(0.5));
	REQUIRE(DrawOnce_Internal(Renderer, Instance));
	REQUIRE(DxLib::ModelTrace.Draws == 1 && DxLib::ModelTrace.Attaches == 1 && DxLib::ModelTrace.LastAttachedClip == 0);
	REQUIRE(DxLib::ModelTrace.LastTime > 15.0f - 1e-3f && DxLib::ModelTrace.LastTime < 15.0f + 1e-3f);
	// DxLibの行ベクトル形式では平行移動が最終行に入る。
	REQUIRE(DxLib::ModelTrace.LastMatrix.m[3][0] == 1 && DxLib::ModelTrace.LastMatrix.m[3][1] == 2 &&
	        DxLib::ModelTrace.LastMatrix.m[3][2] == 3);
	REQUIRE(DxLib::ModelTrace.LastMatrix.m[0][3] == 0 && DxLib::ModelTrace.LastMatrix.m[3][3] == 1);
	// 同じクリップでは付け直さない。
	REQUIRE(Instance.Advance(0.25) && DrawOnce_Internal(Renderer, Instance));
	REQUIRE(DxLib::ModelTrace.Attaches == 1 && DxLib::ModelTrace.Detaches == 0);
	// クリップの変更と停止は、前のクリップを外してから反映する。
	REQUIRE(Instance.Play("Twist") && DrawOnce_Internal(Renderer, Instance));
	REQUIRE(DxLib::ModelTrace.Detaches == 1 && DxLib::ModelTrace.Attaches == 2 &&
	        DxLib::ModelTrace.LastAttachedClip == 1);
	Instance.Stop();
	REQUIRE(DrawOnce_Internal(Renderer, Instance));
	REQUIRE(DxLib::ModelTrace.Detaches == 2 && DxLib::ModelTrace.Attaches == 2);
	// 照明は3Dビューの設定（無効）のまま、深度は検査・書込みの両方を有効にして描く。
	REQUIRE(DxLib::ModelTrace.LightingAtDraw == 0 && DxLib::ModelTrace.DepthAtDraw == 3);
}

TEST("Native model draw failure does not present and released instances are deleted once")
{
	FNativeModelFixture Fixture;
	FRenderSystem Renderer(Fixture.Services.Renderer);
	auto Model = Fixture.Assets.LoadModel("Assets/Models/SkinnedColumn.fbx");
	REQUIRE(Model);
	FModelInstance Instance = Toolbox::Move(Fixture.Assets.CreateModelInstance(Model.Value())).Value();
	const Toolbox::int32 Presented = DxLib::Trace.Presentations;
	DxLib::ModelTrace.bFailDraw = true;
	REQUIRE(!DrawOnce_Internal(Renderer, Instance));
	REQUIRE(DxLib::Trace.Presentations == Presented);
	Instance = FModelInstance();
	REQUIRE(DxLib::ModelTrace.Deleted == 1);
	Fixture.Assets.Shutdown();
	REQUIRE(DxLib::ModelTrace.Deleted == 2);
}

TEST("Native vertex colors configure all material meshes and reject lost frames")
{
	FNativeModelFixture Fixture;
	// 既存モデルに頂点色を持つフレームの記録を追加し、境界の設定と解放を検証する。
	Toolbox::TVector<Toolbox::uint8> Bytes;
	REQUIRE(Toolbox::ReadFileBytes(Toolbox::FPath(DXF_TEST_ASSET_DIR) / "Models/SkinnedColumn.fbx", Bytes,
	                               16u * 1024u * 1024u));
	auto Converted = ImportFbxModel(Bytes.Data(), Bytes.Size());
	REQUIRE(Converted);
	Converted.Value().VertexColorFrames.PushBack("Colored");
	auto Loaded = Fixture.Services.pModels->LoadModel(Converted.Value(), {});
	REQUIRE(Loaded);
	REQUIRE(DxLib::ModelTrace.ColorMeshes == 2);
	Fixture.Services.pModels->DeleteModel(Loaded.Value().NativeHandle);
	DxLib::ModelTrace.bFailColorFrame = true;
	REQUIRE(!Fixture.Services.pModels->LoadModel(Converted.Value(), {}));
	REQUIRE(DxLib::ModelTrace.Deleted == 2);
	DxLib::ModelTrace.bFailColorFrame = false;
	DxLib::ModelTrace.bFailColorSetup = true;
	REQUIRE(!Fixture.Services.pModels->LoadModel(Converted.Value(), {}));
	REQUIRE(DxLib::ModelTrace.Deleted == 3);
}

TEST("Native model material snapshots remain independent and lights restore")
{
	FNativeModelFixture Fixture;
	FRenderSystem Renderer(Fixture.Services.Renderer);
	auto Model = Fixture.Assets.LoadModel("Assets/Models/SkinnedColumn.fbx");
	REQUIRE(Model);
	auto Instance = Fixture.Assets.CreateModelInstance(Model.Value());
	REQUIRE(Instance);
	FModelMaterial3D Material;
	Material.bLit = true;
	Material.Tint = {255, 0, 0, 255};
	REQUIRE(Instance.Value().SetMaterial(Material));
	REQUIRE(Renderer.BeginFrame(640, 480));
	REQUIRE(Renderer.GetContext().Get3D().DrawModel(Instance.Value()));
	Material.bLit = false;
	Material.Tint = {0, 255, 0, 255};
	REQUIRE(Instance.Value().SetMaterial(Material));
	REQUIRE(Renderer.GetContext().Get3D().DrawModel(Instance.Value()));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(DxLib::ModelTrace.DrawLighting[0] == 1);
	REQUIRE(DxLib::ModelTrace.DrawLighting[1] == 0);
	REQUIRE(DxLib::ModelTrace.DrawTints[0].r == 1 && DxLib::ModelTrace.DrawTints[0].g == 0);
	REQUIRE(DxLib::ModelTrace.DrawTints[1].r == 0 && DxLib::ModelTrace.DrawTints[1].g == 1);
	REQUIRE(DxLib::ModelTrace.DrawForeignLights[0] == 0);
	REQUIRE(DxLib::ModelTrace.CreatedLights == 1 && DxLib::ModelTrace.DeletedLights == 1);
	REQUIRE(DxLib::ModelTrace.DefaultLight == 1 && DxLib::ModelTrace.ExternalLight == 1);
	REQUIRE(DxLib::ViewTrace.Lighting == 1 && DxLib::ViewTrace.Z3D == 0);
	// 拒否された半透明の設定で以前の材質を書き換えない。
	Material.Tint.A = 128;
	REQUIRE(!Instance.Value().SetMaterial(Material));
	REQUIRE(Instance.Value().GetMaterial().Tint.A == 255);
}

TEST("Native model light failure releases owned light and restores external lights")
{
	for (Toolbox::int32 Failure = 0; Failure < 3; ++Failure)
	{
		FNativeModelFixture Fixture;
		FRenderSystem Renderer(Fixture.Services.Renderer);
		auto Model = Fixture.Assets.LoadModel("Assets/Models/SkinnedColumn.fbx");
		REQUIRE(Model);
		auto Instance = Fixture.Assets.CreateModelInstance(Model.Value());
		REQUIRE(Instance);
		FModelMaterial3D Material;
		Material.bLit = true;
		REQUIRE(Instance.Value().SetMaterial(Material));
		DxLib::ModelTrace.bFailLightCreate = Failure == 0;
		DxLib::ModelTrace.bFailLightSetup = Failure == 1;
		DxLib::ModelTrace.bFailDraw = Failure == 2;
		REQUIRE(!DrawOnce_Internal(Renderer, Instance.Value()));
		REQUIRE(DxLib::ModelTrace.DefaultLight == 1 && DxLib::ModelTrace.ExternalLight == 1);
		REQUIRE(DxLib::ModelTrace.OwnedLight == 0);
		REQUIRE(DxLib::ModelTrace.DeletedLights == (Failure == 0 ? 0 : 1));
		REQUIRE(DxLib::ViewTrace.Lighting == 1 && DxLib::ViewTrace.Z3D == 0);
	}
}

TEST("Native old model backend rejects extended attributes without silently dropping them")
{
	FNativeModelFixture Fixture;
	Toolbox::TVector<Toolbox::uint8> Bytes;
	REQUIRE(Toolbox::ReadFileBytes(Toolbox::FPath(DXF_TEST_ASSET_DIR).Parent() / "Tests/Assets/AdditionalUv.fbx", Bytes,
	                               100000));
	auto Model = ImportFbxModel(Bytes.Data(), Bytes.Size());
	REQUIRE(Model);
	auto Result = Fixture.Services.pModels->LoadModel(Model.Value(), {});
	REQUIRE(!Result);
	REQUIRE(Result.Error().Code == EErrorCode::BackendFailure);
	REQUIRE(DxLib::ModelTrace.Loads == 0);
}
