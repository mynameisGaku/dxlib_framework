// SPDX-License-Identifier: NOASSERTION
#include "TestCases.h"
#include "Toolbox/ContinuousCollision.h"
using namespace Toolbox;
namespace
{
// 接触時刻はフレーム内の割合なので、秒数ではなく無次元の値で比較する。
bool Near_Internal(f64 A, f64 B, f64 Epsilon = 1e-10)
{
	return Abs(A - B) <= Epsilon;
}
void SpherePassThrough_Internal()
{
	const FSweepHit3D Hit = Sweep(FSphere{{-10, 0, 0}, 1}, {20, 0, 0}, FSphere{{}, 1}, {});
	PHYSICS_REQUIRE(Hit.bHit);
	PHYSICS_REQUIRE(Near_Internal(Hit.Time, 0.4));
	PHYSICS_REQUIRE(Hit.Normal.X == -1 && Hit.Normal.Y == 0 && Hit.Normal.Z == 0);
	PHYSICS_REQUIRE(!Hit.bInitialContact);
}
void RelativeMotion_Internal()
{
	const FSphere A{{-10, 0, 0}, 1};
	const FSphere B{{10, 0, 0}, 1};
	const FSweepHit3D AB = Sweep(A, {20, 0, 0}, B, {-20, 0, 0});
	const FSweepHit3D BA = Sweep(B, {-20, 0, 0}, A, {20, 0, 0});
	PHYSICS_REQUIRE(AB.bHit && BA.bHit);
	PHYSICS_REQUIRE(Near_Internal(AB.Time, 0.45) && AB.Time == BA.Time);
	PHYSICS_REQUIRE(AB.Normal == -BA.Normal);
}
void SweepTangency_Internal()
{
	const FSweepHit3D Hit = Sweep(FSphere{{-5, 2, 0}, 1}, {10, 0, 0}, FSphere{{}, 1}, {});
	PHYSICS_REQUIRE(Hit.bHit && Near_Internal(Hit.Time, 0.5));
	PHYSICS_REQUIRE(Near_Internal(Hit.Normal.Y, 1));
	PHYSICS_REQUIRE(!Sweep(FSphere{{-5, 2.0001f, 0}, 1}, {10, 0, 0}, FSphere{{}, 1}, {}).bHit);
}
void InitialAndEndContact_Internal()
{
	const FSweepHit3D Initial = Sweep(FSphere{{}, 1}, {}, FSphere{{}, 1}, {});
	PHYSICS_REQUIRE(Initial.bHit && Initial.bInitialContact && Initial.Time == 0);
	PHYSICS_REQUIRE(Initial.Normal.IsValid());
	const FSweepHit3D End = Sweep(FSphere{{-4, 0, 0}, 1}, {2, 0, 0}, FSphere{{}, 1}, {});
	PHYSICS_REQUIRE(End.bHit && End.Time == 1);
}
void MotionMisses_Internal()
{
	const FSphere A{{-4, 0, 0}, 1};
	const FSphere B{{}, 1};
	PHYSICS_REQUIRE(!Sweep(A, {}, B, {}).bHit);
	PHYSICS_REQUIRE(!Sweep(A, {-10, 0, 0}, B, {}).bHit);
	PHYSICS_REQUIRE(!Sweep(A, {1, 0, 0}, B, {}).bHit);
	PHYSICS_REQUIRE(!Sweep(A, {100, 0, 0}, B, {100, 0, 0}).bHit);
}
void HighSpeedPrecision_Internal()
{
	const FSweepHit3D Hit = Sweep(FSphere{{-1e30f, 0, 0}, 1}, {2e30f, 0, 0}, FSphere{{}, 1}, {});
	PHYSICS_REQUIRE(Hit.bHit && Near_Internal(Hit.Time, 0.5));
	PHYSICS_REQUIRE(Hit.Normal.X == -1);
	PHYSICS_REQUIRE(!Sweep(FSphere{{-1e30f, 3, 0}, 1}, {2e30f, 0, 0}, FSphere{{}, 1}, {}).bHit);
}
void SphereThinWall_Internal()
{
	const FAABB Wall{{0, -2, -2}, {0, 2, 2}};
	const FSweepHit3D Hit = Sweep(FSphere{{-10, 0.25f, 0}, 0.5f}, {20, 0, 0}, Wall, {});
	PHYSICS_REQUIRE(Hit.bHit && Near_Internal(Hit.Time, 0.475));
	PHYSICS_REQUIRE(Hit.Normal.X == -1);
}
void RoundedCornerNotExpandedBox_Internal()
{
	const FAABB Box{{-1, -1, -1}, {1, 1, 1}};
	// 単純に半径だけ拡張したAABBには当たるが、実際の球は角の外を通る。
	PHYSICS_REQUIRE(!Sweep(FSphere{{-3, 1.8f, 1.8f}, 1}, {6, 0, 0}, Box, {}).bHit);
	const FSweepHit3D Hit = Sweep(FSphere{{-3, 1.6f, 1}, 1}, {6, 0, 0}, Box, {});
	PHYSICS_REQUIRE(Hit.bHit);
	const f64 Offset = f64(1.6f) - 1;
	const f64 Expected = (2 - Sqrt(1 - Offset * Offset)) / 6;
	PHYSICS_REQUIRE(Near_Internal(Hit.Time, Expected));
}
void MovingBoxAndPoint_Internal()
{
	const FAABB Box{{-1, -1, -1}, {1, 1, 1}};
	const FSweepHit3D Hit = Sweep(FSphere{{-5, 0, 0}, 0}, {}, Box, {-10, 0, 0});
	PHYSICS_REQUIRE(Hit.bHit && Near_Internal(Hit.Time, 0.4));
	PHYSICS_REQUIRE(Hit.Normal.IsValid());
}
void CircleAndBox2D_Internal()
{
	const FCircle2D A{{-5, 0}, 1};
	const FCircle2D B{{}, 1};
	const FSweepHit2D Hit = Sweep(A, {10, 0}, B, {});
	PHYSICS_REQUIRE(Hit.bHit && Near_Internal(Hit.Time, 0.3));
	PHYSICS_REQUIRE(Hit.Normal.X == -1 && Hit.Normal.Y == 0);
	const FAABB2D Wall{{0, -2}, {0, 2}};
	const FSweepHit2D WallHit = Sweep(A, {10, 0}, Wall, {});
	PHYSICS_REQUIRE(WallHit.bHit && Near_Internal(WallHit.Time, 0.4));
}
void Static2DDistance_Internal()
{
	const FAABB2D Box{{-1, -1}, {1, 1}};
	PHYSICS_REQUIRE(Intersects(FCircle2D{{-2, -0.875f}, 1}, Box));
	PHYSICS_REQUIRE(Intersects(Box, FCircle2D{{-2, -0.875f}, 1}));
	PHYSICS_REQUIRE(!Intersects(FCircle2D{{2, 2}, 1}, Box));
	PHYSICS_REQUIRE(Intersects(FCircle2D{{2, 0}, 1}, FCircle2D{{}, 1}));
	PHYSICS_REQUIRE(Intersects(FAABB2D{{0, 0}, {1, 1}}, FAABB2D{{1, 1}, {2, 2}}));
	PHYSICS_REQUIRE(!Intersects(FAABB2D{{0, 0}, {1, 1}}, FAABB2D{{1.1f, 1.1f}, {2, 2}}, 0.125f));
}
void PlanarEquivalence_Internal()
{
	for (int32 Y = -40; Y <= 40; ++Y)
	{
		const f32 Offset = static_cast<f32>(Y) * 0.125f;
		const FSweepHit2D A = Sweep(FCircle2D{{-5, Offset}, 1}, {10, 0}, FCircle2D{{}, 1}, {});
		const FSweepHit3D B = Sweep(FSphere{{-5, Offset, 0}, 1}, {10, 0, 0}, FSphere{{}, 1}, {});
		PHYSICS_REQUIRE(A.bHit == B.bHit && A.Time == B.Time);
		PHYSICS_REQUIRE(A.Normal.X == B.Normal.X && A.Normal.Y == B.Normal.Y);
	}
}
void InvalidSweeps_Internal()
{
	bool bThrown = false;
	try
	{
		(void)Sweep(FSphere{}, {TNumericLimits<f32>::QuietNaN(), 0, 0}, FSphere{}, {});
	}
	catch (const FException&)
	{
		bThrown = true;
	}
	PHYSICS_REQUIRE(bThrown);
	bThrown = false;
	try
	{
		(void)Sweep(FCircle2D{}, {}, FAABB2D{{1, 1}, {-1, -1}}, {});
	}
	catch (const FException&)
	{
		bThrown = true;
	}
	PHYSICS_REQUIRE(bThrown);
}
// 固定シードの整数列を使い、STLなしで同じ検証入力を再現する。
f32 RandomGrid_Internal(uint32& Seed, int32 Scale)
{
	Seed = Seed * 1664525u + 1013904223u;
	const int32 Value = static_cast<int32>((Seed >> 8) % static_cast<uint32>(Scale * 2 + 1)) - Scale;
	return static_cast<f32>(Value) * 0.125f;
}
// 製品側の区間分割や二次方程式を使わない、時刻ごとの距離評価。
f64 ReferenceDistance_Internal(const FSphere& A, FVector3 Move, const FAABB& B, f64 Time)
{
	f64 Squared = 0;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const f64 Position = f64(A.Center.Component(Axis)) + f64(Move.Component(Axis)) * Time;
		const f64 Gap = Position - Clamp(Position, f64(B.Min.Component(Axis)), f64(B.Max.Component(Axis)));
		Squared += Gap * Gap;
	}
	return Squared;
}
// 距離の凸性を使った三分探索と二分探索を独立オラクルにする。
void RandomSphereBoxOracle_Internal()
{
	uint32 Seed = 0x9142AB7u;
	for (int32 Trial = 0; Trial < 20000; ++Trial)
	{
		const FAABB Box{{-2, -3, -1}, {2, 3, 1}};
		const FSphere Ball{
		    {RandomGrid_Internal(Seed, 128), RandomGrid_Internal(Seed, 128), RandomGrid_Internal(Seed, 128)},
		    Abs(RandomGrid_Internal(Seed, 24)) + 0.125f};
		const FVector3 Move{RandomGrid_Internal(Seed, 256), RandomGrid_Internal(Seed, 256),
		                    RandomGrid_Internal(Seed, 256)};
		f64 Low = 0;
		f64 High = 1;
		for (int32 Iteration = 0; Iteration < 90; ++Iteration)
		{
			const f64 Left = Low + (High - Low) / 3;
			const f64 Right = High - (High - Low) / 3;
			if (ReferenceDistance_Internal(Ball, Move, Box, Left) < ReferenceDistance_Internal(Ball, Move, Box, Right))
			{
				High = Right;
			}
			else
			{
				Low = Left;
			}
		}
		const f64 MinimumTime = (Low + High) * 0.5;
		const f64 RadiusSquared = f64(Ball.Radius) * Ball.Radius;
		const f64 MinimumDistance =
		    Min(ReferenceDistance_Internal(Ball, Move, Box, MinimumTime),
		        Min(ReferenceDistance_Internal(Ball, Move, Box, 0), ReferenceDistance_Internal(Ball, Move, Box, 1)));
		const bool bExpected = MinimumDistance <= RadiusSquared;
		const FSweepHit3D Hit = Sweep(Ball, Move, Box, {});
		const FSweepHit3D Reverse = Sweep(Box, {}, Ball, Move);
		PHYSICS_REQUIRE(Hit.bHit == bExpected);
		PHYSICS_REQUIRE(Reverse.bHit == Hit.bHit && Reverse.Time == Hit.Time);
		if (!Hit.bHit)
		{
			continue;
		}
		PHYSICS_REQUIRE(Hit.Normal.IsValid() && Reverse.Normal == -Hit.Normal);
		PHYSICS_REQUIRE(Abs(f64(LengthSquared(Hit.Normal)) - 1) < 5e-6);
		Low = 0;
		High = ReferenceDistance_Internal(Ball, Move, Box, 1) < MinimumDistance ? 1 : MinimumTime;
		for (int32 Iteration = 0; Iteration < 64; ++Iteration)
		{
			const f64 Middle = (Low + High) * 0.5;
			if (ReferenceDistance_Internal(Ball, Move, Box, Middle) <= RadiusSquared)
			{
				High = Middle;
			}
			else
			{
				Low = Middle;
			}
		}
		PHYSICS_REQUIRE(Abs(Hit.Time - High) < 1e-7);
	}
}
void SweepInflatedTolerance_Internal()
{
	const FSphere Ball{{-5, 0, 0}, 1};
	const FAABB Box{{-1, -1, -1}, {1, 1, 1}};
	const FSweepHit3D Hit = Sweep(Ball, {10, 0, 0}, Box, {}, 0.5f);
	PHYSICS_REQUIRE(Hit.bHit && Near_Internal(Hit.Time, 0.25));
	const FSweepHit2D A = Sweep(FCircle2D{{-5, 0}, 1}, {10, 0}, FAABB2D{{-1, -1}, {1, 1}}, {}, 0.5f);
	const FSweepHit2D B = Sweep(FAABB2D{{-1, -1}, {1, 1}}, {}, FCircle2D{{-5, 0}, 1}, {10, 0}, 0.5f);
	PHYSICS_REQUIRE(A.bHit && A.Time == B.Time && A.Normal.X == -B.Normal.X);
}
void SmallGeometry_Internal()
{
	const FSweepHit3D Hit = Sweep(FSphere{{-1e-20f, 0, 0}, 1e-21f}, {2e-20f, 0, 0}, FSphere{{}, 1e-21f}, {});
	PHYSICS_REQUIRE(Hit.bHit && Abs(Hit.Time - 0.4) < 1e-7);
	PHYSICS_REQUIRE(Hit.Normal.X == -1);
	PHYSICS_REQUIRE(!Sweep(FSphere{{-1e-20f, 3e-21f, 0}, 1e-21f}, {2e-20f, 0, 0}, FSphere{{}, 1e-21f}, {}).bHit);
}
const PhysicsTest::FCase Cases[] = {
    {"continuous sphere pass-through", SpherePassThrough_Internal},
    {"relative motion and swapped sweep normals", RelativeMotion_Internal},
    {"sweep tangent and near miss", SweepTangency_Internal},
    {"initial overlap and exact endpoint contact", InitialAndEndContact_Internal},
    {"stationary receding late and co-moving misses", MotionMisses_Internal},
    {"high speed keeps finite contact normals", HighSpeedPrecision_Internal},
    {"sphere crosses a zero-thickness wall", SphereThinWall_Internal},
    {"sphere sweep uses rounded corners not expanded AABB", RoundedCornerNotExpandedBox_Internal},
    {"moving box and zero-radius point", MovingBoxAndPoint_Internal},
    {"2D circle and thin wall sweeps", CircleAndBox2D_Internal},
    {"2D static Euclidean distances", Static2DDistance_Internal},
    {"2D and XY-3D sweep equivalence", PlanarEquivalence_Internal},
    {"invalid sweep inputs", InvalidSweeps_Internal},
    {"20000 sphere-box trajectories against a scalar search oracle", RandomSphereBoxOracle_Internal},
    {"sweep tolerance and reversed 2D normal", SweepInflatedTolerance_Internal},
    {"tiny shapes preserve continuous contacts", SmallGeometry_Internal},
};
} // namespace
namespace PhysicsTest
{
const FCase* GetSweepCases(size_t& Count) noexcept
{
	Count = sizeof(Cases) / sizeof(Cases[0]);
	return Cases;
}
} // namespace PhysicsTest
