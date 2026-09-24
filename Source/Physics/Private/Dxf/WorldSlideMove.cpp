// SPDX-License-Identifier: NOASSERTION
#include "Dxf/WorldSlideMove2D.h"
#include "Dxf/WorldSlideMove3D.h"
namespace Dxf
{
namespace
{
// 公開座標をf64の3成分へ読み込む（2Dの第3成分は0）。
void Load_Internal(Toolbox::FVector2 Value, Toolbox::f64 (&Out)[3]) noexcept
{
	Out[0] = Value.X;
	Out[1] = Value.Y;
	Out[2] = 0;
}
void Load_Internal(Toolbox::FVector3 Value, Toolbox::f64 (&Out)[3]) noexcept
{
	Out[0] = Value.X;
	Out[1] = Value.Y;
	Out[2] = Value.Z;
}
// f64の値を公開座標のf32へ戻す。有限でf32の範囲に収まらなければ計算不能として例外にする。
Toolbox::f32 ToF32_Internal(Toolbox::f64 Value)
{
	if (!Toolbox::IsFinite(Value) || Toolbox::Abs(Value) > Toolbox::f64(Toolbox::TNumericLimits<Toolbox::f32>::Max()))
	{
		throw Toolbox::FException("Unrepresentable slide move center");
	}
	return static_cast<Toolbox::f32>(Value);
}
void Store_Internal(const Toolbox::f64 (&Value)[3], Toolbox::FVector2& Out)
{
	Out = {ToF32_Internal(Value[0]), ToF32_Internal(Value[1])};
}
void Store_Internal(const Toolbox::f64 (&Value)[3], Toolbox::FVector3& Out)
{
	Out = {ToF32_Internal(Value[0]), ToF32_Internal(Value[1]), ToF32_Internal(Value[2])};
}
// 問い合わせ形状の条件（有限の中心と、有限の正の半径）。点の滑りは提供しない。
bool IsValidProbe_Internal(const Toolbox::FCircle2D& Shape) noexcept
{
	return Toolbox::IsValid(Shape) && Toolbox::IsFinite(Shape.Radius) && Shape.Radius > 0;
}
bool IsValidProbe_Internal(const Toolbox::FSphere& Shape) noexcept
{
	return Shape.Center.IsValid() && Toolbox::IsFinite(Shape.Radius) && Shape.Radius > 0;
}
// 3成分の内積。
Toolbox::f64 Dot_Internal(const Toolbox::f64 (&A)[3], const Toolbox::f64 (&B)[3]) noexcept
{
	return A[0] * B[0] + A[1] * B[1] + A[2] * B[2];
}
// StartからEndへの区間で、ヒット割合Fractionから経路に沿ってDistanceだけ戻した中心。
// safe = max(0, Fraction - Distance / L)、候補 = (1 - safe) * Start + safe * End（すべてf64）。
// safeが0なら開始中心そのものを返す（最初のスイープで非接触を確認した同じ値）。
template <typename TVector>
TVector Backoff_Internal(TVector Start, TVector End, Toolbox::f64 Fraction, Toolbox::f64 Distance)
{
	Toolbox::f64 From[3]{};
	Toolbox::f64 To[3]{};
	Load_Internal(Start, From);
	Load_Internal(End, To);
	const Toolbox::f64 Delta[3] = {To[0] - From[0], To[1] - From[1], To[2] - From[2]};
	// 最大成分で割ってから長さを求め、平方の桁あふれ・消失を避ける。初期接触でないヒットの区間は長さが正。
	const Toolbox::f64 Scale =
	    Toolbox::Max(Toolbox::Abs(Delta[0]), Toolbox::Max(Toolbox::Abs(Delta[1]), Toolbox::Abs(Delta[2])));
	if (!(Scale > 0))
	{
		throw Toolbox::FException("Invalid slide move segment");
	}
	const Toolbox::f64 Scaled[3] = {Delta[0] / Scale, Delta[1] / Scale, Delta[2] / Scale};
	const Toolbox::f64 Length = Scale * Toolbox::Sqrt(Dot_Internal(Scaled, Scaled));
	const Toolbox::f64 Safe = Toolbox::Max(0.0, Fraction - Distance / Length);
	if (Safe == 0)
	{
		return Start;
	}
	const Toolbox::f64 Candidate[3] = {(1 - Safe) * From[0] + Safe * To[0], (1 - Safe) * From[1] + Safe * To[1],
	                                   (1 - Safe) * From[2] + Safe * To[2]};
	TVector Result;
	Store_Internal(Candidate, Result);
	return Result;
}
// 手前へ戻した候補を求め、開始中心と異なれば、開始中心からf32へ丸めた候補までを同じ条件で再スイープする。
// 再検査で接触すれば空（その区間を採用しない）。再検査の例外は伝播する。
template <typename TWorld, typename TShape, typename TVector, typename TBodyId>
Toolbox::TOptional<TVector> Advance_Internal(const TWorld& World, const TShape& From, TVector End,
                                             Toolbox::f64 Fraction, Toolbox::f64 Distance,
                                             const Toolbox::TOptional<TBodyId>& ExcludedBody,
                                             const FWorldQueryFilter& Filter)
{
	const TVector Candidate = Backoff_Internal(From.Center, End, Fraction, Distance);
	if (Candidate == From.Center)
	{
		return Candidate;
	}
	if (World.SweepClosest(From, Candidate, ExcludedBody, Filter))
	{
		return {};
	}
	return Candidate;
}
// 2D／3D共通の手順。SweepClosestは最大4回（最初の移動・その再検査・滑り経路・その再検査）。
// 途中の結果は局所変数にだけ持ち、例外時は何も返さない。
template <typename TResult, typename TWorld, typename TShape, typename TVector, typename TBodyId>
TResult Slide_Internal(const TWorld& World, const TShape& StartShape, TVector DesiredEndCenter,
                       Toolbox::f64 BackoffDistance, const Toolbox::TOptional<TBodyId>& ExcludedBody,
                       const FWorldQueryFilter& Filter)
{
	if (!IsValidProbe_Internal(StartShape) || !DesiredEndCenter.IsValid())
	{
		throw Toolbox::FException("Invalid slide move shape or end center");
	}
	if (!Toolbox::IsFinite(BackoffDistance) || !(BackoffDistance > 0))
	{
		throw Toolbox::FException("Invalid slide move backoff distance");
	}
	TResult Result;
	Result.EndCenter = StartShape.Center;
	// 最初の移動。移動0・マスク0でも、既存の状態・除外IDの検査を通る。
	Result.FirstHit = World.SweepClosest(StartShape, DesiredEndCenter, ExcludedBody, Filter);
	if (!Result.FirstHit)
	{
		// 非交差の区間は、同じf32の希望終点まで検査済み。
		Result.EndCenter = DesiredEndCenter;
		Result.Stop =
		    StartShape.Center == DesiredEndCenter ? EWorldSlideStop::NoMovement : EWorldSlideStop::ReachedDesiredEnd;
		return Result;
	}
	if (Result.FirstHit->bInitialContact)
	{
		// 離脱・押し出しは試さない。開始中心が重なっていても、そのまま返す。
		Result.Stop = EWorldSlideStop::InitialContact;
		return Result;
	}
	const Toolbox::TOptional<TVector> After = Advance_Internal(
	    World, StartShape, DesiredEndCenter, Result.FirstHit->Fraction, BackoffDistance, ExcludedBody, Filter);
	if (!After)
	{
		Result.Stop = EWorldSlideStop::PrecisionLimit;
		return Result;
	}
	Result.EndCenter = *After;
	if (!Result.FirstHit->Normal)
	{
		// 法線が空でも衝突は成立している。希望終点へは進めない。
		Result.Stop = EWorldSlideStop::MissingNormal;
		return Result;
	}
	// 採用した中心Aから希望終点Qへの残りRから、法線Nの内向き成分だけを除く（反射・正規化はしない）。
	Toolbox::f64 A[3]{};
	Toolbox::f64 Q[3]{};
	Toolbox::f64 N[3]{};
	Load_Internal(*After, A);
	Load_Internal(DesiredEndCenter, Q);
	Load_Internal(*Result.FirstHit->Normal, N);
	// f32で保持された単位法線の丸めを考慮し、長さの二乗で割る。
	const Toolbox::f64 NormalSquared = Dot_Internal(N, N);
	if (!Toolbox::IsFinite(NormalSquared) || !(NormalSquared > 0))
	{
		throw Toolbox::FException("Invalid slide move normal");
	}
	const Toolbox::f64 Rest[3] = {Q[0] - A[0], Q[1] - A[1], Q[2] - A[2]};
	const Toolbox::f64 Inward = Toolbox::Min(Dot_Internal(Rest, N), 0.0) / NormalSquared;
	const Toolbox::f64 Slide[3] = {Rest[0] - N[0] * Inward, Rest[1] - N[1] * Inward, Rest[2] - N[2] * Inward};
	if (Slide[0] == 0 && Slide[1] == 0 && Slide[2] == 0)
	{
		// 残りがすべて内向きで、補正後に進む成分がない。
		Result.Stop = EWorldSlideStop::Blocked;
		return Result;
	}
	const Toolbox::f64 SlideEndValue[3] = {A[0] + Slide[0], A[1] + Slide[1], A[2] + Slide[2]};
	TVector SlideEnd;
	Store_Internal(SlideEndValue, SlideEnd);
	if (SlideEnd == *After)
	{
		// 計算上は進む成分があっても、f32では同じ中心になる。再試行しない。
		Result.Stop = EWorldSlideStop::PrecisionLimit;
		return Result;
	}
	// 補正は1回だけ。滑り経路は同じ半径・除外ID・Filterで問い合わせ、最初の対象も除外しない。
	TShape Probe = StartShape;
	Probe.Center = *After;
	Result.SlideHit = World.SweepClosest(Probe, SlideEnd, ExcludedBody, Filter);
	if (!Result.SlideHit)
	{
		Result.EndCenter = SlideEnd;
		Result.Stop = EWorldSlideStop::SlideCompleted;
		return Result;
	}
	if (Result.SlideHit->bInitialContact)
	{
		Result.Stop = EWorldSlideStop::Blocked;
		return Result;
	}
	const Toolbox::TOptional<TVector> Stopped =
	    Advance_Internal(World, Probe, SlideEnd, Result.SlideHit->Fraction, BackoffDistance, ExcludedBody, Filter);
	if (!Stopped)
	{
		Result.Stop = EWorldSlideStop::PrecisionLimit;
		return Result;
	}
	Result.EndCenter = *Stopped;
	Result.Stop = EWorldSlideStop::Blocked;
	return Result;
}
} // namespace
// 2Dの円。共通の手順を使う。
FWorldSlideResult2D ComputeSlideMove(const FPhysicsWorld2D& World, const Toolbox::FCircle2D& StartShape,
                                     Toolbox::FVector2 DesiredEndCenter, Toolbox::f64 BackoffDistance,
                                     Toolbox::TOptional<FBodyId2D> ExcludedBody, const FWorldQueryFilter& Filter)
{
	return Slide_Internal<FWorldSlideResult2D>(World, StartShape, DesiredEndCenter, BackoffDistance, ExcludedBody,
	                                           Filter);
}
// 3Dの球。共通の手順を使う。
FWorldSlideResult3D ComputeSlideMove(const FPhysicsWorld3D& World, const Toolbox::FSphere& StartShape,
                                     Toolbox::FVector3 DesiredEndCenter, Toolbox::f64 BackoffDistance,
                                     Toolbox::TOptional<FBodyId3D> ExcludedBody, const FWorldQueryFilter& Filter)
{
	return Slide_Internal<FWorldSlideResult3D>(World, StartShape, DesiredEndCenter, BackoffDistance, ExcludedBody,
	                                           Filter);
}
} // namespace Dxf
