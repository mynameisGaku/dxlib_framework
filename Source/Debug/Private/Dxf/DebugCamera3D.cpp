// SPDX-License-Identifier: NOASSERTION
#include "Dxf/DebugCamera3D.h"
namespace Dxf
{
namespace
{
constexpr Toolbox::f64 PitchLimit = 1.5533430342749532;
constexpr Toolbox::f64 TwoPi = 6.2831853071795865;
TResult<void> InvalidCamera_Internal()
{
	return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid debug camera input");
}
Toolbox::FVector3 Forward_Internal(const FDebugCameraPose3D& Pose)
{
	const Toolbox::f64 Horizontal = Toolbox::Cos(Pose.Pitch);
	return {static_cast<Toolbox::f32>(Toolbox::Sin(Pose.Yaw) * Horizontal),
		static_cast<Toolbox::f32>(Toolbox::Sin(Pose.Pitch)),
		static_cast<Toolbox::f32>(Toolbox::Cos(Pose.Yaw) * Horizontal)};
}
}
bool FDebugCamera3D::IsValidPose_Internal(const FDebugCameraPose3D& Pose) noexcept
{
	return Pose.Focus.IsValid() && Toolbox::Abs(Pose.Focus.X) <= 100000 &&
		Toolbox::Abs(Pose.Focus.Y) <= 100000 && Toolbox::Abs(Pose.Focus.Z) <= 100000 &&
		Toolbox::IsFinite(Pose.Distance) && Pose.Distance >= 0.1 && Pose.Distance <= 10000 &&
		Toolbox::IsFinite(Pose.Yaw) && Toolbox::Abs(Pose.Yaw) <= 1000000 &&
		Toolbox::IsFinite(Pose.Pitch) && Toolbox::Abs(Pose.Pitch) <= PitchLimit;
}
TResult<void> FDebugCamera3D::SetPose(const FDebugCameraPose3D& Pose)
{
	if (!IsValidPose_Internal(Pose))
	{
		return InvalidCamera_Internal();
	}
	// 入力値だけでなくf32へ丸めた視線も検査し、極点付近の基底消失を拒否する。
	FRenderView3D Probe;
	Probe.Target = Pose.Focus;
	Probe.Eye = Pose.Focus - Forward_Internal(Pose) * static_cast<Toolbox::f32>(Pose.Distance);
	if (!IsValidRenderView3D(Probe))
	{
		return InvalidCamera_Internal();
	}
	m_Pose = Pose;
	return {};
}
TResult<void> FDebugCamera3D::Orbit(Toolbox::f64 YawDelta, Toolbox::f64 PitchDelta)
{
	if (!Toolbox::IsFinite(YawDelta) || !Toolbox::IsFinite(PitchDelta) ||
		Toolbox::Abs(YawDelta) > 10000 || Toolbox::Abs(PitchDelta) > 10000)
	{
		return InvalidCamera_Internal();
	}
	auto Candidate = m_Pose;
	Candidate.Yaw += YawDelta;
	// 角度を有限の一周へ戻し、操作時間で精度が劣化しないようにする。
	while (Candidate.Yaw > TwoPi)
	{
		Candidate.Yaw -= TwoPi;
	}
	while (Candidate.Yaw < -TwoPi)
	{
		Candidate.Yaw += TwoPi;
	}
	Candidate.Pitch = Toolbox::Clamp(Candidate.Pitch + PitchDelta, -PitchLimit, PitchLimit);
	return SetPose(Candidate);
}
TResult<void> FDebugCamera3D::Zoom(Toolbox::f64 Factor)
{
	if (!Toolbox::IsFinite(Factor) || Factor <= 0 || Factor > 10000)
	{
		return InvalidCamera_Internal();
	}
	auto Candidate = m_Pose;
	Candidate.Distance = Toolbox::Clamp(Candidate.Distance * Factor, 0.1, 10000.0);
	return SetPose(Candidate);
}
TResult<void> FDebugCamera3D::MoveLocal(Toolbox::FVector3 Input, Toolbox::f64 RealSeconds, Toolbox::f64 Speed)
{
	if (!Input.IsValid() || Toolbox::Abs(Input.X) > 1 || Toolbox::Abs(Input.Y) > 1 ||
		Toolbox::Abs(Input.Z) > 1 || !Toolbox::IsFinite(RealSeconds) || RealSeconds < 0 || RealSeconds > 0.25 ||
		!Toolbox::IsFinite(Speed) || Speed < 0 || Speed > 1000)
	{
		return InvalidCamera_Internal();
	}
	const auto Forward = Forward_Internal(m_Pose);
	const Toolbox::FVector3 Right{static_cast<Toolbox::f32>(Toolbox::Cos(m_Pose.Yaw)), 0,
		static_cast<Toolbox::f32>(-Toolbox::Sin(m_Pose.Yaw))};
	// 上はワールド上向き。斜め入力の合成後に長さを制限する。
	auto Direction = Right * Input.X + Toolbox::FVector3{0, Input.Y, 0} + Forward * Input.Z;
	const Toolbox::f32 Length = Toolbox::Length(Direction);
	if (Length > 1)
	{
		Direction = Direction / Length;
	}
	auto Candidate = m_Pose;
	Candidate.Focus += Direction * static_cast<Toolbox::f32>(RealSeconds * Speed);
	return SetPose(Candidate);
}
TResult<FRenderView3D> FDebugCamera3D::MakeView(const FRenderView3D& Base) const
{
	auto View = Base;
	const auto Forward = Forward_Internal(m_Pose);
	View.Target = m_Pose.Focus;
	View.Eye = m_Pose.Focus - Forward * static_cast<Toolbox::f32>(m_Pose.Distance);
	View.Up = {0, 1, 0};
	if (!IsValidRenderView3D(View))
	{
		return TResult<FRenderView3D>::Failure(EErrorCode::InvalidArgument, "Invalid debug view");
	}
	return TResult<FRenderView3D>::Success(View);
}
}
