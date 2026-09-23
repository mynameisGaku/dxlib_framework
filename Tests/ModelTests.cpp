// SPDX-License-Identifier: NOASSERTION
// モデルの変換（実際の.fbx）、読み込み・共有・解放順序、秒単位の再生、Get3Dの描画命令を検証する。
// ネイティブ境界だけをテスト実装に置き換え、DxLibとFBX SDKは使わない。
#include "Support/FakeBackend.h"
#include "Support/Test.h"
#include "Support/TestFs.h"
#include "Dxf/AssetService.h"
#include "Dxf/ModelImport.h"
#include "Dxf/RenderSystem.h"
#include "Toolbox/Platform.h"
#include "Toolbox/Thread.h"
#include <string.h>
namespace
{
using namespace Dxf;
using Dxf::Testing::FFakeBackend;
using Toolbox::size_t;

// 変換結果のテキストに部分文字列が含まれる回数。
size_t Count_Internal(const Toolbox::TVector<char>& Text, const char* Needle)
{
	const size_t Length = strlen(Needle);
	size_t Count = 0;
	for (size_t Index = 0; Index + Length <= Text.Size(); ++Index)
	{
		if (memcmp(Text.Data() + Index, Needle, Length) == 0)
		{
			++Count;
		}
	}
	return Count;
}

// リポジトリのAssets/Models内のファイルを読む。
Toolbox::TVector<Toolbox::uint8> ReadModel_Internal(const char* Name)
{
	Toolbox::TVector<Toolbox::uint8> Bytes;
	REQUIRE(Toolbox::ReadFileBytes(Toolbox::FPath(DXF_TEST_ASSET_DIR) / "Models" / Name, Bytes, 64u * 1024u * 1024u));
	return Bytes;
}

// ネイティブモデルの確保・複製・解放を記録する。
class FFakeModelBackend final : public IModelBackend
{
public:
	Toolbox::int32 m_NextHandle = 100;
	Toolbox::uint32 m_Loads = 0;
	Toolbox::uint32 m_Duplicates = 0;
	Toolbox::TVector<Toolbox::int32> m_Deleted;
	Toolbox::TVector<Toolbox::uint64> m_DeleteThreads;
	Toolbox::FString m_LastDirectory;
	size_t m_LastTextures = 0;
	bool m_bFailLoad = false;
	TResult<FModelAllocation> LoadModel(const FImportedModel& Model, const Toolbox::FString& Directory) override
	{
		++m_Loads;
		m_LastDirectory = Directory;
		m_LastTextures = Model.Textures.Size();
		if (m_bFailLoad)
		{
			return TResult<FModelAllocation>::Failure(EErrorCode::BackendFailure, "fake load failure");
		}
		FModelAllocation Allocation;
		Allocation.NativeHandle = m_NextHandle++;
		// ネイティブ側は1秒を30単位で数えるものとする（DxLibの実測値と同じ）。
		for (const auto& Clip : Model.Clips)
		{
			Allocation.NativeClipDurations.PushBack(Clip.DurationSeconds * 30.0);
		}
		return TResult<FModelAllocation>::Success(Toolbox::Move(Allocation));
	}
	TResult<Toolbox::int32> DuplicateModel(Toolbox::int32) override
	{
		++m_Duplicates;
		return TResult<Toolbox::int32>::Success(m_NextHandle++);
	}
	void DeleteModel(Toolbox::int32 Handle) noexcept override
	{
		m_Deleted.PushBack(Handle);
		m_DeleteThreads.PushBack(Toolbox::FThread::CurrentThreadId());
	}
	bool WasDeleted(Toolbox::int32 Handle) const
	{
		for (const Toolbox::int32 Deleted : m_Deleted)
		{
			if (Deleted == Handle)
			{
				return true;
			}
		}
		return false;
	}
};

// 描画順序とモデル描画時の状態を記録する。
class FModelRenderBackend final : public IRenderBackend
{
public:
	struct FDrawn
	{
		Toolbox::int32 Handle = -1;
		Toolbox::int32 Clip = -1;
		Toolbox::f32 NativeTime = 0.0f;
		Toolbox::f32 X = 0.0f;
	};
	Toolbox::TVector<Toolbox::int32> m_Order;
	Toolbox::TVector<FDrawn> m_Models;
	Toolbox::uint32 m_Ends = 0;
	Toolbox::uint32 m_Presentations = 0;
	bool m_bModels = true;
	bool m_bFailModel = false;
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
		m_Order.PushBack(3);
		return {};
	}
	bool SupportsGeometry3D() const noexcept override
	{
		return true;
	}
	bool SupportsModels3D() const noexcept override
	{
		return m_bModels;
	}
	TResult<void> BeginView3D(const FRenderView3D&) override
	{
		m_Order.PushBack(10);
		return {};
	}
	TResult<void> DrawGeometry3D(const FPreparedGeometry3D&) override
	{
		m_Order.PushBack(30);
		return {};
	}
	TResult<void> DrawModel3D(const FModelDraw3D& Model) override
	{
		m_Order.PushBack(40);
		m_Models.PushBack({Model.pInstance != nullptr ? Model.pInstance->GetHandle_Internal() : -1, Model.Clip,
		                   Model.NativeTime, Model.World.Values[3]});
		return m_bFailModel ? TResult<void>::Failure(EErrorCode::BackendFailure, "model failure") : TResult<void>{};
	}
	TResult<void> EndView3D() override
	{
		++m_Ends;
		m_Order.PushBack(11);
		return {};
	}
	TResult<void> Present() override
	{
		++m_Presentations;
		return {};
	}
};

