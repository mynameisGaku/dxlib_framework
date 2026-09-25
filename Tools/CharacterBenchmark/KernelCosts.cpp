// SPDX-License-Identifier: NOASSERTION
#include "KernelCosts.h"
#include "BenchmarkSupport.h"
#include "Toolbox/Collision2D.h"
#include "Toolbox/CollisionShapes.h"
#include "Toolbox/Contact2D.h"
#include "Toolbox/SegmentIntersection.h"
#include "Toolbox/SegmentIntersection2D.h"
#include "Toolbox/ShapeContactQuery2D.h"
#include "Toolbox/ShapeContactQuery3D.h"
#include "Toolbox/ShapeSweep2D.h"
#include "Toolbox/ShapeSweep3D.h"
#include <stdio.h>
namespace Dxf::Benchmark
{
namespace
{
// 1回の計測の呼出し数。
constexpr Toolbox::int32 Calls = 200000;

// 関数を繰り返し呼び、1回あたりの時間（ナノ秒、5回の中央値）を返す。
template <typename F> Toolbox::f64 Time_Internal(F&& Run)
{
	Toolbox::f64 Samples[Repetitions];
	Toolbox::uint64 Sink = 0;
	for (Toolbox::int32 Repetition = 0; Repetition < Repetitions; ++Repetition)
	{
		const Toolbox::uint64 Begin = NowNanoseconds();
		for (Toolbox::int32 Call = 0; Call < Calls; ++Call)
		{
			Sink += Run(Call);
		}
		Samples[Repetition] = static_cast<Toolbox::f64>(NowNanoseconds() - Begin) / Calls;
	}
	if (Sink == 0xffffffffffffffffULL)
	{
		printf("\n");
	}
	return Summarize(Samples).Median;
}
// 呼出しごとに少しずつ変える量（同じ入力の繰り返しを避ける）。
FORCEINLINE Toolbox::f32 Jitter_Internal(Toolbox::int32 Call) noexcept
{
	return static_cast<Toolbox::f32>(Call % 97) * 1e-4f;
}
// 1行を出力する。
void Row_Internal(const char* Case, Toolbox::f64 Nanoseconds2D, Toolbox::f64 Nanoseconds3D)
{
	printf("| %s | %.0f | %.0f | %.1fx |\n", Case, Nanoseconds2D, Nanoseconds3D, Nanoseconds3D / Nanoseconds2D);
	fflush(stdout);
}
} // namespace

void RunKernelCosts()
{
	printf("\n### Detail kernel cost per call (Toolbox functions, no World, no index, no shape transform)\n\n");
	printf("| case | 2D ns/call | 3D ns/call | 3D / 2D |\n|---|---|---|---|\n");
	// 床（上面y=0の大きな箱）と、その上に接触余裕0.02で浮く半径0.5の円／球。
	const Toolbox::FOrientedBox2D Floor2D{{0, -1}, {40, 1}, 0};
	const Toolbox::FOBB Floor3D{{0, -1, 0}, {40, 1, 40}};
	// 30度の坂の箱。
	const Toolbox::FOrientedBox2D Ramp2D{{1, 0}, {1, 0.3f}, 0.5236f};
	Toolbox::FOBB Ramp3D{{1, 0, 0}, {1, 0.3f, 1}};
	Ramp3D.Axes[0] = {static_cast<Toolbox::f32>(Toolbox::Cos(0.5236)), static_cast<Toolbox::f32>(Toolbox::Sin(0.5236)),
	                  0};
	Ramp3D.Axes[1] = {-static_cast<Toolbox::f32>(Toolbox::Sin(0.5236)), static_cast<Toolbox::f32>(Toolbox::Cos(0.5236)),
	                  0};
	const Toolbox::FCircle2D Other2D{{3, 0.5f}, 0.5f};
	const Toolbox::FSphere Other3D{{3, 0.5f, 0}, 0.5f};
	Row_Internal("overlap: ball above floor (miss by 0.02)",
	             Time_Internal(
	                 [&](Toolbox::int32 Call)
	                 {
		                 return Toolbox::Intersects(Toolbox::FCircle2D{{Jitter_Internal(Call), 0.52f}, 0.5f}, Floor2D,
		                                            0.0f)
		                            ? 1u
		                            : 0u;
	                 }),
	             Time_Internal(
	                 [&](Toolbox::int32 Call)
	                 {
		                 return Toolbox::IntersectsSphere(Toolbox::FSphere{{Jitter_Internal(Call), 0.52f, 0}, 0.5f},
		                                                  Floor3D, 0.0f)
		                            ? 1u
		                            : 0u;
	                 }));
	Row_Internal(
	    "contact: ball above floor",
	    Time_Internal(
	        [&](Toolbox::int32 Call)
	        {
		        return Toolbox::FindShapeContact(Toolbox::FCircle2D{{Jitter_Internal(Call), 0.52f}, 0.5f}, Floor2D)
		                       .Normal
		                   ? 1u
		                   : 0u;
	        }),
	    Time_Internal(
	        [&](Toolbox::int32 Call)
	        {
		        return Toolbox::FindShapeContact(Toolbox::FSphere{{Jitter_Internal(Call), 0.52f, 0}, 0.5f}, Floor3D)
		                       .Normal
		                   ? 1u
		                   : 0u;
	        }));
	Row_Internal(
	    "sweep: walking ball over floor (no hit)",
	    Time_Internal(
	        [&](Toolbox::int32 Call)
	        {
		        const Toolbox::f32 X = Jitter_Internal(Call);
		        return Toolbox::SweepToCenter(Toolbox::FCircle2D{{X, 0.52f}, 0.5f}, {X + 0.08f, 0.53f}, Floor2D) ? 1u
		                                                                                                         : 0u;
	        }),
	    Time_Internal(
	        [&](Toolbox::int32 Call)
	        {
		        const Toolbox::f32 X = Jitter_Internal(Call);
		        return Toolbox::SweepToCenter(Toolbox::FSphere{{X, 0.52f, 0}, 0.5f}, {X + 0.08f, 0.53f, 0.03f}, Floor3D)
		                   ? 1u
		                   : 0u;
	        }));
	Row_Internal(
	    "sweep: ball falling onto floor (hit)",
	    Time_Internal(
	        [&](Toolbox::int32 Call)
	        {
		        const Toolbox::f32 X = Jitter_Internal(Call);
		        return Toolbox::SweepToCenter(Toolbox::FCircle2D{{X, 0.6f}, 0.5f}, {X, 0.3f}, Floor2D) ? 1u : 0u;
	        }),
	    Time_Internal(
	        [&](Toolbox::int32 Call)
	        {
		        const Toolbox::f32 X = Jitter_Internal(Call);
		        return Toolbox::SweepToCenter(Toolbox::FSphere{{X, 0.6f, 0}, 0.5f}, {X, 0.3f, 0}, Floor3D) ? 1u : 0u;
	        }));
	Row_Internal("sweep: ball into 30-degree ramp (hit)",
	             Time_Internal(
	                 [&](Toolbox::int32 Call)
	                 {
		                 const Toolbox::f32 X = Jitter_Internal(Call);
		                 return Toolbox::SweepToCenter(Toolbox::FCircle2D{{X - 1.5f, 0.52f}, 0.5f}, {X + 0.5f, 0.52f},
		                                               Ramp2D)
		                            ? 1u
		                            : 0u;
	                 }),
	             Time_Internal(
	                 [&](Toolbox::int32 Call)
	                 {
		                 const Toolbox::f32 X = Jitter_Internal(Call);
		                 return Toolbox::SweepToCenter(Toolbox::FSphere{{X - 1.5f, 0.52f, 0}, 0.5f},
		                                               {X + 0.5f, 0.52f, 0}, Ramp3D)
		                            ? 1u
		                            : 0u;
	                 }));
	Row_Internal(
	    "sweep: ball against ball (hit)",
	    Time_Internal(
	        [&](Toolbox::int32 Call)
	        {
		        const Toolbox::f32 X = Jitter_Internal(Call);
		        return Toolbox::SweepToCenter(Toolbox::FCircle2D{{X, 0.5f}, 0.5f}, {X + 3, 0.5f}, Other2D) ? 1u : 0u;
	        }),
	    Time_Internal(
	        [&](Toolbox::int32 Call)
	        {
		        const Toolbox::f32 X = Jitter_Internal(Call);
		        return Toolbox::SweepToCenter(Toolbox::FSphere{{X, 0.5f, 0}, 0.5f}, {X + 3, 0.5f, 0}, Other3D) ? 1u
		                                                                                                       : 0u;
	        }));
	Row_Internal("segment: downward ray onto floor",
	             Time_Internal(
	                 [&](Toolbox::int32 Call)
	                 {
		                 const Toolbox::f32 X = Jitter_Internal(Call);
		                 return Toolbox::IntersectSegment(Toolbox::FVector2{X, 2}, Toolbox::FVector2{X + 0.3f, -2},
		                                                  Floor2D)
		                            ? 1u
		                            : 0u;
	                 }),
	             Time_Internal(
	                 [&](Toolbox::int32 Call)
	                 {
		                 const Toolbox::f32 X = Jitter_Internal(Call);
		                 return Toolbox::IntersectSegment(Toolbox::FVector3{X, 2, 0},
		                                                  Toolbox::FVector3{X + 0.3f, -2, 0}, Floor3D)
		                            ? 1u
		                            : 0u;
	                 }));
}
} // namespace Dxf::Benchmark
