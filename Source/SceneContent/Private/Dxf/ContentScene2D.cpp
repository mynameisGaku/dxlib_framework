// SPDX-License-Identifier: NOASSERTION
#include "Dxf/ContentScene2D.h"
#include "Dxf/ContentConnectionRuntime.h"
#include "Dxf/SceneContentValidation.h"
namespace Dxf
{
FPreparedScene2D PrepareScene(FSceneDefinition2D Definition, FAssetService& Assets)
{
	ValidateSceneDefinition(Definition);
	if (Definition.Prefabs.Size() != Definition.Placements.Size() || Definition.Prefabs.Size() != Definition.Instances.Size())
	{
		throw Toolbox::FException("Content Scene table size mismatch");
	}
	if (!Definition.Assets.IsEmpty() && !Definition.ProjectRoot.IsEmpty() && Assets.GetProjectRoot().Normalize().ToUtf8() != Definition.ProjectRoot)
	{
		throw Toolbox::FException("Scene ProjectRoot differs from AssetService");
	}
	FPreparedScene2D Result;
	Result.Resources =
	    Toolbox::MakeShared<FContentResources>(FContentResources::Prepare(Definition.Assets, Assets, Definition.Path));
	Result.Prefabs.Reserve(Definition.Prefabs.Size());
	for (const auto& P : Definition.Prefabs)
	{
		Result.Prefabs.PushBack(PreparePrefab(P, Assets));
	}
	Result.Definition = Toolbox::MakeShared<FSceneDefinition2D>(Toolbox::Move(Definition));
	return Result;
}
DContentScene2D::DContentScene2D(FPreparedScene2D Prepared, Toolbox::TOptional<Toolbox::FFixedStepSettings> Settings)
    : DPhysicsScene2D(Settings              ? *Settings : Prepared.Definition ? Prepared.Definition->FixedUpdate : Toolbox::FFixedStepSettings{}),
      m_Prepared(Toolbox::Move(Prepared))
{
	if (!m_Prepared.Definition || m_Prepared.Prefabs.Size() != m_Prepared.Definition->Instances.Size() || m_Prepared.Prefabs.Size() != m_Prepared.Definition->Placements.Size())
	{
		throw Toolbox::FException("Content Scene is not prepared");
	}
	if (m_Prepared.Definition->ViewCount == 0 || m_Prepared.Definition->ViewCount > 2)
	{
		throw Toolbox::FException("Content Scene needs one or two views");
	}
	SetViews(m_Prepared.Definition->Views[0], m_Prepared.Definition->ViewCount == 2 ? Toolbox::TOptional<FContentView2D>{m_Prepared.Definition->Views[1]} : Toolbox::TOptional<FContentView2D>{});
}
TResult<void> DContentScene2D::OnInitialize(const FInitContext&)
{
	GetPhysicsWorld().SetGravity(m_Prepared.Definition->Gravity);
	m_Instances.Reserve(m_Prepared.Prefabs.Size());
	m_Ids.Reserve(m_Prepared.Prefabs.Size());
	for (Toolbox::size_t I = 0; I < m_Prepared.Prefabs.Size(); ++I)
	{
		auto Spawned = SpawnPrefab(m_Prepared.Definition->Instances[I], m_Prepared.Prefabs[I], m_Prepared.Definition->Placements[I]);
		if (!Spawned)
		{
			return TResult<void>::Failure(Spawned.Error());
		}
	}
	if (!m_Prepared.Definition->Connections.IsEmpty())
	{
		const auto Connections = Spawn<ContentPrivate::DContentConnections2D>(*this, m_Prepared.Definition);
		if (!Connections)
		{
			return TResult<void>::Failure(Connections.Error());
		}
		m_Connections = Connections.Value().Cast<DGameObject>();
	}
	return TResult<void>::Success();
}
TObjectHandle<DPrefabInstance2D> DContentScene2D::GetPrefab(Toolbox::FStringView Id) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Content Scene destroyed");
	}
	for (Toolbox::size_t I = 0; I < m_Ids.Size(); ++I)
	{
		if (m_Ids[I] == Id)
		{
			if (I >= m_Instances.Size() || !m_Instances[I])
			{
				throw Toolbox::FException("Content instance not available");
			}
			return m_Instances[I];
		}
	}
	throw Toolbox::FException("Content instance does not exist");
}
// 動的配置もCollectionへ受付し、描画追跡表は非所有handleだけを持つ。
TResult<TObjectHandle<DPrefabInstance2D>> DContentScene2D::SpawnPrefab(Toolbox::FString Id, FPreparedPrefab2D Prepared, FPrefabSpawnOptions2D Placement)
{
	if (Id.IsEmpty() || Id.Size() > 128)
	{
		return TResult<TObjectHandle<DPrefabInstance2D>>::Failure(EErrorCode::InvalidArgument, "Invalid Content instance ID");
	}
	for (Toolbox::size_t I = 0; I < Id.Size(); ++I)
	{
		const char C = Id[I];
		const bool Letter = (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || C == '_';
		const bool Tail = Letter || (C >= '0' && C <= '9') || C == '-';
		if (!(I == 0 ? Letter : Tail))
		{
			return TResult<TObjectHandle<DPrefabInstance2D>>::Failure(EErrorCode::InvalidArgument, "Invalid Content instance ID");
		}
	}
	Toolbox::size_t Slot = m_Instances.Size();
	for (Toolbox::size_t I = 0; I < m_Ids.Size(); ++I)
	{
		if (m_Ids[I] == Id && m_Instances[I])
		{
			return TResult<TObjectHandle<DPrefabInstance2D>>::Failure(EErrorCode::InvalidArgument, "Duplicate Content instance ID");
		}
		if (!m_Instances[I] && Slot == m_Instances.Size())
		{
			Slot = I;
		}
	}
	if (!Prepared.Definition || !Prepared.Resources)
	{
		return TResult<TObjectHandle<DPrefabInstance2D>>::Failure(EErrorCode::InvalidArgument, "Prefab is not prepared");
	}
	// Sensorの既存Listenerが必要とするイベント領域は、Object受付より前に準備する。
	bool Sensor = false;
	for (const auto& Part : Prepared.Definition->Parts)
	{
		for (const auto& Collider : Part.Colliders)
		{
			Sensor = Sensor || Collider.Response == EColliderResponse::Sensor;
		}
	}
	if (Sensor && !GetPhysicsWorld().GetEventSettings().bEnabled)
	{
		auto Events = GetPhysicsWorld().GetEventSettings();
		Events.bEnabled = true;
		GetPhysicsWorld().SetEventSettings(Events);
	}
	// 受付後の追跡表追加が確保失敗しないよう、両表を先に予約する。
	m_Instances.Reserve(Slot + 1);
	m_Ids.Reserve(Slot + 1);
	const auto Spawned = Spawn<DPrefabInstance2D>(Toolbox::Move(Prepared), Placement);
	if (!Spawned)
	{
		return Spawned;
	}
	if (Slot == m_Instances.Size())
	{
		m_Instances.PushBack(Spawned.Value());
		m_Ids.PushBack(Toolbox::Move(Id));
	}
	else
	{
		m_Instances[Slot] = Spawned.Value();
		m_Ids[Slot] = Toolbox::Move(Id);
	}
	return Spawned;
}
void DContentScene2D::SetViews(const FContentView2D& First, const Toolbox::TOptional<FContentView2D>& Second)
{
	if (!IsValidContentView2D(First) || (Second && !IsValidContentView2D(*Second)))
	{
		throw Toolbox::FException("Invalid Content Scene view");
	}
	m_Views[0] = First;
	if (Second)
	{
		m_Views[1] = *Second;
	}
	m_ViewCount = Second ? 2 : 1;
}
void DContentScene2D::OnDraw(FRenderContext& Render) const
{
	for (Toolbox::uint32 View = 0; View < m_ViewCount; ++View)
	{
		for (const auto& Instance : m_Instances)
		{
			if (!Instance || Instance.Get()->GetState() == EPrefabInstanceState::PendingInitialization)
			{
				continue;
			}
			const auto Drawn = Instance.Get()->DrawContent(Render, m_Views[View]);
			if (!Drawn)
			{
				throw Toolbox::FException(Drawn.Error().Message);
			}
		}
	}
}
TObjectHandle<DDistanceJoint2DComponent> DContentScene2D::GetDistanceConnection(Toolbox::FStringView Id) const
{
	if (!m_Connections || !m_Connections.Get()->IsInitialized())
	{
		throw Toolbox::FException("Scene connections are not initialized");
	}
	const auto Handle = static_cast<const ContentPrivate::DContentConnections2D*>(m_Connections.Get())->Distance(Id);
	if (!Handle)
	{
		throw Toolbox::FException("Scene connection expired");
	}
	return Handle;
}
TObjectHandle<DRevoluteJoint2DComponent> DContentScene2D::GetRevoluteConnection(Toolbox::FStringView Id) const
{
	if (!m_Connections || !m_Connections.Get()->IsInitialized())
	{
		throw Toolbox::FException("Scene connections are not initialized");
	}
	const auto Handle = static_cast<const ContentPrivate::DContentConnections2D*>(m_Connections.Get())->Revolute(Id);
	if (!Handle)
	{
		throw Toolbox::FException("Scene connection expired");
	}
	return Handle;
}
TObjectHandle<DFixedJoint2DComponent> DContentScene2D::GetFixedConnection(Toolbox::FStringView Id) const
{
	if (!m_Connections || !m_Connections.Get()->IsInitialized())
	{
		throw Toolbox::FException("Scene connections are not initialized");
	}
	const auto Handle = static_cast<const ContentPrivate::DContentConnections2D*>(m_Connections.Get())->Fixed(Id);
	if (!Handle)
	{
		throw Toolbox::FException("Scene connection expired");
	}
	return Handle;
}
TObjectHandle<DPrismaticJoint2DComponent> DContentScene2D::GetPrismaticConnection(Toolbox::FStringView Id) const
{
	if (!m_Connections || !m_Connections.Get()->IsInitialized())
	{
		throw Toolbox::FException("Scene connections are not initialized");
	}
	const auto Handle = static_cast<const ContentPrivate::DContentConnections2D*>(m_Connections.Get())->Prismatic(Id);
	if (!Handle)
	{
		throw Toolbox::FException("Scene connection expired");
	}
	return Handle;
}
} // namespace Dxf
