// SPDX-License-Identifier: NOASSERTION
#include "Dxf/RigidBody3D.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
// 倍精度で姿勢変換を行う四元数。
struct FQuaternionD
{
	// 虚部のX成分。
	Toolbox::f64 X = 0;
	// 虚部のY成分。
	Toolbox::f64 Y = 0;
	// 虚部のZ成分。
	Toolbox::f64 Z = 0;
	// 実数成分。
	Toolbox::f64 W = 1;
};
// 倍精度の三成分ベクトル。
struct FVector3D
{
	// X軸成分。
	Toolbox::f64 X = 0;
	// Y軸成分。
	Toolbox::f64 Y = 0;
	// Z軸成分。
	Toolbox::f64 Z = 0;
};
// 単位四元数で方向を回転する。Quaternion::Rotateと同じ式の倍精度版。
static FVector3D Rotate_Internal(const FQuaternionD& Q, FVector3D Value) noexcept
{
	// 虚部と入力の外積の二倍。
	const FVector3D Cross = {Q.Y * Value.Z - Q.Z * Value.Y, Q.Z * Value.X - Q.X * Value.Z,
	                         Q.X * Value.Y - Q.Y * Value.X};
	const FVector3D Twice = {Cross.X * 2, Cross.Y * 2, Cross.Z * 2};
	// 外積と回転後の外積の合成。
	const FVector3D Outer = {Q.Y * Twice.Z - Q.Z * Twice.Y, Q.Z * Twice.X - Q.X * Twice.Z,
	                         Q.X * Twice.Y - Q.Y * Twice.X};
	FVector3D Result = {Value.X + Twice.X * Q.W + Outer.X, Value.Y + Twice.Y * Q.W + Outer.Y,
	                    Value.Z + Twice.Z * Q.W + Outer.Z};
	return Result;
}
// 対角慣性を姿勢で挟んでワールド量へ変換する。逆数と通常で共用する。
static FVector3D TransformDiagonal_Internal(const FQuaternionD& Q, FVector3D Diagonal, FVector3D Value) noexcept
{
	// 共役でボディ座標へ戻した成分。
	const FQuaternionD Conjugate = {-Q.X, -Q.Y, -Q.Z, Q.W};
	const FVector3D Local = Rotate_Internal(Conjugate, Value);
	// 対角成分を掛けてワールド座標へ戻す。
	const FVector3D Scaled = {Local.X * Diagonal.X, Local.Y * Diagonal.Y, Local.Z * Diagonal.Z};
	return Rotate_Internal(Q, Scaled);
}
// 一つの立体剛体が持つ運動状態と蓄積した外力。
struct FBodyRecord3D
{
	// スロット再使用を見分ける世代。
	Toolbox::uint64 Generation = 0;
	// 有効な登録か。
	bool bAlive = false;
	// 運動区分。
	EBodyType Type = EBodyType::Dynamic;
	// 重心位置。メートル単位で右手系。
	Toolbox::FVector3 Position;
	// 姿勢。右手則の単位四元数。
	Toolbox::FQuaternion Orientation;
	// 重心速度。メートル毎秒単位。
	Toolbox::FVector3 Velocity;
	// 角速度。ワールド軸回りのラジアン毎秒。
	Toolbox::FVector3 AngularVelocity;
	// 質量の逆数。StaticとKinematicはゼロ。
	Toolbox::f32 InverseMass = 1;
	// ボディ座標の対角慣性の逆数。StaticとKinematicはゼロ。
	Toolbox::FVector3 InverseDiagonalInertia{1, 1, 1};
	// ボディ座標の対角慣性。角運動量の計算に使う。
	Toolbox::FVector3 DiagonalInertia{1, 1, 1};
	// 速度の減衰率。1毎秒単位。
	Toolbox::f32 LinearDamping = 0;
	// 角速度の減衰率。1毎秒単位。
	Toolbox::f32 AngularDamping = 0;
	// ワールド重力への追従倍率。
	Toolbox::f32 GravityScale = 1;
	// 次の更新で使う蓄積力。ニュートン単位。
	Toolbox::FVector3 Force;
	// 次の更新で使う蓄積トルク。ワールド軸回り。
	Toolbox::FVector3 Torque;
};
// 立体剛体の登録スロットをまとめた実装。
struct FPhysicsWorld3D::FImpl
{
	// 別ワールドのID混入を検出する識別子。
	Toolbox::uint64 World = NextWorld_Internal();
	// スロット番号で直接参照する登録領域。
	Toolbox::TVector<FBodyRecord3D> Slots;
	// 再使用可能な空きスロット番号。
	Toolbox::TVector<Toolbox::size_t> Free;
	// ワールド全体の重力加速度。
	Toolbox::FVector3 Gravity{0, -9.8f, 0};
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
	FBodyRecord3D* Find_Internal(FBodyId3D Id) noexcept
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
		FBodyRecord3D& Record = Slots[Id.Index];
		return (Record.bAlive && Record.Generation == Id.Generation) ? &Record : nullptr;
	}
	// IDが有効な登録を指す場合だけ記録を返す。
	const FBodyRecord3D* Find_Internal(FBodyId3D Id) const noexcept
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
		const FBodyRecord3D& Record = Slots[Id.Index];
		return (Record.bAlive && Record.Generation == Id.Generation) ? &Record : nullptr;
	}
	// 有効な記録を返す。期限切れIDは例外で通知する。
	FBodyRecord3D& Resolve_Internal(FBodyId3D Id)
	{
		FBodyRecord3D* Record = Find_Internal(Id);
		if (Record == nullptr)
		{
			throw Toolbox::FException("Invalid 3D body id");
		}
		return *Record;
	}
	// 有効な記録を返す。期限切れIDは例外で通知する。
	const FBodyRecord3D& Resolve_Internal(FBodyId3D Id) const
	{
		const FBodyRecord3D* Record = Find_Internal(Id);
		if (Record == nullptr)
		{
			throw Toolbox::FException("Invalid 3D body id");
		}
		return *Record;
	}
};
// 姿勢を表す単精度四元数を倍精度へ写す。
static FQuaternionD ToDouble_Internal(Toolbox::FQuaternion Q) noexcept
{
	FQuaternionD Result;
	Result.X = Q.X;
	Result.Y = Q.Y;
	Result.Z = Q.Z;
	Result.W = Q.W;
	return Result;
}
// ワールド角速度の四元数微分で姿勢を進める。ノルム異常時は姿勢を保つ。
static void IntegrateOrientation_Internal(Toolbox::FQuaternion& Orientation, FVector3D Angular,
                                          Toolbox::f64 StepSeconds) noexcept
{
	const FQuaternionD Q = ToDouble_Internal(Orientation);
	FQuaternionD Next = {Q.X + 0.5 * StepSeconds * (Angular.X * Q.W + Angular.Y * Q.Z - Angular.Z * Q.Y),
	                     Q.Y + 0.5 * StepSeconds * (-Angular.X * Q.Z + Angular.Y * Q.W + Angular.Z * Q.X),
	                     Q.Z + 0.5 * StepSeconds * (Angular.X * Q.Y - Angular.Y * Q.X + Angular.Z * Q.W),
	                     Q.W + 0.5 * StepSeconds * (-Angular.X * Q.X - Angular.Y * Q.Y - Angular.Z * Q.Z)};
	// 姿勢ノルムの二乗。
	const Toolbox::f64 Norm = Next.X * Next.X + Next.Y * Next.Y + Next.Z * Next.Z + Next.W * Next.W;
	if (Toolbox::IsFinite(Norm) && Norm > 1e-24)
	{
		// 逆平方根で単位化する。
		const Toolbox::f64 Inverse = 1.0 / Toolbox::Sqrt(Norm);
		Orientation = {static_cast<Toolbox::f32>(Next.X * Inverse), static_cast<Toolbox::f32>(Next.Y * Inverse),
		               static_cast<Toolbox::f32>(Next.Z * Inverse), static_cast<Toolbox::f32>(Next.W * Inverse)};
	}
}
// ワールド逆慣性でトルクを角加速度へ変換する。
static FVector3D ToAngularAcceleration_Internal(const FBodyRecord3D& Record, const FQuaternionD& Q,
                                               FVector3D Torque) noexcept
{
	// 逆対角慣性を倍精度で取り出す。
	const FVector3D Inverse = {Record.InverseDiagonalInertia.X, Record.InverseDiagonalInertia.Y,
	                           Record.InverseDiagonalInertia.Z};
	return TransformDiagonal_Internal(Q, Inverse, Torque);
}
// Dynamicの運動だけを更新する。力は呼び出し元が消去する。
static void IntegrateDynamic_Internal(FBodyRecord3D& Record, Toolbox::FVector3 Gravity, Toolbox::f64 StepSeconds)
{
	// 減衰後の速度へ加速度を足す半陰的Euler。
	const Toolbox::f64 DampLinear = 1.0 / (1.0 + static_cast<Toolbox::f64>(Record.LinearDamping) * StepSeconds);
	const Toolbox::f64 DampAngular = 1.0 / (1.0 + static_cast<Toolbox::f64>(Record.AngularDamping) * StepSeconds);
	// 速度の各成分。
	Toolbox::f64 VelocityX = static_cast<Toolbox::f64>(Record.Velocity.X) * DampLinear;
	Toolbox::f64 VelocityY = static_cast<Toolbox::f64>(Record.Velocity.Y) * DampLinear;
	Toolbox::f64 VelocityZ = static_cast<Toolbox::f64>(Record.Velocity.Z) * DampLinear;
	// 重力と蓄積力による加速度。
	const Toolbox::f64 GravityX = static_cast<Toolbox::f64>(Gravity.X) * Record.GravityScale;
	const Toolbox::f64 GravityY = static_cast<Toolbox::f64>(Gravity.Y) * Record.GravityScale;
	const Toolbox::f64 GravityZ = static_cast<Toolbox::f64>(Gravity.Z) * Record.GravityScale;
	const Toolbox::f64 ForceX = static_cast<Toolbox::f64>(Record.Force.X) * Record.InverseMass;
	const Toolbox::f64 ForceY = static_cast<Toolbox::f64>(Record.Force.Y) * Record.InverseMass;
	const Toolbox::f64 ForceZ = static_cast<Toolbox::f64>(Record.Force.Z) * Record.InverseMass;
	VelocityX += (GravityX + ForceX) * StepSeconds;
	VelocityY += (GravityY + ForceY) * StepSeconds;
	VelocityZ += (GravityZ + ForceZ) * StepSeconds;
	// 現在の角速度と姿勢の倍精度写像。
	FVector3D Angular = {static_cast<Toolbox::f64>(Record.AngularVelocity.X) * DampAngular,
	                     static_cast<Toolbox::f64>(Record.AngularVelocity.Y) * DampAngular,
	                     static_cast<Toolbox::f64>(Record.AngularVelocity.Z) * DampAngular};
	const FQuaternionD Q = ToDouble_Internal(Record.Orientation);
	// 対角慣性を倍精度で取り出す。
	const FVector3D Diagonal = {Record.DiagonalInertia.X, Record.DiagonalInertia.Y, Record.DiagonalInertia.Z};
	// 自由回転の角運動量を保つジャイロ項を差し引いた正味トルク。
	const FVector3D Momentum = TransformDiagonal_Internal(Q, Diagonal, Angular);
	const FVector3D Gyro = {Angular.Y * Momentum.Z - Angular.Z * Momentum.Y,
	                        Angular.Z * Momentum.X - Angular.X * Momentum.Z,
	                        Angular.X * Momentum.Y - Angular.Y * Momentum.X};
	const FVector3D Applied = {Record.Torque.X, Record.Torque.Y, Record.Torque.Z};
	const FVector3D Net = {Applied.X - Gyro.X, Applied.Y - Gyro.Y, Applied.Z - Gyro.Z};
	// 角加速度を足す。
	const FVector3D Alpha = ToAngularAcceleration_Internal(Record, Q, Net);
	Angular.X += Alpha.X * StepSeconds;
	Angular.Y += Alpha.Y * StepSeconds;
	Angular.Z += Alpha.Z * StepSeconds;
	// 更新後の速度で位置を進める。
	Record.Velocity = {static_cast<Toolbox::f32>(VelocityX), static_cast<Toolbox::f32>(VelocityY),
	                   static_cast<Toolbox::f32>(VelocityZ)};
	Record.AngularVelocity = {static_cast<Toolbox::f32>(Angular.X), static_cast<Toolbox::f32>(Angular.Y),
	                          static_cast<Toolbox::f32>(Angular.Z)};
	Record.Position += {static_cast<Toolbox::f32>(VelocityX * StepSeconds),
	                    static_cast<Toolbox::f32>(VelocityY * StepSeconds),
	                    static_cast<Toolbox::f32>(VelocityZ * StepSeconds)};
	// ワールド角速度の四元数微分で姿勢を進める。
	IntegrateOrientation_Internal(Record.Orientation, Angular, StepSeconds);
}
// 指定速度どおりに運動させる。外力と減衰は適用しない。
static void IntegrateKinematic_Internal(FBodyRecord3D& Record, Toolbox::f64 StepSeconds) noexcept
{
	Record.Position += {static_cast<Toolbox::f32>(static_cast<Toolbox::f64>(Record.Velocity.X) * StepSeconds),
	                    static_cast<Toolbox::f32>(static_cast<Toolbox::f64>(Record.Velocity.Y) * StepSeconds),
	                    static_cast<Toolbox::f32>(static_cast<Toolbox::f64>(Record.Velocity.Z) * StepSeconds)};
	// 指定角速度で姿勢を進める。
	const FVector3D Angular = {Record.AngularVelocity.X, Record.AngularVelocity.Y, Record.AngularVelocity.Z};
	IntegrateOrientation_Internal(Record.Orientation, Angular, StepSeconds);
}
FPhysicsWorld3D::FPhysicsWorld3D() : m_pImpl(Toolbox::MakeUnique<FImpl>())
{
}
FPhysicsWorld3D::~FPhysicsWorld3D() = default;
FBodyId3D FPhysicsWorld3D::CreateBody(const FBodyDescription3D& Description)
{
	if (!Description.Position.IsValid() || !Description.Velocity.IsValid())
	{
		throw Toolbox::FException("Invalid 3D body position or velocity");
	}
	if (!Description.AngularVelocity.IsValid())
	{
		throw Toolbox::FException("Invalid 3D body angular velocity");
	}
	if (!Toolbox::IsFinite(Description.LinearDamping) || Description.LinearDamping < 0)
	{
		throw Toolbox::FException("Invalid 3D body linear damping");
	}
	if (!Toolbox::IsFinite(Description.AngularDamping) || Description.AngularDamping < 0)
	{
		throw Toolbox::FException("Invalid 3D body angular damping");
	}
	if (!Toolbox::IsFinite(Description.GravityScale))
	{
		throw Toolbox::FException("Invalid 3D body gravity scale");
	}
	// 正規化可能な姿勢だけを受け付ける。
	Toolbox::FQuaternion Orientation;
	try
	{
		Orientation = Description.Orientation.Normalized();
	}
	catch (const Toolbox::FException&)
	{
		throw Toolbox::FException("Invalid 3D body orientation");
	}
	// 新しい登録の初期状態。
	FBodyRecord3D Record;
	Record.Type = Description.Type;
	Record.Position = Description.Position;
	Record.Orientation = Orientation;
	Record.Velocity = Description.Velocity;
	Record.AngularVelocity = Description.AngularVelocity;
	Record.LinearDamping = Description.LinearDamping;
	Record.AngularDamping = Description.AngularDamping;
	Record.GravityScale = Description.GravityScale;
	if (Description.Type == EBodyType::Dynamic)
	{
		if (!Toolbox::IsFinite(Description.Mass) || Description.Mass <= 0)
		{
			throw Toolbox::FException("Invalid 3D body mass");
		}
		if (!Description.DiagonalInertia.IsValid())
		{
			throw Toolbox::FException("Invalid 3D body inertia");
		}
		if (Description.DiagonalInertia.X <= 0 || Description.DiagonalInertia.Y <= 0 ||
		    Description.DiagonalInertia.Z <= 0)
		{
			throw Toolbox::FException("Invalid 3D body inertia");
		}
		Record.InverseMass = 1.0f / Description.Mass;
		Record.DiagonalInertia = Description.DiagonalInertia;
		Record.InverseDiagonalInertia = {1.0f / Description.DiagonalInertia.X,
		                                 1.0f / Description.DiagonalInertia.Y,
		                                 1.0f / Description.DiagonalInertia.Z};
	}
	else
	{
		Record.InverseMass = 0;
		Record.InverseDiagonalInertia = {0, 0, 0};
		Record.DiagonalInertia = {0, 0, 0};
	}
	// 空きスロットの再使用または末尾への追加。
	Toolbox::size_t Index = 0;
	if (!m_pImpl->Free.IsEmpty())
	{
		Index = m_pImpl->Free.Back();
		m_pImpl->Free.PopBack();
		FBodyRecord3D& Slot = m_pImpl->Slots[Index];
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
bool FPhysicsWorld3D::DestroyBody(FBodyId3D Id) noexcept
{
	FBodyRecord3D* Record = m_pImpl->Find_Internal(Id);
	if (Record == nullptr)
	{
		return false;
	}
	Record->bAlive = false;
	Record->Generation += 1;
	Record->Force = {};
	Record->Torque = {};
	m_pImpl->Free.PushBack(Id.Index);
	return true;
}
bool FPhysicsWorld3D::IsAlive(FBodyId3D Id) const noexcept
{
	return m_pImpl->Find_Internal(Id) != nullptr;
}
Toolbox::FVector3 FPhysicsWorld3D::GetPosition(FBodyId3D Id) const
{
	return m_pImpl->Resolve_Internal(Id).Position;
}
Toolbox::FQuaternion FPhysicsWorld3D::GetOrientation(FBodyId3D Id) const
{
	return m_pImpl->Resolve_Internal(Id).Orientation;
}
Toolbox::FVector3 FPhysicsWorld3D::GetVelocity(FBodyId3D Id) const
{
	return m_pImpl->Resolve_Internal(Id).Velocity;
}
Toolbox::FVector3 FPhysicsWorld3D::GetAngularVelocity(FBodyId3D Id) const
{
	return m_pImpl->Resolve_Internal(Id).AngularVelocity;
}
Toolbox::FVector3 FPhysicsWorld3D::GetAngularMomentum(FBodyId3D Id) const
{
	// 非Dynamicの運動量は追跡しない。
	const FBodyRecord3D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		return {};
	}
	// 対角慣性で角速度を運動量へ変換し、姿勢でワールド座標へ戻す。
	const FQuaternionD Q = ToDouble_Internal(Record.Orientation);
	const FVector3D Diagonal = {Record.DiagonalInertia.X, Record.DiagonalInertia.Y, Record.DiagonalInertia.Z};
	const FVector3D Angular = {Record.AngularVelocity.X, Record.AngularVelocity.Y, Record.AngularVelocity.Z};
	const FVector3D Momentum = TransformDiagonal_Internal(Q, Diagonal, Angular);
	return {static_cast<Toolbox::f32>(Momentum.X), static_cast<Toolbox::f32>(Momentum.Y),
	        static_cast<Toolbox::f32>(Momentum.Z)};
}
void FPhysicsWorld3D::SetVelocity(FBodyId3D Id, Toolbox::FVector3 Velocity)
{
	if (!Velocity.IsValid())
	{
		throw Toolbox::FException("Invalid 3D body velocity");
	}
	FBodyRecord3D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type == EBodyType::Static)
	{
		throw Toolbox::FException("Static 3D body has no velocity");
	}
	Record.Velocity = Velocity;
}
void FPhysicsWorld3D::SetAngularVelocity(FBodyId3D Id, Toolbox::FVector3 AngularVelocity)
{
	if (!AngularVelocity.IsValid())
	{
		throw Toolbox::FException("Invalid 3D body angular velocity");
	}
	FBodyRecord3D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type == EBodyType::Static)
	{
		throw Toolbox::FException("Static 3D body has no angular velocity");
	}
	Record.AngularVelocity = AngularVelocity;
}
void FPhysicsWorld3D::ApplyForce(FBodyId3D Id, Toolbox::FVector3 Force)
{
	if (!Force.IsValid())
	{
		throw Toolbox::FException("Invalid 3D body force");
	}
	FBodyRecord3D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		throw Toolbox::FException("Only dynamic 3D bodies accept forces");
	}
	Record.Force += Force;
}
void FPhysicsWorld3D::ApplyTorque(FBodyId3D Id, Toolbox::FVector3 Torque)
{
	if (!Torque.IsValid())
	{
		throw Toolbox::FException("Invalid 3D body torque");
	}
	FBodyRecord3D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		throw Toolbox::FException("Only dynamic 3D bodies accept torques");
	}
	Record.Torque += Torque;
}
void FPhysicsWorld3D::ApplyLinearImpulse(FBodyId3D Id, Toolbox::FVector3 Impulse)
{
	if (!Impulse.IsValid())
	{
		throw Toolbox::FException("Invalid 3D body impulse");
	}
	FBodyRecord3D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		throw Toolbox::FException("Only dynamic 3D bodies accept impulses");
	}
	// 力積は速度へ即時反映し、分割数に依存しない。
	Record.Velocity += Impulse * Record.InverseMass;
}
void FPhysicsWorld3D::ApplyAngularImpulse(FBodyId3D Id, Toolbox::FVector3 Impulse)
{
	if (!Impulse.IsValid())
	{
		throw Toolbox::FException("Invalid 3D body angular impulse");
	}
	FBodyRecord3D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		throw Toolbox::FException("Only dynamic 3D bodies accept impulses");
	}
	// 力積モーメントをワールド逆慣性で角速度へ変換する。
	const FQuaternionD Q = ToDouble_Internal(Record.Orientation);
	const FVector3D Vector = {Impulse.X, Impulse.Y, Impulse.Z};
	const FVector3D Delta = ToAngularAcceleration_Internal(Record, Q, Vector);
	Record.AngularVelocity += {static_cast<Toolbox::f32>(Delta.X), static_cast<Toolbox::f32>(Delta.Y),
	                           static_cast<Toolbox::f32>(Delta.Z)};
}
void FPhysicsWorld3D::ApplyImpulseAtPoint(FBodyId3D Id, Toolbox::FVector3 Impulse, Toolbox::FVector3 WorldPoint)
{
	if (!Impulse.IsValid() || !WorldPoint.IsValid())
	{
		throw Toolbox::FException("Invalid 3D body impulse point");
	}
	FBodyRecord3D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		throw Toolbox::FException("Only dynamic 3D bodies accept impulses");
	}
	// 重心からの腕と力積の外積が回転を生む。
	const FVector3D Arm = {static_cast<Toolbox::f64>(WorldPoint.X) - Record.Position.X,
	                       static_cast<Toolbox::f64>(WorldPoint.Y) - Record.Position.Y,
	                       static_cast<Toolbox::f64>(WorldPoint.Z) - Record.Position.Z};
	Record.Velocity += Impulse * Record.InverseMass;
	const FVector3D Vector = {Impulse.X, Impulse.Y, Impulse.Z};
	const FVector3D Moment = {Arm.Y * Vector.Z - Arm.Z * Vector.Y, Arm.Z * Vector.X - Arm.X * Vector.Z,
	                          Arm.X * Vector.Y - Arm.Y * Vector.X};
	const FQuaternionD Q = ToDouble_Internal(Record.Orientation);
	const FVector3D Delta = ToAngularAcceleration_Internal(Record, Q, Moment);
	Record.AngularVelocity += {static_cast<Toolbox::f32>(Delta.X), static_cast<Toolbox::f32>(Delta.Y),
	                           static_cast<Toolbox::f32>(Delta.Z)};
}
void FPhysicsWorld3D::SetGravity(Toolbox::FVector3 Gravity)
{
	if (!Gravity.IsValid())
	{
		throw Toolbox::FException("Invalid 3D gravity");
	}
	m_pImpl->Gravity = Gravity;
}
Toolbox::FVector3 FPhysicsWorld3D::GetGravity() const noexcept
{
	return m_pImpl->Gravity;
}
void FPhysicsWorld3D::Step(Toolbox::f64 DeltaSeconds, Toolbox::uint32 SubSteps)
{
	if (!Toolbox::IsFinite(DeltaSeconds) || DeltaSeconds <= 0)
	{
		throw Toolbox::FException("Invalid 3D step seconds");
	}
	if (SubSteps < 1 || SubSteps > 1024)
	{
		throw Toolbox::FException("Invalid 3D sub step count");
	}
	// 一回の更新を等分割し、蓄積力は全分割で保持する。
	const Toolbox::f64 Slice = DeltaSeconds / static_cast<Toolbox::f64>(SubSteps);
	for (Toolbox::uint32 SliceIndex = 0; SliceIndex < SubSteps; ++SliceIndex)
	{
		for (Toolbox::size_t Index = 0; Index < m_pImpl->Slots.Size(); ++Index)
		{
			FBodyRecord3D& Record = m_pImpl->Slots[Index];
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
		FBodyRecord3D& Record = m_pImpl->Slots[Index];
		Record.Force = {};
		Record.Torque = {};
	}
}
} // namespace Dxf
