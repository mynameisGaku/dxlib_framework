// SPDX-License-Identifier: NOASSERTION
#include "Dxf/PrefabRuntime.h"
namespace Dxf
{
DPrefabInstance3D::DPrefabInstance3D(FPreparedPrefab3D Prepared, FPrefabSpawnOptions3D Placement)
    : m_pRuntime(Toolbox::MakeUnique<ContentPrivate::FPrefabRuntime3D>(Toolbox::Move(Prepared), Placement))
{
}
DPrefabInstance3D::~DPrefabInstance3D() = default;
EPrefabInstanceState DPrefabInstance3D::GetState() const noexcept
{
	return IsDestroyRequested() ? EPrefabInstanceState::Destroyed : m_pRuntime->State();
}
const FPrefabDefinition3D& DPrefabInstance3D::GetDefinition() const noexcept
{
	return m_pRuntime->Definition();
}
TResult<void> DPrefabInstance3D::OnInitialize(const FInitContext& Context)
{
	m_pRuntime->Initialize(*this, Context.Assets);
	return TResult<void>::Success();
}
void DPrefabInstance3D::OnFixedTick(const FFixedTickContext& C)
{
	if (!m_pRuntime->RequiresPhysics())
	{
		return;
	}
	m_pRuntime->Begin(C);
	C.PrePhysicsStep->Enqueue(*this);
}
void DPrefabInstance3D::OnPrePhysicsStep_Internal(const FFixedTickContext& C)
{
	if (IsDestroyRequested())
	{
		return;
	}
	m_pRuntime->BeforeStep();
	C.PostPhysicsStep->Enqueue(*this);
}
void DPrefabInstance3D::OnPostPhysicsStep_Internal(const FFixedTickContext&)
{
	m_pRuntime->AfterStep();
}
bool DPrefabInstance3D::IsPostPhysicsStepAlive_Internal() const noexcept
{
	return !IsDestroyRequested();
}
void DPrefabInstance3D::OnDeinitialize() noexcept
{
	m_pRuntime->Shutdown();
}
const FSound& DPrefabInstance3D::GetSound(Toolbox::FStringView N) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Sound(N);
}
TObjectHandle<DRigidBody3DComponent> DPrefabInstance3D::GetRigidBody(Toolbox::FStringView N) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Rigid(N);
}
TObjectHandle<DKinematicMover3DComponent> DPrefabInstance3D::GetKinematicMover(Toolbox::FStringView N) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Mover(N);
}
TObjectHandle<DDistanceJoint3DComponent> DPrefabInstance3D::GetDistanceJoint(Toolbox::FStringView N) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Distance(N);
}
TObjectHandle<DRevoluteJoint3DComponent> DPrefabInstance3D::GetRevoluteJoint(Toolbox::FStringView N) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Revolute(N);
}
TObjectHandle<DFixedJoint3DComponent> DPrefabInstance3D::GetFixedJoint(Toolbox::FStringView N) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Fixed(N);
}
TObjectHandle<DPrismaticJoint3DComponent> DPrefabInstance3D::GetPrismaticJoint(Toolbox::FStringView N) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Prismatic(N);
}
TObjectHandle<DContactListener3DComponent> DPrefabInstance3D::GetContactListener(Toolbox::FStringView N) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Listener(N);
}

const FTexture& DPrefabInstance3D::GetTexture(Toolbox::FStringView Name) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Texture(Name);
}
const FModel& DPrefabInstance3D::GetModel(Toolbox::FStringView Name) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Model(Name);
}
const FFont& DPrefabInstance3D::GetFont(Toolbox::FStringView Name) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Font(Name);
}
void DPrefabInstance3D::OnTick(const FTickContext& Context)
{
	m_pRuntime->Tick(Context);
}
TResult<void> DPrefabInstance3D::DrawContent(FRenderContext& Render) const
{
	if (IsDestroyRequested())
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Prefab destroyed");
	}
	try
	{
		m_pRuntime->Draw(Render);
		return TResult<void>::Success();
	}
	catch (const Toolbox::FException& Error)
	{
		return TResult<void>::Failure(EErrorCode::UserException, Error.What());
	}
	catch (...)
	{
		return TResult<void>::Failure(EErrorCode::UserException, "Unexpected Prefab drawing exception");
	}
}

} // namespace Dxf
