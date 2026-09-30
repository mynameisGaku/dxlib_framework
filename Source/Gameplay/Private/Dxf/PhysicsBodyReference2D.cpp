// SPDX-License-Identifier: NOASSERTION
#include "Dxf/PhysicsBodyReference2D.h"
namespace Dxf
{
FPhysicsBodyReference2D FPhysicsBodyReference2D::FromRigidBody(TObjectHandle<DRigidBody2DComponent> Handle) noexcept
{
	// 剛体の識別子を保持する。
	FPhysicsBodyReference2D Result;
	Result.m_Kind = EKind::Rigid;
	Result.m_Rigid = Handle;
	return Result;
}
FPhysicsBodyReference2D FPhysicsBodyReference2D::FromKinematicMover(TObjectHandle<DKinematicMover2DComponent> Handle) noexcept
{
	// 支点の識別子を保持する。
	FPhysicsBodyReference2D Result;
	Result.m_Kind = EKind::Mover;
	Result.m_Mover = Handle;
	return Result;
}
FPhysicsBodyReference2D FPhysicsBodyReference2D::FromBodyId(FBodyId2D Id) noexcept
{
	// 明示IDをそのまま保持する。
	FPhysicsBodyReference2D Result;
	Result.m_Kind = EKind::Explicit;
	Result.m_Body = Id;
	return Result;
}
bool FPhysicsBodyReference2D::operator==(const FPhysicsBodyReference2D& Other) const noexcept
{
	return m_Kind == Other.m_Kind && m_Rigid == Other.m_Rigid && m_Mover == Other.m_Mover && m_Body == Other.m_Body;
}
TResult<Toolbox::TOptional<FBodyId2D>> FPhysicsBodyReference2D::Resolve_Internal(const FPhysicsWorld2D& World) const
{
	// 登録完了後に検証するID。
	Toolbox::TOptional<FBodyId2D> Id;
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
			return TResult<Toolbox::TOptional<FBodyId2D>>::Failure(EErrorCode::InvalidState, "Joint rigid endpoint expired");
		}
		if (!Rigid->IsInitialized() || !Rigid->HasBody())
		{
			return TResult<Toolbox::TOptional<FBodyId2D>>::Success({});
		}
		Id = Rigid->GetBodyId();
	}
	else if (m_Kind == EKind::Mover)
	{
		// 登録前は待機し、破棄後は失敗させる。
		const auto* Mover = m_Mover.Get();
		if (Mover == nullptr)
		{
			return TResult<Toolbox::TOptional<FBodyId2D>>::Failure(EErrorCode::InvalidState, "Joint mover endpoint expired");
		}
		if (!Mover->IsInitialized())
		{
			return TResult<Toolbox::TOptional<FBodyId2D>>::Success({});
		}
		Id = Mover->GetBodyId();
		if (!Id)
		{
			return TResult<Toolbox::TOptional<FBodyId2D>>::Success(Id);
		}
	}
	else
	{
		return TResult<Toolbox::TOptional<FBodyId2D>>::Failure(EErrorCode::InvalidArgument, "Joint endpoint is unspecified");
	}
	if (!World.IsAlive(*Id))
	{
		return TResult<Toolbox::TOptional<FBodyId2D>>::Failure(EErrorCode::InvalidState, "Joint endpoint expired or belongs to another world");
	}
	return TResult<Toolbox::TOptional<FBodyId2D>>::Success(Id);
}
Toolbox::FVector2 FPhysicsBodyReference2D::RenderAnchor_Internal(const FPhysicsWorld2D& World, Toolbox::FVector2 Local) const
{
	// 描画時も失効した参照を使わない。
	const auto Resolved = Resolve_Internal(World);
	if (!Resolved || !Resolved.Value())
	{
		throw Toolbox::FException("Joint render endpoint is unavailable");
	}
	// 明示IDの現在姿勢を既定とする。
	Toolbox::FVector2 Position = World.GetPosition(*Resolved.Value());
	Toolbox::f32 Rotation = World.GetAngle(*Resolved.Value());
	if (m_Kind == EKind::Rigid)
	{
		Position = m_Rigid.Get()->GetRenderPosition();
		Rotation = m_Rigid.Get()->GetRenderAngle();
	}
	else if (m_Kind == EKind::Mover)
	{
		Position = m_Mover.Get()->GetRenderPosition();
		Rotation = m_Mover.Get()->GetRenderRotation();
	}
	return Position + Toolbox::FVector2{static_cast<Toolbox::f32>(Toolbox::Cos(Rotation) * Local.X - Toolbox::Sin(Rotation) * Local.Y), static_cast<Toolbox::f32>(Toolbox::Sin(Rotation) * Local.X + Toolbox::Cos(Rotation) * Local.Y)};
}
} // namespace Dxf
namespace Dxf
{
bool FPhysicsBodyReference2D::IsAvailable_Internal(const FPhysicsWorld2D& World) const noexcept
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
