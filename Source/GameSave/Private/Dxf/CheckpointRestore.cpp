// SPDX-License-Identifier: NOASSERTION
#include "Dxf/CheckpointRestore.h"
#include "Dxf/SaveCodec.h"
#include "Dxf/SaveError.h"
#include "Dxf/SceneContentValidation.h"
namespace Dxf
{
namespace
{
using namespace Toolbox;
// 不正な保存やカタログは、資源取得やScene要求の前に拒否する。
void Require(bool Condition, const char* Message)
{
	if (!Condition)
	{
		throw FSaveError(ESaveError::InvalidDocument, Message);
	}
}
void Compatible(bool Condition, const char* Message)
{
	if (!Condition)
	{
		throw FSaveError(ESaveError::IncompatibleContent, Message);
	}
}
bool Same(const FContentParameterValue& A, const FContentParameterValue& B)
{
	if (A.Id != B.Id || A.Kind != B.Kind)
	{
		return false;
	}
	switch (A.Kind)
	{
	case EContentParameterKind::Boolean:
		return A.Boolean == B.Boolean;
	case EContentParameterKind::Number:
		return A.Number == B.Number;
	case EContentParameterKind::Vector2:
		return A.Vector2.X == B.Vector2.X && A.Vector2.Y == B.Vector2.Y;
	case EContentParameterKind::Vector3:
		return A.Vector3.X == B.Vector3.X && A.Vector3.Y == B.Vector3.Y && A.Vector3.Z == B.Vector3.Z;
	case EContentParameterKind::Color:
		return A.Color.R == B.Color.R && A.Color.G == B.Color.G && A.Color.B == B.Color.B && A.Color.A == B.Color.A;
	case EContentParameterKind::Asset:
		return A.Asset == B.Asset;
	}
	return false;
}
bool SameParameters(const TVector<FContentParameterValue>& A, const TVector<FContentParameterValue>& B)
{
	if (A.Size() != B.Size())
	{
		return false;
	}
	for (const auto& Value : A)
	{
		bool Found = false;
		for (const auto& Candidate : B)
		{
			Found = Found || Same(Value, Candidate);
		}
		if (!Found)
		{
			return false;
		}
	}
	return true;
}
// 保存には配列位置を持たせず、今回の新Sceneで一度だけ解決する。
template <typename T>
size_t FindInstance(const T& Scene, FStringView Key)
{
	for (size_t I = 0; I < Scene.Instances.Size(); ++I)
	{
		if (Scene.Instances[I] == Key)
		{
			return I;
		}
	}
	return Scene.Instances.Size();
}
template <typename T>
const auto& Recipe(const T& Catalog, FStringView Key)
{
	for (const auto& P : Catalog.Prefabs)
	{
		if (P.Key == Key)
		{
			return P;
		}
	}
	throw FSaveError(ESaveError::IncompatibleContent, "Checkpoint Prefab is not in allowed catalog");
}
const FCheckpointJointState& State(const TVector<FCheckpointJointState>& Values, FStringView Instance, FStringView Export)
{
	for (const auto& Value : Values)
	{
		if (Value.Instance == Instance && Value.Export == Export)
		{
			return Value;
		}
	}
	throw FSaveError(ESaveError::InvalidDocument, "Checkpoint is missing a Joint state");
}
EJointKind JointKind(EContentExportKind Kind)
{
	switch (Kind)
	{
	case EContentExportKind::Distance:
		return EJointKind::Distance;
	case EContentExportKind::Revolute:
		return EJointKind::Revolute;
	case EContentExportKind::Fixed:
		return EJointKind::Fixed;
	case EContentExportKind::Prismatic:
		return EJointKind::Prismatic;
	default:
		throw FSaveError(ESaveError::InvalidDocument, "Checkpoint export is not a Joint");
	}
}
bool IsJoint(EContentExportKind Kind)
{
	return Kind == EContentExportKind::Distance || Kind == EContentExportKind::Revolute || Kind == EContentExportKind::Fixed || Kind == EContentExportKind::Prismatic;
}
// 同じJointの別名は許すが、別名を使った矛盾する操作は採用しない。
bool SameTarget(const TOptional<FAngularJointTargetSettings>& A, const TOptional<FAngularJointTargetSettings>& B)
{
	if (static_cast<bool>(A) != static_cast<bool>(B))
	{
		return false;
	}
	return !A || (A->TargetAngle == B->TargetAngle && A->MaxAngularSpeed == B->MaxAngularSpeed && A->SlowdownAngle == B->SlowdownAngle && A->AngleTolerance == B->AngleTolerance && A->AngularSpeedTolerance == B->AngularSpeedTolerance && A->MaxTorque == B->MaxTorque);
}
bool SameTarget(const TOptional<FLinearJointTargetSettings>& A, const TOptional<FLinearJointTargetSettings>& B)
{
	if (static_cast<bool>(A) != static_cast<bool>(B))
	{
		return false;
	}
	return !A || (A->TargetTranslation == B->TargetTranslation && A->MaxSpeed == B->MaxSpeed && A->SlowdownDistance == B->SlowdownDistance && A->PositionTolerance == B->PositionTolerance && A->SpeedTolerance == B->SpeedTolerance && A->MaxForce == B->MaxForce);
}
bool SameState(const FCheckpointJointState& A, const FCheckpointJointState& B)
{
	if (A.Kind != B.Kind || A.bConnected != B.bConnected)
	{
		return false;
	}
	switch (A.Kind)
	{
	case EJointKind::Distance:
		return static_cast<bool>(A.DistanceLength) == static_cast<bool>(B.DistanceLength) && (!A.DistanceLength || *A.DistanceLength == *B.DistanceLength);
	case EJointKind::Fixed:
		return true;
	case EJointKind::Revolute:
		return A.AngularDrive.bEnabled == B.AngularDrive.bEnabled && A.AngularDrive.TargetAngularSpeed == B.AngularDrive.TargetAngularSpeed && A.AngularDrive.MaxTorque == B.AngularDrive.MaxTorque && A.AngularLimits.bEnabled == B.AngularLimits.bEnabled && A.AngularLimits.LowerAngle == B.AngularLimits.LowerAngle && A.AngularLimits.UpperAngle == B.AngularLimits.UpperAngle && SameTarget(A.AngularTarget, B.AngularTarget);
	case EJointKind::Prismatic:
		return A.LinearDrive.bEnabled == B.LinearDrive.bEnabled && A.LinearDrive.TargetSpeed == B.LinearDrive.TargetSpeed && A.LinearDrive.MaxForce == B.LinearDrive.MaxForce && A.LinearLimits.bEnabled == B.LinearLimits.bEnabled && A.LinearLimits.LowerTranslation == B.LinearLimits.LowerTranslation && A.LinearLimits.UpperTranslation == B.LinearLimits.UpperTranslation && SameTarget(A.LinearTarget, B.LinearTarget);
	}
	return false;
}
template <typename T>
void Apply(T& Joint, const FCheckpointJointState& Saved)
{
	Require(Joint.Kind == Saved.Kind, "Checkpoint Joint kind mismatch");
	Joint.bConnect = Saved.bConnected;
	switch (Joint.Kind)
	{
	case EJointKind::Distance:
		if (Saved.DistanceLength)
		{
			Joint.Distance.Length = *Saved.DistanceLength;
		}
		break;
	case EJointKind::Revolute:
		Joint.Revolute.Drive = Saved.AngularDrive;
		Joint.Revolute.Limits = Saved.AngularLimits;
		break;
	case EJointKind::Prismatic:
		Joint.Prismatic.Drive = Saved.LinearDrive;
		Joint.Prismatic.Limits = Saved.LinearLimits;
		break;
	case EJointKind::Fixed:
		break;
	default:
		Require(false, "Unknown checkpoint Joint kind");
	}
}
template <typename TScene>
FContentSceneEndpoint Endpoint(const TScene& Original, const TScene& Restored, const FContentSceneEndpoint& E)
{
	Require(E.Instance < Original.Instances.Size(), "Catalog endpoint index mismatch");
	const size_t Index = FindInstance(Restored, Original.Instances[E.Instance]);
	Require(Index < Restored.Instances.Size(), "Connected checkpoint endpoint was removed");
	for (const auto& Export : Restored.Prefabs[Index].Exports)
	{
		if (Export.Id == E.Export.Id)
		{
			Require(Export.Kind == E.Export.Kind, "Checkpoint Body export kind mismatch");
			return {static_cast<uint32>(Index), Export};
		}
	}
	throw FSaveError(ESaveError::InvalidDocument, "Checkpoint endpoint export is missing");
}
template <typename TPlan, typename TDocument, typename TCatalog>
TPlan Build(const TDocument& D, const TCatalog& C, FSceneContentLimits Limits, uint32 CatalogLimit)
{
	// カタログの定義も既存の純粋検証に通し、壊れた参照を読まない。
	Require(C.Identity.Payload == "{}", "Checkpoint catalog identity must have empty payload");
	FSaveCodec::RequireCompatible(D.Identity, C.Identity);
	Compatible(D.DefinitionFingerprint == C.Scene.ContentFingerprint, "Checkpoint Scene definition fingerprint mismatch");
	ValidateSceneDefinition(C.Scene, Limits);
	Require(C.InitialPrefabKeys.Size() == C.Scene.Instances.Size(), "Checkpoint initial Prefab table mismatch");
	Require(C.Prefabs.Size() <= CatalogLimit, "Checkpoint catalog count limit");
	for (size_t I = 0; I < C.Prefabs.Size(); ++I)
	{
		Require(!C.Prefabs[I].Key.IsEmpty(), "Checkpoint catalog key is empty");
		ValidatePrefabDefinition(C.Prefabs[I].Definition, Limits);
		for (size_t J = 0; J < I; ++J)
		{
			Require(C.Prefabs[J].Key != C.Prefabs[I].Key, "Duplicate checkpoint catalog key");
		}
	}
	// 初期個体の欠落を除去と推測しない。明示した存在記録を全個体で要求する。
	for (size_t I = 0; I < C.Scene.Instances.Size(); ++I)
	{
		bool Found = false;
		const auto& P = Recipe(C, C.InitialPrefabKeys[I]);
		Compatible(P.Definition.Path == C.Scene.Prefabs[I].Path, "Checkpoint initial Prefab recipe mismatch");
		for (const auto& S : D.Instances)
		{
			if (S.Key == C.Scene.Instances[I])
			{
				Require(!S.bDynamic, "Initial checkpoint instance marked dynamic");
				Compatible(S.Prefab == P.Key, "Initial checkpoint Prefab key mismatch");
				Found = true;
			}
		}
		Require(Found, "Checkpoint is missing an initial instance");
	}
	TPlan Result;
	Result.Scene = C.Scene;
	Result.Scene.Prefabs.Clear();
	Result.Scene.Placements.Clear();
	Result.Scene.Instances.Clear();
	Result.Scene.Connections.Clear();
	Result.GamePayload = D.GamePayload;
	size_t UsedStates = 0;
	for (const auto& S : D.Instances)
	{
		const auto& P = Recipe(C, S.Prefab);
		Compatible(S.DefinitionFingerprint == P.Definition.ContentFingerprint, "Checkpoint Prefab definition fingerprint mismatch");
		const size_t Initial = FindInstance(C.Scene, S.Key);
		Require(S.bDynamic == (Initial == C.Scene.Instances.Size()), "Checkpoint instance origin mismatch");
		if (!S.bPresent)
		{
			continue;
		}
		auto Definition = P.Definition;
		if (!SameParameters(S.Parameters, Definition.Parameters))
		{
			Compatible(static_cast<bool>(P.Resolve), "Checkpoint parameters require an explicit CPU recipe");
			Definition = P.Resolve(S.Parameters);
			Compatible(Definition.ContentFingerprint == P.Definition.ContentFingerprint && Definition.ProjectRoot == P.Definition.ProjectRoot && Definition.Path == P.Definition.Path, "Checkpoint CPU recipe changed trusted definition");
			Require(SameParameters(S.Parameters, Definition.Parameters), "Checkpoint CPU recipe returned different parameters");
		}
		ValidatePrefabDefinition(Definition, Limits);
		TVector<const FCheckpointJointState*> Applied;
		for (size_t I = 0; I < Definition.Joints.Size(); ++I)
		{
			Applied.PushBack(nullptr);
		}
		for (const auto& E : Definition.Exports)
		{
			if (!IsJoint(E.Kind))
			{
				continue;
			}
			const auto& Saved = State(D.Joints, S.Key, E.Id);
			Require(Saved.Kind == JointKind(E.Kind), "Checkpoint export kind mismatch");
			++UsedStates;
			if (Applied[E.Index] != nullptr)
			{
				Require(SameState(*Applied[E.Index], Saved), "Checkpoint Joint aliases have conflicting states");
				continue;
			}
			Applied[E.Index] = &Saved;
			Apply(Definition.Joints[E.Index], Saved);
			if (Saved.AngularTarget || Saved.LinearTarget)
			{
				Result.Targets.PushBack(Saved);
			}
		}
		Result.Scene.Prefabs.PushBack(Move(Definition));
		Result.Scene.Placements.PushBack(S.Placement);
		Result.Scene.Instances.PushBack(S.Key);
	}
	for (const auto& Cn : C.Scene.Connections)
	{
		const auto& Saved = State(D.Joints, "", Cn.Joint.Id);
		Require(Saved.Kind == Cn.Joint.Kind, "Checkpoint Scene connection kind mismatch");
		++UsedStates;
		// 除去された個体の解除済み接続は、実体を作らず論理履歴だけに残す。
		const bool MissingA = FindInstance(Result.Scene, C.Scene.Instances[Cn.BodyA.Instance]) == Result.Scene.Instances.Size();
		const bool MissingB = FindInstance(Result.Scene, C.Scene.Instances[Cn.BodyB.Instance]) == Result.Scene.Instances.Size();
		if (MissingA || MissingB)
		{
			Require(!Saved.bConnected && !Saved.AngularTarget && !Saved.LinearTarget, "Removed checkpoint endpoint is still connected or targeted");
			continue;
		}
		auto Restored = Cn;
		Restored.BodyA = Endpoint(C.Scene, Result.Scene, Cn.BodyA);
		Restored.BodyB = Endpoint(C.Scene, Result.Scene, Cn.BodyB);
		Apply(Restored.Joint, Saved);
		Result.Scene.Connections.PushBack(Move(Restored));
		if (Saved.AngularTarget || Saved.LinearTarget)
		{
			Result.Targets.PushBack(Saved);
		}
	}
	Require(UsedStates == D.Joints.Size(), "Checkpoint contains unknown Joint state");
	ValidateSceneDefinition(Result.Scene, Limits);
	return Result;
}
// ゲームが渡したCPU処理の例外は失敗として返し、旧Sceneを操作しない。
template <typename TPlan, typename TDocument, typename TCatalog>
TPlan CheckedBuild(const TDocument& D, const TCatalog& C, FSceneContentLimits Limits, uint32 CatalogLimit)
{
	try
	{
		return Build<TPlan>(D, C, Limits, CatalogLimit);
	}
	catch (const FSaveError&)
	{
		throw;
	}
	catch (const FException& E)
	{
		throw FSaveError(ESaveError::InvalidDocument, E.What());
	}
}
} // namespace
FCheckpointResumePlan2D FCheckpointRestore::Build(const FCheckpointDocument2D& D, const FCheckpointCatalog2D& C, FCheckpointLimits L, FSceneContentLimits CL)
{
	const auto Validated = FCheckpointCodec::Decode2D(FCheckpointCodec::Encode(D, L), L);
	return CheckedBuild<FCheckpointResumePlan2D>(Validated, C, CL, L.MaxCatalogEntries);
}
FCheckpointResumePlan3D FCheckpointRestore::Build(const FCheckpointDocument3D& D, const FCheckpointCatalog3D& C, FCheckpointLimits L, FSceneContentLimits CL)
{
	const auto Validated = FCheckpointCodec::Decode3D(FCheckpointCodec::Encode(D, L), L);
	return CheckedBuild<FCheckpointResumePlan3D>(Validated, C, CL, L.MaxCatalogEntries);
}
} // namespace Dxf
