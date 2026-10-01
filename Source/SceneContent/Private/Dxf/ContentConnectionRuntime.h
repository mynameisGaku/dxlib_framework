// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_CONNECTION_RUNTIME_H
#define DXF_CONTENT_CONNECTION_RUNTIME_H
#include "Dxf/PrefabRuntime.h"
#include "Dxf/ContentScene2D.h"
#include "Dxf/ContentScene3D.h"
namespace Dxf::ContentPrivate
{
/**
 * 別個体のBodyを所有せず、既存Joint ComponentだけをCollectionへ追加する。
 */
template <typename T, typename TScene, typename TDefinition>
class TContentConnections final : public DGameObject
{
public:
	TContentConnections(TScene& Scene, Toolbox::TSharedPtr<const TDefinition> Definition)
	    : m_pScene(&Scene), m_pDefinition(Toolbox::Move(Definition))
	{
	}
	TObjectHandle<typename T::FDistance> Distance(Toolbox::FStringView Id) const
	{
		return Require(Id, EJointKind::Distance).Distance;
	}
	TObjectHandle<typename T::FRevolute> Revolute(Toolbox::FStringView Id) const
	{
		return Require(Id, EJointKind::Revolute).Revolute;
	}
	TObjectHandle<typename T::FFixed> Fixed(Toolbox::FStringView Id) const
	{
		return Require(Id, EJointKind::Fixed).Fixed;
	}
	TObjectHandle<typename T::FPrismatic> Prismatic(Toolbox::FStringView Id) const
	{
		return Require(Id, EJointKind::Prismatic).Prismatic;
	}

protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		m_Entries.Reserve(m_pDefinition->Connections.Size());
		for (const auto& C : m_pDefinition->Connections)
		{
			const auto A = Resolve(C.BodyA);
			const auto B = Resolve(C.BodyB);
			FEntry Entry;
			Entry.Id = C.Joint.Id;
			Entry.Kind = C.Joint.Kind;
			switch (C.Joint.Kind)
			{
			case EJointKind::Distance:
				Entry.Distance = Add<typename T::FDistance, typename T::FDistanceDescription>(A, B, C.Joint.Distance, C.Joint.bConnect);
				break;
			case EJointKind::Revolute:
				Entry.Revolute = Add<typename T::FRevolute, typename T::FRevoluteDescription>(A, B, C.Joint.Revolute, C.Joint.bConnect);
				break;
			case EJointKind::Fixed:
				Entry.Fixed =
				    Add<typename T::FFixed, typename T::FFixedDescription>(A, B, C.Joint.Fixed, C.Joint.bConnect);
				break;
			case EJointKind::Prismatic:
				Entry.Prismatic = Add<typename T::FPrismatic, typename T::FPrismaticDescription>( A, B, C.Joint.Prismatic, C.Joint.bConnect);
				break;
			default:
				throw Toolbox::FException("Unknown Scene connection kind");
			}
			m_Entries.PushBack(Toolbox::Move(Entry));
		}
		return {};
	}

private:
	// indexは準備で解決済み。文字列はComponent公開先を初期化時に一度だけ取得する。
	typename T::FReference Resolve(const FContentSceneEndpoint& E) const
	{
		if (E.Instance >= m_pDefinition->Instances.Size())
		{
			throw Toolbox::FException("Scene connection instance index mismatch");
		}
		const auto P = m_pScene->GetPrefab(m_pDefinition->Instances[E.Instance]);
		if (E.Export.Kind == EContentExportKind::RigidBody)
		{
			return T::FReference::FromRigidBody(P.Get()->GetRigidBody(E.Export.Id));
		}
		if (E.Export.Kind == EContentExportKind::KinematicMover)
		{
			return T::FReference::FromKinematicMover(P.Get()->GetKinematicMover(E.Export.Id));
		}
		throw Toolbox::FException("Scene connection requires Body export");
	}
	template <typename TComponent, typename TSettings, typename TJoint>
	TObjectHandle<TComponent> Add(typename T::FReference A, typename T::FReference B, const TJoint& Joint, bool Connected)
	{
		TSettings Settings;
		Settings.BodyA = A;
		Settings.BodyB = B;
		Settings.Joint = Joint;
		const auto Result = AddComponent<TComponent>(Settings);
		if (!Result)
		{
			throw Toolbox::FException(Result.Error().Message);
		}
		if (!Connected)
		{
			Result.Value().Get()->RequestDisconnect();
		}
		return Result.Value();
	}
	struct FEntry
	{
		Toolbox::FString Id;
		EJointKind Kind = EJointKind::Distance;
		TObjectHandle<typename T::FDistance> Distance;
		TObjectHandle<typename T::FRevolute> Revolute;
		TObjectHandle<typename T::FFixed> Fixed;
		TObjectHandle<typename T::FPrismatic> Prismatic;
	};
	const FEntry& Require(Toolbox::FStringView Id, EJointKind Kind) const
	{
		for (const auto& E : m_Entries)
		{
			if (E.Id == Id)
			{
				if (E.Kind != Kind)
				{
					throw Toolbox::FException("Scene connection kind mismatch");
				}
				return E;
			}
		}
		throw Toolbox::FException("Scene connection not initialized or unknown");
	}
	// SceneのCollectionがこのObjectも参照先Prefabも所有する。
	TScene* m_pScene;
	Toolbox::TSharedPtr<const TDefinition> m_pDefinition;
	// 専任Componentへの非所有参照。Bodyの解放やStepを行わない。
	Toolbox::TVector<FEntry> m_Entries;
};
using DContentConnections2D = TContentConnections<FRuntime2D, DContentScene2D, FSceneDefinition2D>;
using DContentConnections3D = TContentConnections<FRuntime3D, DContentScene3D, FSceneDefinition3D>;
} // namespace Dxf::ContentPrivate
#endif