// テスト用のAssetService一式。ProjectRootはリポジトリのルート。
struct FModelAssets
{
	FFakeBackend Backend;
	FFakeModelBackend Models;
	FAssetService Assets{Backend, Backend, Backend, &Models};
	FModelAssets()
	{
		REQUIRE(Assets.SetProjectRoot(Toolbox::FPath(DXF_TEST_ASSET_DIR).Parent()));
	}
};

FModel LoadColumn_Internal(FAssetService& Assets)
{
	auto Model = Assets.LoadModel("Assets/Models/SkinnedColumn.fbx");
	REQUIRE(Model);
	return Model.Value();
}

FModelInstance Instance_Internal(FAssetService& Assets, const FModel& Model)
{
	auto Instance = Assets.CreateModelInstance(Model);
	REQUIRE(Instance);
	return Toolbox::Move(Instance).Value();
}

void DestroyOnThread_Internal(void* Context)
{
	auto* Instance = static_cast<FModelInstance*>(Context);
	*Instance = FModelInstance();
}
} // namespace

TEST("model_import converts the skinned column with bones, clips and a texture reference")
{
	const auto Bytes = ReadModel_Internal("SkinnedColumn.fbx");
	auto Imported = ImportFbxModel(Bytes.Data(), Bytes.Size());
	REQUIRE(Imported);
	const FImportedModel& Model = Imported.Value();
	REQUIRE(Model.ModelData.Size() > 16 && memcmp(Model.ModelData.Data(), "xof 0303txt 0032", 16) == 0);
	REQUIRE(Model.ModelData[Model.ModelData.Size() - 1] == '\0');
	REQUIRE(Count_Internal(Model.ModelData, "Frame Root {") == 1);
	REQUIRE(Count_Internal(Model.ModelData, "Frame Bone1 {") == 1);
	REQUIRE(Count_Internal(Model.ModelData, "SkinWeights {") == 2);
	REQUIRE(Count_Internal(Model.ModelData, "AnimationSet clip_") == 2);
	REQUIRE(Model.Clips.Size() == 2);
	REQUIRE(Model.Clips[0].Name == Toolbox::FString("Bend") && Model.Clips[1].Name == Toolbox::FString("Twist"));
	REQUIRE(Model.Clips[0].DurationSeconds > 0.99 && Model.Clips[0].DurationSeconds < 1.01);
	REQUIRE(Model.SamplesPerSecond == 30);
	REQUIRE(Model.Textures.Size() == 1);
	REQUIRE(Model.Textures[0].RelativePath == Toolbox::FString("ModelChecker.bmp"));
	REQUIRE(Model.Textures[0].Embedded.IsEmpty());
	// 参照名は.xの文字列に書けるASCIIだけで作る。
	REQUIRE(Model.Textures[0].Name == Toolbox::FString("dxf_texture_0.bmp"));
}

