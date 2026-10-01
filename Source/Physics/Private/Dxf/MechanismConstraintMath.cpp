// SPDX-License-Identifier: NOASSERTION
#include "MechanismConstraintMath.h"
namespace Dxf::PhysicsPrivate
{
// 有限で非ゼロの入力だけを正規化する。
FMechanismRotation Normalize(FMechanismRotation Q)
{
	if (!Toolbox::IsFinite(Q.X) || !Toolbox::IsFinite(Q.Y) || !Toolbox::IsFinite(Q.Z) || !Toolbox::IsFinite(Q.W))
	{
		throw Toolbox::FException("Non-finite joint frame quaternion");
	}
	// 最大成分で割ってから正規化し、有限な大きい入力の二乗overflowを避ける。
	const Toolbox::f64 M = Toolbox::Max(Toolbox::Max(Toolbox::Abs(Q.X), Toolbox::Abs(Q.Y)), Toolbox::Max(Toolbox::Abs(Q.Z), Toolbox::Abs(Q.W)));
	if (!Toolbox::IsFinite(M) || M <= 0)
	{
		throw Toolbox::FException("Invalid joint frame quaternion");
	}
	Q = {Q.X / M, Q.Y / M, Q.Z / M, Q.W / M};
	// 最大成分で割ったQuaternionの長さ。
	const Toolbox::f64 N = Toolbox::Sqrt(Q.X * Q.X + Q.Y * Q.Y + Q.Z * Q.Z + Q.W * Q.W);
	return {Q.X / N, Q.Y / N, Q.Z / N, Q.W / N};
}
// ワールドで表した最短回転誤差を返す。
FMechanismVector RotationError(FMechanismRotation A, FMechanismRotation B) noexcept
{
	// ワールド空間の最短回転。半回転もqと-qで同じ向きを選ぶ。
	FMechanismRotation Q = Multiply(B, Conjugate(A));
	if (Q.W < 0 || (Q.W == 0 && (Q.X < 0 || (Q.X == 0 && (Q.Y < 0 || (Q.Y == 0 && Q.Z < 0))))))
	{
		Q = {-Q.X, -Q.Y, -Q.Z, -Q.W};
	}
	// 回転誤差Quaternionのベクトル部分。
	const FMechanismVector V{Q.X, Q.Y, Q.Z};
	// 回転誤差のベクトル部分の長さ。
	const Toolbox::f64 S = Length(V);
	return S < 1e-12 ? Scale(V, 2) : Scale(V, 2 * Toolbox::Atan2(S, Q.W) / S);
}
// 両端の腕と、Aへ取り付けた方向の微分を一緒に組み立てる。
FMechanismRow LinearRow(FMechanismVector N, FMechanismVector RA, FMechanismVector RB, FMechanismVector D, bool bRotating) noexcept
{
	// 速度微分とImpulse適用の符号を共有する拘束行。
	FMechanismRow R;
	R.bUsed = true;
	R.LinearA = Scale(N, -1);
	R.LinearB = N;
	R.AngularA = Scale(Cross(RA, N), -1);
	if (bRotating)
	{
		// 動く軸の微分：(omegaA×N)・D = omegaA・(N×D)。
		R.AngularA = Add(R.AngularA, Cross(N, D));
	}
	R.AngularB = Cross(RB, N);
	R.Error = Dot(N, D);
	return R;
}
// SO(3)の対数の微分で姿勢誤差とImpulseの空間を揃える。
FMechanismRow RotationRow(FMechanismVector N, FMechanismVector Error, FMechanismRotation Relative) noexcept
{
	// 姿勢誤差の主値角。
	const Toolbox::f64 Angle = Length(Error);
	// 姿勢誤差の微分係数。
	const Toolbox::f64 Factor = Angle < 1e-4 ? 1.0 / 12.0 + Angle * Angle / 720.0 : (1 - 0.5 * Angle * Toolbox::Cos(0.5 * Angle) / Toolbox::Sin(0.5 * Angle)) / (Angle * Angle);
	// 誤差を速度で微分したワールド方向。
	const FMechanismVector Gradient = Add(N, Add(Scale(Cross(Error, N), 0.5), Scale(Cross(Error, Cross(Error, N)), Factor)));
	// 速度微分とImpulse適用の符号を共有する拘束行。
	FMechanismRow R = AngularRow(Gradient, Dot(N, Error));
	R.AngularA = Scale(Rotate(Conjugate(Relative), Gradient), -1);
	return R;
}
// 種類別の拘束と残す自由度を、同じFrameから組み立てる。
FMechanismRows BuildMechanismRows(EJointKind Kind, bool bPlanar, const FMechanismSettings& S, const FMechanismBody& A, const FMechanismBody& B, Toolbox::f64 H)
{
	// Anchor、Frame、軸、観察、Jacobianはこの一つの組立てを共有する。
	FMechanismRows Out;
	// A側Anchorの重心からのワールド腕。
	const FMechanismVector RA = Rotate(A.Rotation, S.AnchorA);
	// B側Anchorの重心からのワールド腕。
	const FMechanismVector RB = Rotate(B.Rotation, S.AnchorB);
	// 二つのワールドAnchorの差。
	const FMechanismVector D = Subtract(Add(B.Position, RB), Add(A.Position, RA));
	// A側Body姿勢とLocal Frameを合成した姿勢。
	const FMechanismRotation QA = Multiply(A.Rotation, S.RotationA);
	// B側Body姿勢とLocal Frameを合成した姿勢。
	const FMechanismRotation QB = Multiply(B.Rotation, S.RotationB);
	// A側FrameのX軸。直動の自由方向。
	const FMechanismVector X = Rotate(QA, {1, 0, 0});
	// A側FrameのY軸。横拘束方向。
	const FMechanismVector Y = Rotate(QA, {0, 1, 0});
	// A側FrameのZ軸。回転Jointの自由軸。
	const FMechanismVector Z = Rotate(QA, {0, 0, 1});
	// Frame間の最短回転誤差をワールド空間で表した値。
	const FMechanismVector Error = RotationError(QA, QB);
	// LimitとMotorが操作する一座標の拘束行。
	FMechanismRow Free;
	if (Kind == EJointKind::Prismatic)
	{
		Out.Values[0] = LinearRow(Y, RA, RB, D, true);
		if (!bPlanar)
		{
			Out.Values[1] = LinearRow(Z, RA, RB, D, true);
		}
		Free = LinearRow(X, RA, RB, D, true);
		Out.Coordinate = Free.Error;
		Out.AnchorError = Toolbox::Sqrt(Out.Values[0].Error * Out.Values[0].Error + Out.Values[1].Error * Out.Values[1].Error);
	}
	else
	{
		Out.Values[0] = LinearRow({1, 0, 0}, RA, RB, D, false);
		Out.Values[1] = LinearRow({0, 1, 0}, RA, RB, D, false);
		if (!bPlanar)
		{
			Out.Values[2] = LinearRow({0, 0, 1}, RA, RB, D, false);
		}
		Out.AnchorError = Length(D);
	}
	if (Kind == EJointKind::Revolute)
	{
		// B側Frameの回転軸。
		const FMechanismVector ZB = Rotate(QB, {0, 0, 1});
		// 両回転軸の内積。反平行の退化判定にも使う。
		const Toolbox::f64 Alignment = Dot(Z, ZB);
		if (Alignment < -0.999999)
		{
			throw Toolbox::FException("Revolute frame axes are antiparallel");
		}
		// 両回転軸の外積。
		const FMechanismVector Tilt = Cross(Z, ZB);
		// 軸の外積の長さ。
		const Toolbox::f64 TiltLength = Length(Tilt);
		Out.AngularError = Toolbox::Atan2(TiltLength, Alignment);
		// 自由な軸回転を除いた傾き誤差。
		const FMechanismVector Swing = TiltLength < 1e-12 ? Tilt : Scale(Tilt, Out.AngularError / TiltLength);
		if (!bPlanar)
		{
			// AのFrameで表したswingの微分。Frame自体の回転も係数へ含める。
			const FMechanismVector LocalZ = Rotate(Conjugate(QA), ZB);
			// 姿勢誤差の微分係数。
			const Toolbox::f64 Factor = TiltLength < 1e-6 ? 1 + Out.AngularError * Out.AngularError / 6 : Out.AngularError / TiltLength;
			// 軸の傾き係数を内積で微分した値。
			const Toolbox::f64 Derivative = TiltLength < 1e-6 ? -1.0 / 3.0 : (Out.AngularError * Alignment - TiltLength) / (TiltLength * TiltLength * TiltLength);
			// A側Frame X方向の傾き誤差の微分。
			const FMechanismVector GX{Factor * Alignment - LocalZ.Y * LocalZ.Y * Derivative, LocalZ.X * LocalZ.Y * Derivative, -Factor * LocalZ.X};
			// A側Frame Y方向の傾き誤差の微分。
			const FMechanismVector GY{LocalZ.X * LocalZ.Y * Derivative, Factor * Alignment - LocalZ.X * LocalZ.X * Derivative, -Factor * LocalZ.Y};
			Out.Values[3] = AngularRow(Rotate(QA, GX), Dot(X, Swing));
			Out.Values[4] = AngularRow(Rotate(QA, GY), Dot(Y, Swing));
		}
		// 二つのFrame間の相対Quaternion。
		const FMechanismRotation Relative = Multiply(Conjugate(QA), QB);
		// twistを投影して正規化する二乗長。
		const Toolbox::f64 Den = Relative.W * Relative.W + Relative.Z * Relative.Z;
		if (Den < 1e-12)
		{
			throw Toolbox::FException("Undefined revolute twist");
		}
		Out.Coordinate = Principal(2 * Toolbox::Atan2(Relative.Z, Relative.W));
		// swingが残った際もtwist主値の微分を使う。qと-qで同じ係数になる。
		const FMechanismVector Gradient = Rotate(QA, {(Relative.W * Relative.Y + Relative.Z * Relative.X) / Den, (-Relative.W * Relative.X + Relative.Z * Relative.Y) / Den, 1});
		Free = AngularRow(Gradient, Out.Coordinate);
	}
	else
	{
		Out.AngularError = Length(Error);
		if (!bPlanar)
		{
			Out.Values[3] = RotationRow({1, 0, 0}, Error, Multiply(QB, Conjugate(QA)));
			Out.Values[4] = RotationRow({0, 1, 0}, Error, Multiply(QB, Conjugate(QA)));
			Out.Values[5] = RotationRow({0, 0, 1}, Error, Multiply(QB, Conjugate(QA)));
		}
		else
		{
			Out.Values[5] = AngularRow({0, 0, 1}, Error.Z);
		}
	}
	if (Kind == EJointKind::Fixed)
	{
		return Out;
	}
	Out.Rate = Rate(Free, A, B);
	Out.Side = LimitSide(S, Out.Coordinate);
	Out.Values[6] = Free;
	Out.Values[6].bUsed = S.bMotor && S.Maximum > 0;
	Out.Values[6].bPosition = false;
	Out.Values[6].Target = S.Target;
	Out.Values[6].Lower = -S.Maximum * H;
	Out.Values[6].Upper = S.Maximum * H;
	if (!Toolbox::IsFinite(Out.Values[6].Upper))
	{
		throw Toolbox::FException("Mechanism motor impulse budget overflow");
	}
	Out.Values[7] = Free;
	Out.Values[7].bUsed = false;
	if (S.bLimit)
	{
		// 範囲内でも次の移動で越えるときだけ、残った距離に応じた速度を許す。
		const bool bLower = Out.Coordinate <= S.Lower || Out.Coordinate + Out.Rate * H < S.Lower;
		// 現在または次の移動が上限へ到達するか。
		const bool bUpper = Out.Coordinate >= S.Upper || Out.Coordinate + Out.Rate * H > S.Upper;
		if (S.Lower == S.Upper || bLower || bUpper)
		{
			auto& R = Out.Values[7];
			R.bUsed = true;
			R.Error = Out.Coordinate - (bUpper && S.Lower != S.Upper ? S.Upper : S.Lower);
			R.Bias = R.Error / H;
			if (S.Lower != S.Upper)
			{
				R.Lower = bUpper ? -1e100 : 0;
				R.Upper = bUpper ? 0 : 1e100;
				R.bPosition = bUpper ? Out.Coordinate > S.Upper : Out.Coordinate < S.Lower;
			}
		}
	}
	return Out;
}
// 両端の動的作業値へ同じ符号のImpulseを反映する。
void ApplyRow(const FMechanismRow& R, FMechanismBody& A, FMechanismBody& B, Toolbox::f64 Impulse, bool bPosition)
{
	// A側の線形速度または位置の補正量。
	const FMechanismVector LA = Scale(R.LinearA, A.InverseMass * Impulse);
	// B側の線形速度または位置の補正量。
	const FMechanismVector LB = Scale(R.LinearB, B.InverseMass * Impulse);
	// A側の角速度または姿勢の補正量。
	const FMechanismVector AA = Scale(Inertia(A, R.AngularA), Impulse);
	// B側の角速度または姿勢の補正量。
	const FMechanismVector AB = Scale(Inertia(B, R.AngularB), Impulse);
	if (!bPosition)
	{
		A.Velocity = Add(A.Velocity, LA);
		B.Velocity = Add(B.Velocity, LB);
		A.Angular = Add(A.Angular, AA);
		B.Angular = Add(B.Angular, AB);
		return;
	}
	A.Position = Add(A.Position, LA);
	B.Position = Add(B.Position, LB);
	// ワールド微小回転を左から合成する。速度には補正を足さない。
	A.Rotation = Normalize(Multiply({AA.X * 0.5, AA.Y * 0.5, AA.Z * 0.5, 1}, A.Rotation));
	B.Rotation = Normalize(Multiply({AB.X * 0.5, AB.Y * 0.5, AB.Z * 0.5, 1}, B.Rotation));
}
// 累積Impulseの上限を反復全体で守る。
void SolveMechanismRow(const FMechanismRow& R, FMechanismBody& A, FMechanismBody& B, Toolbox::f64& Accumulated)
{
	if (!R.bUsed)
	{
		Accumulated = 0;
		return;
	}
	// 単位Impulseに対する拘束速度の変化量。
	const Toolbox::f64 K = EffectiveMass(R, A, B);
	if (K == 0)
	{
		return;
	}
	// 各項が非負の一行なので相殺による条件悪化はない。正の有限Kを使う。
	if (!Toolbox::IsFinite(K) || K < 0)
	{
		throw Toolbox::FException("Singular mechanism constraint row");
	}
	// 上限を適用する前の累積Impulse。
	const Toolbox::f64 Old = Accumulated;
	Accumulated = Toolbox::Clamp(Old - (Rate(R, A, B) - R.Target + R.Bias) / K, R.Lower, R.Upper);
	if (!Toolbox::IsFinite(Accumulated))
	{
		throw Toolbox::FException("Mechanism accumulated impulse overflow");
	}
	ApplyRow(R, A, B, Accumulated - Old, false);
}
} // namespace Dxf::PhysicsPrivate
