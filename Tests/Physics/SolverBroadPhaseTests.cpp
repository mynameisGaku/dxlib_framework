// SPDX-License-Identifier: NOASSERTION
// Solver・イベントの組の候補を索引（AABB木）から集める経路（S5）と、総当たりの参照経路の一致。
// 同じ操作列を二つのWorld（索引あり／SetSolverBroadPhaseEnabled_Internal(false)）へ行い、毎Stepの姿勢・速度・角速度・休止・
// イベントのバッチ（種類・段階・ID・法線）をビット単位で比べる。形状（球・箱・カプセル）・Static／Kinematic／Dynamic・
// Sensor・衝突フィルター・休止・分割Step・途中の登録／削除／取り付け／取り外し／瞬間移動・動く床・キャラクターの押し合い・
// 索引を使えない座標を含む。疎な1024個では、接触を調べる組の数が大きく減ることを確かめる。
#include "TestCases.h"
#include "Dxf/CharacterMovement2D.h"
#include "Dxf/CharacterMovement3D.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
	constexpr f64 StepSeconds = 1.0 / 60.0;

// 同じ操作を二つのWorldへ行い、どちらのWorldかを識別子とともに渡す。
enum class EWhichWorld
{
	// 索引（AABB木）を使うWorld。
	Indexed,
	// 総当たりの参照経路のWorld。
	Reference
};

// 識別子は登録先のWorldの識別子を持つため、別々のWorldの完全な値は一致しない。
// 試験は同じ生成順で同じ操作を二つのWorldへ行い、Worldだけを除いた意味部分（スロットと世代）を比べる。
// Worldが違ってもIndexとGenerationが同じなら、試験前提の同じ生成順の同じ登録を指す。
template <typename TId> bool SameBodySlot_Internal(const TId& A, const TId& B)
{
	return A.Index == B.Index && A.Generation == B.Generation;
}

// コライダーの意味部分（取り付け先のBodyと、自体のスロットと世代）を比べる。Worldは比較しない。
template <typename TId> bool SameColliderSlot_Internal(const TId& A, const TId& B)
{
	return SameBodySlot_Internal(A.Body, B.Body) && A.Index == B.Index && A.Generation == B.Generation;
}

// 決まった列を返す線形合同法。
struct FSequence_Internal
{
	uint32 State = 12345u;
	f32 Next(f32 Low, f32 High)
	{
		State = State * 1664525u + 1013904223u;
		return Low + (High - Low) * static_cast<f32>((State >> 8) & 0xFFFFu) / 65535.0f;
	}
	uint32 Pick(uint32 Count)
	{
		State = State * 1664525u + 1013904223u;
		return (State >> 16) % Count;
	}
};

// 2D Worldの型と登録操作。
struct F2D
{
	using FWorld = FPhysicsWorld2D;
	using FBodyId = FBodyId2D;
	using FColliderId = FColliderId2D;
	using FVector = FVector2;
	using FDescription = FColliderDescription2D;
	using FBatch = FWorldEventBatch2D;
	using FSettings = FCharacterMoveSettings2D;
	using FState = FCharacterState2D;
	using FInput = FCharacterMoveInput2D;
	static FVector At(f32 X, f32 Y)
	{
		return {X, Y};
	}
	static FBodyId Body(FWorld& World, FVector Position, EBodyType Type, f32 Angle = 0)
	{
		FBodyDescription2D Description;
		Description.Position = Position;
		Description.Type = Type;
		Description.Angle = Angle;
		return World.CreateBody(Description);
	}
	static FDescription Shape(uint32 Kind, f32 Size)
	{
		FDescription Description;
		if (Kind == 0)
		{
			Description.Shape = FCircle2D{{0, 0}, Size};
		}
		else if (Kind == 1)
		{
			Description.Shape = FOrientedBox2D{{0, 0}, {Size, Size * 0.7f}, 0.3f};
		}
		else
		{
			Description.Shape = FCapsule2D{{-Size, 0}, {Size, 0}, Size * 0.5f};
		}
		return Description;
	}
	static void Teleport(FWorld& World, FBodyId Body, FVector Position)
	{
		World.SetBodyTransform(Body, Position, 0.2f);
	}
	// 二つのWorldの同じ登録に対応するBodyの状態がビット単位で同じか。
	static bool Same(const FWorld& A, FBodyId IdA, const FWorld& B, FBodyId IdB)
	{
		return A.GetPosition(IdA) == B.GetPosition(IdB) && A.GetAngle(IdA) == B.GetAngle(IdB) &&
		       A.GetVelocity(IdA) == B.GetVelocity(IdB) && A.GetAngularVelocity(IdA) == B.GetAngularVelocity(IdB) &&
		       A.IsSleeping(IdA) == B.IsSleeping(IdB);
	}
};