TEST("model_import keeps file units unless a target unit is given")
{
	const auto Bytes = ReadModel_Internal("StaticBox.fbx");
	auto FileUnits = ImportFbxModel(Bytes.Data(), Bytes.Size());
	FModelImportOptions Centimeters;
	Centimeters.TargetUnitMeters = 0.01;
	auto SameUnits = ImportFbxModel(Bytes.Data(), Bytes.Size(), Centimeters);
	FModelImportOptions Meters;
	Meters.TargetUnitMeters = 1.0;
	auto Converted = ImportFbxModel(Bytes.Data(), Bytes.Size(), Meters);
	REQUIRE(FileUnits && SameUnits && Converted);
	REQUIRE(FileUnits.Value().Clips.IsEmpty());
	// 試験モデルはセンチメートルで書かれている。同じ単位への変換は結果を変えず、メートルへの変換は変える。
	REQUIRE(FileUnits.Value().ModelData.Size() == SameUnits.Value().ModelData.Size());
	REQUIRE(memcmp(FileUnits.Value().ModelData.Data(), SameUnits.Value().ModelData.Data(),
	               FileUnits.Value().ModelData.Size()) == 0);
	REQUIRE(FileUnits.Value().ModelData.Size() != Converted.Value().ModelData.Size() ||
	        memcmp(FileUnits.Value().ModelData.Data(), Converted.Value().ModelData.Data(),
	               FileUnits.Value().ModelData.Size()) != 0);
}

TEST("model_import sample rate controls the animation keys")
{
	const auto Bytes = ReadModel_Internal("SkinnedColumn.fbx");
	FModelImportOptions Options;
	Options.SamplesPerSecond = 60;
	auto Imported = ImportFbxModel(Bytes.Data(), Bytes.Size(), Options);
	REQUIRE(Imported);
	REQUIRE(Imported.Value().SamplesPerSecond == 60);
	REQUIRE(Count_Internal(Imported.Value().ModelData, "AnimTicksPerSecond {\n60;") == 1);
	// 1秒のクリップは0～60の61キー。
	REQUIRE(Count_Internal(Imported.Value().ModelData, "\n61;\n") > 0);
}

TEST("model_import rejects empty, corrupt data and invalid options")
{
	REQUIRE(!ImportFbxModel(nullptr, 0));
	const char Corrupt[] = "Kaydara FBX Binary  \0\x1a\0 broken payload";
	auto Broken = ImportFbxModel(Corrupt, sizeof(Corrupt));
	REQUIRE(!Broken && Broken.Error().Code == EErrorCode::InvalidArgument);
	const auto Bytes = ReadModel_Internal("StaticBox.fbx");
	FModelImportOptions Options;
	Options.SamplesPerSecond = 0;
	REQUIRE(!ImportFbxModel(Bytes.Data(), Bytes.Size(), Options));
	Options.SamplesPerSecond = 30;
	Options.TargetUnitMeters = -1.0;
	REQUIRE(!ImportFbxModel(Bytes.Data(), Bytes.Size(), Options));
}

TEST("model_assets load through the project root and share live data")
{
	FModelAssets Fixture;
	auto First = Fixture.Assets.LoadModel("Assets/Models/SkinnedColumn.fbx");
	auto Second = Fixture.Assets.LoadModel("Assets/Models/./SkinnedColumn.fbx");
	REQUIRE(First && Second);
	REQUIRE(First.Value().GetResource_Internal().Get() == Second.Value().GetResource_Internal().Get());
	REQUIRE(Fixture.Models.m_Loads == 1);
	REQUIRE(Fixture.Models.m_LastTextures == 1);
	REQUIRE(Fixture.Models.m_LastDirectory.Size() >= 6);
	REQUIRE(Toolbox::FString(Fixture.Models.m_LastDirectory.CStr() + Fixture.Models.m_LastDirectory.Size() - 6) ==
	        Toolbox::FString("Models"));
	REQUIRE(First.Value().GetClipCount() == 2 && First.Value().FindClip("Twist") == 1 &&
	        First.Value().FindClip("None") == -1);
	FModelLoadOptions Other;
	Other.SamplesPerSecond = 60;
	REQUIRE(Fixture.Assets.LoadModel("Assets/Models/SkinnedColumn.fbx", Other));
	REQUIRE(Fixture.Models.m_Loads == 2);
	auto Missing = Fixture.Assets.LoadModel("Assets/Models/DoesNotExist.fbx");
	REQUIRE(!Missing && Missing.Error().Code == EErrorCode::NotFound);
	REQUIRE(!Fixture.Assets.LoadModel(""));
}

TEST("model_reload after every reference is released loads the file again")
{
	FModelAssets Fixture;
	Toolbox::int32 FirstHandle = -1;
	{
		FModel Model = LoadColumn_Internal(Fixture.Assets);
		FirstHandle = Model.GetResource_Internal()->GetHandle_Internal();
	}
	REQUIRE(Fixture.Models.WasDeleted(FirstHandle));
	FModel Reloaded = LoadColumn_Internal(Fixture.Assets);
	REQUIRE(Fixture.Models.m_Loads == 2);
	REQUIRE(Reloaded.IsValid() && Reloaded.GetResource_Internal()->GetHandle_Internal() != FirstHandle);
}

