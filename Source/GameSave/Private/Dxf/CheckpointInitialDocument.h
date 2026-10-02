// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHECKPOINT_INITIAL_DOCUMENT_H
#define DXF_CHECKPOINT_INITIAL_DOCUMENT_H
#include "Dxf/CheckpointRestore.h"
#include "Dxf/SaveError.h"
#include "Toolbox/Compiler.h"
namespace Dxf::SavePrivate
{
/**
 * 初期文書と採取で共通の論理条件を確認する。
 * @param Value 必要な条件。falseなら不正文書の例外を出す。
 * @param Reason 条件を満たさない理由。
 */
FORCEINLINE void CheckpointRequire(bool Value, const char* Reason)
{
	if (!Value)
	{
		throw FSaveError(ESaveError::InvalidDocument, Reason);
	}
}
/**
 * @param Kind 保存可能な4種類のJoint公開先かを判定する対象。
 */
FORCEINLINE bool IsCheckpointJointExport(EContentExportKind Kind)
{
	return Kind == EContentExportKind::Distance || Kind == EContentExportKind::Revolute || Kind == EContentExportKind::Fixed || Kind == EContentExportKind::Prismatic;
}
/**
 * 初期Jointの要求だけを値へ移し、実行時IDや求解履歴を持ち込まない。
 * @param Joint 信頼する種類別初期設定。
 * @param Instance 所属個体の論理キー。Scene接続では空。
 * @param Export 公開先の論理キー。
 */
template <typename TJoint>
FCheckpointJointState InitialJoint(const TJoint& Joint, Toolbox::FString Instance, Toolbox::FString Export)
{
	// 既存Componentへ再要求できる種類別の論理状態。
	FCheckpointJointState Result;
	Result.Instance = Toolbox::Move(Instance);
	Result.Export = Toolbox::Move(Export);
	Result.Kind = Joint.Kind;
	Result.bConnected = Joint.bConnect;
	switch (Joint.Kind)
	{
	case EJointKind::Distance:
		Result.DistanceLength = Joint.Distance.Length;
		break;
	case EJointKind::Revolute:
		Result.AngularDrive = Joint.Revolute.Drive;
		Result.AngularLimits = Joint.Revolute.Limits;
		break;
	case EJointKind::Prismatic:
		Result.LinearDrive = Joint.Prismatic.Drive;
		Result.LinearLimits = Joint.Prismatic.Limits;
		break;
	case EJointKind::Fixed:
		break;
	}
	return Result;
}
/**
 * 両次元で共有する純粋な初期文書生成。CollectionやWorldを変更しない。
 * @param Catalog 不変の許可構成と明示されたCPUレシピ。
 * @param Limits 確保前に確認する文書・個体・Jointの上限。
 * @param GamePayload 呼出し側が意味を検証した初期進行Object。
 * @param ContentLimits 展開後の初期構成へ適用する上限。
 */
template <typename TDocument, typename TCatalog, typename TInstance>
TDocument InitialDocument(const TCatalog& Catalog, FCheckpointLimits Limits, Toolbox::FStringView GamePayload = "{}", FSceneContentLimits ContentLimits = {})
{
	CheckpointRequire(Catalog.Scene.Instances.Size() == Catalog.InitialPrefabKeys.Size() && Catalog.Scene.Prefabs.Size() == Catalog.Scene.Instances.Size() && Catalog.Scene.Placements.Size() == Catalog.Scene.Instances.Size(), "Checkpoint initial table mismatch");
	CheckpointRequire(GamePayload.Size() <= Limits.Document.MaxBytes, "Checkpoint initial game payload exceeds limit");
	CheckpointRequire(Catalog.Prefabs.Size() <= Limits.MaxCatalogEntries, "Checkpoint initial catalog exceeds limit");
	// 呼出し側の値を所有コピーし、共有カタログへ書き込まない。
	TDocument Result;
	Result.GamePayload = Toolbox::FString(GamePayload.Data(), GamePayload.Size());
	Result.Identity = Catalog.Identity;
	Result.DefinitionFingerprint = Catalog.Scene.ContentFingerprint;
	(void)FCheckpointCodec::Encode(Result, Limits);
	// 意味のある初期生成順は配列の順序のまま残す。
	for (Toolbox::size_t I = 0; I < Catalog.Scene.Instances.Size(); ++I)
	{
		CheckpointRequire(Result.Instances.Size() < Limits.MaxInstances, "Checkpoint instance limit exceeded");
		// この配置の不変Prefab設定。
		const auto& Prefab = Catalog.Scene.Prefabs[I];
		// 生存する初期個体の論理記録。
		TInstance Instance;
		Instance.Key = Catalog.Scene.Instances[I];
		Instance.Prefab = Catalog.InitialPrefabKeys[I];
		Instance.Placement = Catalog.Scene.Placements[I];
		Instance.Parameters = Prefab.Parameters;
		// 許可したPrefabキーが見つかったか。
		bool Found = false;
		for (const auto& Recipe : Catalog.Prefabs)
		{
			if (Recipe.Key == Instance.Prefab)
			{
				Instance.DefinitionFingerprint = Recipe.Definition.ContentFingerprint;
				Found = true;
			}
		}
		CheckpointRequire(Found, "Checkpoint initial Prefab is not allowed");
		for (const auto& Export : Prefab.Exports)
		{
			if (IsCheckpointJointExport(Export.Kind))
			{
				CheckpointRequire(Result.Joints.Size() < Limits.MaxJoints && Export.Index < Prefab.Joints.Size(), "Checkpoint Joint limit or export invalid");
				Result.Joints.PushBack(InitialJoint(Prefab.Joints[Export.Index], Instance.Key, Export.Id));
			}
		}
		Result.Instances.PushBack(Toolbox::Move(Instance));
	}
	for (const auto& Connection : Catalog.Scene.Connections)
	{
		CheckpointRequire(Result.Joints.Size() < Limits.MaxJoints, "Checkpoint Joint limit exceeded");
		Result.Joints.PushBack(InitialJoint(Connection.Joint, {}, Connection.Joint.Id));
	}
	// 初期定義と公開先の整合を既存の純粋な再開検証にも通す。
	(void)FCheckpointRestore::Build(Result, Catalog, Limits, ContentLimits);
	return Result;
}
} // namespace Dxf::SavePrivate
#endif
