// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHECKPOINT_CAPTURE_RUNTIME_H
#define DXF_CHECKPOINT_CAPTURE_RUNTIME_H
#include "Dxf/CheckpointCodec.h"
#include "Dxf/SaveCodec.h"
#include "Dxf/CheckpointRestore.h"
#include "Dxf/SaveError.h"
#include "Dxf/CheckpointParameters.h"
namespace Dxf::SavePrivate
{
// 個体やJointの実体は既存CollectionとWorldに置いたまま、論理履歴だけを採取する。
inline void CaptureRequire(bool Value, const char* Reason)
{
	if (!Value)
	{
		throw FSaveError(ESaveError::InvalidDocument, Reason);
	}
}
inline bool CaptureIsJoint(EContentExportKind Kind)
{
	return Kind == EContentExportKind::Distance || Kind == EContentExportKind::Revolute || Kind == EContentExportKind::Fixed || Kind == EContentExportKind::Prismatic;
}
template <typename TJoint>
FCheckpointJointState InitialJoint(const TJoint& Joint, Toolbox::FString Instance, Toolbox::FString Export)
{
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
template <typename TDocument, typename TCatalog, typename TInstance>
TDocument InitialDocument(const TCatalog& Catalog, FCheckpointLimits Limits)
{
	CaptureRequire(Catalog.Scene.Instances.Size() == Catalog.InitialPrefabKeys.Size() && Catalog.Scene.Prefabs.Size() == Catalog.Scene.Instances.Size() && Catalog.Scene.Placements.Size() == Catalog.Scene.Instances.Size(), "Checkpoint initial table mismatch");
	TDocument Result;
	Result.Identity = Catalog.Identity;
	Result.DefinitionFingerprint = Catalog.Scene.ContentFingerprint;
	(void)FCheckpointCodec::Encode(Result, Limits);
	for (Toolbox::size_t I = 0; I < Catalog.Scene.Instances.Size(); ++I)
	{
		CaptureRequire(Result.Instances.Size() < Limits.MaxInstances, "Checkpoint instance limit exceeded");
		const auto& Prefab = Catalog.Scene.Prefabs[I];
		TInstance Instance;
		Instance.Key = Catalog.Scene.Instances[I];
		Instance.Prefab = Catalog.InitialPrefabKeys[I];
		Instance.Placement = Catalog.Scene.Placements[I];
		Instance.Parameters = Prefab.Parameters;
		bool Found = false;
		for (const auto& Recipe : Catalog.Prefabs)
		{
			if (Recipe.Key == Instance.Prefab)
			{
				Instance.DefinitionFingerprint = Recipe.Definition.ContentFingerprint;
				Found = true;
			}
		}
		CaptureRequire(Found, "Checkpoint initial Prefab is not allowed");
		for (const auto& Export : Prefab.Exports)
		{
			if (CaptureIsJoint(Export.Kind))
			{
				CaptureRequire(Result.Joints.Size() < Limits.MaxJoints && Export.Index < Prefab.Joints.Size(), "Checkpoint Joint limit or export invalid");
				Result.Joints.PushBack(InitialJoint(Prefab.Joints[Export.Index], Instance.Key, Export.Id));
			}
		}
		Result.Instances.PushBack(Toolbox::Move(Instance));
	}
	for (const auto& Connection : Catalog.Scene.Connections)
	{
		CaptureRequire(Result.Joints.Size() < Limits.MaxJoints, "Checkpoint Joint limit exceeded");
		Result.Joints.PushBack(InitialJoint(Connection.Joint, {}, Connection.Joint.Id));
	}
	// 初期定義と公開先の整合を既存の純粋な再開検証にも通す。
	(void)FCheckpointRestore::Build(Result, Catalog, Limits);
	return Result;
}
template <typename TComponent>
bool ReadJointBase(TComponent* Component, FCheckpointJointState& Saved)
{
	if (Component == nullptr || Component->HasPendingRequest())
	{
		return false;
	}
	const auto State = Component->GetConnectionState();
	if (State == EJointConnection::PendingBodies || State == EJointConnection::EndpointLost)
	{
		return false;
	}
	Saved.bConnected = State == EJointConnection::Connected;
	return !Saved.bConnected || static_cast<bool>(Component->GetObservation());
}
// Getterは読み取りだけ。種類別Componentから最後に採用された設定を取得する。
template <typename TGetter>
bool ReadLiveJoint(TGetter& Getter, FCheckpointJointState& Saved)
{
	switch (Saved.Kind)
	{
	case EJointKind::Distance:
	{
		auto* Component = Getter.Distance(Saved.Export).Get();
		if (!ReadJointBase(Component, Saved))
		{
			return false;
		}
		Saved.DistanceLength = Component->GetDescription().Joint.Length;
		return true;
	}
	case EJointKind::Revolute:
	{
		auto* Component = Getter.Revolute(Saved.Export).Get();
		if (!ReadJointBase(Component, Saved))
		{
			return false;
		}
		Saved.AngularDrive = Component->GetDescription().Joint.Drive;
		Saved.AngularLimits = Component->GetDescription().Joint.Limits;
		return true;
	}
	case EJointKind::Fixed:
		return ReadJointBase(Getter.Fixed(Saved.Export).Get(), Saved);
	case EJointKind::Prismatic:
	{
		auto* Component = Getter.Prismatic(Saved.Export).Get();
		if (!ReadJointBase(Component, Saved))
		{
			return false;
		}
		Saved.LinearDrive = Component->GetDescription().Joint.Drive;
		Saved.LinearLimits = Component->GetDescription().Joint.Limits;
		return true;
	}
	}
	return false;
}
template <typename TPrefab>
struct FPrefabJointGetter
{
	TPrefab& Prefab;
	auto Distance(Toolbox::FStringView Name)
	{
		return Prefab.GetDistanceJoint(Name);
	}
	auto Revolute(Toolbox::FStringView Name)
	{
		return Prefab.GetRevoluteJoint(Name);
	}
	auto Fixed(Toolbox::FStringView Name)
	{
		return Prefab.GetFixedJoint(Name);
	}
	auto Prismatic(Toolbox::FStringView Name)
	{
		return Prefab.GetPrismaticJoint(Name);
	}
};
template <typename TScene>
struct FSceneJointGetter
{
	TScene& Scene;
	auto Distance(Toolbox::FStringView Name)
	{
		return Scene.GetDistanceConnection(Name);
	}
	auto Revolute(Toolbox::FStringView Name)
	{
		return Scene.GetRevoluteConnection(Name);
	}
	auto Fixed(Toolbox::FStringView Name)
	{
		return Scene.GetFixedConnection(Name);
	}
	auto Prismatic(Toolbox::FStringView Name)
	{
		return Scene.GetPrismaticConnection(Name);
	}
};
template <typename TDocument, typename TInstance, typename TScene, typename TPrefab, typename TPrepared>
class TCheckpointCaptureRuntime
{
public:
	explicit TCheckpointCaptureRuntime(TDocument Document, FCheckpointLimits Limits) : m_Document(Toolbox::Move(Document)), m_Limits(Limits)
	{
		(void)FCheckpointCodec::Encode(m_Document, m_Limits);
	}
	void Bind(TScene& Scene)
	{
		CaptureRequire(!m_bRetired && m_pScene == nullptr && Scene.IsInitialized() && !Scene.IsDestroyRequested(), "Checkpoint Scene cannot be bound");
		CaptureRequire(Scene.GetDefinition().ContentFingerprint == m_Document.DefinitionFingerprint, "Checkpoint Scene fingerprint mismatch");
		Toolbox::TVector<TObjectHandle<TPrefab>> Handles;
		Handles.Reserve(m_Document.Instances.Size());
		for (const auto& Instance : m_Document.Instances)
		{
			Handles.PushBack(Instance.bPresent ? Scene.GetPrefab(Instance.Key) : TObjectHandle<TPrefab>{});
			if (Instance.bPresent)
			{
				const auto& Definition = Handles.Back().Get()->GetDefinition();
				// 初期Scene内のPrefabはScene全体の指紋に含まれる。独立準備には個別指紋がある。
				CaptureRequire(SameParameters(Definition.Parameters, Instance.Parameters), "Checkpoint instance parameter mismatch");
				CaptureRequire(Definition.ContentFingerprint.IsEmpty() || Definition.ContentFingerprint == Instance.DefinitionFingerprint, "Checkpoint Prefab fingerprint mismatch");
			}
		}
		m_Instances = Toolbox::Move(Handles);
		m_pScene = &Scene;
	}
	TResult<TObjectHandle<TPrefab>> Spawn(TScene& Scene, TInstance Instance, TPrepared Prepared)
	{
		RequireScene(Scene);
		CaptureRequire(Instance.bPresent && Prepared.Definition && Prepared.Definition->ContentFingerprint == Instance.DefinitionFingerprint, "Checkpoint dynamic Prefab mismatch");
		TDocument Candidate = m_Document;
		Toolbox::size_t Index = Candidate.Instances.Size();
		for (Toolbox::size_t I = 0; I < Candidate.Instances.Size(); ++I)
		{
			if (Candidate.Instances[I].Key == Instance.Key)
			{
				CaptureRequire(!Candidate.Instances[I].bPresent && !m_Instances[I], "Checkpoint duplicate live key");
				Index = I;
				Instance.bDynamic = Candidate.Instances[I].bDynamic;
			}
		}
		CaptureRequire(Index < Candidate.Instances.Size() || Instance.bDynamic, "New checkpoint instance must be dynamic");
		CaptureRequire(Index < Candidate.Instances.Size() || Candidate.Instances.Size() < m_Limits.MaxInstances, "Checkpoint instance limit exceeded");
		// 履歴の値は準備済みの公開値から採り、呼出し側の未検証値を代用しない。
		Instance.Parameters = Prepared.Definition->Parameters;
		for (Toolbox::size_t I = 0; I < Candidate.Joints.Size();)
		{
			if (Candidate.Joints[I].Instance == Instance.Key)
			{
				Candidate.Joints.Erase(Candidate.Joints.Data() + I);
			}
			else
			{
				++I;
			}
		}
		for (const auto& Export : Prepared.Definition->Exports)
		{
			if (CaptureIsJoint(Export.Kind))
			{
				CaptureRequire(Candidate.Joints.Size() < m_Limits.MaxJoints && Export.Index < Prepared.Definition->Joints.Size(), "Checkpoint Joint limit exceeded");
				Candidate.Joints.PushBack(InitialJoint(Prepared.Definition->Joints[Export.Index], Instance.Key, Export.Id));
			}
		}
		const auto Placement = Instance.Placement;
		Toolbox::FString Key = Instance.Key;
		if (Index == Candidate.Instances.Size())
		{
			Candidate.Instances.PushBack(Toolbox::Move(Instance));
		}
		else
		{
			Candidate.Instances[Index] = Toolbox::Move(Instance);
		}
		(void)FCheckpointCodec::Encode(Candidate, m_Limits);
		// Object受付後に確保しない。次の初期化失敗は既存Application契約へ伝わる。
		m_Instances.Reserve(Candidate.Instances.Size());
		const auto Spawned = Scene.SpawnPrefab(Toolbox::Move(Key), Toolbox::Move(Prepared), Placement);
		if (!Spawned)
		{
			return Spawned;
		}
		if (Index == m_Instances.Size())
		{
			m_Instances.PushBack(Spawned.Value());
		}
		else
		{
			m_Instances[Index] = Spawned.Value();
		}
		m_Document = Toolbox::Move(Candidate);
		return Spawned;
	}
	bool Destroy(TScene& Scene, Toolbox::FStringView Key) noexcept
	{
		if (m_bRetired || m_pScene != &Scene)
		{
			return false;
		}
		for (Toolbox::size_t I = 0; I < m_Document.Instances.Size(); ++I)
		{
			if (m_Document.Instances[I].Key == Key && m_Instances[I] && Scene.Destroy(m_Instances[I]))
			{
				m_Document.Instances[I].bPresent = false;
				m_Instances[I] = {};
				for (Toolbox::size_t J = 0; J < m_Document.Joints.Size();)
				{
					if (m_Document.Joints[J].Instance == Key)
					{
						m_Document.Joints.Erase(m_Document.Joints.Data() + J);
					}
					else
					{
						++J;
					}
				}
				return true;
			}
		}
		return false;
	}
	bool Request() noexcept
	{
		if (m_bRetired)
		{
			return false;
		}
		m_bRequested = true;
		return true;
	}
	bool Request(Toolbox::FString CheckpointKey)
	{
		if (m_bRetired)
		{
			return false;
		}
		auto Identity = m_Document.Identity;
		Identity.CheckpointKey = Toolbox::Move(CheckpointKey);
		(void)FSaveCodec::Encode(Identity, m_Limits.Document);
		m_Document.Identity = Toolbox::Move(Identity);
		m_bRequested = true;
		return true;
	}
	bool Pending() const noexcept
	{
		return m_bRequested && !m_bRetired;
	}
	bool Capture(TScene& Scene, Toolbox::FStringView GamePayload)
	{
		if (!m_bRequested || m_bRetired)
		{
			return false;
		}
		if (!Scene.IsAtPhysicsCompletionBoundary() || Scene.GetCompletedPhysicsStepCount() == m_CapturedStep)
		{
			return false;
		}
		RequireScene(Scene);
		for (Toolbox::size_t I = 0; I < m_Instances.Size(); ++I)
		{
			if (m_Document.Instances[I].bPresent && (!m_Instances[I] || m_Instances[I].Get()->GetState() != EPrefabInstanceState::Ready))
			{
				// 世代が失効した個体を、同名の新個体へ検索し直さない。
				return false;
			}
		}
		TDocument Candidate = m_Document;
		for (Toolbox::size_t I = 0; I < Candidate.Joints.Size();)
		{
			auto& Saved = Candidate.Joints[I];
			if (!Saved.Instance.IsEmpty())
			{
				Toolbox::size_t Index = Candidate.Instances.Size();
				for (Toolbox::size_t N = 0; N < Candidate.Instances.Size(); ++N)
				{
					if (Candidate.Instances[N].Key == Saved.Instance)
					{
						Index = N;
					}
				}
				CaptureRequire(Index < Candidate.Instances.Size(), "Checkpoint Joint instance missing");
				if (!Candidate.Instances[Index].bPresent)
				{
					Candidate.Joints.Erase(Candidate.Joints.Data() + I);
					continue;
				}
				FPrefabJointGetter<TPrefab> Getter{*m_Instances[Index].Get()};
				if (!ReadLiveJoint(Getter, Saved))
				{
					return false;
				}
			}
			else
			{
				bool Declared = false;
				for (const auto& Connection : Scene.GetDefinition().Connections)
				{
					Declared = Declared || Connection.Joint.Id == Saved.Export;
				}
				if (Declared)
				{
					FSceneJointGetter<TScene> Getter{Scene};
					if (!ReadLiveJoint(Getter, Saved))
					{
						return false;
					}
				}
				else
				{
					CaptureRequire(!Saved.bConnected, "Checkpoint connection missing");
				}
			}
			++I;
		}
		Candidate.GamePayload = Toolbox::FString(GamePayload.Data(), GamePayload.Size());
		// codec失敗は旧正常値を維持する。ゲーム固有の意味は採取側codecが検査する。
		(void)FCheckpointCodec::Encode(Candidate, m_Limits);
		m_LastCapture = Toolbox::Move(Candidate);
		m_CapturedStep = Scene.GetCompletedPhysicsStepCount();
		m_bRequested = false;
		return true;
	}
	const Toolbox::TOptional<TDocument>& Last() const noexcept
	{
		return m_LastCapture;
	}
	Toolbox::uint64 Step() const noexcept
	{
		return m_CapturedStep;
	}
	void Retire() noexcept
	{
		m_bRetired = true;
		m_bRequested = false;
		m_pScene = nullptr;
		m_Instances.Clear();
	}

private:
	void RequireScene(const TScene& Scene) const
	{
		CaptureRequire(!m_bRetired && m_pScene == &Scene && Scene.IsInitialized() && !Scene.IsDestroyRequested(), "Checkpoint Scene unavailable");
	}
	// CPUだけの初期構成・操作履歴。現在のBody Poseへ追従して書き換えない。
	TDocument m_Document;
	// Collectionの世代を保持し、通常Tickの名前検索を避ける。
	Toolbox::TVector<TObjectHandle<TPrefab>> m_Instances;
	// 同じ所有側呼出しで比較する非所有Scene。Retire後は参照しない。
	TScene* m_pScene = nullptr;
	// 採取時にも適用する有限上限。
	FCheckpointLimits m_Limits;
	// 最後に成功した一貫した文書。
	Toolbox::TOptional<TDocument> m_LastCapture;
	// 最後に採取した成功固定更新番号。
	Toolbox::uint64 m_CapturedStep = 0;
	// 未採取要求は最新の成功境界へまとめる。
	bool m_bRequested = false;
	// 終了後の再解決を禁止する。
	bool m_bRetired = false;
};
} // namespace Dxf::SavePrivate
#endif