TEST("model_instances have independent transforms and playback in seconds")
{
	FModelAssets Fixture;
	FModel Model = LoadColumn_Internal(Fixture.Assets);
	FModelInstance A = Instance_Internal(Fixture.Assets, Model);
	FModelInstance B = Instance_Internal(Fixture.Assets, Model);
	REQUIRE(Fixture.Models.m_Duplicates == 2);
	REQUIRE(A.GetResource_Internal()->GetHandle_Internal() != B.GetResource_Internal()->GetHandle_Internal());
	REQUIRE(A.SetTransform(Toolbox::FMatrix4::Translation({-100, 0, 0})));
	REQUIRE(B.SetTransform(Toolbox::FMatrix4::Translation({100, 0, 0})));
	REQUIRE(A.GetTransform().Values[3] == -100 && B.GetTransform().Values[3] == 100);
	Toolbox::FMatrix4 Invalid;
	Invalid.Values[0] = Toolbox::TNumericLimits<Toolbox::f32>::QuietNaN();
	REQUIRE(!A.SetTransform(Invalid) && A.GetTransform().Values[3] == -100);
	const double Duration = Model.GetClip(0)->DurationSeconds;
	REQUIRE(A.Play("Bend") && B.Play(1));
	REQUIRE(A.Advance(0.25) && B.Advance(0.5));
	REQUIRE(A.GetTime() == 0.25 && B.GetTime() == 0.5);
	// 片方だけ一時停止しても、もう片方は進む。
	A.Pause();
	REQUIRE(A.Advance(0.25) && B.Advance(0.25));
	REQUIRE(A.GetTime() == 0.25 && !A.IsPlaying() && B.GetTime() == 0.75);
	A.Resume();
	REQUIRE(A.IsPlaying());
	// 速度は各インスタンスに独立。ループは折り返す。
	REQUIRE(B.SetSpeed(2.0) && B.Advance(0.25));
	REQUIRE(B.GetTime() > 0.25 - 1e-6 && B.GetTime() < 0.25 + 1e-6);
	REQUIRE(A.GetSpeed() == 1.0);
	// 秒からネイティブの時間単位への変換（1秒=30）。
	REQUIRE(A.GetNativeTime_Internal() > 7.5f - 1e-4f && A.GetNativeTime_Internal() < 7.5f + 1e-4f);
	// ループしないクリップは終端で止まる。
	REQUIRE(A.Play(0, false) && A.Advance(Duration * 3.0));
	REQUIRE(A.GetTime() == Duration && !A.IsPlaying());
	REQUIRE(A.SetTime(-1.0) && A.GetTime() == 0.0);
	A.SetLooping(true);
	REQUIRE(A.SetTime(-0.25 * Duration) && A.GetTime() > 0.74 * Duration && A.GetTime() < 0.76 * Duration);
	A.Stop();
	REQUIRE(A.GetClip() == -1 && A.GetNativeTime_Internal() == 0.0f && !A.IsPlaying());
	REQUIRE(!A.SetTime(0.5));
	REQUIRE(!A.SetSpeed(-1.0) && !A.Advance(-0.1) && !A.Advance(Toolbox::TNumericLimits<double>::QuietNaN()));
	auto Missing = A.Play("None");
	REQUIRE(!Missing && Missing.Error().Code == EErrorCode::NotFound);
	REQUIRE(!A.Play(99));
	REQUIRE(!FModelInstance().Play(0));
}

TEST("model_release on the owner thread deletes at once and other threads defer to the owner")
{
	FModelAssets Fixture;
	FModel Model = LoadColumn_Internal(Fixture.Assets);
	{
		FModelInstance Local = Instance_Internal(Fixture.Assets, Model);
		const Toolbox::int32 Handle = Local.GetResource_Internal()->GetHandle_Internal();
		Local = FModelInstance();
		REQUIRE(Fixture.Models.WasDeleted(Handle));
	}
	FModelInstance Remote = Instance_Internal(Fixture.Assets, Model);
	const Toolbox::int32 Handle = Remote.GetResource_Internal()->GetHandle_Internal();
	Toolbox::FThread Worker;
	REQUIRE(Worker.Start(&DestroyOnThread_Internal, &Remote));
	Worker.Join();
	// ワーカーでは解放せず、所有スレッドの回収まで保留する。
	REQUIRE(!Fixture.Models.WasDeleted(Handle));
	Fixture.Assets.CollectUnused();
	REQUIRE(Fixture.Models.WasDeleted(Handle));
	REQUIRE(Fixture.Models.m_DeleteThreads[Fixture.Models.m_DeleteThreads.Size() - 1] ==
	        Toolbox::FThread::CurrentThreadId());
}

