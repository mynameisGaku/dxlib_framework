// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_MECHANISM_CONSTRAINT_MATH_H
#define DXF_MECHANISM_CONSTRAINT_MATH_H
#include "Dxf/JointKind.h"
#include "Dxf/JointLimitState.h"
#include "Toolbox/Utility.h"
namespace Dxf::PhysicsPrivate
{
/**
 * 倍精度の三成分。2DもZ=0の同じ行計算を使う。
 */
struct FMechanismVector
{
	/**
	 * ワールド各軸の成分。
	 */
	Toolbox::f64 X = 0;
	/**
	 * Y軸の成分。
	 */
	Toolbox::f64 Y = 0;
	/**
	 * Z軸の成分。
	 */
	Toolbox::f64 Z = 0;
};
/**
 * 小さい数式部だけで使う四元数。
 */
struct FMechanismRotation
{
	/**
	 * 虚部と実部。
	 */
	Toolbox::f64 X = 0;
	/**
	 * Y軸の成分。
	 */
	Toolbox::f64 Y = 0;
	/**
	 * Z軸の成分。
	 */
	Toolbox::f64 Z = 0;
	/**
	 * Quaternionの実部。
	 */
	Toolbox::f64 W = 1;
};
/**
 * World recordから一度写す非所有の作業Pose。
 */
struct FMechanismBody
{
	/**
	 * 重心位置・姿勢・速度。
	 */
	FMechanismVector Position;
	/**
	 * ワールド姿勢。
	 */
	FMechanismRotation Rotation;
	/**
	 * ワールド並進速度。
	 */
	FMechanismVector Velocity;
	/**
	 * ワールド角速度。
	 */
	FMechanismVector Angular;
	/**
	 * ワールド側の動的・休止契約を反映した逆質量とLocal逆慣性。
	 */
	Toolbox::f64 InverseMass = 0;
	/**
	 * Body局所軸での逆慣性。
	 */
	FMechanismVector InverseInertia;
};
/**
 * 共通slot内の明示的な新種類データ。
 */
struct FMechanismSettings
{
	/**
	 * Local取付位置と方向。Distanceでは使用しない。
	 */
	FMechanismVector AnchorA;
	/**
	 * Body Bの重心基準Local Anchor。
	 */
	FMechanismVector AnchorB;
	/**
	 * Body Aへ取り付けるLocal Frameの向き。
	 */
	FMechanismRotation RotationA;
	/**
	 * Body Bへ取り付けるLocal Frameの向き。
	 */
	FMechanismRotation RotationB;
	/**
	 * 自由座標のLimitとDrive。種類ごとの公開単位から変換する。
	 */
	bool bLimit = false;
	/**
	 * 座標または累積Impulseの下限。
	 */
	Toolbox::f64 Lower = -1;
	/**
	 * 座標または累積Impulseの上限。
	 */
	Toolbox::f64 Upper = 1;
	/**
	 * 有限の速度Motorを使うか。
	 */
	bool bMotor = false;
	/**
	 * 目標速度。
	 */
	Toolbox::f64 Target = 0;
	/**
	 * 最大TorqueまたはForce。
	 */
	Toolbox::f64 Maximum = 0;
};
/**
 * Step作業領域でだけ更新し、成功時にrecordへ確定する。
 */
struct FMechanismCache
{
	/**
	 * 基本6行・Motor・Limitの累積Impulse。
	 */
	Toolbox::f64 Impulses[8]{};
	/**
	 * Limit側が変わった際の古い片側Impulseの失効判定。
	 */
	EJointLimitState Side = EJointLimitState::Disabled;
};
/**
 * Cの微分と同じ符号の一般化Impulseを保持する一行。
 */
struct FMechanismRow
{
	/**
	 * A/Bの並進と回転に掛かる係数。
	 */
	FMechanismVector LinearA;
	/**
	 * B側の並進係数。
	 */
	FMechanismVector LinearB;
	/**
	 * A側の角速度係数。
	 */
	FMechanismVector AngularA;
	/**
	 * B側の角速度係数。
	 */
	FMechanismVector AngularB;
	/**
	 * 位置誤差、目標速度、速度反復の追加bias。
	 */
	Toolbox::f64 Error = 0;
	/**
	 * 目標速度。
	 */
	Toolbox::f64 Target = 0;
	/**
	 * Limitの位置誤差から求める追加速度。
	 */
	Toolbox::f64 Bias = 0;
	/**
	 * 累積Impulseの下上限。
	 */
	Toolbox::f64 Lower = -1e100;
	/**
	 * 座標または累積Impulseの上限。
	 */
	Toolbox::f64 Upper = 1e100;
	/**
	 * 有効か、位置補正に使うか。
	 */
	bool bUsed = false;
	/**
	 * この行を位置補正にも使うか。
	 */
	bool bPosition = true;
};
/**
 * 一Jointの固定長の行。登録・反復中の追加確保はしない。
 */
struct FMechanismRows
{
	/**
	 * 固定された正準順の拘束行。
	 */
	FMechanismRow Values[8];
	/**
	 * 自由座標・その速度・姿勢誤差の観察。
	 */
	Toolbox::f64 Coordinate = 0;
	/**
	 * 自由座標の現在速度。
	 */
	Toolbox::f64 Rate = 0;
	/**
	 * Anchorまたは横方向の誤差。
	 */
	Toolbox::f64 AnchorError = 0;
	/**
	 * 姿勢または軸整合の誤差。
	 */
	Toolbox::f64 AngularError = 0;
	EJointLimitState Side = EJointLimitState::Disabled;
};
/**
 * 同じ空間のベクトルA／Bを加算する。
 */
FORCEINLINE FMechanismVector Add(FMechanismVector A, FMechanismVector B) noexcept
{
	return {A.X + B.X, A.Y + B.Y, A.Z + B.Z};
}
/**
 * ベクトルAの各成分へ有限の係数Sを掛ける。
 */
FORCEINLINE FMechanismVector Scale(FMechanismVector A, Toolbox::f64 S) noexcept
{
	return {A.X * S, A.Y * S, A.Z * S};
}
/**
 * 同じ空間のAからBを引く。
 */
FORCEINLINE FMechanismVector Subtract(FMechanismVector A, FMechanismVector B) noexcept
{
	return Add(A, Scale(B, -1));
}
/**
 * 同じ空間のA／Bの内積を返す。
 */
FORCEINLINE Toolbox::f64 Dot(FMechanismVector A, FMechanismVector B) noexcept
{
	return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
}
/**
 * 右手系のA／Bの外積を返す。
 */
FORCEINLINE FMechanismVector Cross(FMechanismVector A, FMechanismVector B) noexcept
{
	return {A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X};
}
/**
 * ベクトルAの大きさを返す。
 */
FORCEINLINE Toolbox::f64 Length(FMechanismVector A) noexcept
{
	return Toolbox::Sqrt(Dot(A, A));
}
/**
 * 単位Quaternion Qの逆回転を返す。
 */
FORCEINLINE FMechanismRotation Conjugate(FMechanismRotation Q) noexcept
{
	return {-Q.X, -Q.Y, -Q.Z, Q.W};
}
/**
 * Bの回転にAを左から合成する。
 */
FORCEINLINE FMechanismRotation Multiply(FMechanismRotation A, FMechanismRotation B) noexcept
{
	return {A.W * B.X + A.X * B.W + A.Y * B.Z - A.Z * B.Y, A.W * B.Y - A.X * B.Z + A.Y * B.W + A.Z * B.X, A.W * B.Z + A.X * B.Y - A.Y * B.X + A.Z * B.W, A.W * B.W - A.X * B.X - A.Y * B.Y - A.Z * B.Z};
}
/**
 * Quaternion Qを単位化する。非有限・ゼロ長は拒否する。
 */
FMechanismRotation Normalize(FMechanismRotation Q);
/**
 * 単位Quaternion QでLocalベクトルVを回す。
 */
FORCEINLINE FMechanismVector Rotate(FMechanismRotation Q, FMechanismVector V) noexcept
{
	// R v。慣性変換のR D R^Tとは別の演算。
	const FMechanismVector U{Q.X, Q.Y, Q.Z};
	const FMechanismVector T = Scale(Cross(U, V), 2);
	return Add(V, Add(Scale(T, Q.W), Cross(U, T)));
}
/**
 * Body BのLocal逆慣性をワールドへ回しVへ作用させる。
 */
FORCEINLINE FMechanismVector Inertia(const FMechanismBody& B, FMechanismVector V) noexcept
{
	const FMechanismVector L = Rotate(Conjugate(B.Rotation), V);
	return Rotate(B.Rotation, {L.X * B.InverseInertia.X, L.Y * B.InverseInertia.Y, L.Z * B.InverseInertia.Z});
}
/**
 * 角Aを主値(-π,π]へ移す。
 */
FORCEINLINE Toolbox::f64 Principal(Toolbox::f64 A) noexcept
{
	// atan2の-πだけを+πへ移して公開区間を一意にする。
	const Toolbox::f64 V = Toolbox::Atan2(Toolbox::Sin(A), Toolbox::Cos(A));
	return V <= -3.14159265358979323846 ? 3.14159265358979323846 : V;
}
/**
 * Frame AからBへの最短回転をワールド空間で返す。
 */
FMechanismVector RotationError(FMechanismRotation A, FMechanismRotation B) noexcept;
/**
 * 方向N、両端の腕RA/RB、間隔Dから並進行を作る。bRotatingはAに付いた軸の微分を含める。
 */
FMechanismRow LinearRow(FMechanismVector N, FMechanismVector RA, FMechanismVector RB, FMechanismVector D, bool bRotating) noexcept;
/**
 * ワールド方向Nの角度誤差Cを拘束する行を作る。
 */
FORCEINLINE FMechanismRow AngularRow(FMechanismVector N, Toolbox::f64 C) noexcept
{
	FMechanismRow R;
	R.bUsed = true;
	R.AngularA = Scale(N, -1);
	R.AngularB = N;
	R.Error = C;
	return R;
}
// 最短回転誤差の成分を微分する。誤差が大きいときも速度と位置で同じ行を使う。
/**
 * 最短回転ErrorのN方向成分をRelativeの両端角速度で微分する。
 */
FMechanismRow RotationRow(FMechanismVector N, FMechanismVector Error, FMechanismRotation Relative) noexcept;
/**
 * 行Rの微分をBody A/Bの現在速度へ作用させる。
 */
FORCEINLINE Toolbox::f64 Rate(const FMechanismRow& R, const FMechanismBody& A, const FMechanismBody& B) noexcept
{
	return Dot(R.LinearA, A.Velocity) + Dot(R.LinearB, B.Velocity) + Dot(R.AngularA, A.Angular) + Dot(R.AngularB, B.Angular);
}
/**
 * 行Rへ単位Impulseを加えた速度変化KをBody A/Bから求める。
 */
FORCEINLINE Toolbox::f64 EffectiveMass(const FMechanismRow& R, const FMechanismBody& A, const FMechanismBody& B) noexcept
{
	return A.InverseMass * Dot(R.LinearA, R.LinearA) + B.InverseMass * Dot(R.LinearB, R.LinearB) + Dot(R.AngularA, Inertia(A, R.AngularA)) + Dot(R.AngularB, Inertia(B, R.AngularB));
}
/**
 * 設定Sの区間に対する現在座標Cの位置を返す。
 */
FORCEINLINE EJointLimitState LimitSide(const FMechanismSettings& S, Toolbox::f64 C) noexcept
{
	if (!S.bLimit)
	{
		return EJointLimitState::Disabled;
	}
	if (S.Lower == S.Upper)
	{
		return EJointLimitState::Locked;
	}
	if (C <= S.Lower)
	{
		return EJointLimitState::Lower;
	}
	if (C >= S.Upper)
	{
		return EJointLimitState::Upper;
	}
	return EJointLimitState::Inside;
}
/**
 * Kindと次元bPlanar、設定S、Body A/B、SubStep秒Hから固定順の行を作る。退化Frameと数値範囲外は拒否する。
 */
FMechanismRows BuildMechanismRows(EJointKind Kind, bool bPlanar, const FMechanismSettings& S, const FMechanismBody& A, const FMechanismBody& B, Toolbox::f64 H);
/**
 * 行RのImpulseをBody作業値A/Bへ作用させる。bPositionなら速度を変えずPoseだけ補正する。
 */
void ApplyRow(const FMechanismRow& R, FMechanismBody& A, FMechanismBody& B, Toolbox::f64 Impulse, bool bPosition);
/**
 * 行RをBody作業値A/Bで一度解き、累積値Accumulatedを予算内に保持する。数値範囲外は拒否する。
 */
void SolveMechanismRow(const FMechanismRow& R, FMechanismBody& A, FMechanismBody& B, Toolbox::f64& Accumulated);
} // namespace Dxf::PhysicsPrivate
#endif
