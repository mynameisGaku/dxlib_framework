// SPDX-License-Identifier: NOASSERTION
#include "ScalingScenarios.h"
#include "Report.h"
#include <stdio.h>
namespace Dxf::Benchmark
{
namespace
{
// 行の間隔（2Dは床の高さ、3Dは奥行き）。
constexpr Toolbox::f32 RowSpacing2D = 10.0f;
// 3Dの行の間隔。
constexpr Toolbox::f32 RowSpacing3D = 3.0f;
// 遠方の障害物を置き始めるX。
constexpr Toolbox::f32 FarStart = 10000.0f;
// 動かすBodyを置き始めるX（キャラクターの行から離す）。
constexpr Toolbox::f32 MoverStart = 500.0f;

// 2Dの型と配置。行ごとに別の床（高さRowSpacing2Dずつ）。
struct FField2D
{
	using FWorld = FPhysicsWorld2D;
	using FSettings = FCharacterMoveSettings2D;
	using FState = FCharacterState2D;
	using FInput = FCharacterMoveInput2D;
	using FBody = FBodyId2D;
	using FCollider = FColliderId2D;
	using FVector = Toolbox::FVector2;
	using FBodyDescription = FBodyDescription2D;
	using FColliderDescription = FColliderDescription2D;
	static constexpr const char* Name = "2D";
	// 行Rowの、X・高さHeightの点。
	static FVector At(Toolbox::f32 X, Toolbox::f32 Height, Toolbox::int32 Row)
	{
		return {X, Height + RowSpacing2D * static_cast<Toolbox::f32>(Row)};
	}
	// 行Rowの、中心(X, Height)・半幅・Z回りの角度の箱。
	static FColliderDescription Box(Toolbox::f32 X, Toolbox::f32 Height, Toolbox::int32 Row, Toolbox::f32 HalfX,
	                                Toolbox::f32 HalfY, Toolbox::f32 Angle)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FOrientedBox2D{At(X, Height, Row), {HalfX, HalfY}, Angle};
		return Description;
	}
	// 床。bSingleなら全行を覆う一枚（2Dでは各行に同じ幅の床を置く）。
	static void AddFloors(FWorld& World, FBody Level, Toolbox::int32 Rows, Toolbox::f32 CenterX, Toolbox::f32 HalfX)
	{
		for (Toolbox::int32 Row = 0; Row < Rows; ++Row)
		{
			World.AttachCollider(Level, Box(CenterX, -1, Row, HalfX, 1, 0));
		}
	}
	// 半径Radiusの円。
	static FColliderDescription Ball(Toolbox::f32 Radius)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FCircle2D{{}, Radius};
		return Description;
	}
	// +X方向の入力。
	static FInput Forward(bool bJump)
	{
		FInput Input;
		Input.Move = {1, 0};
		Input.bJump = bJump;
		return Input;
	}
	// Bodyを置く。
	static void Place(FWorld& World, FBody Body, FVector Center)
	{
		World.SetBodyTransform(Body, Center, 0);
	}
	// X成分。
	static Toolbox::f32 X(FVector Value)
	{
		return Value.X;
	}
};
// 3Dの型と配置。行はZ方向にRowSpacing3Dずつ並べ、床は行ごとの帯（つなげると一枚の床）。
struct FField3D
{
	using FWorld = FPhysicsWorld3D;
	using FSettings = FCharacterMoveSettings3D;
	using FState = FCharacterState3D;
	using FInput = FCharacterMoveInput3D;
	using FBody = FBodyId3D;
	using FCollider = FColliderId3D;
	using FVector = Toolbox::FVector3;
	using FBodyDescription = FBodyDescription3D;
	using FColliderDescription = FColliderDescription3D;
	static constexpr const char* Name = "3D";
	static FVector At(Toolbox::f32 X, Toolbox::f32 Height, Toolbox::int32 Row)
	{
		return {X, Height, RowSpacing3D * static_cast<Toolbox::f32>(Row)};
	}
	static FColliderDescription Box(Toolbox::f32 X, Toolbox::f32 Height, Toolbox::int32 Row, Toolbox::f32 HalfX,
	                                Toolbox::f32 HalfY, Toolbox::f32 Angle)
	{
		Toolbox::FOBB Shape{At(X, Height, Row), {HalfX, HalfY, 1.0f}};
		const Toolbox::f32 C = static_cast<Toolbox::f32>(Toolbox::Cos(Toolbox::f64(Angle)));
		const Toolbox::f32 S = static_cast<Toolbox::f32>(Toolbox::Sin(Toolbox::f64(Angle)));
		Shape.Axes[0] = {C, S, 0};
		Shape.Axes[1] = {-S, C, 0};
		FColliderDescription Description;
		Description.Shape = Shape;
		return Description;
	}
	static void AddFloors(FWorld& World, FBody Level, Toolbox::int32 Rows, Toolbox::f32 CenterX, Toolbox::f32 HalfX)
	{
		// 全行を覆う一枚の床（Z方向も同じ半幅）。
		FColliderDescription Description;
		const Toolbox::f32 CenterZ = RowSpacing3D * static_cast<Toolbox::f32>(Rows - 1) * 0.5f;
		const Toolbox::f32 HalfZ = Toolbox::Max(HalfX, RowSpacing3D * static_cast<Toolbox::f32>(Rows));
		Description.Shape = Toolbox::FOBB{{CenterX, -1, CenterZ}, {HalfX, 1, HalfZ}};
		World.AttachCollider(Level, Description);
	}
	static FColliderDescription Ball(Toolbox::f32 Radius)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FSphere{{}, Radius};
		return Description;
	}
	static FInput Forward(bool bJump)
	{
		FInput Input;
		Input.Move = {1, 0, 0};
		Input.bJump = bJump;
		return Input;
	}
	static void Place(FWorld& World, FBody Body, FVector Center)
	{
		World.SetBodyTransform(Body, Center, Toolbox::FQuaternion{});
	}
	static Toolbox::f32 X(FVector Value)
	{
		return Value.X;
	}
};