TEST("model_shutdown releases instances and shared data and rejects later loads")
{
	FModelAssets Fixture;
	FModel Model = LoadColumn_Internal(Fixture.Assets);
	FModelInstance A = Instance_Internal(Fixture.Assets, Model);
	FModelInstance B = Instance_Internal(Fixture.Assets, Model);
	Fixture.Assets.Shutdown();
	REQUIRE(Fixture.Models.m_Deleted.Size() == 3);
	REQUIRE(!Model.IsValid() && !A.IsValid() && !B.IsValid());
	auto Late = Fixture.Assets.LoadModel("Assets/Models/SkinnedColumn.fbx");
	REQUIRE(!Late && Late.Error().Code == EErrorCode::InvalidState);
	REQUIRE(!Fixture.Assets.CreateModelInstance(Model));
	// 解放済みのハンドルを再び解放しない。
	A = FModelInstance();
	REQUIRE(Fixture.Models.m_Deleted.Size() == 3);
}

TEST("model_loading without a model backend fails with setup guidance")
{
	Testing::FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	auto Result = Assets.LoadModel("Missing.fbx");
	REQUIRE(!Result && Result.Error().Code == EErrorCode::InvalidState);
	REQUIRE(strstr(Result.Error().Message.CStr(), "Setup.cmd") != nullptr);
}

TEST("model_loader rejects a model from another asset service and native load failure")
{
	FModelAssets First;
	FModelAssets Second;
	FModel Model = LoadColumn_Internal(First.Assets);
	auto Foreign = Second.Assets.CreateModelInstance(Model);
	REQUIRE(!Foreign && Foreign.Error().Code == EErrorCode::InvalidArgument);
	Second.Models.m_bFailLoad = true;
	auto Failed = Second.Assets.LoadModel("Assets/Models/StaticBox.fbx");
	REQUIRE(!Failed && Failed.Error().Code == EErrorCode::BackendFailure);
}

TEST("model_draw keeps the instance alive until the queued command executes")
{
	FModelAssets Fixture;
	FModelRenderBackend Render;
	FRenderSystem Renderer(Render);
	FModel Model = LoadColumn_Internal(Fixture.Assets);
	FModelInstance Instance = Instance_Internal(Fixture.Assets, Model);
	const Toolbox::int32 Handle = Instance.GetResource_Internal()->GetHandle_Internal();
	REQUIRE(Instance.SetTransform(Toolbox::FMatrix4::Translation({42, 0, 0})));
	REQUIRE(Instance.Play("Bend") && Instance.Advance(0.5));
	REQUIRE(Renderer.BeginFrame(640, 480));
	REQUIRE(Renderer.GetContext().Get3D().DrawModel(Instance));
	// 受付後の変更と破棄は、記録済みの命令に影響しない。
	REQUIRE(Instance.Advance(0.25));
	Instance = FModelInstance();
	REQUIRE(!Fixture.Models.WasDeleted(Handle));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Render.m_Models.Size() == 1);
	REQUIRE(Render.m_Models[0].Handle == Handle && Render.m_Models[0].Clip == 0 && Render.m_Models[0].X == 42);
	REQUIRE(Render.m_Models[0].NativeTime ==
	        static_cast<float>(0.5 / Model.GetClip(0)->DurationSeconds * Model.GetClip(0)->NativeDuration));
	REQUIRE(Fixture.Models.WasDeleted(Handle));
	REQUIRE(Render.m_Presentations == 1);
}