// 3D Worldの型と登録操作。
struct F3D
{
	using FWorld = FPhysicsWorld3D;
	using FBodyId = FBodyId3D;
	using FColliderId = FColliderId3D;
	using FVector = FVector3;
	using FDescription = FColliderDescription3D;
	using FBatch = FWorldEventBatch3D;
	using FSettings = FCharacterMoveSettings3D;
	using FState = FCharacterState3D;
	using FInput = FCharacterMoveInput3D;
	static FVector At(f32 X, f32 Y)
	{
		return {X, Y, X * 0.25f};
	}
	static FBodyId Body(FWorld& World, FVector Position, EBodyType Type, f32 Angle = 0)
	{
		FBodyDescription3D Description;
		Description.Position = Position;
		Description.Type = Type;
		Description.Orientation = {0, 0, static_cast<f32>(Sin(f64(Angle) * 0.5)), static_cast<f32>(Cos(f64(Angle) * 0.5))};
		return World.CreateBody(Description);
	}
	static FDescription Shape(uint32 Kind, f32 Size)
	{
		FDescription Description;
		if (Kind == 0)
		{
			Description.Shape = FSphere{{0, 0, 0}, Size};
		}
		else if (Kind == 1)
		{
			Description.Shape = FOBB{{0, 0, 0}, {Size, Size * 0.7f, Size * 0.8f}};
		}
		else
		{
			Description.Shape = FCapsule{{-Size, 0, 0}, {Size, 0, 0}, Size * 0.5f};
		}
		return Description;
	}
	static void Teleport(FWorld& World, FBodyId Body, FVector Position)
	{
		World.SetBodyTransform(Body, Position, FQuaternion{0, 0, 0.0998334f, 0.9950042f});
	}
	static bool Same(const FWorld& A, FBodyId IdA, const FWorld& B, FBodyId IdB)
	{
		const FQuaternion QA = A.GetOrientation(IdA);
		const FQuaternion QB = B.GetOrientation(IdB);
		return A.GetPosition(IdA) == B.GetPosition(IdB) && QA.X == QB.X && QA.Y == QB.Y && QA.Z == QB.Z &&
		       QA.W == QB.W && A.GetVelocity(IdA) == B.GetVelocity(IdB) &&
		       A.GetAngularVelocity(IdA) == B.GetAngularVelocity(IdB) && A.IsSleeping(IdA) == B.IsSleeping(IdB);
	}
};

// 二つのバッチの全イベントが同じか（法線はビット単位）。
template <typename TBatch> bool SameBatch_Internal(const TBatch& A, const TBatch& B)
{
	if (A.bPublished != B.bPublished || A.bOverflowed != B.bOverflowed || A.PairCount != B.PairCount ||
	    A.RequiredPairs != B.RequiredPairs || A.Events.Size() != B.Events.Size())
	{
		return false;
	}
	for (size_t Index = 0; Index < A.Events.Size(); ++Index)
	{
		const auto& X = A.Events[Index];
		const auto& Y = B.Events[Index];
		if (X.Kind != Y.Kind || X.Phase != Y.Phase || X.EndReason != Y.EndReason ||
		    !SameColliderSlot_Internal(X.ColliderA, Y.ColliderA) || !SameColliderSlot_Internal(X.ColliderB, Y.ColliderB) ||
		    X.Normal.HasValue() != Y.Normal.HasValue() || (X.Normal && !(*X.Normal == *Y.Normal)))
		{
			return false;
		}
	}
	return true;
}

