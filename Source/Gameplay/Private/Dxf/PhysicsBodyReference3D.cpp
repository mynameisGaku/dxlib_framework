// SPDX-License-Identifier: NOASSERTION
#include "Dxf/PhysicsBodyReference3D.h"
namespace Dxf
{
FPhysicsBodyReference3D FPhysicsBodyReference3D::FromRigidBody(TObjectHandle<DRigidBody3DComponent> Handle) noexcept
{
	// 剛体の識別子を保持する。
	FPhysicsBodyReference3D Result;
	Result.m_Kind = EKind::Rigid;
	Result.m_Rigid = Handle;
	return Result;
}
FPhysicsBodyReference3D FPhysicsBodyReference3D::FromKinematicMover(TObjectHandle<DKinematicMover3DComponent> Handle) noexcept
{
	// 支点の識別子を保持する。
	FPhysicsBodyReference3D Result;
	Result.m_Kind = EKind::Mover;
	Result.m_Mover = Handle;
	return Result;
}
FPhysicsBodyReference3D FPhysicsBodyReference3D::FromBodyId(FBodyId3D Id) noexcept
{
	// 明示IDをそのまま保持する。
	FPhysicsBodyReference3D Result;
	Result.m_Kind = EKind::Explicit;
	Result.m_Body = Id;
	return Result;
}
bool FPhysicsBodyReference3D::operator==(const FPhysicsBodyReference3D& Other) const noexcept
{
	return m_Kind == Other.m_Kind && m_Rigid == Other.m_Rigid && m_Mover == Other.m_Mover && m_Body == Other.m_Body;
}
TResult<Toolbox::TOptional<FBodyId3D>> FPhysicsBodyReference3D::Resolve_Internal(const FPhysicsWorld3D& World) const
{
	// 登録完了後に検証するID。
	Toolbox::TOptional<FBodyId3D> Id;
	if (m_Kind == EKind::Explicit)
	{
		Id = m_Body;
	}
	else if (m_Kind == EKind::Rigid)
	{
		// Destroy要求済みもハンドルで除外する。
		const auto* Rigid = m_Rigid.Get();
		if (Rigid == nullptr)
		{
			return TResult<Toolbox::TOptional<FBodyId3D>>::Failure(EErrorCode::InvalidState, "Joint rigid endpoint expired");
		}
		if (!Rigid->IsInitialized() || !Rigid->HasBody())
		{
			return TResult<Toolbox::TOptional<FBodyId3D>>::Success({});
		}
		Id = Rigid->GetBodyId();
	}
	else if (m_Kind == EKind::Mover)
	{
		// 登録前は待機し、破棄後は失敗させる。
		const auto* Mover = m_Mover.Get();
		if (Mover == nullptr)
		{
			return TResult<Toolbox::TOptional<FBodyId3D>>::Failure(EErrorCode::InvalidState, "Joint mover endpoint expired");
		}
		if (!Mover->IsInitialized())
		{
			return TResult<Toolbox::TOptional<FBodyId3D>>::Success({});
		}
		Id = Mover->GetBodyId();
		if (!Id)
		{
			return TResult<Toolbox::TOptional<FBodyId3D>>::Success(Id);
		}
	}
	else
	{
		return TResult<Toolbox::TOptional<FBodyId3D>>::Failure(EErrorCode::InvalidArgument, "Joint endpoint is unspecified");
	}
	if (!World.IsAlive(*Id))
	{
		return TResult<Toolbox::TOptional<FBodyId3D>>::Failure(EErrorCode::InvalidState, "Joint endpoint expired or belongs to another world");
	}
	return TResult<Toolbox::TOptional<FBodyId3D>>::Success(Id);
}
Toolbox::FVector3 FPhysicsBodyReference3D::RenderAnchor_Internal(const FPhysicsWorld3D& World, Toolbox::FVector3 Local) const
{
	// 描画時も失効した参照を使わない。
	const auto Resolved = Resolve_Internal(World);
	if (!Resolved || !Resolved.Value())
	{
		throw Toolbox::FException("Joint render endpoint is unavailable");
	}
	// 明示IDの現在姿勢を既定とする。
	Toolbox::FVector3 Position = World.GetPosition(*Resolved.Value());
	Toolbox::FQuaternion Rotation = World.GetOrientation(*Resolved.Value());
	if (m_Kind == EKind::Rigid)
	{
		Position = m_Rigid.Get()->GetRenderPosition();
		Rotation = m_Rigid.Get()->GetRenderOrientation();
	}
	else if (m_Kind == EKind::Mover)
	{
		Position = m_Mover.Get()->GetRenderPosition();
		Rotation = m_Mover.Get()->GetRenderRotation();
	}
	return Position + Rotation.Rotate(Local);
}
} // namespace Dxf
namespace Dxf
{
bool FPhysicsBodyReference3D::IsAvailable_Internal(const FPhysicsWorld3D& World) const noexcept
{
	// 読み取りではエラー文字列の確保を行わない。
	if (m_Kind == EKind::Explicit)
	{
		return World.IsAlive(m_Body);
	}
	if (m_Kind == EKind::Rigid)
	{
		const auto* Rigid = m_Rigid.Get();
		return Rigid != nullptr && Rigid->IsInitialized() && Rigid->HasBody() && World.IsAlive(Rigid->GetBodyId());
	}
	if (m_Kind == EKind::Mover)
	{
		const auto* Mover = m_Mover.Get();
		if (Mover != nullptr && Mover->IsInitialized())
		{
			// 比較に使う世代付きID。
			const auto Id = Mover->GetBodyId();
			return Id && World.IsAlive(*Id);
		}
	}
	return false;
}
} // namespace Dxf