TEST("model_draw runs before geometry of the same view and splits at view changes")
{
	FModelAssets Fixture;
	FModelRenderBackend Render;
	FRenderSystem Renderer(Render);
	FModel Model = LoadColumn_Internal(Fixture.Assets);
	FModelInstance A = Instance_Internal(Fixture.Assets, Model);
	FModelInstance B = Instance_Internal(Fixture.Assets, Model);
	REQUIRE(Renderer.BeginFrame(640, 480));
	auto& Draw = Renderer.GetContext().Get3D();
	FRenderView3D View;
	REQUIRE(Draw.SetView(View));
	REQUIRE(Draw.DrawLine({0, 0, 0}, {1, 0, 0}));
	REQUIRE(Draw.DrawModel(A));
	View.Eye = {0, 5, -10};
	REQUIRE(Draw.SetView(View));
	REQUIRE(Draw.DrawModel(B));
	REQUIRE(Renderer.EndFrame());
	const Toolbox::int32 Expected[] = {10, 40, 30, 11, 10, 40, 11};
	REQUIRE(Render.m_Order.Size() == 7);
	for (size_t Index = 0; Index < 7; ++Index)
	{
		REQUIRE(Render.m_Order[Index] == Expected[Index]);
	}
}

TEST("model_draw failure closes the view and suppresses presentation")
{
	FModelAssets Fixture;
	FModelRenderBackend Render;
	Render.m_bFailModel = true;
	FRenderSystem Renderer(Render);
	FModel Model = LoadColumn_Internal(Fixture.Assets);
	FModelInstance Instance = Instance_Internal(Fixture.Assets, Model);
	REQUIRE(Renderer.BeginFrame(640, 480));
	REQUIRE(Renderer.GetContext().Get3D().DrawModel(Instance));
	REQUIRE(!Renderer.EndFrame());
	REQUIRE(Render.m_Ends == 1 && Render.m_Presentations == 0);
}

TEST("model_draw on a backend without model support fails explicitly")
{
	FModelAssets Fixture;
	FModelRenderBackend Render;
	Render.m_bModels = false;
	FRenderSystem Renderer(Render);
	FModel Model = LoadColumn_Internal(Fixture.Assets);
	FModelInstance Instance = Instance_Internal(Fixture.Assets, Model);
	REQUIRE(Renderer.BeginFrame(640, 480));
	REQUIRE(Renderer.GetContext().Get3D().DrawModel(Instance));
	REQUIRE(!Renderer.EndFrame());
	REQUIRE(Render.m_Models.IsEmpty() && Render.m_Presentations == 0);
}

TEST("model_draw rejects invalid instances and cancelled frames release queued models")
{
	FModelAssets Fixture;
	FModelRenderBackend Render;
	FRenderSystem Renderer(Render);
	REQUIRE(Renderer.BeginFrame(640, 480));
	REQUIRE(!Renderer.GetContext().Get3D().DrawModel(FModelInstance()));
	FModel Model = LoadColumn_Internal(Fixture.Assets);
	FModelInstance Instance = Instance_Internal(Fixture.Assets, Model);
	const Toolbox::int32 Handle = Instance.GetResource_Internal()->GetHandle_Internal();
	REQUIRE(Renderer.GetContext().Get3D().DrawModel(Instance));
	Instance = FModelInstance();
	REQUIRE(!Fixture.Models.WasDeleted(Handle));
	Renderer.CancelFrame();
	REQUIRE(Fixture.Models.WasDeleted(Handle));
	REQUIRE(Render.m_Models.IsEmpty() && Render.m_Presentations == 0);
}

// 1/100秒だけ移動するASCII FBX。既定30標本/秒より短いクリップの終端を確認する。
TEST("model_import preserves the final pose of a sub-frame clip")
{
	// FBXの時刻は1秒あたり46186158000単位。終端の移動量は7。
	const char Source[] = R"FBX(; FBX 7.4.0 project file
FBXHeaderExtension: { FBXVersion: 7400 }
Objects: {
 Model: 1, "Model::Mover", "Null" {
  Properties70: { P: "Lcl Translation", "Lcl Translation", "", "A",0,0,0 }
 }
 AnimationStack: 2, "AnimStack::Short", "" {
  Properties70: {
   P: "LocalStart", "KTime", "Time", "",0
   P: "LocalStop", "KTime", "Time", "",461861580
  }
 }
 AnimationLayer: 3, "AnimLayer::Base", "" { }
 AnimationCurveNode: 4, "AnimCurveNode::T", "" {
  Properties70: { P: "d|X", "Number", "", "A",0 }
 }
 AnimationCurve: 5, "AnimCurve::X", "" {
  KeyTime: *2 { a: 0,461861580 }
  KeyValueFloat: *2 { a: 0,7 }
  KeyAttrFlags: *1 { a: 4 }
  KeyAttrDataFloat: *4 { a: 0,0,0,0 }
  KeyAttrRefCount: *1 { a: 2 }
 }
}
Connections: {
 C: "OO",1,0
 C: "OO",3,2
 C: "OO",4,3
 C: "OP",4,1,"Lcl Translation"
 C: "OP",5,4,"d|X"
}
)FBX";
	// 2つの行列キーを持ち、最後の行列には実ファイルの終端姿勢が残る。
	auto Imported = ImportFbxModel(Source, sizeof(Source) - 1);
	REQUIRE(Imported);
	REQUIRE(Imported.Value().Clips.Size() == 1);
	REQUIRE(Imported.Value().Clips[0].DurationSeconds > 0.0099);
	REQUIRE(Imported.Value().Clips[0].DurationSeconds < 0.0101);
	REQUIRE(Count_Internal(Imported.Value().ModelData, "AnimationKey {\n4;\n2;\n") == 1);
	REQUIRE(Count_Internal(Imported.Value().ModelData, "7,0,0,1;;;") == 1);
}