// 索引ありと総当たりの二つのWorldへ、同じ操作を行う。
// Bodyの識別子は登録先のWorldの識別子を持つため、同じ登録に対応する二つの識別子を別々に持つ。
template <typename T> struct TPairWorlds
{
	using FBodyId = typename T::FBodyId;
	// 同じ生成順の同じ登録を指す、二つのWorldのBodyの識別子。
	struct FBodyPair
	{
		// 索引ありのWorldの識別子。
		FBodyId Indexed;
		// 総当たりの参照のWorldの識別子。
		FBodyId Reference;
		// 指定したWorldの識別子を返す。
		FBodyId Id(EWhichWorld Which) const
		{
			return Which == EWhichWorld::Indexed ? Indexed : Reference;
		}
	};
	typename T::FWorld Indexed;
	typename T::FWorld Reference;
	Toolbox::TVector<FBodyPair> Bodies;
	TPairWorlds(uint32 SubSteps = 1) : m_SubSteps(SubSteps)
	{
		Reference.SetSolverBroadPhaseEnabled_Internal(false);
		FWorldEventSettings Events;
		Events.bEnabled = true;
		Events.MaxPairs = 4096;
		Indexed.SetEventSettings(Events);
		Reference.SetEventSettings(Events);
	}
	// 指定したWorldへ操作を行う。登録順を保つため、参照のWorldを先に操作する。
	template <typename F> void Both(F&& Operation)
	{
		Operation(Reference, EWhichWorld::Reference);
		Operation(Indexed, EWhichWorld::Indexed);
	}
	// 同じ登録を両方のWorldへ作り、識別子の組を返す。生成順を保つため参照のWorldを先に作る。
	FBodyPair Add(typename T::FVector Position, EBodyType Type, const typename T::FDescription& Shape, f32 Angle = 0)
	{
		const auto Make = [&](typename T::FWorld& World)
		{
			const auto Body = T::Body(World, Position, Type, Angle);
			World.AttachCollider(Body, Shape);
			return Body;
		};
		FBodyPair Pair;
		Pair.Reference = Make(Reference);
		Pair.Indexed = Make(Indexed);
		Bodies.PushBack(Pair);
		return Pair;
	}
	// 1Stepを進め、全Bodyの状態とイベントのバッチが同じかを返す。
	bool Step()
	{
		Indexed.Step(StepSeconds, m_SubSteps);
		Reference.Step(StepSeconds, m_SubSteps);
		for (const auto& Pair : Bodies)
		{
			if (Indexed.IsAlive(Pair.Indexed) != Reference.IsAlive(Pair.Reference))
			{
				return false;
			}
			if (Indexed.IsAlive(Pair.Indexed) && !T::Same(Indexed, Pair.Indexed, Reference, Pair.Reference))
			{
				return false;
			}
		}
		return SameBatch_Internal(Indexed.GetEventBatch(), Reference.GetEventBatch());
	}

private:
	uint32 m_SubSteps;
};

