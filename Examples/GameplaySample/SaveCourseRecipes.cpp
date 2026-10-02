// SPDX-License-Identifier: NOASSERTION
#include "SaveCourseRecipes.h"
#include "SaveCourseProgressCodec.h"
#include "Dxf/SaveError.h"
#include "Dxf/SceneContentValidation.h"
namespace Dxf::GameplaySample
{
namespace
{
using namespace Toolbox;
// 保存用コースに必要な条件だけを確認する。汎用Physicsの検証は既存窓口へ任せる。
void Require(bool Condition, const char* Reason)
{
	if (!Condition)
	{
		throw FSaveError(ESaveError::InvalidDocument, Reason);
	}
}
// 型付きの完全な公開値から一つを取得する。余分な値や重複も別途拒否する。
const FContentParameterValue& Parameter(const TVector<FContentParameterValue>& Values, FStringView Key, EContentParameterKind Kind)
{
	for (const auto& Value : Values)
	{
		if (Value.Id == Key)
		{
			Require(Value.Kind == Kind, "Save course parameter type mismatch");
			return Value;
		}
	}
	throw FSaveError(ESaveError::InvalidDocument, "Save course parameter is missing");
}
// 閉じた初期定義のBodyとLocal Frameからレール方向を一度だけ求める。
Toolbox::FVector2 RailAxis(const FPrefabDefinition2D& Definition, const FContentJointDefinition2D& Joint)
{
	// Aに取り付けたFrameのWorld角。
	const f64 Angle = Definition.Parts[Joint.BodyA].Body.Angle + Joint.Prismatic.FrameA.LocalAngle;
	return {static_cast<f32>(Cos(Angle)), static_cast<f32>(Sin(Angle))};
}
Toolbox::FVector3 RailAxis(const FPrefabDefinition3D& Definition, const FContentJointDefinition3D& Joint)
{
	return (Definition.Parts[Joint.BodyA].Body.Orientation * Joint.Prismatic.FrameA.LocalRotation).Rotate({1, 0, 0});
}
// 数学だけを次元別の窓口に残し、公開値と所有の処理を共有する。
template <typename TPrefab>
TPrefab ResolveDoor(const TPrefab& Closed, const TVector<FContentParameterValue>& Values)
{
	Require(Values.Size() == Closed.Parameters.Size() && Values.Size() == 5, "Save course door requires complete parameters");
	for (size_t I = 0; I < Values.Size(); ++I)
	{
		for (size_t J = 0; J < I; ++J)
		{
			Require(Values[I].Id != Values[J].Id, "Duplicate save course parameter");
		}
	}
	// schemaで宣言した数値範囲も、保存からの公開値へ適用する。
	const f64 Speed = Parameter(Values, "speed", EContentParameterKind::Number).Number;
	const f64 Travel = Parameter(Values, "travel", EContentParameterKind::Number).Number;
	const f64 Effort = Parameter(Values, "effort", EContentParameterKind::Number).Number;
	const auto Tint = Parameter(Values, "tint", EContentParameterKind::Color).Color;
	const bool Open = Parameter(Values, "open", EContentParameterKind::Boolean).Boolean;
	Require(IsFinite(Speed) && Speed >= -3 && Speed <= 3 && IsFinite(Travel) && Travel >= .1 && Travel <= 5 && IsFinite(Effort) && Effort >= 0 && Effort <= 80, "Save course door parameter out of range");
	// 個体用の独立した候補。指紋、資源参照、Local Frameを保持する。
	TPrefab Result = Closed;
	Result.Parameters = Values;
	// 復元対象のレール。indexを定数として保存しない。
	bool Found = false;
	for (auto& Joint : Result.Joints)
	{
		if (Joint.Id != "rail")
		{
			continue;
		}
		Require(Joint.Kind == EJointKind::Prismatic && Joint.BodyA < Result.Parts.Size() && Joint.BodyB < Result.Parts.Size(), "Save course rail definition mismatch");
		// 初期姿勢を変更する前の方向。BodyローカルのAnchorを二重に回さない。
		const auto Offset = RailAxis(Closed, Joint) * static_cast<f32>(Open ? Travel : 0);
		for (auto& Part : Result.Parts)
		{
			if (Part.Id == "leaf" || Part.Id == "cargo")
			{
				Part.Body.Position = Part.Body.Position + Offset;
			}
			if (Part.Id == "leaf")
			{
				Part.Visual.Color = Tint;
			}
		}
		Joint.Prismatic.Drive.TargetSpeed = Speed;
		Joint.Prismatic.Drive.MaxForce = Effort;
		Joint.Prismatic.Limits.UpperTranslation = Travel;
		Found = true;
	}
	Require(Found, "Save course rail is missing");
	ValidatePrefabDefinition(Result);
	return Result;
}
// Sceneの論理個体を許可Prefabへ明示対応させる。保存のパスや配列位置は信用しない。
FString PrefabKey(FStringView Instance)
{
	if (Instance == "doorA" || Instance == "doorB")
	{
		return "door";
	}
	if (Instance == "northKey" || Instance == "southKey")
	{
		return "key";
	}
	if (Instance == "northCheckpoint")
	{
		return "checkpoint";
	}
	Require(Instance == "player" || Instance == "floor", "Unknown save course initial instance");
	return FString(Instance.Data(), Instance.Size());
}
template <typename TCatalog, typename TRecipe, typename TScene, typename TPrefab>
TCatalog Catalog(TScene Scene, TPrefab Cargo, uint32 Dimension)
{
	// CPU値を明示所有するカタログ。Native資源やSceneを所有しない。
	TCatalog Result;
	Result.Identity.GameId = "dxfGameplaySave";
	Result.Identity.ContentRevision = "saveCourse1";
	Result.Identity.SceneKey = Dimension == 2 ? "saveCourse2d" : "saveCourse3d";
	Result.Identity.CheckpointKey = "start";
	Result.Identity.Dimension = Dimension;
	Result.Scene = Move(Scene);
	for (size_t I = 0; I < Result.Scene.Instances.Size(); ++I)
	{
		// この初期個体の許可キー。
		const auto Key = PrefabKey(Result.Scene.Instances[I]);
		Result.InitialPrefabKeys.PushBack(Key);
		// 同じ定義を別配置で使う場合の重複登録抑止。
		bool Found = false;
		for (const auto& Recipe : Result.Prefabs)
		{
			Found = Found || Recipe.Key == Key;
		}
		if (!Found)
		{
			// 一度読み終えた不変の定義を、各再開候補の生成元にする。
			TRecipe Recipe;
			Recipe.Key = Key;
			Recipe.Definition = Result.Scene.Prefabs[I];
			if (Key == "door")
			{
				// ファイルのopen既定値もNew Gameへ反映する。捕捉元の閉じた配置は別に保つ。
				const auto Closed = Recipe.Definition;
				Recipe.Definition = ResolveDoor(Closed, Closed.Parameters);
				Recipe.Resolve = [Closed](const TVector<FContentParameterValue>& Values)
				{
					return ResolveDoor(Closed, Values);
				};
			}
			Result.Prefabs.PushBack(Move(Recipe));
		}
		if (Key == "door")
		{
			// Sceneの各overrideも一度だけ初期配置へ反映する。
			Result.Scene.Prefabs[I] = ResolveDoor(Result.Scene.Prefabs[I], Result.Scene.Prefabs[I].Parameters);
		}
	}
	// 明示Spawnにだけ使用する荷物。Sceneには初期生成しない。
	TRecipe Recipe;
	Recipe.Key = "cargo";
	Recipe.Definition = Move(Cargo);
	Result.Prefabs.PushBack(Move(Recipe));
	return Result;
}
// New Gameの論理進行も初期定義から得る。データ由来の開扉を閉扉の進行と混ぜない。
template <typename TCatalog>
auto InitialProgress(const TCatalog& Catalog)
{
	// プレイ開始前の鍵・得点・番号を含むゲームの値。
	FSaveCourseProgress Progress;
	for (size_t I = 0; I < Catalog.Scene.Instances.Size(); ++I)
	{
		// 論理キーから選ぶ扉だけが初期開閉を持つ。
		const auto& Key = Catalog.Scene.Instances[I];
		if (Key == "doorA" || Key == "doorB")
		{
			const size_t Door = Key == "doorA" ? 0 : 1;
			const bool Open = Parameter(Catalog.Scene.Prefabs[I].Parameters, "open", EContentParameterKind::Boolean).Boolean;
			Progress.bDoorOpen[Door] = Open;
			Progress.bDoorUnlocked[Door] = Open;
		}
	}
	return FCheckpointRestore::MakeInitial(Catalog, FSaveCourseProgressCodec::Encode(Progress));
}
// 安全地点はゲームが選ぶWorld位置。Prefabの回転で重力や地点を回さない。
void PlayerPlacement(FPrefabSpawnOptions2D& Placement, bool North)
{
	Placement.Position = North ? Toolbox::FVector2{0, 5} : Toolbox::FVector2{-6, -3};
	Placement.Rotation = 0;
}
void PlayerPlacement(FPrefabSpawnOptions3D& Placement, bool North)
{
	Placement.Position = North ? Toolbox::FVector3{0, 5, 0} : Toolbox::FVector3{-6, -3, 0};
	Placement.Rotation = {};
}
template <typename TDocument>
TDocument RestoreProgress(const TDocument& Document)
{
	// 副作用を出す前に、ゲーム固有の全進行値を検証する。
	const auto Progress = FSaveCourseProgressCodec::Decode(Document.GamePayload);
	Require(Document.Identity.CheckpointKey == Progress.Checkpoint, "Save course checkpoint and payload disagree");
	// 旧保存を変更しない独立した候補。
	TDocument Result = Document;
	for (auto& Instance : Result.Instances)
	{
		for (const auto& Collected : Progress.CollectedKeys)
		{
			if (Instance.Key == Collected)
			{
				Instance.bPresent = false;
			}
		}
		if (Instance.Key == "player")
		{
			PlayerPlacement(Instance.Placement, Progress.Checkpoint == "north");
		}
		if (Instance.Key != "doorA" && Instance.Key != "doorB")
		{
			continue;
		}
		// C++内の進行配列と、保存の論理キーの対応。
		const size_t Door = Instance.Key == "doorA" ? 0 : 1;
		// 初期開閉を選ぶ公開値が見つかったか。
		bool Found = false;
		for (auto& Value : Instance.Parameters)
		{
			if (Value.Id == "open")
			{
				Require(Value.Kind == EContentParameterKind::Boolean, "Save course open parameter type mismatch");
				Value.Boolean = Progress.bDoorOpen[Door];
				Found = true;
			}
		}
		Require(Found, "Save course open parameter missing");
		for (auto& Joint : Result.Joints)
		{
			if (Joint.Instance == Instance.Key && Joint.Export == "doorDrive")
			{
				Require(Joint.Kind == EJointKind::Prismatic, "Save course door drive kind mismatch");
				// チェックポイントでは安全な開閉Poseから停止状態で再開する。
				Joint.LinearDrive.TargetSpeed = 0;
				Joint.LinearTarget.Reset();
			}
		}
	}
	return Result;
}
} // namespace
FCheckpointCatalog2D FSaveCourseRecipes::Load2D(const FSceneContentSource& Source)
{
	return Catalog<FCheckpointCatalog2D, FCheckpointPrefabRecipe2D>(Source.LoadScene2D("Assets/SaveCourse/course2d.dxfscene.json"), Source.LoadPrefab2D("Assets/SaveCourse/cargo2d.dxfprefab.json"), 2);
}
FCheckpointCatalog3D FSaveCourseRecipes::Load3D(const FSceneContentSource& Source)
{
	return Catalog<FCheckpointCatalog3D, FCheckpointPrefabRecipe3D>(Source.LoadScene3D("Assets/SaveCourse/course3d.dxfscene.json"), Source.LoadPrefab3D("Assets/SaveCourse/cargo3d.dxfprefab.json"), 3);
}
FCheckpointDocument2D FSaveCourseRecipes::NewGame(const FCheckpointCatalog2D& Catalog)
{
	return InitialProgress(Catalog);
}
FCheckpointDocument3D FSaveCourseRecipes::NewGame(const FCheckpointCatalog3D& Catalog)
{
	return InitialProgress(Catalog);
}
FCheckpointDocument2D FSaveCourseRecipes::Restore(const FCheckpointDocument2D& Document)
{
	return RestoreProgress(Document);
}
FCheckpointDocument3D FSaveCourseRecipes::Restore(const FCheckpointDocument3D& Document)
{
	return RestoreProgress(Document);
}
} // namespace Dxf::GameplaySample
