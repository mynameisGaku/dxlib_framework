// SPDX-License-Identifier: NOASSERTION
#include "LegacyScenarios.h"
namespace Dxf::Benchmark
{
namespace
{
// 前回の2Dの条件。
class FLegacy2D final : public IScenario
{
public:
	FLegacy2D(Toolbox::int32 Obstacles, Toolbox::int32 Characters, bool bStaticLevel) : m_Characters(Characters)
	{
		const Toolbox::int32 PerLane = (Obstacles + Characters - 1) / Characters;
		const Toolbox::f32 Half = static_cast<Toolbox::f32>(PerLane) * 1.5f + 3.0f;
		// 前回の条件は既定のBody（Dynamic）で床と障害物を作っていた（重力で落ち続ける）。bStaticLevelならStaticにする。
		FBodyDescription2D LevelDescription;
		LevelDescription.Type = bStaticLevel ? EBodyType::Static : EBodyType::Dynamic;
		const FBodyId2D Level = m_World.CreateBody(LevelDescription);
		FColliderDescription2D Box;
		for (Toolbox::int32 Lane = 0; Lane < Characters; ++Lane)
		{
			const Toolbox::f32 Floor = static_cast<Toolbox::f32>(Lane) * 10.0f;
			Box.Shape = Toolbox::FOrientedBox2D{{0, Floor - 1}, {Half, 1}, 0};
			m_World.AttachCollider(Level, Box);
		}
		for (Toolbox::int32 Index = 0; Index < Obstacles; ++Index)
		{
			const Toolbox::int32 Lane = Index % Characters;
			const Toolbox::int32 Slot = Index / Characters;
			const Toolbox::f32 Floor = static_cast<Toolbox::f32>(Lane) * 10.0f;
			const Toolbox::f32 X = -Half + 3.0f + static_cast<Toolbox::f32>(Slot) * 3.0f + 1.5f;
			// 低い段差・高い壁・30度の坂を順に置く。
			switch (Slot % 3)
			{
			case 0:
				Box.Shape = Toolbox::FOrientedBox2D{{X, Floor + 0.1f}, {0.5f, 0.1f}, 0};
				break;
			case 1:
				Box.Shape = Toolbox::FOrientedBox2D{{X, Floor + 0.6f}, {0.5f, 0.6f}, 0};
				break;
			default:
				Box.Shape = Toolbox::FOrientedBox2D{{X, Floor}, {1.0f, 0.3f}, 0.5236f};
				break;
			}
			m_World.AttachCollider(Level, Box);
		}
		for (Toolbox::int32 Index = 0; Index < Characters; ++Index)
		{
			m_States[Index].Center = {-Half + 1.5f, static_cast<Toolbox::f32>(Index) * 10.0f + 0.52f};
			FBodyDescription2D Body;
			Body.Type = EBodyType::Kinematic;
			Body.Position = m_States[Index].Center;
			m_Bodies[Index] = m_World.CreateBody(Body);
			FColliderDescription2D Collider;
			Collider.Shape = Toolbox::FCircle2D{{}, m_Settings.Radius};
			m_World.AttachCollider(m_Bodies[Index], Collider);
		}
	}
	void Step(Toolbox::int64 StepIndex, FStepTotals& Totals) override
	{
		const Toolbox::uint64 Begin = NowNanoseconds();
		for (Toolbox::int32 Index = 0; Index < m_Characters; ++Index)
		{
			FCharacterMoveInput2D Input;
			Input.Move = {Toolbox::Cos(LegacyAngle(Index, StepIndex)) >= 0 ? 1.0f : -1.0f, 0};
			Input.bJump = (StepIndex + Index) % 90 == 0;
			const FBodyId2D Body = m_Bodies[Index];
			StepCharacterTimed(
			    m_World, m_Settings, m_States[Index], Input, Body,
			    [&](Toolbox::FVector2 Center)
			    {
				    m_World.SetBodyTransform(Body, Center, 0);
			    },
			    Totals);
		}
		Totals.MoveNs += NowNanoseconds() - Begin;
		StepWorldTimed(m_World, Totals);
	}
	Toolbox::int32 GetCharacterCount() const override
	{
		return m_Characters;
	}
	void SetReference(bool bReference) override
	{
		SetReferencePath(m_World, bReference);
	}
	void BeginCounts(bool bEnabled) override
	{
		BeginQueryCounts(m_World, bEnabled);
	}
	FQueryCounts ReadCounts() const override
	{
		return ReadQueryCounts(m_World);
	}
	FQueryCostTable MeasureQueryCosts(Toolbox::int32 Calls) override
	{
		Toolbox::FVector2 Centers[64];
		for (Toolbox::int32 Index = 0; Index < m_Characters; ++Index)
		{
			Centers[Index] = m_States[Index].Center;
		}
		return Benchmark::MeasureQueryCosts(m_World, Centers, m_Bodies, m_Characters, Calls);
	}

private:
	// World。
	FPhysicsWorld2D m_World;
	// 移動の設定（既定値）。
	FCharacterMoveSettings2D m_Settings;
	// 各個体の状態。
	FCharacterState2D m_States[64];
	// 各個体のBody。
	FBodyId2D m_Bodies[64];
	// 個体の数。
	Toolbox::int32 m_Characters = 0;
};

// 前回の3Dの条件。
class FLegacy3D final : public IScenario
{
public:
	FLegacy3D(Toolbox::int32 Obstacles, Toolbox::int32 Characters, bool bStaticLevel) : m_Characters(Characters)
	{
		Toolbox::int32 Side = 1;
		while (Side * Side < Obstacles || Side * Side < Characters)
		{
			++Side;
		}
		const Toolbox::f32 Half = static_cast<Toolbox::f32>(Side) * 1.5f;
		// 前回の条件は既定のBody（Dynamic）で床と障害物を作っていた（重力で落ち続ける）。bStaticLevelならStaticにする。
		FBodyDescription3D LevelDescription;
		LevelDescription.Type = bStaticLevel ? EBodyType::Static : EBodyType::Dynamic;
		const FBodyId3D Level = m_World.CreateBody(LevelDescription);
		FColliderDescription3D Box;
		Box.Shape = Toolbox::FOBB{{0, -1, 0}, {Half + 2, 1, Half + 2}};
		m_World.AttachCollider(Level, Box);
		for (Toolbox::int32 Index = 0; Index < Obstacles; ++Index)
		{
			const Toolbox::f32 X = -Half + static_cast<Toolbox::f32>(Index % Side) * 3.0f + 1.5f;
			const Toolbox::f32 Z = -Half + static_cast<Toolbox::f32>(Index / Side) * 3.0f + 1.5f;
			switch (Index % 3)
			{
			case 0:
				Box.Shape = Toolbox::FOBB{{X, 0.1f, Z}, {0.5f, 0.1f, 0.5f}};
				break;
			case 1:
				Box.Shape = Toolbox::FOBB{{X, 0.6f, Z}, {0.5f, 0.6f, 0.5f}};
				break;
			default:
			{
				// 30度の坂（Z軸回りに回したOBB）。
				Toolbox::FOBB Ramp{{X, 0, Z}, {1.0f, 0.3f, 0.5f}};
				const Toolbox::f32 C = static_cast<Toolbox::f32>(Toolbox::Cos(0.5236));
				const Toolbox::f32 S = static_cast<Toolbox::f32>(Toolbox::Sin(0.5236));
				Ramp.Axes[0] = {C, S, 0};
				Ramp.Axes[1] = {-S, C, 0};
				Box.Shape = Ramp;
			}
			break;
			}
			m_World.AttachCollider(Level, Box);
		}
		for (Toolbox::int32 Index = 0; Index < Characters; ++Index)
		{
			m_States[Index].Center = {-Half + static_cast<Toolbox::f32>(Index % Side) * 3.0f, 0.52f,
			                          -Half + static_cast<Toolbox::f32>(Index / Side) * 3.0f};
			FBodyDescription3D Body;
			Body.Type = EBodyType::Kinematic;
			Body.Position = m_States[Index].Center;
			m_Bodies[Index] = m_World.CreateBody(Body);
			FColliderDescription3D Collider;
			Collider.Shape = Toolbox::FSphere{{}, m_Settings.Radius};
			m_World.AttachCollider(m_Bodies[Index], Collider);
		}
	}
	void Step(Toolbox::int64 StepIndex, FStepTotals& Totals) override
	{
		const Toolbox::uint64 Begin = NowNanoseconds();
		for (Toolbox::int32 Index = 0; Index < m_Characters; ++Index)
		{
			const Toolbox::f32 Angle = LegacyAngle(Index, StepIndex);
			FCharacterMoveInput3D Input;
			Input.Move = {static_cast<Toolbox::f32>(Toolbox::Cos(Angle)), 0,
			              static_cast<Toolbox::f32>(Toolbox::Sin(Angle))};
			Input.bJump = (StepIndex + Index) % 90 == 0;
			const FBodyId3D Body = m_Bodies[Index];
			StepCharacterTimed(
			    m_World, m_Settings, m_States[Index], Input, Body,
			    [&](Toolbox::FVector3 Center)
			    {
				    m_World.SetBodyTransform(Body, Center, Toolbox::FQuaternion{});
			    },
			    Totals);
		}
		Totals.MoveNs += NowNanoseconds() - Begin;
		StepWorldTimed(m_World, Totals);
	}
	Toolbox::int32 GetCharacterCount() const override
	{
		return m_Characters;
	}
	void SetReference(bool bReference) override
	{
		SetReferencePath(m_World, bReference);
	}
	void BeginCounts(bool bEnabled) override
	{
		BeginQueryCounts(m_World, bEnabled);
	}
	FQueryCounts ReadCounts() const override
	{
		return ReadQueryCounts(m_World);
	}
	FQueryCostTable MeasureQueryCosts(Toolbox::int32 Calls) override
	{
		Toolbox::FVector3 Centers[64];
		for (Toolbox::int32 Index = 0; Index < m_Characters; ++Index)
		{
			Centers[Index] = m_States[Index].Center;
		}
		return Benchmark::MeasureQueryCosts(m_World, Centers, m_Bodies, m_Characters, Calls);
	}

private:
	// World。
	FPhysicsWorld3D m_World;
	// 移動の設定（既定値）。
	FCharacterMoveSettings3D m_Settings;
	// 各個体の状態。
	FCharacterState3D m_States[64];
	// 各個体のBody。
	FBodyId3D m_Bodies[64];
	// 個体の数。
	Toolbox::int32 m_Characters = 0;
};
} // namespace

Toolbox::f32 LegacyAngle(Toolbox::int32 Character, Toolbox::int64 StepIndex)
{
	return static_cast<Toolbox::f32>(Character) * 2.39996f + static_cast<Toolbox::f32>(StepIndex) * 0.01f;
}
Toolbox::TUniquePtr<IScenario> MakeLegacy2D(Toolbox::int32 Obstacles, Toolbox::int32 Characters)
{
	return Toolbox::MakeUnique<FLegacy2D>(Obstacles, Characters, false);
}
Toolbox::TUniquePtr<IScenario> MakeLegacyStatic2D(Toolbox::int32 Obstacles, Toolbox::int32 Characters)
{
	return Toolbox::MakeUnique<FLegacy2D>(Obstacles, Characters, true);
}
Toolbox::TUniquePtr<IScenario> MakeLegacy3D(Toolbox::int32 Obstacles, Toolbox::int32 Characters)
{
	return Toolbox::MakeUnique<FLegacy3D>(Obstacles, Characters, false);
}
Toolbox::TUniquePtr<IScenario> MakeLegacyStatic3D(Toolbox::int32 Obstacles, Toolbox::int32 Characters)
{
	return Toolbox::MakeUnique<FLegacy3D>(Obstacles, Characters, true);
}
} // namespace Dxf::Benchmark