// 混合した配置：床・斜めの台・動く床・Sensorの領域・衝突フィルターで分けた群・Dynamicの形状の山。途中で登録・削除・
// 取り付け・取り外し・瞬間移動を行う。分割Stepでも同じ。
template <typename T> void MixedScene_Internal(uint32 SubSteps)
{
	TPairWorlds<T> Worlds(SubSteps);
	FSequence_Internal Sequence;
	auto Floor = T::Shape(1, 30);
	Worlds.Add(T::At(0, -30), EBodyType::Static, Floor);
	auto Ramp = T::Shape(1, 3);
	Worlds.Add(T::At(-6, 1), EBodyType::Static, Ramp, 0.4f);
	const auto Platform = Worlds.Add(T::At(6, 0.5f), EBodyType::Kinematic, T::Shape(1, 2));
	Worlds.Both(
	    [&](typename T::FWorld& World, EWhichWorld Which)
	    {
		    World.SetVelocity(Platform.Id(Which), T::At(-0.5f, 0.2f));
	    });
	auto Sensor = T::Shape(0, 2.5f);
	Sensor.Response = EColliderResponse::Sensor;
	Worlds.Add(T::At(0, 3), EBodyType::Static, Sensor);
	for (int32 Index = 0; Index < 48; ++Index)
	{
		auto Shape = T::Shape(Sequence.Pick(3), Sequence.Next(0.2f, 0.6f));
		// 二つの群は互いに衝突しない（接触・Triggerを調べない）。
		if (Index % 5 == 0)
		{
			Shape.Collision.Category = 2u;
			Shape.Collision.Mask = ~4u;
		}
		else if (Index % 7 == 0)
		{
			Shape.Collision.Category = 4u;
			Shape.Collision.Mask = ~2u;
		}
		Shape.Friction = Sequence.Next(0.1f, 0.9f);
		Shape.Restitution = Index % 4 == 0 ? 0.4f : 0;
		Worlds.Add(T::At(Sequence.Next(-8, 8), Sequence.Next(1, 14)), EBodyType::Dynamic, Shape, Sequence.Next(0, 3));
	}
	for (int32 Step = 0; Step < 240; ++Step)
	{
		if (Step == 60)
		{
			// 途中の瞬間移動・取り外し・取り付け・削除・再登録（スロットの再使用）。
			Worlds.Both(
			    [&](typename T::FWorld& World, EWhichWorld Which)
			    {
				    const auto Moved = Worlds.Bodies[10].Id(Which);
				    const auto Removed = Worlds.Bodies[11].Id(Which);
				    const auto Target = Worlds.Bodies[12].Id(Which);
				    T::Teleport(World, Moved, T::At(2, 12));
				    World.DestroyBody(Removed);
				    const auto Snapshot = World.CaptureSnapshot();
				    for (const auto& Collider : Snapshot.Colliders)
				    {
					    if (SameBodySlot_Internal(Collider.Id.Body, Target))
					    {
						    World.DetachCollider(Collider.Id);
					    }
				    }
				    World.AttachCollider(Target, T::Shape(2, 0.4f));
			    });
			Worlds.Add(T::At(-2, 10), EBodyType::Dynamic, T::Shape(0, 0.5f));
		}
		PHYSICS_REQUIRE(Worlds.Step());
	}
	// 動く床を止めてから落ち着かせる。索引の経路でも参照の経路でも、同じ数のBodyが休止する。
	Worlds.Both(
	    [&](typename T::FWorld& World, EWhichWorld Which)
	    {
		    World.SetVelocity(Platform.Id(Which), T::At(0, 0));
	    });
	for (int32 Step = 0; Step < 240; ++Step)
	{
		PHYSICS_REQUIRE(Worlds.Step());
	}
	uint32 Sleeping = 0;
	uint32 ReferenceSleeping = 0;
	for (const auto& Pair : Worlds.Bodies)
	{
		if (Worlds.Indexed.IsAlive(Pair.Indexed) && Worlds.Indexed.IsSleeping(Pair.Indexed))
		{
			++Sleeping;
		}
		if (Worlds.Reference.IsAlive(Pair.Reference) && Worlds.Reference.IsSleeping(Pair.Reference))
		{
			++ReferenceSleeping;
		}
	}
	// 索引の経路と参照の経路が、同じBodyを同じ時点で休止させる（この配置は「落ち着く」配置ではないため
	// 休止する個数そのものは規定しない。休止する数と順番そのものは、専用の安定性の試験が確かめる）。
	PHYSICS_REQUIRE(Sleeping == ReferenceSleeping);
	PHYSICS_REQUIRE(Sleeping > 0);
}

// キャラクター（カプセル、押す・押される設定）が箱を押す配置で、キャラクターの状態と押す要求まで同じ。
template <typename T> void CharacterPush_Internal()
{
	TPairWorlds<T> Worlds;
	Worlds.Add(T::At(0, -1), EBodyType::Static, T::Shape(1, 40));
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Worlds.Add(T::At(2.0f + Index * 1.1f, 1.0f), EBodyType::Dynamic, T::Shape(1, 0.45f));
	}
	typename T::FSettings Settings;
	Settings.Shape = ECharacterShape::Capsule;
	Settings.HalfHeight = 0.4;
	Settings.bPushDynamicBodies = true;
	Settings.bReceiveDynamicPush = true;
	typename T::FState StateA;
	StateA.Center = T::At(0, 1.5f);
	typename T::FState StateB = StateA;
	for (int32 Step = 0; Step < 180; ++Step)
	{
		typename T::FInput Input;
		Input.Move = T::At(Step < 120 ? 1.0f : -1.0f, 0);
		Input.bJump = Step == 90;
		const auto ResultA = StepCharacter(Worlds.Indexed, Settings, StateA, Input, StepSeconds);
		const auto ResultB = StepCharacter(Worlds.Reference, Settings, StateB, Input, StepSeconds);
		StateA = ResultA.State;
		StateB = ResultB.State;
		PHYSICS_REQUIRE(StateA.Center == StateB.Center && StateA.Velocity == StateB.Velocity &&
		                ResultA.Pushes.Count == ResultB.Pushes.Count);
		for (uint32 Index = 0; Index < ResultA.Pushes.Count; ++Index)
		{
			PHYSICS_REQUIRE(ResultA.Pushes.Items[Index].Impulse == ResultB.Pushes.Items[Index].Impulse);
			Worlds.Indexed.ApplyLinearImpulse(ResultA.Pushes.Items[Index].Body, ResultA.Pushes.Items[Index].Impulse);
			Worlds.Reference.ApplyLinearImpulse(ResultB.Pushes.Items[Index].Body, ResultB.Pushes.Items[Index].Impulse);
		}
		PHYSICS_REQUIRE(Worlds.Step());
	}
}