// 系列の構成。
struct FFieldConfig
{
	// 近くの障害物（行ごとに低い段差と坂を交互に置く）の数。
	Toolbox::int32 NearObstacles = 128;
	// 遠方の障害物の数。
	Toolbox::int32 FarObstacles = 0;
	// 毎回動かすKinematicのBodyの数。
	Toolbox::int32 Movers = 0;
	// キャラクターの周りに重ねる箱の数。
	Toolbox::int32 DenseObstacles = 0;
	// 計測前のColliderの削除と追加の回数。
	Toolbox::int32 ChurnCycles = 0;
	// 床の半幅（0なら行の長さに合わせる）。
	Toolbox::f32 FloorHalf = 0;
};
// 場面を作る関数へ渡す現在の構成（作る直前に系列の実行側が設定する）。
FFieldConfig CurrentConfig;

// 行ごとに+X方向へ歩き続ける場面。
template <typename T> class TFieldScenario final : public IScenario
{
public:
	TFieldScenario(const FFieldConfig& Config, Toolbox::int32 Characters) : m_Characters(Characters)
	{
		const Toolbox::int32 PerRow = Toolbox::Max(1, Config.NearObstacles / Characters);
		m_RowLength = static_cast<Toolbox::f32>(PerRow) * 3.0f + 3.0f;
		// 床と障害物は動かないBody（Static）。
		typename T::FBodyDescription LevelDescription;
		LevelDescription.Type = EBodyType::Static;
		const typename T::FBody Level = m_World.CreateBody(LevelDescription);
		const Toolbox::f32 FloorHalf = Config.FloorHalf > 0 ? Config.FloorHalf : m_RowLength * 0.5f + 4.0f;
		T::AddFloors(m_World, Level, Characters, m_RowLength * 0.5f, FloorHalf);
		// 近くの障害物: 低い段差と30度の坂を交互に置く（壁はないので止まらない）。
		for (Toolbox::int32 Index = 0; Index < Config.NearObstacles; ++Index)
		{
			const Toolbox::int32 Row = Index % Characters;
			const Toolbox::int32 Slot = Index / Characters;
			const Toolbox::f32 X = 3.0f + static_cast<Toolbox::f32>(Slot) * 3.0f;
			if (Slot % 2 == 0)
			{
				m_World.AttachCollider(Level, T::Box(X, 0.1f, Row, 0.5f, 0.1f, 0));
			}
			else
			{
				m_World.AttachCollider(Level, T::Box(X, -0.2f, Row, 1.0f, 0.3f, 0.5236f));
			}
		}
		// 遠方の障害物。
		for (Toolbox::int32 Index = 0; Index < Config.FarObstacles; ++Index)
		{
			const Toolbox::int32 Row = Index % Characters;
			const Toolbox::f32 X = FarStart + static_cast<Toolbox::f32>(Index / Characters) * 3.0f;
			m_World.AttachCollider(Level, T::Box(X, 0.6f, Row, 0.5f, 0.6f, 0));
		}
		// キャラクターの周りに重なり合う箱（中心を2m四方へ散らす）。
		for (Toolbox::int32 Index = 0; Index < Config.DenseObstacles; ++Index)
		{
			const Toolbox::int32 Row = Index % Characters;
			const Toolbox::f32 X = 0.5f + static_cast<Toolbox::f32>((Index * 7) % 20) * 0.1f;
			const Toolbox::f32 Height = -0.9f + static_cast<Toolbox::f32>((Index * 3) % 10) * 0.02f;
			m_World.AttachCollider(Level, T::Box(X, Height, Row, 0.3f, 0.3f, static_cast<Toolbox::f32>(Index) * 0.1f));
		}
		// 登録の入替: 近くの障害物と同じ種類の箱を外しては別の位置へ付け直す（最後の配置の数は変えない）。
		if (Config.ChurnCycles > 0)
		{
			const typename T::FBody Churn = m_World.CreateBody(LevelDescription);
			Toolbox::TVector<typename T::FCollider> Alive;
			for (Toolbox::int32 Index = 0; Index < 256; ++Index)
			{
				Alive.PushBack(m_World.AttachCollider(
				    Churn, T::Box(MoverStart + static_cast<Toolbox::f32>(Index), 5, 0, 0.4f, 0.4f, 0)));
			}
			for (Toolbox::int32 Cycle = 0; Cycle < Config.ChurnCycles; ++Cycle)
			{
				const Toolbox::size_t Slot = static_cast<Toolbox::size_t>((Cycle * 37) % 256);
				m_World.DetachCollider(Alive[Slot]);
				const Toolbox::f32 X = MoverStart + static_cast<Toolbox::f32>((Cycle * 53) % 400);
				Alive[Slot] = m_World.AttachCollider(Churn, T::Box(X, 5, Cycle % Characters, 0.4f, 0.4f, 0));
			}
		}
		// 毎回動かすKinematicのBody。
		for (Toolbox::int32 Index = 0; Index < Config.Movers; ++Index)
		{
			typename T::FBodyDescription Body;
			Body.Type = EBodyType::Kinematic;
			Body.Position = T::At(MoverStart + static_cast<Toolbox::f32>(Index % 64) * 3.0f, 1.0f, Index / 64);
			const typename T::FBody Mover = m_World.CreateBody(Body);
			m_World.AttachCollider(Mover, T::Box(0, 0, 0, 0.5f, 0.5f, 0));
			m_Movers.PushBack(Mover);
			m_MoverBases.PushBack(Body.Position);
		}
		for (Toolbox::int32 Index = 0; Index < Characters; ++Index)
		{
			m_States[Index].Center = T::At(0, 0.52f, Index);
			typename T::FBodyDescription Body;
			Body.Type = EBodyType::Kinematic;
			Body.Position = m_States[Index].Center;
			m_Bodies[Index] = m_World.CreateBody(Body);
			m_World.AttachCollider(m_Bodies[Index], T::Ball(m_Settings.Radius));
		}
	}
	void Step(Toolbox::int64 StepIndex, FStepTotals& Totals) override
	{
		const Toolbox::uint64 Begin = NowNanoseconds();
		for (Toolbox::int32 Index = 0; Index < m_Characters; ++Index)
		{
			const typename T::FBody Body = m_Bodies[Index];
			StepCharacterTimed(
			    m_World, m_Settings, m_States[Index], T::Forward((StepIndex + Index) % 120 == 0), Body,
			    [&](typename T::FVector Center)
			    {
				    T::Place(m_World, Body, Center);
			    },
			    Totals);
			// 行の端で始点へ戻す（位置が変わり続ける経路）。
			if (T::X(m_States[Index].Center) > m_RowLength)
			{
				m_States[Index] = {};
				m_States[Index].Center = T::At(0, 0.52f, Index);
				T::Place(m_World, Body, m_States[Index].Center);
			}
		}
		Totals.MoveNs += NowNanoseconds() - Begin;
		// キャラクター以外のBodyを動かす（1回に最大0.2ずつ、余裕を持たせた境界から度々出る）。
		const Toolbox::uint64 Allocations = TotalAllocations();
		const Toolbox::uint64 Moving = NowNanoseconds();
		const Toolbox::f32 Offset =
		    static_cast<Toolbox::f32>(Toolbox::Sin(static_cast<Toolbox::f64>(StepIndex) * 0.1)) * 2.0f;
		for (Toolbox::size_t Index = 0; Index < m_Movers.Size(); ++Index)
		{
			typename T::FVector Position = m_MoverBases[Index];
			Position.X += Offset;
			T::Place(m_World, m_Movers[Index], Position);
		}
		Totals.OtherNs += NowNanoseconds() - Moving;
		Totals.OtherAllocations += TotalAllocations() - Allocations;
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

private:
	// World。
	typename T::FWorld m_World;
	// 移動の設定（既定値）。
	typename T::FSettings m_Settings;
	// 各個体の状態。
	typename T::FState m_States[64];
	// 各個体のBody。
	typename T::FBody m_Bodies[64];
	// 毎回動かすBody。
	Toolbox::TVector<typename T::FBody> m_Movers;
	// 動かすBodyの基準の位置。
	Toolbox::TVector<typename T::FVector> m_MoverBases;
	// 個体の数。
	Toolbox::int32 m_Characters = 0;
	// 行の長さ（端で始点へ戻す）。
	Toolbox::f32 m_RowLength = 0;
};

// 現在の構成で場面を作る。
template <typename T> Toolbox::TUniquePtr<IScenario> MakeField_Internal(Toolbox::int32, Toolbox::int32 Characters)
{
	return Toolbox::MakeUnique<TFieldScenario<T>>(CurrentConfig, Characters);
}
// 一つの系列を、変数の値ごとに両方の経路で計って出力する。
template <typename T>
void RunSeries_Internal(const char* Series, FFieldConfig Base, Toolbox::int32 FFieldConfig::* Variable,
                        const Toolbox::int32* Values, Toolbox::int32 ValueCount, Toolbox::int32 Warmup,
                        Toolbox::int32 Measured)
{
	char Label[64];
	for (Toolbox::int32 Index = 0; Index < ValueCount; ++Index)
	{
		CurrentConfig = Base;
		CurrentConfig.*Variable = Values[Index];
		for (Toolbox::int32 Path = 0; Path < (HasQueryIndex() ? 2 : 1); ++Path)
		{
			const bool bReference = Path == 1;
			const FRunResult Result = RunScenario(&MakeField_Internal<T>, 0, 8, Warmup, Measured, bReference);
			Label[0] = 0;
			snprintf(Label, sizeof(Label), "%s %s", T::Name, Series);
			PrintRunRow(Label, Values[Index], 8, bReference, Result);
		}
	}
}
// 床の大きさの系列（変数が浮動小数点のため別に書く）。
template <typename T> void RunFloorSeries_Internal(Toolbox::int32 Warmup, Toolbox::int32 Measured)
{
	const Toolbox::int32 Halves[] = {0, 5000};
	char Label[64];
	for (const Toolbox::int32 Half : Halves)
	{
		CurrentConfig = {};
		CurrentConfig.NearObstacles = 512;
		CurrentConfig.FloorHalf = static_cast<Toolbox::f32>(Half);
		for (Toolbox::int32 Path = 0; Path < (HasQueryIndex() ? 2 : 1); ++Path)
		{
			const bool bReference = Path == 1;
			const FRunResult Result = RunScenario(&MakeField_Internal<T>, 0, 8, Warmup, Measured, bReference);
			snprintf(Label, sizeof(Label), "%s floor", T::Name);
			PrintRunRow(Label, Half, 8, bReference, Result);
		}
	}
}
// 一つの次元のすべての系列。
template <typename T> void RunDimension_Internal(Toolbox::int32 Warmup, Toolbox::int32 Measured)
{
	FFieldConfig Base;
	const Toolbox::int32 Near[] = {128, 512};
	RunSeries_Internal<T>("moving", Base, &FFieldConfig::NearObstacles, Near, 2, Warmup, Measured);
	const Toolbox::int32 Far[] = {0, 1024, 4096};
	RunSeries_Internal<T>("local(far)", Base, &FFieldConfig::FarObstacles, Far, 3, Warmup, Measured);
	const Toolbox::int32 Movers[] = {0, 256, 1024};
	RunSeries_Internal<T>("dynamic(movers)", Base, &FFieldConfig::Movers, Movers, 3, Warmup, Measured);
	const Toolbox::int32 Dense[] = {64, 256};
	RunSeries_Internal<T>("dense", Base, &FFieldConfig::DenseObstacles, Dense, 2, Warmup, Measured);
	const Toolbox::int32 Churn[] = {0, 20000};
	RunSeries_Internal<T>("churn", Base, &FFieldConfig::ChurnCycles, Churn, 2, Warmup, Measured);
	RunFloorSeries_Internal<T>(Warmup, Measured);
}
} // namespace

void RunScalingSeries(Toolbox::int32 Warmup, Toolbox::int32 Measured)
{
	PrintRunHeader("Scaling series (8 characters walking +X over low steps and 30-degree ramps)");
	RunDimension_Internal<FField2D>(Warmup, Measured);
	RunDimension_Internal<FField3D>(Warmup, Measured);
}
} // namespace Dxf::Benchmark
