// SPDX-License-Identifier: NOASSERTION
#include "Dxf/PrefabRuntime.h"
namespace Dxf
{
DPrefabInstance2D::DPrefabInstance2D(FPreparedPrefab2D Prepared, FPrefabSpawnOptions2D Placement)
    : m_pRuntime(Toolbox::MakeUnique<ContentPrivate::FPrefabRuntime2D>(Toolbox::Move(Prepared), Placement))
{
}
DPrefabInstance2D::~DPrefabInstance2D() = default;
EPrefabInstanceState DPrefabInstance2D::GetState() const noexcept
{
	return IsDestroyRequested() ? EPrefabInstanceState::Destroyed : m_pRuntime->State();
}
const FPrefabDefinition2D& DPrefabInstance2D::GetDefinition() const noexcept
{
	return m_pRuntime->Definition();
}
TResult<void> DPrefabInstance2D::OnInitialize(const FInitContext& Context)
{
	m_pRuntime->Initialize(*this, Context.Assets);
	return TResult<void>::Success();
}
void DPrefabInstance2D::OnFixedTick(const FFixedTickContext& C)
{
	if (!m_pRuntime->RequiresPhysics())
	{
		return;
	}
	m_pRuntime->Begin(C);
	C.PrePhysicsStep->Enqueue(*this);
}
void DPrefabInstance2D::OnPrePhysicsStep_Internal(const FFixedTickContext& C)
{
	if (IsDestroyRequested())
	{
		return;
	}
	m_pRuntime->BeforeStep();
	C.PostPhysicsStep->Enqueue(*this);
}
void DPrefabInstance2D::OnPostPhysicsStep_Internal(const FFixedTickContext&)
{
	m_pRuntime->AfterStep();
}
bool DPrefabInstance2D::IsPostPhysicsStepAlive_Internal() const noexcept
{
	return !IsDestroyRequested();
}
void DPrefabInstance2D::OnDeinitialize() noexcept
{
	m_pRuntime->Shutdown();
}
const FSound& DPrefabInstance2D::GetSound(Toolbox::FStringView N) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Sound(N);
}
TObjectHandle<DRigidBody2DComponent> DPrefabInstance2D::GetRigidBody(Toolbox::FStringView N) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Rigid(N);
}
TObjectHandle<DKinematicMover2DComponent> DPrefabInstance2D::GetKinematicMover(Toolbox::FStringView N) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Mover(N);
}
TObjectHandle<DDistanceJoint2DComponent> DPrefabInstance2D::GetDistanceJoint(Toolbox::FStringView N) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Distance(N);
}
TObjectHandle<DRevoluteJoint2DComponent> DPrefabInstance2D::GetRevoluteJoint(Toolbox::FStringView N) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Revolute(N);
}
TObjectHandle<DFixedJoint2DComponent> DPrefabInstance2D::GetFixedJoint(Toolbox::FStringView N) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Fixed(N);
}
TObjectHandle<DPrismaticJoint2DComponent> DPrefabInstance2D::GetPrismaticJoint(Toolbox::FStringView N) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Prismatic(N);
}
TObjectHandle<DContactListener2DComponent> DPrefabInstance2D::GetContactListener(Toolbox::FStringView N) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Listener(N);
}

const FTexture& DPrefabInstance2D::GetTexture(Toolbox::FStringView Name) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Texture(Name);
}
const FModel& DPrefabInstance2D::GetModel(Toolbox::FStringView Name) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Model(Name);
}
const FFont& DPrefabInstance2D::GetFont(Toolbox::FStringView Name) const
{
	if (IsDestroyRequested())
	{
		throw Toolbox::FException("Prefab destroyed");
	}
	return m_pRuntime->Font(Name);
}
void DPrefabInstance2D::OnTick(const FTickContext& Context)
{
	m_pRuntime->Tick(Context);
}
TResult<void> DPrefabInstance2D::DrawContent(FRenderContext& Render, const FContentView2D& View) const
{
	if (IsDestroyRequested())
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Prefab destroyed");
	}
	try
	{
		m_pRuntime->Draw(Render, View);
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
