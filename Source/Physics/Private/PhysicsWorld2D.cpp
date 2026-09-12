// SPDX-License-Identifier: NOASSERTION
#include "Dxf/RigidBody2D.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
// 一つの平面剛体が持つ運動状態と蓄積した外力。
struct FBodyRecord2D
{
	// スロット再使用を見分ける世代。
	Toolbox::uint64 Generation = 0;
	// 有効な登録か。
	bool bAlive = false;
	// 運動区分。
	EBodyType Type = EBodyType::Dynamic;
	// 重心位置。メートル単位でY軸が上向き。
	Toolbox::FVector2 Position;
	// 姿勢角。ラジアン単位で反時計回りが正。
	Toolbox::f32 Angle = 0;
	// 重心速度。メートル毎秒単位。
	Toolbox::FVector2 Velocity;
	// 角速度。ラジアン毎秒単位で反時計回りが正。
	Toolbox::f32 AngularVelocity = 0;
	// 質量の逆数。StaticとKinematicはゼロ。
	Toolbox::f32 InverseMass = 1;
	// 重心回り慣性の逆数。StaticとKinematicはゼロ。
	Toolbox::f32 InverseInertia = 1;
	// 速度の減衰率。1毎秒単位。
	Toolbox::f32 LinearDamping = 0;
	// 角速度の減衰率。1毎秒単位。
	Toolbox::f32 AngularDamping = 0;
	// ワールド重力への追従倍率。
	Toolbox::f32 GravityScale = 1;
	// 次の更新で使う蓄積力。ニュートン単位。
	Toolbox::FVector2 Force;
	// 次の更新で使う蓄積トルク。ニュートンメートル単位。
	Toolbox::f32 Torque = 0;
};
// 平面剛体の登録スロットと蓄積時間をまとめた実装。
struct FPhysicsWorld2D::FImpl
{
	// 別ワールドのID混入を検出する識別子。
	Toolbox::uint64 World = NextWorld_Internal();
	// スロット番号で直接参照する登録領域。
	Toolbox::TVector<FBodyRecord2D> Slots;
	// 再使用可能な空きスロット番号。
	Toolbox::TVector<Toolbox::size_t> Free;
	// ワールド全体の重力加速度。
	Toolbox::FVector2 Gravity{0, -9.8f};
	// 新しいワールドへ重ならない識別子を発行する。
	static Toolbox::uint64 NextWorld_Internal()
	{
		// 単一スレッド利用を前提とした通し番号。
		static Toolbox::uint64 Next = 1;
		const Toolbox::uint64 Issued = Next;
		Next += 1;
		return Issued;
	}
	// IDが有効な登録を指す場合だけ記録を返す。
	FBodyRecord2D* Find_Internal(FBodyId2D Id) noexcept
	{
		if (Id.World != World)
		{
			return nullptr;
		}
		if (Id.Index >= Slots.Size())
		{
			return nullptr;
		}
		// 世代が一致する有効な登録。
		FBodyRecord2D& Record = Slots[Id.Index];
		return (Record.bAlive && Record.Generation == Id.Generation) ? &Record : nullptr;
	}
	// IDが有効な登録を指す場合だけ記録を返す。
	const FBodyRecord2D* Find_Internal(FBodyId2D Id) const noexcept
	{
		if (Id.World != World)
		{
			return nullptr;
		}
		if (Id.Index >= Slots.Size())
		{
			return nullptr;
		}
		// 世代が一致する有効な登録。
		const FBodyRecord2D& Record = Slots[Id.Index];
		return (Record.bAlive && Record.Generation == Id.Generation) ? &Record : nullptr;
	}
	// 有効な記録を返す。期限切れIDは例外で通知する。
	FBodyRecord2D& Resolve_Internal(FBodyId2D Id)
	{
		FBodyRecord2D* Record = Find_Internal(Id);
		if (Record == nullptr)
		{
			throw Toolbox::FException("Invalid 2D body id");
		}
		return *Record;
	}
	// 有効な記録を返す。期限切れIDは例外で通知する。
	const FBodyRecord2D& Resolve_Internal(FBodyId2D Id) const
	{
		const FBodyRecord2D* Record = Find_Internal(Id);
		if (Record == nullptr)
		{
			throw Toolbox::FException("Invalid 2D body id");
		}
		return *Record;
	}
};
// Dynamicの運動だけを更新する。力は呼び出し元が消去する。
static void IntegrateDynamic_Internal(FBodyRecord2D& Record, Toolbox::FVector2 Gravity, Toolbox::f64 StepSeconds)
{
	// 減衰後の速度へ加速度を足す半陰的Euler。
	const Toolbox::f64 DampLinear = 1.0 / (1.0 + static_cast<Toolbox::f64>(Record.LinearDamping) * StepSeconds);
	const Toolbox::f64 DampAngular = 1.0 / (1.0 + static_cast<Toolbox::f64>(Record.AngularDamping) * StepSeconds);
	// 速度の各成分。
	Toolbox::f64 VelocityX = static_cast<Toolbox::f64>(Record.Velocity.X) * DampLinear;
	Toolbox::f64 VelocityY = static_cast<Toolbox::f64>(Record.Velocity.Y) * DampLinear;
	// 重力と蓄積力による加速度。
	const Toolbox::f64 GravityX = static_cast<Toolbox::f64>(Gravity.X) * Record.GravityScale;
	const Toolbox::f64 GravityY = static_cast<Toolbox::f64>(Gravity.Y) * Record.GravityScale;
	const Toolbox::f64 ForceX = static_cast<Toolbox::f64>(Record.Force.X) * Record.InverseMass;
	const Toolbox::f64 ForceY = static_cast<Toolbox::f64>(Record.Force.Y) * Record.InverseMass;
	VelocityX += (GravityX + ForceX) * StepSeconds;
	VelocityY += (GravityY + ForceY) * StepSeconds;
	// 角速度。
	Toolbox::f64 Angular = static_cast<Toolbox::f64>(Record.AngularVelocity) * DampAngular;
	Angular += static_cast<Toolbox::f64>(Record.Torque) * Record.InverseInertia * StepSeconds;
	// 更新後の速度で位置と姿勢を進める。
	Record.Velocity = {static_cast<Toolbox::f32>(VelocityX), static_cast<Toolbox::f32>(VelocityY)};
	Record.AngularVelocity = static_cast<Toolbox::f32>(Angular);
	const Toolbox::f32 MovedX = static_cast<Toolbox::f32>(VelocityX * StepSeconds);
	const Toolbox::f32 MovedY = static_cast<Toolbox::f32>(VelocityY * StepSeconds);
	Record.Position += {MovedX, MovedY};
	Record.Angle = static_cast<Toolbox::f32>(static_cast<Toolbox::f64>(Record.Angle) + Angular * StepSeconds);
}
// 指定速度どおりに運動させる。外力と減衰は適用しない。
static void IntegrateKinematic_Internal(FBodyRecord2D& Record, Toolbox::f64 StepSeconds) noexcept
{
	Record.Position += {static_cast<Toolbox::f32>(static_cast<Toolbox::f64>(Record.Velocity.X) * StepSeconds),
	                    static_cast<Toolbox::f32>(static_cast<Toolbox::f64>(Record.Velocity.Y) * StepSeconds)};
	// 指定角速度で姿勢を進める。
	const Toolbox::f64 Turned = static_cast<Toolbox::f64>(Record.AngularVelocity) * StepSeconds;
	Record.Angle = static_cast<Toolbox::f32>(static_cast<Toolbox::f64>(Record.Angle) + Turned);
}
FPhysicsWorld2D::FPhysicsWorld2D() : m_pImpl(Toolbox::MakeUnique<FImpl>())
{
}
FPhysicsWorld2D::~FPhysicsWorld2D() = default;
FBodyId2D FPhysicsWorld2D::CreateBody(const FBodyDescription2D& Description)
{
	if (!Description.Position.IsValid() || !Description.Velocity.IsValid())
	{
		throw Toolbox::FException("Invalid 2D body position or velocity");
	}
	if (!Toolbox::IsFinite(Description.Angle) || !Toolbox::IsFinite(Description.AngularVelocity))
	{
		throw Toolbox::FException("Invalid 2D body angle or angular velocity");
	}
	if (!Toolbox::IsFinite(Description.LinearDamping) || Description.LinearDamping < 0)
	{
		throw Toolbox::FException("Invalid 2D body linear damping");
	}
	if (!Toolbox::IsFinite(Description.AngularDamping) || Description.AngularDamping < 0)
	{
		throw Toolbox::FException("Invalid 2D body angular damping");
	}
	if (!Toolbox::IsFinite(Description.GravityScale))
	{
		throw Toolbox::FException("Invalid 2D body gravity scale");
	}
	// 新しい登録の初期状態。
	FBodyRecord2D Record;
	Record.Type = Description.Type;
	Record.Position = Description.Position;
	Record.Angle = Description.Angle;
	Record.Velocity = Description.Velocity;
	Record.AngularVelocity = Description.AngularVelocity;
	Record.LinearDamping = Description.LinearDamping;
	Record.AngularDamping = Description.AngularDamping;
	Record.GravityScale = Description.GravityScale;
	if (Description.Type == EBodyType::Dynamic)
	{
		if (!Toolbox::IsFinite(Description.Mass) || Description.Mass <= 0)
		{
			throw Toolbox::FException("Invalid 2D body mass");
		}
		if (!Toolbox::IsFinite(Description.Inertia) || Description.Inertia <= 0)
		{
			throw Toolbox::FException("Invalid 2D body inertia");
		}
		Record.InverseMass = 1.0f / Description.Mass;
		Record.InverseInertia = 1.0f / Description.Inertia;
	}
	else
	{
		Record.InverseMass = 0;
		Record.InverseInertia = 0;
	}
	// 空きスロットの再使用または末尾への追加。
	Toolbox::size_t Index = 0;
	if (!m_pImpl->Free.IsEmpty())
	{
		Index = m_pImpl->Free.Back();
		m_pImpl->Free.PopBack();
		FBodyRecord2D& Slot = m_pImpl->Slots[Index];
		// 破棄時に進めた世代を引き継ぎ、古いIDと区別する。
		const Toolbox::uint64 NextGeneration = Slot.Generation + 1;
		Slot = Record;
		Slot.Generation = NextGeneration;
		Slot.bAlive = true;
	}
	else
	{
		Index = m_pImpl->Slots.Size();
		Record.Generation = 1;
		Record.bAlive = true;
		m_pImpl->Slots.PushBack(Record);
	}
	return {m_pImpl->World, Index, m_pImpl->Slots[Index].Generation};
}
bool FPhysicsWorld2D::DestroyBody(FBodyId2D Id) noexcept
{
	FBodyRecord2D* Record = m_pImpl->Find_Internal(Id);
	if (Record == nullptr)
	{
		return false;
	}
	Record->bAlive = false;
	Record->Generation += 1;
	Record->Force = {};
	Record->Torque = 0;
	m_pImpl->Free.PushBack(Id.Index);
	return true;
}
bool FPhysicsWorld2D::IsAlive(FBodyId2D Id) const noexcept
{
	return m_pImpl->Find_Internal(Id) != nullptr;
}
Toolbox::FVector2 FPhysicsWorld2D::GetPosition(FBodyId2D Id) const
{
	return m_pImpl->Resolve_Internal(Id).Position;
}
Toolbox::f32 FPhysicsWorld2D::GetAngle(FBodyId2D Id) const
{
	return m_pImpl->Resolve_Internal(Id).Angle;
}
Toolbox::FVector2 FPhysicsWorld2D::GetVelocity(FBodyId2D Id) const
{
	return m_pImpl->Resolve_Internal(Id).Velocity;
}
Toolbox::f32 FPhysicsWorld2D::GetAngularVelocity(FBodyId2D Id) const
{
	return m_pImpl->Resolve_Internal(Id).AngularVelocity;
}
Toolbox::f32 FPhysicsWorld2D::GetAngularMomentum(FBodyId2D Id) const
{
	// 非Dynamicの運動量は追跡しない。
	const FBodyRecord2D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		return 0;
	}
	return Record.AngularVelocity / Record.InverseInertia;
}
void FPhysicsWorld2D::SetVelocity(FBodyId2D Id, Toolbox::FVector2 Velocity)
{
	if (!Velocity.IsValid())
	{
		throw Toolbox::FException("Invalid 2D body velocity");
	}
	FBodyRecord2D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type == EBodyType::Static)
	{
		throw Toolbox::FException("Static 2D body has no velocity");
	}
	Record.Velocity = Velocity;
}
void FPhysicsWorld2D::SetAngularVelocity(FBodyId2D Id, Toolbox::f32 AngularVelocity)
{
	if (!Toolbox::IsFinite(AngularVelocity))
	{
		throw Toolbox::FException("Invalid 2D body angular velocity");
	}
	FBodyRecord2D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type == EBodyType::Static)
	{
		throw Toolbox::FException("Static 2D body has no angular velocity");
	}
	Record.AngularVelocity = AngularVelocity;
}
void FPhysicsWorld2D::ApplyForce(FBodyId2D Id, Toolbox::FVector2 Force)
{
	if (!Force.IsValid())
	{
		throw Toolbox::FException("Invalid 2D body force");
	}
	FBodyRecord2D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		throw Toolbox::FException("Only dynamic 2D bodies accept forces");
	}
	Record.Force += Force;
}
void FPhysicsWorld2D::ApplyTorque(FBodyId2D Id, Toolbox::f32 Torque)
{
	if (!Toolbox::IsFinite(Torque))
	{
		throw Toolbox::FException("Invalid 2D body torque");
	}
	FBodyRecord2D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		throw Toolbox::FException("Only dynamic 2D bodies accept torques");
	}
	Record.Torque += Torque;
}
void FPhysicsWorld2D::ApplyLinearImpulse(FBodyId2D Id, Toolbox::FVector2 Impulse)
{
	if (!Impulse.IsValid())
	{
		throw Toolbox::FException("Invalid 2D body impulse");
	}
	FBodyRecord2D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		throw Toolbox::FException("Only dynamic 2D bodies accept impulses");
	}
	// 力積は速度へ即時反映し、分割数に依存しない。
	Record.Velocity += {Impulse.X * Record.InverseMass, Impulse.Y * Record.InverseMass};
}
void FPhysicsWorld2D::ApplyAngularImpulse(FBodyId2D Id, Toolbox::f32 Impulse)
{
	if (!Toolbox::IsFinite(Impulse))
	{
		throw Toolbox::FException("Invalid 2D body angular impulse");
	}
	FBodyRecord2D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		throw Toolbox::FException("Only dynamic 2D bodies accept impulses");
	}
	Record.AngularVelocity += Impulse * Record.InverseInertia;
}
void FPhysicsWorld2D::ApplyImpulseAtPoint(FBodyId2D Id, Toolbox::FVector2 Impulse, Toolbox::FVector2 WorldPoint)
{
	if (!Impulse.IsValid() || !WorldPoint.IsValid())
	{
		throw Toolbox::FException("Invalid 2D body impulse point");
	}
	FBodyRecord2D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		throw Toolbox::FException("Only dynamic 2D bodies accept impulses");
	}
	// 重心からの腕と力積の外積が回転を生む。
	const Toolbox::f64 ArmX = static_cast<Toolbox::f64>(WorldPoint.X) - Record.Position.X;
	const Toolbox::f64 ArmY = static_cast<Toolbox::f64>(WorldPoint.Y) - Record.Position.Y;
	Record.Velocity += {Impulse.X * Record.InverseMass, Impulse.Y * Record.InverseMass};
	Record.AngularVelocity = static_cast<Toolbox::f32>(static_cast<Toolbox::f64>(Record.AngularVelocity) +
	                                                   (ArmX * Impulse.Y - ArmY * Impulse.X) * Record.InverseInertia);
}
void FPhysicsWorld2D::SetGravity(Toolbox::FVector2 Gravity)
{
	if (!Gravity.IsValid())
	{
		throw Toolbox::FException("Invalid 2D gravity");
	}
	m_pImpl->Gravity = Gravity;
}
Toolbox::FVector2 FPhysicsWorld2D::GetGravity() const noexcept
{
	return m_pImpl->Gravity;
}
void FPhysicsWorld2D::Step(Toolbox::f64 DeltaSeconds, Toolbox::uint32 SubSteps)
{
	if (!Toolbox::IsFinite(DeltaSeconds) || DeltaSeconds <= 0)
	{
		throw Toolbox::FException("Invalid 2D step seconds");
	}
	if (SubSteps < 1 || SubSteps > 1024)
	{
		throw Toolbox::FException("Invalid 2D sub step count");
	}
	// 一回の更新を等分割し、蓄積力は全分割で保持する。
	const Toolbox::f64 Slice = DeltaSeconds / static_cast<Toolbox::f64>(SubSteps);
	for (Toolbox::uint32 SliceIndex = 0; SliceIndex < SubSteps; ++SliceIndex)
	{
		for (Toolbox::size_t Index = 0; Index < m_pImpl->Slots.Size(); ++Index)
		{
			FBodyRecord2D& Record = m_pImpl->Slots[Index];
			if (!Record.bAlive)
			{
				continue;
			}
			if (Record.Type == EBodyType::Dynamic)
			{
				IntegrateDynamic_Internal(Record, m_pImpl->Gravity, Slice);
			}
			else if (Record.Type == EBodyType::Kinematic)
			{
				IntegrateKinematic_Internal(Record, Slice);
			}
		}
	}
	// 蓄積した力とトルクを一度だけ消去する。
	for (Toolbox::size_t Index = 0; Index < m_pImpl->Slots.Size(); ++Index)
	{
		FBodyRecord2D& Record = m_pImpl->Slots[Index];
		Record.Force = {};
		Record.Torque = 0;
	}
}
} // namespace Dxf