// 長すぎるクリップは、巨大な出力を確保する前に失敗させる。
TEST("model_import rejects animation exceeding the total key limit")
{
	// 40000秒のクリップは、既定の30標本/秒で100万キーを超える。
	const char Source[] = R"FBX(; FBX 7.4.0 project file
FBXHeaderExtension: { FBXVersion: 7400 }
Objects: {
 AnimationStack: 1, "AnimStack::TooLong", "" {
  Properties70: {
   P: "LocalStart", "KTime", "Time", "",0
   P: "LocalStop", "KTime", "Time", "",1847446320000000
  }
 }
}
)FBX";
	// 失敗理由まで確認し、単なるファイル解析失敗と区別する。
	auto Imported = ImportFbxModel(Source, sizeof(Source) - 1);
	REQUIRE(!Imported);
	REQUIRE(Imported.Error().Code == EErrorCode::InvalidArgument);
	REQUIRE(Imported.Error().Message == "FBX animation duration is invalid or exceeds the key limit");
}

namespace
{
// 未対応属性を含めた最小FBX。外部SDKでの生成には依存しない。
Toolbox::FString FeatureFbx_Internal(const char* ExtraObjects, const char* ExtraConnections = "")
{
	// 面・頂点・2組のUV・頂点色を持つ三角形。
	Toolbox::FString Source = R"FBX(; FBX 7.4.0 project file
FBXHeaderExtension: { FBXVersion: 7400 }
Objects: {
 Model: 1, "Model::Triangle", "Mesh" { }
 Geometry: 2, "Geometry::Triangle", "Mesh" {
  Vertices: *9 { a: 0,0,0,1,0,0,0,1,0 }
  PolygonVertexIndex: *3 { a: 0,1,-3 }
  LayerElementUV: 0 {
   Name: "UV0"
   MappingInformationType: "ByPolygonVertex"
   ReferenceInformationType: "Direct"
   UV: *6 { a: 0,0,1,0,0,1 }
  }
  LayerElementUV: 1 {
   Name: "UV1"
   MappingInformationType: "ByPolygonVertex"
   ReferenceInformationType: "Direct"
   UV: *6 { a: 0,0,0.5,0,0,0.5 }
  }
  LayerElementColor: 0 {
   MappingInformationType: "ByPolygonVertex"
   ReferenceInformationType: "Direct"
   Colors: *12 { a: 1,0,0,1,0,1,0,1,0,0,1,1 }
  }
 }
)FBX";
	Source += ExtraObjects;
	Source += "\n}\nConnections: {\n C: \"OO\",1,0\n C: \"OO\",2,1\n";
	Source += ExtraConnections;
	Source += "\n}\n";
	return Source;
}

// 警告が対象機能を説明しているかを確認する。
bool HasWarning_Internal(const FImportedModel& Model, const char* Fragment)
{
	// 探す文言のバイト数。
	const Toolbox::FString Expected(Fragment);
	for (const Toolbox::FString& Warning : Model.Warnings)
	{
		for (Toolbox::size_t Index = 0; Index + Expected.Size() <= Warning.Size(); ++Index)
		{
			if (Toolbox::FString(Warning.Data() + Index, Expected.Size()) == Expected)
			{
				return true;
			}
		}
	}
	return false;
}
} // namespace