// 索引を使えない座標（2^100を超える）のColliderがあれば、索引の経路も総当たりへ戻り、結果は同じ。
template <typename T> void FarFallback_Internal()
{
	TPairWorlds<T> Worlds;
	Worlds.Add(T::At(0, -1), EBodyType::Static, T::Shape(1, 10));
	Worlds.Add(T::At(0, 2), EBodyType::Dynamic, T::Shape(2, 0.5f));
	Worlds.Add(T::At(3e30f, 0), EBodyType::Static, T::Shape(0, 1));
	for (int32 Step = 0; Step < 60; ++Step)
	{
		PHYSICS_REQUIRE(Worlds.Step());
	}
}

// 疎な1024個のDynamicの球（互いに離れて浮いている）: 接触を調べる組の数が、総当たりの約52万から大きく減る。結果は同じ。
template <typename T> void SparseReduction_Internal()
{
	TPairWorlds<T> Worlds;
	Worlds.Both(
	    [](typename T::FWorld& World, EWhichWorld)
	    {
		    World.SetGravity(T::At(0, 0) * 0.0f);
	    });
	for (int32 Index = 0; Index < 1024; ++Index)
	{
		Worlds.Add(T::At(static_cast<f32>(Index % 32) * 3, static_cast<f32>(Index / 32) * 3), EBodyType::Dynamic,
		           T::Shape(0, 0.5f));
	}
	PHYSICS_REQUIRE(Worlds.Step());
	const uint64 Indexed = Worlds.Indexed.GetExecutionDiagnostics().CandidatePairCount;
	const uint64 Reference = Worlds.Reference.GetExecutionDiagnostics().CandidatePairCount;
	PHYSICS_REQUIRE(Reference == 1024u * 1023u / 2u);
	PHYSICS_REQUIRE(Indexed * 100 < Reference);
	PHYSICS_REQUIRE(Worlds.Indexed.GetExecutionDiagnostics().ManifoldCount == 0 &&
	                Worlds.Reference.GetExecutionDiagnostics().ManifoldCount == 0);
}

void Mixed2D_Internal()
{
	MixedScene_Internal<F2D>(1);
	MixedScene_Internal<F2D>(3);
}
void Mixed3D_Internal()
{
	MixedScene_Internal<F3D>(1);
	MixedScene_Internal<F3D>(3);
}

const PhysicsTest::FCase Cases_Internal[] = {
    {"2D solver broad phase matches brute force in a mixed scene", &Mixed2D_Internal},
    {"3D solver broad phase matches brute force in a mixed scene", &Mixed3D_Internal},
    {"2D solver broad phase matches brute force with a pushing character", &CharacterPush_Internal<F2D>},
    {"3D solver broad phase matches brute force with a pushing character", &CharacterPush_Internal<F3D>},
    {"2D solver broad phase falls back for far coordinates", &FarFallback_Internal<F2D>},
    {"3D solver broad phase falls back for far coordinates", &FarFallback_Internal<F3D>},
    {"2D solver broad phase reduces sparse narrow-phase pairs", &SparseReduction_Internal<F2D>},
    {"3D solver broad phase reduces sparse narrow-phase pairs", &SparseReduction_Internal<F3D>}};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetSolverBroadPhaseCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
