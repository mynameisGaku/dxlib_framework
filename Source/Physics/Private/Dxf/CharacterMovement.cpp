// SPDX-License-Identifier: NOASSERTION
#include "Dxf/CharacterMovement2D.h"
#include "Dxf/CharacterMovement3D.h"
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
#include "WorldInteractionProbe.h"
#endif
namespace Dxf
{
namespace
{
using Toolbox::f32;
using Toolbox::f64;
using Toolbox::int32;
using Toolbox::uint32;

// 内部計算用のf64の3成分（2Dは第3成分を0で使う）。
struct FVec_Internal
{
	// 第1成分。
	f64 X = 0;
	// 第2成分。
	f64 Y = 0;
	// 第3成分。
	f64 Z = 0;
};
FVec_Internal Add_Internal(const FVec_Internal& A, const FVec_Internal& B) noexcept
{
	return {A.X + B.X, A.Y + B.Y, A.Z + B.Z};
}
FVec_Internal Sub_Internal(const FVec_Internal& A, const FVec_Internal& B) noexcept
{
	return {A.X - B.X, A.Y - B.Y, A.Z - B.Z};
}
FVec_Internal Scale_Internal(const FVec_Internal& A, f64 Factor) noexcept
{
	return {A.X * Factor, A.Y * Factor, A.Z * Factor};
}
f64 Dot_Internal(const FVec_Internal& A, const FVec_Internal& B) noexcept
{
	return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
}
FVec_Internal Cross_Internal(const FVec_Internal& A, const FVec_Internal& B) noexcept
{
	return {A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X};
}
bool Equal_Internal(const FVec_Internal& A, const FVec_Internal& B) noexcept
{
	return A.X == B.X && A.Y == B.Y && A.Z == B.Z;
}
// 最大成分で割ってから求める長さ（平方の桁あふれ・消失を避ける）。
f64 Length_Internal(const FVec_Internal& A) noexcept
{
	const f64 Scale = Toolbox::Max(Toolbox::Abs(A.X), Toolbox::Max(Toolbox::Abs(A.Y), Toolbox::Abs(A.Z)));
	if (Scale == 0)
	{
		return 0;
	}
	const FVec_Internal Unit = Scale_Internal(A, 1 / Scale);
	return Scale * Toolbox::Sqrt(Dot_Internal(Unit, Unit));
}
// f64の値を公開座標のf32へ戻す。有限でf32の範囲に収まらなければ計算不能として例外にする。
f32 ToF32_Internal(f64 Value)
{
	if (!Toolbox::IsFinite(Value) || Toolbox::Abs(Value) > f64(Toolbox::TNumericLimits<f32>::Max()))
	{
		throw Toolbox::FException("Unrepresentable character movement coordinate");
	}
	return static_cast<f32>(Value);
}

// 2Dの型と変換。
struct F2D_Internal
{
	using FWorld = FPhysicsWorld2D;
	using FShape = Toolbox::FCircle2D;
	using FCapsuleShape = Toolbox::FCapsule2D;
	using FResize = FCharacterResize2D;
	using FVector = Toolbox::FVector2;
	using FBodyId = FBodyId2D;
	using FColliderId = FColliderId2D;
	using FSettings = FCharacterMoveSettings2D;
	using FRecovery = FCharacterRecovery2D;
	using FMoveResult = FCharacterMoveResult2D;
	using FGround = FCharacterGround2D;
	using FState = FCharacterState2D;
	using FInput = FCharacterMoveInput2D;
	using FStepResult = FCharacterStepResult2D;
	static constexpr int32 Dimension = 2;
	static FVec_Internal Load(FVector Value) noexcept
	{
		return {Value.X, Value.Y, 0};
	}
	static FVector Store(const FVec_Internal& Value)
	{
		return {ToF32_Internal(Value.X), ToF32_Internal(Value.Y)};
	}
	static FShape Shape(FVector Center, f32 Radius) noexcept
	{
		return {Center, Radius};
	}
	static FCapsuleShape Capsule(const FVec_Internal& Start, const FVec_Internal& End, f32 Radius)
	{
		return {Store(Start), Store(End), Radius};
	}
	// 2Dの角速度を、共通の外積に使うZ軸回りの値へ移す。
	static FVec_Internal AngularVelocity(const FWorld& World, FBodyId Body)
	{
		return {0, 0, World.GetAngularVelocity(Body)};
	}
};
// 3Dの型と変換。
struct F3D_Internal
{
	using FWorld = FPhysicsWorld3D;
	using FShape = Toolbox::FSphere;
	using FCapsuleShape = Toolbox::FCapsule;
	using FResize = FCharacterResize3D;
	using FVector = Toolbox::FVector3;
	using FBodyId = FBodyId3D;
	using FColliderId = FColliderId3D;
	using FSettings = FCharacterMoveSettings3D;
	using FRecovery = FCharacterRecovery3D;
	using FMoveResult = FCharacterMoveResult3D;
	using FGround = FCharacterGround3D;
	using FState = FCharacterState3D;
	using FInput = FCharacterMoveInput3D;
	using FStepResult = FCharacterStepResult3D;
	static constexpr int32 Dimension = 3;
	static FVec_Internal Load(FVector Value) noexcept
	{
		return {Value.X, Value.Y, Value.Z};
	}
	static FVector Store(const FVec_Internal& Value)
	{
		return {ToF32_Internal(Value.X), ToF32_Internal(Value.Y), ToF32_Internal(Value.Z)};
	}
	static FShape Shape(FVector Center, f32 Radius) noexcept
	{
		return {Center, Radius};
	}
	static FCapsuleShape Capsule(const FVec_Internal& Start, const FVec_Internal& End, f32 Radius)
	{
		return {Store(Start), Store(End), Radius};
	}
	// 3Dの角速度はWorld座標のまま共通の外積へ渡す。
	static FVec_Internal AngularVelocity(const FWorld& World, FBodyId Body)
	{
		return Load(World.GetAngularVelocity(Body));
	}
};

// 1回の呼出しでのWorld問い合わせの残り。
struct FBudget_Internal
{
	// 行った回数。
	int32 Used = 0;
	// 上限。
	int32 Limit = 0;
	// 1回分を使う。上限に達していればfalse。
	bool Take() noexcept
	{
		if (Used >= Limit)
		{
			return false;
		}
		++Used;
		return true;
	}
};

// 検査済みの設定と、単位化したUp・最大傾斜の余弦。
struct FCheckedSettings_Internal
{
	// 単位化したUp。
	FVec_Internal Up;
	// cos(MaxSlopeAngle)。
	f64 CosMaxSlope = 0;
};
bool Finite_Internal(f64 Value) noexcept
{
	return Toolbox::IsFinite(Value);
}
// 設定を検査する。不正な値はFException。
template <typename T> FCheckedSettings_Internal CheckSettings_Internal(const typename T::FSettings& Settings)
{
	const bool bValid =
	    Toolbox::IsFinite(Settings.Radius) && Settings.Radius > 0 && Finite_Internal(Settings.SkinWidth) &&
	    Settings.SkinWidth > 0 && Finite_Internal(Settings.MinMoveDistance) && Settings.MinMoveDistance > 0 &&
	    Settings.MaxIterations >= 1 && Settings.MaxQueries >= 1 && Settings.MaxRecoveryIterations >= 0 &&
	    Finite_Internal(Settings.MaxRecoveryDistance) && Settings.MaxRecoveryDistance >= 0 &&
	    Finite_Internal(Settings.MaxSlopeAngle) && Settings.MaxSlopeAngle >= 0 &&
	    Settings.MaxSlopeAngle < 1.5707963267948966 && Finite_Internal(Settings.StepHeight) &&
	    Settings.StepHeight >= 0 && Finite_Internal(Settings.GroundSnapDistance) && Settings.GroundSnapDistance >= 0 &&
	    Finite_Internal(Settings.MaxSpeed) && Settings.MaxSpeed >= 0 && Finite_Internal(Settings.Acceleration) &&
	    Settings.Acceleration >= 0 && Finite_Internal(Settings.Deceleration) && Settings.Deceleration >= 0 &&
	    Finite_Internal(Settings.AirControl) && Settings.AirControl >= 0 && Settings.AirControl <= 1 &&
	    Finite_Internal(Settings.JumpSpeed) && Settings.JumpSpeed >= 0 && Finite_Internal(Settings.Gravity) &&
	    Settings.Gravity >= 0 && Finite_Internal(Settings.MaxFallSpeed) && Settings.MaxFallSpeed > 0 &&
	    Finite_Internal(Settings.MaxGroundCarrySpeed) && Settings.MaxGroundCarrySpeed > 0 && Settings.Up.IsValid() &&
	    (Settings.Shape == ECharacterShape::Round ||
	     (Settings.Shape == ECharacterShape::Capsule && Finite_Internal(Settings.HalfHeight) &&
	      Settings.HalfHeight >= 0));
	if (!bValid)
	{
		throw Toolbox::FException("Invalid character move settings");
	}
	const FVec_Internal Up = T::Load(Settings.Up);
	const f64 Length = Length_Internal(Up);
	if (!(Length > 0))
	{
		throw Toolbox::FException("Invalid character up direction");
	}
	FCheckedSettings_Internal Checked;
	Checked.Up = Scale_Internal(Up, 1 / Length);
	Checked.CosMaxSlope = Toolbox::Cos(Settings.MaxSlopeAngle);
	return Checked;
}

// Capsuleの形状：中心線はUpに沿い、中心は中心線の中点。設定は検査済み。
template <typename T>
typename T::FCapsuleShape CapsuleAt_Internal(const typename T::FSettings& Settings, const typename T::FVector& Center,
                                             f32 Radius)
{
	const FVec_Internal Up = T::Load(Settings.Up);
	const FVec_Internal Axis = Scale_Internal(Up, Settings.HalfHeight / Length_Internal(Up));
	const FVec_Internal Middle = T::Load(Center);
	return T::Capsule(Sub_Internal(Middle, Axis), Add_Internal(Middle, Axis), Radius);
}
// 形状の選択に応じた接触の問い合わせ。Roundは従来どおり円／球で調べる。
template <typename T>
auto ShapeContacts_Internal(const typename T::FWorld& World, const typename T::FSettings& Settings,
                            const typename T::FVector& Center, f32 Radius, f64 Margin,
                            const Toolbox::TOptional<typename T::FBodyId>& Excluded, const FWorldQueryFilter& Filter)
{
	if (Settings.Shape == ECharacterShape::Capsule)
	{
		return World.QueryCapsuleContacts(CapsuleAt_Internal<T>(Settings, Center, Radius), Margin, Excluded, Filter);
	}
	return World.QueryContacts(T::Shape(Center, Radius), Margin, Excluded, Filter);
}
// 形状の選択に応じた移動の問い合わせ（開始時の接触を除く）。CapsuleのTargetは中心線の中点の終点。
template <typename T>
auto ShapeSweep_Internal(const typename T::FWorld& World, const typename T::FSettings& Settings,
                         const typename T::FVector& From, f32 Radius, const typename T::FVector& Target,
                         const Toolbox::TOptional<typename T::FBodyId>& Excluded, const FWorldQueryFilter& Filter)
{
	if (Settings.Shape == ECharacterShape::Capsule)
	{
		// 問い合わせの移動の基準はカプセルの中点（丸めでFromと1ulpほど違い得る）。その差を終点にも加え、移動量をTarget−From
		// に保つ（面に沿う移動が、丸めの差で面へ近づく移動にならない）。
		const auto Capsule = CapsuleAt_Internal<T>(Settings, From, Radius);
		const FVec_Internal Offset = Sub_Internal(T::Load(Toolbox::CapsuleCenter(Capsule)), T::Load(From));
		return World.SweepCapsuleClosestIgnoringInitialContacts(
		    Capsule, T::Store(Add_Internal(T::Load(Target), Offset)), Excluded, Filter);
	}
	return World.SweepClosestIgnoringInitialContacts(T::Shape(From, Radius), Target, Excluded, Filter);
}
// 移動の問い合わせに使う、接触余裕を含めた半径。これで当たった中心は、対象の表面から接触余裕の距離にある。
f32 Inflated_Internal(f32 Radius, f64 SkinWidth)
{
	return ToF32_Internal(f64(Radius) + SkinWidth);
}
// 移動中の制約面。
struct FPlane_Internal
{
	// 単位法線（対象から中心へ向く）。
	FVec_Internal Normal;
};
// 制約面の集合。容量は結果に残せる接触の数と同じ。
struct FPlanes_Internal
{
	// 保持した面。
	Toolbox::TArray<FPlane_Internal, FCharacterMoveResult2D::MaxContacts> Items;
	// 有効な件数。
	uint32 Count = 0;
};
// VをすべてのPlanesの内向き成分を持たない方向へ投影する。2Dは二つの面の角、3Dは三つの面で0になる。
FVec_Internal Clip_Internal(const FVec_Internal& Value, const FPlanes_Internal& Planes, int32 Dimension) noexcept
{
	const f64 Limit = -Length_Internal(Value) * 1e-12;
	const auto Satisfies = [&](const FVec_Internal& Candidate)
	{
		for (uint32 Index = 0; Index < Planes.Count; ++Index)
		{
			if (Dot_Internal(Candidate, Planes.Items[Index].Normal) < Limit)
			{
				return false;
			}
		}
		return true;
	};
	if (Satisfies(Value))
	{
		return Value;
	}
	// 一つの面の内向き成分だけを除いて、すべての面を満たすか。
	for (uint32 Index = 0; Index < Planes.Count; ++Index)
	{
		const FVec_Internal& Normal = Planes.Items[Index].Normal;
		const f64 Inward = Dot_Internal(Value, Normal);
		if (Inward >= 0)
		{
			continue;
		}
		const FVec_Internal Candidate =
		    Sub_Internal(Value, Scale_Internal(Normal, Inward / Dot_Internal(Normal, Normal)));
		if (Satisfies(Candidate))
		{
			return Candidate;
		}
	}
	// 3Dは二つの面の稜線の方向へ。
	if (Dimension == 3)
	{
		for (uint32 First = 0; First < Planes.Count; ++First)
		{
			for (uint32 Second = First + 1; Second < Planes.Count; ++Second)
			{
				const FVec_Internal Edge = Cross_Internal(Planes.Items[First].Normal, Planes.Items[Second].Normal);
				const f64 Length = Length_Internal(Edge);
				if (!(Length > 1e-9))
				{
					continue;
				}
				const FVec_Internal Direction = Scale_Internal(Edge, 1 / Length);
				const FVec_Internal Candidate = Scale_Internal(Direction, Dot_Internal(Value, Direction));
				if (Satisfies(Candidate))
				{
					return Candidate;
				}
			}
		}
	}
	return {};
}
// Upに沿う成分を除いた方向（水平の移動で急な面を壁として扱う）。長さが小さすぎれば空。
bool Horizontal_Internal(const FVec_Internal& Normal, const FVec_Internal& Up, FVec_Internal& Out) noexcept
{
	const FVec_Internal Flat = Sub_Internal(Normal, Scale_Internal(Up, Dot_Internal(Normal, Up)));
	const f64 Length = Length_Internal(Flat);
	if (!(Length > 1e-6))
	{
		return false;
	}
	Out = Scale_Internal(Flat, 1 / Length);
	return true;
}

// 移動の方式。水平の移動では、歩ける傾斜でない面を水平な壁として扱い、急な面を上らない。
struct FMoveMode_Internal
{
	// 急な面を水平化するか。
	bool bHorizontal = false;
	// 単位化したUp。
	FVec_Internal Up;
	// cos(MaxSlopeAngle)。
	f64 CosMaxSlope = 0;
};
// 制約として使う法線。水平の移動では歩けない面を水平化する。真上・真下を向く面（天井など）は水平の移動を妨げないのでfalse。
bool ConstraintNormal_Internal(const FVec_Internal& Normal, const FMoveMode_Internal& Mode, FVec_Internal& Out) noexcept
{
	Out = Normal;
	if (Mode.bHorizontal && Dot_Internal(Normal, Mode.Up) < Mode.CosMaxSlope)
	{
		return Horizontal_Internal(Normal, Mode.Up, Out);
	}
	return true;
}
// 制約面を追加する。ほぼ同じ向きの面は一つにまとめる。追加できたらtrue、重複・制約にならない面ならfalse、容量超過はbFull。
// 結果の接触には実際の法線を、制約には水平化した法線を使う。
template <typename T>
bool AddPlane_Internal(FPlanes_Internal& Planes, typename T::FMoveResult& Result, const FVec_Internal& Actual,
                       typename T::FColliderId Collider, const FMoveMode_Internal& Mode, bool& bFull)
{
	FVec_Internal Normal;
	if (!ConstraintNormal_Internal(Actual, Mode, Normal))
	{
		return false;
	}
	for (uint32 Index = 0; Index < Planes.Count; ++Index)
	{
		if (Dot_Internal(Planes.Items[Index].Normal, Normal) > 1 - 1e-9)
		{
			return false;
		}
	}
	if (Planes.Count >= Planes.Items.Size())
	{
		bFull = true;
		return false;
	}
	Planes.Items[Planes.Count].Normal = Normal;
	++Planes.Count;
	auto& Contact = Result.Contacts[Result.ContactCount];
	Contact.Collider = Collider;
	Contact.Normal = T::Store(Actual);
	++Result.ContactCount;
	return true;
}

// 移動を止める理由の任意値。
using TOptional_Stop_Internal = Toolbox::TOptional<ECharacterMoveStop>;
// 反復型の衝突付き移動。すべての経路を検査し、止まった理由と未処理の移動を返す。
template <typename T>
typename T::FMoveResult MoveCore_Internal(const typename T::FWorld& World, const typename T::FVector& StartCenter,
                                          const FVec_Internal& Displacement, const typename T::FSettings& Settings,
                                          const FMoveMode_Internal& Mode,
                                          const Toolbox::TOptional<typename T::FBodyId>& Excluded,
                                          const FWorldQueryFilter& Filter, FBudget_Internal& Budget)
{
	typename T::FMoveResult Result;
	const FVec_Internal Start = T::Load(StartCenter);
	FVec_Internal Center = Start;
	FVec_Internal Remaining = Displacement;
	const auto Finish = [&](ECharacterMoveStop Stop)
	{
		Result.Stop = Stop;
		Result.EndCenter = T::Store(Center);
		Result.Applied = T::Store(Sub_Internal(Center, Start));
		Result.Remaining = T::Store(Remaining);
		return Result;
	};
	if (Length_Internal(Remaining) < Settings.MinMoveDistance)
	{
		Remaining = {};
		return Finish(ECharacterMoveStop::NoMovement);
	}
	// 接触余裕以内の面を制約にする。方向の決まらない接触・重なりがあれば動かない。
	// 開始時と、当たった位置へ進んだ後に取り直す（その位置で接触余裕以内になった面は、接触余裕を含めた問い合わせでは
	// 初期接触として除かれるため、ここで制約にする）。
	FPlanes_Internal Planes;
	bool bFull = false;
	const f32 Inflated = Inflated_Internal(Settings.Radius, Settings.SkinWidth);
	TOptional_Stop_Internal Failure;
	const auto Gather = [&]() -> bool
	{
		if (!Budget.Take())
		{
			Failure = ECharacterMoveStop::QueryLimit;
			return false;
		}
		++Result.Queries;
		const auto Contacts = ShapeContacts_Internal<T>(World, Settings, T::Store(Center), Settings.Radius,
		                                                Settings.SkinWidth, Excluded, Filter);
		if (!Contacts.IsComplete())
		{
			Failure = ECharacterMoveStop::ContactLimit;
			return false;
		}
		for (uint32 Index = 0; Index < Contacts.Count; ++Index)
		{
			const auto& Contact = Contacts.Items[Index];
			if (!Contact.Normal)
			{
				if (Contact.Separation <= 0)
				{
					Failure = ECharacterMoveStop::AmbiguousContact;
					return false;
				}
				continue;
			}
			FVec_Internal Normal = T::Load(*Contact.Normal);
			Normal = Scale_Internal(Normal, 1 / Length_Internal(Normal));
			AddPlane_Internal<T>(Planes, Result, Normal, Contact.Collider, Mode, bFull);
			if (bFull)
			{
				Failure = ECharacterMoveStop::ContactLimit;
				return false;
			}
		}
		return true;
	};
	if (!Gather())
	{
		return Finish(*Failure);
	}
	bool bModified = false;
	for (int32 Iteration = 0; Iteration < Settings.MaxIterations; ++Iteration)
	{
		++Result.Iterations;
		const FVec_Internal Step = Clip_Internal(Remaining, Planes, T::Dimension);
		if (Length_Internal(Step) < Settings.MinMoveDistance)
		{
			// 制約で進める成分がない。取り除いた内向き成分は未処理の移動に含めない。
			Remaining = {};
			return Finish(ECharacterMoveStop::Blocked);
		}
		bModified = bModified || !Equal_Internal(Step, Remaining);
		const typename T::FVector Target = T::Store(Add_Internal(Center, Step));
		const FVec_Internal TargetValue = T::Load(Target);
		if (Equal_Internal(TargetValue, Center))
		{
			Remaining = Step;
			return Finish(ECharacterMoveStop::PrecisionLimit);
		}
		if (!Budget.Take())
		{
			Remaining = Step;
			return Finish(ECharacterMoveStop::QueryLimit);
		}
		++Result.Queries;
		const typename T::FVector From = T::Store(Center);
		// 接触余裕を含めた半径で調べる。開始時に接触余裕以内の面は、この問い合わせでは初期接触として除かれ、制約で扱う。
		const auto Hit = ShapeSweep_Internal<T>(World, Settings, From, Inflated, Target, Excluded, Filter);
		const FVec_Internal Path = Sub_Internal(TargetValue, Center);
		if (!Hit)
		{
			// 非交差の区間は、同じf32の終点まで検査済み。
			Center = TargetValue;
			Remaining = {};
			return Finish(bModified ? ECharacterMoveStop::Slid : ECharacterMoveStop::Completed);
		}
		const f64 Fraction = Hit->Fraction;
		// 丸めた候補を開始中心から実際の半径のSweepで確認して採用する（重ならないこと）。接触すれば採用しない。
		const auto TryMove = [&](const FVec_Internal& Candidate, bool& bQueryLimit)
		{
			const typename T::FVector Rounded = T::Store(Candidate);
			const FVec_Internal RoundedValue = T::Load(Rounded);
			if (Equal_Internal(RoundedValue, Center))
			{
				return false;
			}
			if (!Budget.Take())
			{
				bQueryLimit = true;
				return false;
			}
			++Result.Queries;
			if (ShapeSweep_Internal<T>(World, Settings, From, Settings.Radius, Rounded, Excluded, Filter))
			{
				return false;
			}
			Center = RoundedValue;
			return true;
		};
		// 当たった中心（対象の表面から接触余裕の距離）まで進む。
		bool bQueryLimit = false;
		const bool bMoved = TryMove(Add_Internal(Center, Scale_Internal(Path, Fraction)), bQueryLimit);
		if (!Hit->Normal)
		{
			// 法線がなければ、接触の手前で止める。
			Remaining = Sub_Internal(TargetValue, Center);
			return Finish(bQueryLimit ? ECharacterMoveStop::QueryLimit : ECharacterMoveStop::MissingNormal);
		}
		FVec_Internal Normal = T::Load(*Hit->Normal);
		Normal = Scale_Internal(Normal, 1 / Length_Internal(Normal));
		Remaining = Scale_Internal(Path, 1 - Fraction);
		if (bQueryLimit)
		{
			return Finish(ECharacterMoveStop::QueryLimit);
		}
		const bool bAdded = AddPlane_Internal<T>(Planes, Result, Normal, Hit->Collider, Mode, bFull);
		if (bFull)
		{
			return Finish(ECharacterMoveStop::ContactLimit);
		}
		if (bMoved && !Gather())
		{
			return Finish(*Failure);
		}
		if (!bMoved && !bAdded)
		{
			// 進めず、新しい制約もない。同じ問い合わせを繰り返さない。
			return Finish(ECharacterMoveStop::PrecisionLimit);
		}
	}
	return Finish(ECharacterMoveStop::IterationLimit);
}

// 初期重なりの解消。失敗した場合は元の中心と理由を返す。
template <typename T>
typename T::FRecovery RecoverCore_Internal(const typename T::FWorld& World, const typename T::FVector& StartCenter,
                                           const typename T::FSettings& Settings,
                                           const Toolbox::TOptional<typename T::FBodyId>& Excluded,
                                           const FWorldQueryFilter& Filter, FBudget_Internal& Budget)
{
	typename T::FRecovery Result;
	Result.Center = StartCenter;
	FVec_Internal Center = T::Load(StartCenter);
	f64 Total = 0;
	int32 Iterations = 0;
	const auto Fail = [&](ECharacterRecoveryStatus Status)
	{
		// 途中の補正は採用しない。
		Result.Status = Status;
		Result.Center = StartCenter;
		Result.Distance = 0;
		Result.Iterations = Iterations;
		return Result;
	};
	for (;;)
	{
		if (!Budget.Take())
		{
			return Fail(ECharacterRecoveryStatus::QueryLimit);
		}
		++Result.Queries;
		const typename T::FVector Current = T::Store(Center);
		const auto Contacts = ShapeContacts_Internal<T>(World, Settings, Current, Settings.Radius, 0, Excluded, Filter);
		if (!Contacts.IsComplete())
		{
			return Fail(ECharacterRecoveryStatus::ContactLimit);
		}
		// 最も深い重なり（同じ深さは先のスロット）。境界だけの接触（0）は重なりではない。
		int32 Deepest = -1;
		for (uint32 Index = 0; Index < Contacts.Count; ++Index)
		{
			const f64 Separation = Contacts.Items[Index].Separation;
			if (Separation < 0 && (Deepest < 0 || Separation < Contacts.Items[static_cast<uint32>(Deepest)].Separation))
			{
				Deepest = static_cast<int32>(Index);
			}
		}
		if (Deepest < 0)
		{
			Result.Status = Iterations == 0 ? ECharacterRecoveryStatus::NoOverlap : ECharacterRecoveryStatus::Resolved;
			Result.Center = Current;
			Result.Distance = Total;
			Result.Iterations = Iterations;
			return Result;
		}
		const auto& Contact = Contacts.Items[static_cast<uint32>(Deepest)];
		Result.Collider = Contact.Collider;
		if (Iterations >= Settings.MaxRecoveryIterations)
		{
			return Fail(ECharacterRecoveryStatus::IterationLimit);
		}
		if (!Contact.Normal)
		{
			return Fail(ECharacterRecoveryStatus::Ambiguous);
		}
		// 接触余裕の位置まで、法線方向へ動かす。
		const f64 Push = Settings.SkinWidth - Contact.Separation;
		Total += Push;
		if (Total > Settings.MaxRecoveryDistance)
		{
			return Fail(ECharacterRecoveryStatus::TooDeep);
		}
		FVec_Internal Normal = T::Load(*Contact.Normal);
		Normal = Scale_Internal(Normal, 1 / Length_Internal(Normal));
		const typename T::FVector Candidate = T::Store(Add_Internal(Center, Scale_Internal(Normal, Push)));
		// 補正の経路が、開始時に接触していない別のColliderに当たらないこと。
		if (!Budget.Take())
		{
			return Fail(ECharacterRecoveryStatus::QueryLimit);
		}
		++Result.Queries;
		const auto Blocking =
		    ShapeSweep_Internal<T>(World, Settings, Current, Settings.Radius, Candidate, Excluded, Filter);
		if (Blocking)
		{
			Result.Collider = Blocking->Collider;
			return Fail(ECharacterRecoveryStatus::Blocked);
		}
		Center = T::Load(Candidate);
		++Iterations;
	}
}

// 足元の支持。接触余裕の2倍以内で、Up側を向く面から選ぶ（歩ける面を優先し、近いもの、同じなら先のスロット）。
template <typename T>
typename T::FGround GroundCore_Internal(const typename T::FWorld& World, const typename T::FVector& Center,
                                        const typename T::FSettings& Settings, const FCheckedSettings_Internal& Checked,
                                        const Toolbox::TOptional<typename T::FBodyId>& Excluded,
                                        const FWorldQueryFilter& Filter, FBudget_Internal& Budget, bool& bQueryLimit)
{
	typename T::FGround Ground;
	if (!Budget.Take())
	{
		bQueryLimit = true;
		Ground.bComplete = false;
		return Ground;
	}
	const auto Contacts =
	    ShapeContacts_Internal<T>(World, Settings, Center, Settings.Radius, Settings.SkinWidth * 2, Excluded, Filter);
	Ground.bComplete = Contacts.IsComplete();
	int32 Best = -1;
	bool bBestWalkable = false;
	for (uint32 Index = 0; Index < Contacts.Count; ++Index)
	{
		const auto& Contact = Contacts.Items[Index];
		if (!Contact.Normal)
		{
			continue;
		}
		const FVec_Internal Normal = T::Load(*Contact.Normal);
		const f64 Support = Dot_Internal(Normal, Checked.Up) / Length_Internal(Normal);
		if (!(Support > 1e-6))
		{
			continue;
		}
		const bool bWalkable = Support >= Checked.CosMaxSlope;
		const bool bBetter =
		    Best < 0 || (bWalkable && !bBestWalkable) ||
		    (bWalkable == bBestWalkable && Contact.Separation < Contacts.Items[static_cast<uint32>(Best)].Separation);
		if (bBetter)
		{
			Best = static_cast<int32>(Index);
			bBestWalkable = bWalkable;
		}
	}
	if (Best >= 0)
	{
		const auto& Contact = Contacts.Items[static_cast<uint32>(Best)];
		Ground.State = bBestWalkable ? ECharacterGroundState::Walkable : ECharacterGroundState::Steep;
		Ground.Collider = Contact.Collider;
		Ground.Normal = Contact.Normal;
		Ground.Separation = Contact.Separation;
	}
	return Ground;
}

// 下向きに床を探して接触余裕の位置まで吸い付く。歩ける床に当たった場合だけ動かす。
// Rejectedがnullptrでなければ、歩けない面に当たった場合にその単位法線を返す（それ以外は0）。
template <typename T>
bool SnapCore_Internal(const typename T::FWorld& World, FVec_Internal& Center, f64 Distance,
                       const typename T::FSettings& Settings, const FCheckedSettings_Internal& Checked,
                       const Toolbox::TOptional<typename T::FBodyId>& Excluded, const FWorldQueryFilter& Filter,
                       FBudget_Internal& Budget, FVec_Internal* Rejected)
{
	if (Rejected != nullptr)
	{
		*Rejected = {};
	}
	const typename T::FVector From = T::Store(Center);
	const typename T::FVector Target = T::Store(Sub_Internal(Center, Scale_Internal(Checked.Up, Distance)));
	if (!Budget.Take())
	{
		return false;
	}
	const auto Hit = ShapeSweep_Internal<T>(
	    World, Settings, From, Inflated_Internal(Settings.Radius, Settings.SkinWidth), Target, Excluded, Filter);
	if (!Hit || !Hit->Normal)
	{
		return false;
	}
	FVec_Internal Normal = T::Load(*Hit->Normal);
	Normal = Scale_Internal(Normal, 1 / Length_Internal(Normal));
	if (Dot_Internal(Normal, Checked.Up) < Checked.CosMaxSlope)
	{
		if (Rejected != nullptr)
		{
			*Rejected = Normal;
		}
		return false;
	}
	const FVec_Internal AtHit =
	    Add_Internal(Center, Scale_Internal(Sub_Internal(T::Load(Target), Center), Hit->Fraction));
	const typename T::FVector Landing = T::Store(AtHit);
	if (Equal_Internal(T::Load(Landing), Center))
	{
		return true;
	}
	if (!Budget.Take())
	{
		return false;
	}
	if (ShapeSweep_Internal<T>(World, Settings, From, Settings.Radius, Landing, Excluded, Filter))
	{
		return false;
	}
	Center = T::Load(Landing);
	return true;
}

// 水平の移動で、歩けない面に止められたか。
template <typename T>
bool BlockedByWall_Internal(const typename T::FMoveResult& Move, const FVec_Internal& Intended,
                            const typename T::FSettings& Settings)
{
	if (Move.Stop != ECharacterMoveStop::Blocked && Move.Stop != ECharacterMoveStop::Slid)
	{
		return false;
	}
	const f64 Length = Length_Internal(Intended);
	if (Length < Settings.MinMoveDistance)
	{
		return false;
	}
	const f64 Progress = Dot_Internal(T::Load(Move.Applied), Scale_Internal(Intended, 1 / Length));
	return Progress < Length - Settings.MinMoveDistance;
}

// 段差上り: 上へ、水平へ、下へ（すべて体積のある移動で検査）。歩ける床に着地し、元の移動より進む場合だけ採用する。
template <typename T>
bool StepUpCore_Internal(const typename T::FWorld& World, const typename T::FVector& Start,
                         const FVec_Internal& Horizontal, const typename T::FMoveResult& Original,
                         const typename T::FSettings& Settings, const FCheckedSettings_Internal& Checked,
                         const FMoveMode_Internal& Mode, const Toolbox::TOptional<typename T::FBodyId>& Excluded,
                         const FWorldQueryFilter& Filter, FBudget_Internal& Budget, typename T::FMoveResult& Accepted)
{
	const FVec_Internal StartValue = T::Load(Start);
	// 上へ。途中で天井に当たる場合は、段差の高さに関係なく段差上りを行わない。
	const typename T::FVector Raised =
	    T::Store(Add_Internal(StartValue, Scale_Internal(Checked.Up, Settings.StepHeight)));
	if (!Budget.Take())
	{
		return false;
	}
	if (ShapeSweep_Internal<T>(World, Settings, Start, Inflated_Internal(Settings.Radius, Settings.SkinWidth), Raised,
	                           Excluded, Filter))
	{
		return false;
	}
	const f64 Raise = Settings.StepHeight;
	// 水平へ（元の移動と同じ変位）。
	typename T::FMoveResult Forward =
	    MoveCore_Internal<T>(World, Raised, Horizontal, Settings, Mode, Excluded, Filter, Budget);
	if (Forward.Stop == ECharacterMoveStop::QueryLimit || Forward.Stop == ECharacterMoveStop::AmbiguousContact ||
	    Forward.Stop == ECharacterMoveStop::ContactLimit)
	{
		return false;
	}
	// 下へ（上げた分と吸い付きの距離）。歩ける床への着地が必要。
	FVec_Internal Landing = T::Load(Forward.EndCenter);
	const f64 Down = Raise + Settings.GroundSnapDistance + Settings.SkinWidth;
	FVec_Internal Corner;
	if (!SnapCore_Internal<T>(World, Landing, Down, Settings, Checked, Excluded, Filter, Budget, &Corner))
	{
		// 段差の角（上向きだが急な法線）に着地する場合は、接触点の真上まで（最大で半径）さらに前へ進めて一度だけ試す。
		const f64 Support = Dot_Internal(Corner, Checked.Up);
		if (!(Support > 1e-6))
		{
			return false;
		}
		const f64 Across =
		    Toolbox::Sqrt(Toolbox::Max(0.0, 1 - Support * Support)) * Settings.Radius + Settings.SkinWidth;
		const f64 Base = Length_Internal(Horizontal);
		const f64 Extended = Base + Toolbox::Min(Across, f64(Settings.Radius));
		Forward = MoveCore_Internal<T>(World, Raised, Scale_Internal(Horizontal, Extended / Base), Settings, Mode,
		                               Excluded, Filter, Budget);
		if (Forward.Stop == ECharacterMoveStop::QueryLimit || Forward.Stop == ECharacterMoveStop::AmbiguousContact ||
		    Forward.Stop == ECharacterMoveStop::ContactLimit)
		{
			return false;
		}
		Landing = T::Load(Forward.EndCenter);
		if (!SnapCore_Internal<T>(World, Landing, Down, Settings, Checked, Excluded, Filter, Budget, nullptr))
		{
			return false;
		}
	}
	// 実際に上った場合だけ段差上りとする（同じ高さへ戻るだけなら、接触余裕を削って進んだことになる）。
	if (!(Dot_Internal(Sub_Internal(Landing, StartValue), Checked.Up) > Settings.MinMoveDistance))
	{
		return false;
	}
	// 元の移動より水平に進む場合だけ採用する。
	const f64 Length = Length_Internal(Horizontal);
	const FVec_Internal Direction = Scale_Internal(Horizontal, 1 / Length);
	const f64 Before = Dot_Internal(T::Load(Original.Applied), Direction);
	const f64 After = Dot_Internal(Sub_Internal(Landing, StartValue), Direction);
	if (!(After > Before + Settings.MinMoveDistance))
	{
		return false;
	}
	Accepted = Forward;
	Accepted.EndCenter = T::Store(Landing);
	Accepted.Applied = T::Store(Sub_Internal(Landing, StartValue));
	Accepted.Queries = 0;
	return true;
}

// 速度成分を目標へ、最大変化量までの範囲で近づける。
FVec_Internal MoveToward_Internal(const FVec_Internal& Current, const FVec_Internal& Target, f64 MaxChange) noexcept
{
	const FVec_Internal Delta = Sub_Internal(Target, Current);
	const f64 Length = Length_Internal(Delta);
	if (Length <= MaxChange)
	{
		return Target;
	}
	return Add_Internal(Current, Scale_Internal(Delta, MaxChange / Length));
}

// 回転床の点の軌道を、曲がりの幅を加えた円・球で覆って検査する。Worldの積分と同じ途中点を使う。
// 支持床以外との接触で軌道から外れたら、その区間の安全な位置で止め、以後の曲線を継ぎ足さない。
template <typename T>
typename T::FMoveResult CarryCurve_Internal(const typename T::FWorld& World, typename T::FBodyId Carrier,
                                            const typename T::FVector& StartCenter, const FVec_Internal& Displacement,
                                            const typename T::FSettings& Settings, const FMoveMode_Internal& Mode,
                                            const Toolbox::TOptional<typename T::FBodyId>& Excluded,
                                            const FWorldQueryFilter& Filter, FBudget_Internal& Budget, f64 DeltaSeconds,
                                            f64 AngularDistance, f64 OrbitRadius, bool& bRejected)
{
	// 分割を増やし続けず、許容外の運動は追従を始める前に拒否する。
	constexpr int32 MaxSegments = 32;
	// 通常追従として扱う角速度×時間の上限。厳密な回転CCDは提供しない。
	constexpr f64 MaxAngularDistance = 3.141592653589793;
	// 全区間の弦から曲線までのずれの保守上限。2Dと正規化四元数微分による3Dの加速度の大きさはr*omega^2以下。
	const f64 FullCurveWidth = OrbitRadius * AngularDistance * AngularDistance * 0.25;
	// 初期の接触余裕を使い切らない幅まで区間を細かくする。
	const f64 AllowedWidth = Settings.SkinWidth * 0.25;
	// 今回に必要な分割数。
	int32 Segments = 1;
	while (Segments < MaxSegments && FullCurveWidth / (Segments * Segments) > AllowedWidth)
	{
		++Segments;
	}
	// 各区間の曲線を覆うために半径へ足す幅。
	const f64 CurveWidth = FullCurveWidth / (Segments * Segments);
	// 実際に進めた位置と、全区間で使った回数。
	typename T::FMoveResult Result;
	FVec_Internal Center = T::Load(StartCenter);
	int32 Iterations = 0;
	int32 Queries = 0;
	// 部分的に進めた場合も、最初の中心からの合計移動と未処理分を返す。
	const auto Finish = [&](ECharacterMoveStop Stop)
	{
		Result.Stop = Stop;
		Result.EndCenter = T::Store(Center);
		Result.Applied = T::Store(Sub_Internal(Center, T::Load(StartCenter)));
		Result.Remaining = T::Store(Sub_Internal(Displacement, T::Load(Result.Applied)));
		Result.Iterations = Iterations;
		Result.Queries = Queries;
		return Result;
	};
	if (!Finite_Internal(FullCurveWidth) || AngularDistance > MaxAngularDistance || CurveWidth > AllowedWidth)
	{
		bRejected = true;
		return Finish(ECharacterMoveStop::IterationLimit);
	}
	// 元の半径に曲がりの幅だけを足す。通常の接触余裕・接触面の扱いはMoveCoreを再利用する。
	typename T::FSettings CurvedSettings = Settings;
	CurvedSettings.Radius = Inflated_Internal(Settings.Radius, CurveWidth);
	for (int32 Index = 1; Index <= Segments; ++Index)
	{
		if (!Budget.Take())
		{
			return Finish(ECharacterMoveStop::QueryLimit);
		}
		++Queries;
		// 膨らませた形状が最初から重なる場合だけ保守的に止める。通常の接触余裕内の近接面はMoveCoreの制約で扱う。
		const auto Contacts =
		    ShapeContacts_Internal<T>(World, Settings, T::Store(Center), Settings.Radius, CurveWidth, Excluded, Filter);
		if (!Contacts.IsComplete())
		{
			return Finish(ECharacterMoveStop::ContactLimit);
		}
		for (uint32 ContactIndex = 0; ContactIndex < Contacts.Count; ++ContactIndex)
		{
			if (Contacts.Items[ContactIndex].Separation < CurveWidth)
			{
				Result.ContactCount = 0;
				if (Contacts.Items[ContactIndex].Normal)
				{
					Result.Contacts[0].Collider = Contacts.Items[ContactIndex].Collider;
					Result.Contacts[0].Normal = *Contacts.Items[ContactIndex].Normal;
					Result.ContactCount = 1;
				}
				return Finish(ECharacterMoveStop::Blocked);
			}
		}
		// 途中点も元の開始姿勢から求め、Worldを仮の姿勢へ書き換えない。
		const auto Target = World.PredictBodyPoint(Carrier, StartCenter, DeltaSeconds * Index / Segments);
		// 前に採用した位置から、次の検査済み区間の終点へ進む。
		const FVec_Internal SegmentMove = Sub_Internal(T::Load(Target), Center);
		Result =
		    MoveCore_Internal<T>(World, T::Store(Center), SegmentMove, CurvedSettings, Mode, Excluded, Filter, Budget);
		Iterations += Result.Iterations;
		Queries += Result.Queries;
		Center = T::Load(Result.EndCenter);
		if (Length_Internal(Sub_Internal(SegmentMove, T::Load(Result.Applied))) > Settings.MinMoveDistance ||
		    (Result.Stop != ECharacterMoveStop::Completed && Result.Stop != ECharacterMoveStop::NoMovement))
		{
			return Finish(Result.Stop);
		}
	}
	return Finish(ECharacterMoveStop::Completed);
}

// 1回の固定更新の計算。
template <typename T>
typename T::FStepResult StepCore_Internal(const typename T::FWorld& World, const typename T::FSettings& Settings,
                                          const typename T::FState& State, const typename T::FInput& Input,
                                          f64 DeltaSeconds, const Toolbox::TOptional<typename T::FBodyId>& Excluded,
                                          const FWorldQueryFilter& Filter)
{
	const FCheckedSettings_Internal Checked = CheckSettings_Internal<T>(Settings);
	if (!Toolbox::IsFinite(DeltaSeconds) || !(DeltaSeconds > 0) || !State.Center.IsValid() ||
	    !State.Velocity.IsValid() || !Input.Move.IsValid())
	{
		throw Toolbox::FException("Invalid character step input");
	}
	FBudget_Internal Budget;
	Budget.Limit = Settings.MaxQueries;
	typename T::FStepResult Result;
	Result.State = State;
	const FVec_Internal& Up = Checked.Up;
	// 1. 初期重なりの解消。できなければ移動しない。
	Result.Recovery = RecoverCore_Internal<T>(World, State.Center, Settings, Excluded, Filter, Budget);
	if (Result.Recovery.Status != ECharacterRecoveryStatus::NoOverlap &&
	    Result.Recovery.Status != ECharacterRecoveryStatus::Resolved)
	{
		Result.Queries = Budget.Used;
		return Result;
	}
	typename T::FVector Center = Result.Recovery.Center;
	// 2. 足元の確認。
	bool bQueryLimit = false;
	const typename T::FGround Before =
	    GroundCore_Internal<T>(World, Center, Settings, Checked, Excluded, Filter, Budget, bQueryLimit);
	const bool bWasGrounded = Before.State == ECharacterGroundState::Walkable;
	// 3. 速度の更新（水平の加減速、ジャンプ、重力）。
	const FVec_Internal Velocity = T::Load(State.Velocity);
	f64 Vertical = Dot_Internal(Velocity, Up);
	FVec_Internal Horizontal = Sub_Internal(Velocity, Scale_Internal(Up, Vertical));
	FVec_Internal Wish = T::Load(Input.Move);
	Wish = Sub_Internal(Wish, Scale_Internal(Up, Dot_Internal(Wish, Up)));
	const f64 WishLength = Length_Internal(Wish);
	if (WishLength > 1)
	{
		Wish = Scale_Internal(Wish, 1 / WishLength);
	}
	const FVec_Internal Target = Scale_Internal(Wish, Settings.MaxSpeed);
	const f64 Control = bWasGrounded ? 1.0 : Settings.AirControl;
	const f64 Rate = (WishLength > 0 ? Settings.Acceleration : Settings.Deceleration) * Control;
	Horizontal = MoveToward_Internal(Horizontal, Target, Rate * DeltaSeconds);
	if (Input.bJump && bWasGrounded)
	{
		Vertical = Settings.JumpSpeed;
		Result.bJumped = true;
	}
	else if (bWasGrounded)
	{
		Vertical = 0;
	}
	else
	{
		Vertical = Toolbox::Max(Vertical - Settings.Gravity * DeltaSeconds, -Settings.MaxFallSpeed);
	}
	// 4. 水平の移動。歩けない面は壁として扱い、止められたら段差上りを試す。
	FMoveMode_Internal Flat;
	Flat.bHorizontal = true;
	Flat.Up = Up;
	Flat.CosMaxSlope = Checked.CosMaxSlope;
	const FVec_Internal HorizontalMove = Scale_Internal(Horizontal, DeltaSeconds);
	Result.Horizontal = MoveCore_Internal<T>(World, Center, HorizontalMove, Settings, Flat, Excluded, Filter, Budget);
	if (bWasGrounded && !Result.bJumped && Settings.StepHeight > 0 &&
	    BlockedByWall_Internal<T>(Result.Horizontal, HorizontalMove, Settings))
	{
		typename T::FMoveResult Stepped;
		if (StepUpCore_Internal<T>(World, Center, HorizontalMove, Result.Horizontal, Settings, Checked, Flat, Excluded,
		                           Filter, Budget, Stepped))
		{
			Result.Horizontal = Stepped;
			Result.bSteppedUp = true;
		}
	}
	Center = Result.Horizontal.EndCenter;
	// 壁に止められた内向きの水平速度は失う（歩ける斜面に沿う場合は保つ）。
	if (!Result.bSteppedUp)
	{
		FPlanes_Internal Walls;
		for (uint32 Index = 0; Index < Result.Horizontal.ContactCount; ++Index)
		{
			const FVec_Internal Normal = T::Load(Result.Horizontal.Contacts[Index].Normal);
			FVec_Internal Wall;
			if (Dot_Internal(Normal, Up) < Checked.CosMaxSlope && Horizontal_Internal(Normal, Up, Wall) &&
			    Walls.Count < Walls.Items.Size())
			{
				Walls.Items[Walls.Count].Normal = Wall;
				++Walls.Count;
			}
		}
		Horizontal = Clip_Internal(Horizontal, Walls, T::Dimension);
	}
	// 5. Up方向の移動（ジャンプ・重力）。天井に当たれば上向きの速度を失う。
	FMoveMode_Internal Free;
	Free.Up = Up;
	Free.CosMaxSlope = Checked.CosMaxSlope;
	Result.Vertical = MoveCore_Internal<T>(World, Center, Scale_Internal(Up, Vertical * DeltaSeconds), Settings, Free,
	                                       Excluded, Filter, Budget);
	Center = Result.Vertical.EndCenter;
	for (uint32 Index = 0; Index < Result.Vertical.ContactCount; ++Index)
	{
		const FVec_Internal Normal = T::Load(Result.Vertical.Contacts[Index].Normal);
		if (Vertical > 0 && Dot_Internal(Normal, Up) < -1e-6)
		{
			Vertical = 0;
			Result.bHitCeiling = true;
		}
	}
	// 6. 接地を続ける場合は、下り坂・小さな段差の下りへ吸い付く（上昇中のジャンプは吸い戻さない）。
	typename T::FGround After =
	    GroundCore_Internal<T>(World, Center, Settings, Checked, Excluded, Filter, Budget, bQueryLimit);
	if (bWasGrounded && !Result.bJumped && Vertical <= 0 && After.State != ECharacterGroundState::Walkable &&
	    Settings.GroundSnapDistance > 0)
	{
		FVec_Internal Snapped = T::Load(Center);
		if (SnapCore_Internal<T>(World, Snapped, Settings.GroundSnapDistance + Settings.SkinWidth, Settings, Checked,
		                         Excluded, Filter, Budget, nullptr))
		{
			Center = T::Store(Snapped);
			Result.bSnapped = true;
			After = GroundCore_Internal<T>(World, Center, Settings, Checked, Excluded, Filter, Budget, bQueryLimit);
		}
	}
	// 7. 足元の状態と速度を確定する。
	const bool bGrounded = After.State == ECharacterGroundState::Walkable;
	if (bGrounded && Vertical <= 0)
	{
		Vertical = 0;
	}
	Result.bLanded = !bWasGrounded && bGrounded;
	Result.bLeftGround = bWasGrounded && !bGrounded;
	FVec_Internal FinalVelocity = Add_Internal(Horizontal, Scale_Internal(Up, Vertical));
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
	// 床を追従する条件の確認から、予測・経路検査・離地速度継承までだけを記録する。
	PhysicsPrivate::FWorldInteractionProbe::FRegion CarryProbe(PhysicsPrivate::FWorldInteractionProbe::EPhase::Carry);
#endif
	// 8. 動く床の追従。ここまでの移動は、固定更新の開始時の床の姿勢に対して決めた（床の上を歩いた）もの。
	// 開始時に乗っていたKinematicの床に固定した中心の点が、この物理Stepで動く量だけ運ぶ（登録順・描画に依存しない）。
	// 乗った直後（開始時に接地していない）には過去の床の移動を遡って加えない。
	if (Settings.bFollowMovingGround && bWasGrounded && Before.Collider && World.IsColliderAlive(*Before.Collider) &&
	    World.GetBodyType(Before.Collider->Body) == EBodyType::Kinematic)
	{
		const typename T::FBodyId Carrier = Before.Collider->Body;
		Result.Carrier = *Before.Collider;
		// 同じ固定更新の開始時の速度と回転中心。描画補間や終点の差分速度は使わない。
		const FVec_Internal LinearVelocity = T::Load(World.GetVelocity(Carrier));
		const FVec_Internal AngularVelocity = T::AngularVelocity(World, Carrier);
		const FVec_Internal Origin = T::Load(World.GetPosition(Carrier));
		// 歩行・ジャンプ前の支持点の速度。離地するときに一度だけ引き継ぐ。
		const FVec_Internal SupportVelocity = Add_Internal(
		    LinearVelocity, Cross_Internal(AngularVelocity, Sub_Internal(T::Load(Result.Recovery.Center), Origin)));
		// 実際に追従する中心の回転半径と、曲線全体の速さの保守上限。
		const f64 Spin = Length_Internal(AngularVelocity);
		const f64 TangentialSpeed =
		    Length_Internal(Cross_Internal(AngularVelocity, Sub_Internal(T::Load(Center), Origin)));
		const FVec_Internal Moved =
		    Sub_Internal(T::Load(World.PredictBodyPoint(Carrier, Center, DeltaSeconds)), T::Load(Center));
		Result.CarryRequested = T::Store(Moved);
		if (!(Length_Internal(LinearVelocity) + TangentialSpeed <= Settings.MaxGroundCarrySpeed))
		{
			Result.bCarryRejected = true;
		}
		else
		{
			// 床自身は（床と一緒に動くため）障害物にせず、壁・天井などの他のSolidとの経路だけを検査する。
			FWorldQueryFilter CarryFilter = Filter;
			ExcludeSecondBody(CarryFilter, Carrier);
			FMoveMode_Internal Carry;
			Carry.Up = Up;
			Carry.CosMaxSlope = Checked.CosMaxSlope;
			if (Spin > 0 && TangentialSpeed > 0)
			{
				Result.Carry = CarryCurve_Internal<T>(World, Carrier, Center, Moved, Settings, Carry, Excluded,
				                                      CarryFilter, Budget, DeltaSeconds, Spin * DeltaSeconds,
				                                      TangentialSpeed / Spin, Result.bCarryRejected);
			}
			else
			{
				Result.Carry =
				    MoveCore_Internal<T>(World, Center, Moved, Settings, Carry, Excluded, CarryFilter, Budget);
			}
			Center = Result.Carry.EndCenter;
			Result.bCarried = !Result.bCarryRejected;
			// 止められた量＝床の運動が求めた量と、実際に動いた量の差。
			Result.bCarryBlocked =
			    !Result.bCarryRejected &&
			    Length_Internal(Sub_Internal(Moved, T::Load(Result.Carry.Applied))) > Settings.MinMoveDistance;
			// 床を離れた固定更新だけ、床の点の速度を一度加える（空中で毎回は加えない）。
			if (!Result.bCarryRejected && Settings.bInheritGroundVelocity && (Result.bJumped || !bGrounded))
			{
				FinalVelocity = Add_Internal(FinalVelocity, SupportVelocity);
				Result.bInheritedGroundVelocity = true;
			}
		}
	}
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
	CarryProbe.Stop();
#endif
	Result.State.Center = Center;
	Result.State.Velocity = T::Store(FinalVelocity);
	Result.State.Ground = After;
	Result.Queries = Budget.Used;
	return Result;
}

// カプセルの半高の変更。足元を保った中心で、伸ばす場合だけ新しい形状の重なりを調べる。
template <typename T>
typename T::FResize ResizeCore_Internal(const typename T::FWorld& World, const typename T::FVector& Center,
                                        const typename T::FSettings& Settings, f64 NewHalfHeight,
                                        const Toolbox::TOptional<typename T::FBodyId>& Excluded,
                                        const FWorldQueryFilter& Filter)
{
	const FCheckedSettings_Internal Checked = CheckSettings_Internal<T>(Settings);
	if (Settings.Shape != ECharacterShape::Capsule)
	{
		throw Toolbox::FException("Character resize requires a capsule shape");
	}
	if (!Finite_Internal(NewHalfHeight) || NewHalfHeight < 0)
	{
		throw Toolbox::FException("Invalid character capsule half height");
	}
	typename T::FResize Result;
	Result.Center = Center;
	// 足元（中心からUpの逆向きにHalfHeight＋Radius）を保つ中心。
	const typename T::FVector Moved =
	    T::Store(Add_Internal(T::Load(Center), Scale_Internal(Checked.Up, NewHalfHeight - Settings.HalfHeight)));
	if (NewHalfHeight > Settings.HalfHeight)
	{
		// 伸ばした形状は途中の高さの形状をすべて含むため、最終の形状の重なりだけを調べる。
		typename T::FSettings Taller = Settings;
		Taller.HalfHeight = NewHalfHeight;
		const auto Contacts = ShapeContacts_Internal<T>(World, Taller, Moved, Settings.Radius, 0, Excluded, Filter);
		for (uint32 Index = 0; Index < Contacts.Count; ++Index)
		{
			if (Contacts.Items[Index].Separation < 0)
			{
				Result.Blocker = Contacts.Items[Index].Collider;
				return Result;
			}
		}
		if (!Contacts.IsComplete())
		{
			// 全件を確かめられない場合は変更しない。
			return Result;
		}
	}
	Result.bResized = true;
	Result.Center = Moved;
	return Result;
}
// 公開関数の共通の入力検査。
template <typename T> void CheckCenter_Internal(const typename T::FVector& Center)
{
	if (!Center.IsValid())
	{
		throw Toolbox::FException("Invalid character center");
	}
}
// キャラクター移動の問い合わせの条件。Sensorは重なりを観測するだけの形状なので、障害物・接地面にしない。
FWorldQueryFilter SolidOnly_Internal(const FWorldQueryFilter& Filter) noexcept
{
	FWorldQueryFilter Solid = Filter;
	Solid.bIncludeSensors = false;
	return Solid;
}
} // namespace

FCharacterRecovery2D ResolveCharacterOverlap(const FPhysicsWorld2D& World, Toolbox::FVector2 Center,
                                             const FCharacterMoveSettings2D& Settings,
                                             Toolbox::TOptional<FBodyId2D> ExcludedBody,
                                             const FWorldQueryFilter& Filter)
{
	(void)CheckSettings_Internal<F2D_Internal>(Settings);
	CheckCenter_Internal<F2D_Internal>(Center);
	FBudget_Internal Budget;
	Budget.Limit = Settings.MaxQueries;
	return RecoverCore_Internal<F2D_Internal>(World, Center, Settings, ExcludedBody, SolidOnly_Internal(Filter),
	                                          Budget);
}
FCharacterRecovery3D ResolveCharacterOverlap(const FPhysicsWorld3D& World, Toolbox::FVector3 Center,
                                             const FCharacterMoveSettings3D& Settings,
                                             Toolbox::TOptional<FBodyId3D> ExcludedBody,
                                             const FWorldQueryFilter& Filter)
{
	(void)CheckSettings_Internal<F3D_Internal>(Settings);
	CheckCenter_Internal<F3D_Internal>(Center);
	FBudget_Internal Budget;
	Budget.Limit = Settings.MaxQueries;
	return RecoverCore_Internal<F3D_Internal>(World, Center, Settings, ExcludedBody, SolidOnly_Internal(Filter),
	                                          Budget);
}
FCharacterMoveResult2D MoveAndSlide(const FPhysicsWorld2D& World, Toolbox::FVector2 Center,
                                    Toolbox::FVector2 Displacement, const FCharacterMoveSettings2D& Settings,
                                    Toolbox::TOptional<FBodyId2D> ExcludedBody, const FWorldQueryFilter& Filter)
{
	const FCheckedSettings_Internal Checked = CheckSettings_Internal<F2D_Internal>(Settings);
	CheckCenter_Internal<F2D_Internal>(Center);
	if (!Displacement.IsValid())
	{
		throw Toolbox::FException("Invalid character displacement");
	}
	FBudget_Internal Budget;
	Budget.Limit = Settings.MaxQueries;
	FMoveMode_Internal Mode;
	Mode.Up = Checked.Up;
	Mode.CosMaxSlope = Checked.CosMaxSlope;
	return MoveCore_Internal<F2D_Internal>(World, Center, F2D_Internal::Load(Displacement), Settings, Mode,
	                                       ExcludedBody, SolidOnly_Internal(Filter), Budget);
}
FCharacterMoveResult3D MoveAndSlide(const FPhysicsWorld3D& World, Toolbox::FVector3 Center,
                                    Toolbox::FVector3 Displacement, const FCharacterMoveSettings3D& Settings,
                                    Toolbox::TOptional<FBodyId3D> ExcludedBody, const FWorldQueryFilter& Filter)
{
	const FCheckedSettings_Internal Checked = CheckSettings_Internal<F3D_Internal>(Settings);
	CheckCenter_Internal<F3D_Internal>(Center);
	if (!Displacement.IsValid())
	{
		throw Toolbox::FException("Invalid character displacement");
	}
	FBudget_Internal Budget;
	Budget.Limit = Settings.MaxQueries;
	FMoveMode_Internal Mode;
	Mode.Up = Checked.Up;
	Mode.CosMaxSlope = Checked.CosMaxSlope;
	return MoveCore_Internal<F3D_Internal>(World, Center, F3D_Internal::Load(Displacement), Settings, Mode,
	                                       ExcludedBody, SolidOnly_Internal(Filter), Budget);
}
FCharacterGround2D ProbeCharacterGround(const FPhysicsWorld2D& World, Toolbox::FVector2 Center,
                                        const FCharacterMoveSettings2D& Settings,
                                        Toolbox::TOptional<FBodyId2D> ExcludedBody, const FWorldQueryFilter& Filter)
{
	const FCheckedSettings_Internal Checked = CheckSettings_Internal<F2D_Internal>(Settings);
	CheckCenter_Internal<F2D_Internal>(Center);
	FBudget_Internal Budget;
	Budget.Limit = Settings.MaxQueries;
	bool bQueryLimit = false;
	return GroundCore_Internal<F2D_Internal>(World, Center, Settings, Checked, ExcludedBody, SolidOnly_Internal(Filter),
	                                         Budget, bQueryLimit);
}
FCharacterGround3D ProbeCharacterGround(const FPhysicsWorld3D& World, Toolbox::FVector3 Center,
                                        const FCharacterMoveSettings3D& Settings,
                                        Toolbox::TOptional<FBodyId3D> ExcludedBody, const FWorldQueryFilter& Filter)
{
	const FCheckedSettings_Internal Checked = CheckSettings_Internal<F3D_Internal>(Settings);
	CheckCenter_Internal<F3D_Internal>(Center);
	FBudget_Internal Budget;
	Budget.Limit = Settings.MaxQueries;
	bool bQueryLimit = false;
	return GroundCore_Internal<F3D_Internal>(World, Center, Settings, Checked, ExcludedBody, SolidOnly_Internal(Filter),
	                                         Budget, bQueryLimit);
}
FCharacterStepResult2D StepCharacter(const FPhysicsWorld2D& World, const FCharacterMoveSettings2D& Settings,
                                     const FCharacterState2D& State, const FCharacterMoveInput2D& Input,
                                     Toolbox::f64 DeltaSeconds, Toolbox::TOptional<FBodyId2D> ExcludedBody,
                                     const FWorldQueryFilter& Filter)
{
	return StepCore_Internal<F2D_Internal>(World, Settings, State, Input, DeltaSeconds, ExcludedBody,
	                                       SolidOnly_Internal(Filter));
}
FCharacterStepResult3D StepCharacter(const FPhysicsWorld3D& World, const FCharacterMoveSettings3D& Settings,
                                     const FCharacterState3D& State, const FCharacterMoveInput3D& Input,
                                     Toolbox::f64 DeltaSeconds, Toolbox::TOptional<FBodyId3D> ExcludedBody,
                                     const FWorldQueryFilter& Filter)
{
	return StepCore_Internal<F3D_Internal>(World, Settings, State, Input, DeltaSeconds, ExcludedBody,
	                                       SolidOnly_Internal(Filter));
}
FCharacterResize2D ResizeCharacterCapsule(const FPhysicsWorld2D& World, Toolbox::FVector2 Center,
                                          const FCharacterMoveSettings2D& Settings, Toolbox::f64 NewHalfHeight,
                                          Toolbox::TOptional<FBodyId2D> ExcludedBody, const FWorldQueryFilter& Filter)
{
	CheckCenter_Internal<F2D_Internal>(Center);
	return ResizeCore_Internal<F2D_Internal>(World, Center, Settings, NewHalfHeight, ExcludedBody,
	                                         SolidOnly_Internal(Filter));
}
FCharacterResize3D ResizeCharacterCapsule(const FPhysicsWorld3D& World, Toolbox::FVector3 Center,
                                          const FCharacterMoveSettings3D& Settings, Toolbox::f64 NewHalfHeight,
                                          Toolbox::TOptional<FBodyId3D> ExcludedBody, const FWorldQueryFilter& Filter)
{
	CheckCenter_Internal<F3D_Internal>(Center);
	return ResizeCore_Internal<F3D_Internal>(World, Center, Settings, NewHalfHeight, ExcludedBody,
	                                         SolidOnly_Internal(Filter));
}
} // namespace Dxf
