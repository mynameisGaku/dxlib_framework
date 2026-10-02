// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/ContentRenderBackend.h"
#include "Support/ContentModelBackend.h"
#include "SaveCourseRecipes.h"
#include "SaveCourseProgressCodec.h"
#include "Dxf/Application.h"
#include "Dxf/CheckpointScene2D.h"
#include "Dxf/CheckpointScene3D.h"
#include "Dxf/SaveError.h"
#include "Toolbox/FileWriter.h"
#include "Toolbox/FilePublication.h"
#include "Toolbox/JsonWriter.h"
#include "Toolbox/Sha256.h"
using namespace Dxf;
using namespace Dxf::GameplaySample;
using namespace Toolbox;
namespace
{
// 浮動小数点の位置を独立した解析値と比較する。
template <typename TVector>
void Near(const TVector& Actual, const TVector& Expected)
{
	const auto Difference = Actual - Expected;
	REQUIRE(Difference.X * Difference.X + Difference.Y * Difference.Y < .00001f);
	if constexpr (Toolbox::IsSame<TVector, Toolbox::FVector3>)
	{
		REQUIRE(Difference.Z * Difference.Z < .00001f);
	}
}
// 読込後の候補だけを変更し、ゲームの安全地点と初期Poseを実Applicationで確認する。
template <typename TCatalog, typename TScene>
void RunRecipe(const TCatalog& Catalog, bool Open)
{
	// 初期構成から作る保存値。まだWorldやNativeを作らない。
	auto Document = FSaveCourseRecipes::NewGame(Catalog);
	// プレイで変わった進行を表す独立した期待値。
	FSaveCourseProgress Progress;
	Progress.bDoorUnlocked[0] = Open;
	Progress.bDoorOpen[0] = Open;
	Progress.Checkpoint = "north";
	Progress.CollectedKeys.PushBack("northKey");
	Progress.Score = 10;
	Progress.SpawnCounter = 1;
	Progress.Revision = 4;
	Document.GamePayload = FSaveCourseProgressCodec::Encode(Progress);
	Document.Identity.CheckpointKey = Progress.Checkpoint;
	// 保存した動的荷物も、新Worldへ論理キーと公開値から生成する。
	auto Cargo = Document.Instances[2];
	Cargo.Key = "cargo1";
	Cargo.Prefab = "cargo";
	Cargo.bDynamic = true;
	Cargo.Placement.Position.X = 8;
	Cargo.Placement.Position.Y = 3;
	Cargo.Parameters.Clear();
	Cargo.DefinitionFingerprint = Catalog.Prefabs[Catalog.Prefabs.Size() - 1].Definition.ContentFingerprint;
	Document.Instances.PushBack(Cargo);
	// 閉じた元値はゲーム変換でも書き換えない。codecを通した保存値から再開する。
	const auto Stored = FCheckpointCodec::Encode(Document);
	const auto Loaded = [&]()
	{
		if constexpr (IsSame<TCatalog, FCheckpointCatalog2D>)
		{
			return FCheckpointCodec::Decode2D(Stored);
		}
		else
		{
			return FCheckpointCodec::Decode3D(Stored);
		}
	}();
	const auto Restored = FSaveCourseRecipes::Restore(Loaded);
	const auto Plan = FCheckpointRestore::Build(Restored, Catalog);
	REQUIRE(Loaded.Instances[3].bPresent && !Restored.Instances[3].bPresent);
	REQUIRE(Catalog.Scene.Prefabs[0].Parts[1].Body.Position.X == 0);
	// 描画とモデルの外部境界だけを置換する。本体Application・Scene・Worldを使う。
	Testing::FFakeBackend Backend;
	Testing::FContentRenderBackend Renderer(Backend);
	Testing::FContentModelBackend Models;
	FApplicationSettings Settings;
	Settings.ProjectRoot = DXF_CONTENT_SAMPLE_ROOT;
	Settings.ExecutionThreadCount = 1;
	FApplication App({Backend, Backend, Backend, Backend, Backend, Renderer, &Models}, Settings);
	REQUIRE(App.Start(MakeUnique<DScene>()));
	REQUIRE(App.Step(0));
	// 全資源準備が済むまで旧Sceneへ切替要求を出さない。
	auto Scene = MakeUnique<TScene>(PrepareScene(Plan.Scene, App.GetAssets()), Restored);
	auto* Current = Scene.Get();
	REQUIRE(Backend.GetTrace().Clones == 0);
	REQUIRE(App.GetScenes().RequestChange(Move(Scene)));
	REQUIRE(App.Step(0));
	REQUIRE(Current->GetCompletedPhysicsStepCount() == 0);
	REQUIRE(Current->GetPrefab("doorA").Get()->GetState() == EPrefabInstanceState::PendingPhysics);
	// 一回の正常固定更新で開いた初期状態になる。隠れた追加Stepを許さない。
	const auto First = App.Step(1.25 / 60.0);
	if (!First)
	{
		throw FException(First.Error().Message);
	}
	REQUIRE(First.Value() && Current->GetCompletedPhysicsStepCount() == 1);
	const auto A = Current->GetPrefab("doorA");
	const auto B = Current->GetPrefab("doorB");
	REQUIRE(A.Get()->GetState() == EPrefabInstanceState::Ready && B.Get()->GetState() == EPrefabInstanceState::Ready);
	const auto Observation = A.Get()->GetPrismaticJoint("doorDrive").Get()->GetObservation();
	REQUIRE(Observation && Abs(Observation->State.Translation - (Open ? 2 : 0)) < .001);
	const auto Limits = A.Get()->GetPrismaticJoint("doorDrive").Get()->GetDescription().Joint.Limits;
	REQUIRE(Limits.LowerTranslation == 0 && Limits.UpperTranslation == 2);
	REQUIRE(Abs(B.Get()->GetPrismaticJoint("doorDrive").Get()->GetObservation()->State.Translation) < .001);
	REQUIRE(A.Get()->GetFixedJoint("assembly").Get()->GetJointId());
	REQUIRE(A.Get()->GetDistanceJoint("hanger").Get()->GetJointId());
	REQUIRE(A.Get()->GetRevoluteJoint("armDrive").Get()->GetJointId());
	const auto Leaf = Current->GetPhysicsWorld().GetPosition(A.Get()->GetRigidBody("doorBody").Get()->GetBodyId());
	const auto Attached = Current->GetPhysicsWorld().GetPosition(A.Get()->GetRigidBody("cargoBody").Get()->GetBodyId());
	const auto Player = Current->GetPhysicsWorld().GetPosition(Current->GetPrefab("player").Get()->GetRigidBody("body").Get()->GetBodyId());
	if constexpr (IsSame<TScene, ACheckpointScene2D>)
	{
		Near(Leaf, Toolbox::FVector2{-4, Open ? 2.f : 0.f});
		Near(Attached - Leaf, Toolbox::FVector2{-1.2f, 0});
		Near(Player, Toolbox::FVector2{0, 5});
	}
	else
	{
		Near(Leaf, Toolbox::FVector3{-3, Open ? 2.f : 0.f, 1});
		Near(Attached - Leaf, Toolbox::FVector3{0, 0, 1.2f});
		Near(Player, Toolbox::FVector3{0, 5, 0});
	}
	REQUIRE(Current->GetPrefab("cargo1").Get()->GetState() == EPrefabInstanceState::Ready);
	REQUIRE(Current->GetPrefab("southKey").Get()->GetState() == EPrefabInstanceState::Ready);
	// 取得済みだけを除去し、同名別個体や未取得の鍵は残す。
	bool Absent = false;
	try
	{
		Current->GetPrefab("northKey");
	}
	catch (const FException&)
	{
		Absent = true;
	}
	REQUIRE(Absent && Backend.GetTrace().Clones == 0);
	REQUIRE(FSaveCourseProgressCodec::Decode(Current->GetGamePayload()).Score == 10);
	App.GetScenes().RequestQuit();
	const auto Quit = App.Step(2.0 / 60.0);
	REQUIRE(Quit && !Quit.Value() && !A && !B);
}
} // namespace
TEST("Save course 2D recipe starts open at rotated placement without extra physics steps")
{
	FSceneContentSource Source{FPath(DXF_CONTENT_SAMPLE_ROOT)};
	const auto Catalog = FSaveCourseRecipes::Load2D(Source);
	RunRecipe<FCheckpointCatalog2D, ACheckpointScene2D>(Catalog, false);
	RunRecipe<FCheckpointCatalog2D, ACheckpointScene2D>(Catalog, true);
}
TEST("Save course 3D recipe preserves local frames fixed cargo keys and dynamic instance")
{
	FSceneContentSource Source{FPath(DXF_CONTENT_SAMPLE_ROOT)};
	const auto Catalog = FSaveCourseRecipes::Load3D(Source);
	RunRecipe<FCheckpointCatalog3D, ACheckpointScene3D>(Catalog, false);
	RunRecipe<FCheckpointCatalog3D, ACheckpointScene3D>(Catalog, true);
}
TEST("Save course pure door recipes reject invalid complete values without changing trusted definitions")
{
	// どちらの次元も実ファイルから検証済みの公開値を得る。
	FSceneContentSource Source{FPath(DXF_CONTENT_SAMPLE_ROOT)};
	const auto C2 = FSaveCourseRecipes::Load2D(Source);
	const auto C3 = FSaveCourseRecipes::Load3D(Source);
	// 同じ不正値を両次元の明示レシピへ渡す。
	for (uint32 Dimension = 2; Dimension <= 3; ++Dimension)
	{
		// 受理済みの完全な値集合。以後の変更は候補だけ。
		const auto Original = Dimension == 2 ? C2.Prefabs[0].Definition.Parameters : C3.Prefabs[0].Definition.Parameters;
		for (uint32 Mutation = 0; Mutation < 5; ++Mutation)
		{
			auto Values = Original;
			if (Mutation == 0)
			{
				Values.PopBack();
			}
			else if (Mutation == 1)
			{
				Values[0].Id = "unknown";
			}
			else if (Mutation == 2)
			{
				Values[1].Id = Values[0].Id;
			}
			else
			{
				for (auto& Value : Values)
				{
					if (Value.Id == "travel")
					{
						Value.Number = 6;
						if (Mutation == 4)
						{
							Value.Kind = EContentParameterKind::Boolean;
						}
					}
				}
			}
			// 拒否された候補が通常の再開定義へ混ざらない。
			bool Rejected = false;
			try
			{
				if (Dimension == 2)
				{
					(void)C2.Prefabs[0].Resolve(Values);
				}
				else
				{
					(void)C3.Prefabs[0].Resolve(Values);
				}
			}
			catch (const FSaveError& Error)
			{
				REQUIRE(Error.GetCode() == ESaveError::InvalidDocument);
				Rejected = true;
			}
			REQUIRE(Rejected);
		}
	}
	REQUIRE(C2.Prefabs[0].Definition.Parts[1].Body.Position.X == 0);
	REQUIRE(C3.Prefabs[0].Definition.Parts[1].Body.Position.X == 0);
	// 識別と進行が矛盾する保存も、Scene要求を出す前に拒否する。
	auto Document = FCheckpointRestore::MakeInitial(C2, FSaveCourseProgressCodec::Encode({}));
	Document.Identity.CheckpointKey = "north";
	bool Rejected = false;
	try
	{
		(void)FSaveCourseRecipes::Restore(Document);
	}
	catch (const FSaveError&)
	{
		Rejected = true;
	}
	REQUIRE(Rejected && Document.Instances[3].bPresent);
}
TEST("Save course data open default is applied once and recipes keep the loaded bytes after disk changes")
{
	// 原本Assetsは読み取りだけ。今回の試験が所有する新しいコピーRootへ配置する。
	const FPath OriginalRoot(DXF_CONTENT_SAMPLE_ROOT);
	const FPath CopyRoot = FPath(DXF_SAVE_COURSE_TEST_ROOT) / FPath(FJsonWriter::Unsigned(MonotonicNanoseconds()));
	// 既存の単一ディレクトリ作成を順に使い、再帰作成を仮定しない。
	(void)CreateDirectory(FPath(DXF_SAVE_COURSE_TEST_ROOT));
	REQUIRE(CreateDirectory(CopyRoot));
	REQUIRE(CreateDirectory(CopyRoot / FPath("Assets")));
	REQUIRE(CreateDirectory(CopyRoot / FPath("Assets/SaveCourse")));
	// 元の扉定義の内容が試験前後で変わらないことを別の指紋で確認する。
	FString OriginalDoor[2];
	const char* Names[] = {"course", "door", "player", "key", "checkpoint", "cargo", "floor"};
	for (uint32 Dimension = 2; Dimension <= 3; ++Dimension)
	{
		for (const char* Name : Names)
		{
			const FString Relative = FString("Assets/SaveCourse/") + Name + FJsonWriter::Unsigned(Dimension) + "d" + (FStringView(Name) == "course" ? ".dxfscene.json" : ".dxfprefab.json");
			// コピーする実バイト。
			TVector<uint8> Bytes;
			REQUIRE(ReadFileBytes(OriginalRoot / FPath(Relative), Bytes, 4 * 1024 * 1024));
			FString Text(reinterpret_cast<const char*>(Bytes.Data()), Bytes.Size());
			if (FStringView(Name) == "door")
			{
				OriginalDoor[Dimension - 2] = FSha256::Compute(Text);
				// 定義の既定値だけを変更し、C++の生成元を変えない。
				const FStringView From = "\"default\": false";
				FString Changed;
				bool Found = false;
				for (size_t I = 0; I < Text.Size(); ++I)
				{
					if (!Found && I + From.Size() <= Text.Size() && FStringView(Text.Data() + I, From.Size()) == From)
					{
						Changed += "\"default\": true";
						I += From.Size() - 1;
						Found = true;
					}
					else
					{
						Changed += Text[I];
					}
				}
				REQUIRE(Found);
				Text = Move(Changed);
			}
			FFileWriter Writer(CopyRoot / FPath(Relative));
			Writer.Write(Text);
			Writer.Flush();
			Writer.Close();
		}
	}
	FSceneContentSource Source(CopyRoot);
	const auto C2 = FSaveCourseRecipes::Load2D(Source);
	const auto C3 = FSaveCourseRecipes::Load3D(Source);
	const auto Initial2 = FSaveCourseRecipes::NewGame(C2);
	const auto Initial3 = FSaveCourseRecipes::NewGame(C3);
	// 同じコードがデータ由来の初期開扉を使い、二重加算しない。
	REQUIRE(C2.Scene.Prefabs[0].Parts[1].Body.Position.X == 2);
	REQUIRE(C3.Scene.Prefabs[0].Parts[1].Body.Position.X == 2);
	REQUIRE(FCheckpointRestore::Build(Initial2, C2).Scene.Prefabs[0].Parts[1].Body.Position.X == 2);
	REQUIRE(FCheckpointRestore::Build(Initial3, C3).Scene.Prefabs[0].Parts[1].Body.Position.X == 2);
	REQUIRE(FSaveCourseProgressCodec::Decode(Initial2.GamePayload).bDoorOpen[0]);
	REQUIRE(FSaveCourseProgressCodec::Decode(Initial3.GamePayload).bDoorUnlocked[0]);
	REQUIRE(FCheckpointRestore::Build(FSaveCourseRecipes::Restore(Initial2), C2).Scene.Prefabs[0].Parts[1].Body.Position.X == 2);
	REQUIRE(FCheckpointRestore::Build(FSaveCourseRecipes::Restore(Initial3), C3).Scene.Prefabs[0].Parts[1].Body.Position.X == 2);
	for (uint32 Dimension = 2; Dimension <= 3; ++Dimension)
	{
		// 原本は同一のままで、読込済みレシピはディスクに依存せず別候補を作る。
		TVector<uint8> Bytes;
		const FString Relative = FString("Assets/SaveCourse/door") + FJsonWriter::Unsigned(Dimension) + "d.dxfprefab.json";
		REQUIRE(ReadFileBytes(OriginalRoot / FPath(Relative), Bytes, 4 * 1024 * 1024));
		REQUIRE(FSha256::Compute(FStringView(reinterpret_cast<const char*>(Bytes.Data()), Bytes.Size())) == OriginalDoor[Dimension - 2]);
		// 試験コピーだけを別名に移す。レシピが誤って再読込すると失敗する。
		PublishFile(CopyRoot / FPath(Relative), CopyRoot / FPath(Relative + ".moved"));
		const auto Values = Dimension == 2 ? C2.Prefabs[0].Definition.Parameters : C3.Prefabs[0].Definition.Parameters;
		if (Dimension == 2)
		{
			REQUIRE(C2.Prefabs[0].Resolve(Values).Parts[1].Body.Position.X == 2);
		}
		else
		{
			REQUIRE(C3.Prefabs[0].Resolve(Values).Parts[1].Body.Position.X == 2);
		}
	}
}