TEST("model_import reports partial geometry and unsupported material features")
{
	// モーフ、独自PBR材質、カメラ、ライトは取り込まないことを明示する。
	const Toolbox::FString Source = FeatureFbx_Internal(R"FBX(
 Deformer: 3, "Deformer::Morph", "BlendShape" { }
 Material: 4, "Material::CustomPBR", "" { ShadingModel: "CustomPBR" }
 NodeAttribute: 5, "NodeAttribute::Camera", "Camera" { }
 NodeAttribute: 6, "NodeAttribute::Light", "Light" { }
)FBX");
	// 対応部分のメッシュを保ちつつ、失われた各属性が通知される。
	auto Imported = ImportFbxModel(Source.Data(), Source.Size());
	REQUIRE(Imported);
	REQUIRE(Count_Internal(Imported.Value().ModelData, "Mesh Triangle_mesh") == 1);
	REQUIRE(HasWarning_Internal(Imported.Value(), "Morph"));
	REQUIRE(HasWarning_Internal(Imported.Value(), "UV"));
	REQUIRE(HasWarning_Internal(Imported.Value(), "Vertex colors"));
	REQUIRE(HasWarning_Internal(Imported.Value(), "PBR"));
	REQUIRE(HasWarning_Internal(Imported.Value(), "cameras and lights"));
}

TEST("model_import rejects dual quaternion and multiple skin deformers")
{
	// 線形変換へ黙って置き換えられないスキン方式。
	const Toolbox::FString Dual = FeatureFbx_Internal(R"FBX(
 Deformer: 3, "Deformer::Skin", "Skin" { SkinningType: "DualQuaternion" }
)FBX", " C: \"OO\",3,2\n");
	auto DualResult = ImportFbxModel(Dual.Data(), Dual.Size());
	REQUIRE(!DualResult);
	REQUIRE(DualResult.Error().Message == "FBX dual-quaternion skinning is unsupported");
	// 2つ目のスキンを捨てて成功させない。
	const Toolbox::FString Multiple = FeatureFbx_Internal(R"FBX(
 Deformer: 3, "Deformer::A", "Skin" { }
 Deformer: 4, "Deformer::B", "Skin" { }
)FBX", " C: \"OO\",3,2\n C: \"OO\",4,2\n");
	auto MultipleResult = ImportFbxModel(Multiple.Data(), Multiple.Size());
	REQUIRE(!MultipleResult);
	REQUIRE(MultipleResult.Error().Message == "FBX meshes with multiple skins are unsupported");
}

TEST("model_import refuses a different format instead of silently accepting it as FBX")
{
	// ufbxが自動判定なら読み込めるOBJを、FBX専用入口では拒否する。
	const char Source[] = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
	REQUIRE(!ImportFbxModel(Source, sizeof(Source) - 1));
}

TEST("model warnings remain accessible on a cached model")
{
	// 未対応のUVと頂点色を含むファイルを、キャッシュ経由で2回取得する。
	const Toolbox::FPath Scratch = Test::PrepareScratchDirectory("model-warning-cache");
	Test::WriteScratchFile(Scratch / "features.fbx", FeatureFbx_Internal(""));
	FFakeBackend Backend;
	FFakeModelBackend Models;
	FAssetService Assets(Backend, Backend, Backend, &Models);
	REQUIRE(Assets.SetProjectRoot(Scratch));
	auto First = Assets.LoadModel("features.fbx");
	REQUIRE(First);
	REQUIRE(First.Value().GetImportWarningCount() > 0);
	auto Cached = Assets.LoadModel("features.fbx");
	REQUIRE(Cached);
	REQUIRE(Models.m_Loads == 1);
	REQUIRE(Cached.Value().GetImportWarningCount() == First.Value().GetImportWarningCount());
	REQUIRE(*Cached.Value().GetImportWarning(0) == *First.Value().GetImportWarning(0));
	REQUIRE(Cached.Value().GetImportWarning(Cached.Value().GetImportWarningCount()) == nullptr);
}

TEST("model cache does not merge distinct unit conversion settings")
{
	// 表示上同じ9桁になる設定でも、同じキャッシュへまとめない。
	FFakeBackend Backend;
	FFakeModelBackend Models;
	FAssetService Assets(Backend, Backend, Backend, &Models);
	REQUIRE(Assets.SetProjectRoot(Toolbox::FPath(DXF_TEST_ASSET_DIR).Parent()));
	FModelLoadOptions Options;
	Options.TargetUnitMeters = 1.0;
	auto First = Assets.LoadModel("Assets/Models/StaticBox.fbx", Options);
	REQUIRE(First);
	Options.TargetUnitMeters = 1.0000000001;
	auto Second = Assets.LoadModel("Assets/Models/StaticBox.fbx", Options);
	REQUIRE(Second);
	REQUIRE(Models.m_Loads == 2);
}
