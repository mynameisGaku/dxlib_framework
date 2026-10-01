// SPDX-License-Identifier: NOASSERTION
#include "Dxf/JointTargetMotion.h"
#include "Dxf/MechanismComponentValidation.h"
namespace Dxf
{
namespace
{
// 位置の変更を行わず、減速域と残り時間から速度要求だけを得る。
Toolbox::f64 DesiredSpeed_Internal(Toolbox::f64 Error, Toolbox::f64 Maximum, Toolbox::f64 Slowdown, Toolbox::f64 Seconds)
{
	const Toolbox::f64 Magnitude = Toolbox::Min(Maximum * Toolbox::Min(Toolbox::Abs(Error) / Slowdown, 1.0), Toolbox::Abs(Error) / Seconds);
	return Error < 0 ? -Magnitude : Magnitude;
}
} // namespace
FAngularJointTargetCommand ComputeJointTargetDrive(const FAngularJointTargetSettings& Settings, Toolbox::f64 Coordinate, Toolbox::f64 Speed, const FAngularJointLimits& Limits, Toolbox::f64 Seconds)
{
	GameplayPrivate::ValidateJointLimits(Limits);
	if (!Toolbox::IsFinite(Settings.TargetAngle) || !Toolbox::IsFinite(Settings.MaxAngularSpeed) || !Toolbox::IsFinite(Settings.SlowdownAngle) || !Toolbox::IsFinite(Settings.AngleTolerance) || !Toolbox::IsFinite(Settings.AngularSpeedTolerance) || !Toolbox::IsFinite(Settings.MaxTorque) || !Toolbox::IsFinite(Coordinate) || !Toolbox::IsFinite(Speed) || !Toolbox::IsFinite(Seconds) || Seconds <= 0 || Settings.MaxAngularSpeed < 0 || Settings.SlowdownAngle <= 0 || Settings.AngleTolerance < 0 || Settings.AngularSpeedTolerance < 0 || Settings.MaxTorque < 0)
	{
		throw Toolbox::FException("Invalid joint target motion input");
	}
	if (Limits.bEnabled && (Settings.TargetAngle < Limits.LowerAngle || Settings.TargetAngle > Limits.UpperAngle))
	{
		throw Toolbox::FException("Joint target is outside limits");
	}
	if (Settings.TargetAngle <= -3.14159265358979323846 || Settings.TargetAngle >= 3.14159265358979323846 || Coordinate <= -3.14159265358979323846 || Coordinate > 3.14159265358979323846)
	{
		throw Toolbox::FException("Joint angular target crosses principal boundary");
	}
	// 位置と速度の両条件を満たした場合だけ到達扱いにする。
	const Toolbox::f64 Error = Settings.TargetAngle - Coordinate;
	const bool bPositionReached = Toolbox::Abs(Error) <= Settings.AngleTolerance;
	FAngularJointTargetCommand Command;
	Command.State = bPositionReached ? (Toolbox::Abs(Speed) <= Settings.AngularSpeedTolerance ? EJointTargetState::Reached : EJointTargetState::Stopping) : (Settings.MaxAngularSpeed > 0 ? EJointTargetState::Driving : EJointTargetState::Stopping);
	Command.Drive = {true, bPositionReached ? 0 : DesiredSpeed_Internal(Error, Settings.MaxAngularSpeed, Settings.SlowdownAngle, Seconds), Settings.MaxTorque};
	return Command;
}
FLinearJointTargetCommand ComputeJointTargetDrive(const FLinearJointTargetSettings& Settings, Toolbox::f64 Coordinate, Toolbox::f64 Speed, const FLinearJointLimits& Limits, Toolbox::f64 Seconds)
{
	GameplayPrivate::ValidateJointLimits(Limits);
	if (!Toolbox::IsFinite(Settings.TargetTranslation) || !Toolbox::IsFinite(Settings.MaxSpeed) || !Toolbox::IsFinite(Settings.SlowdownDistance) || !Toolbox::IsFinite(Settings.PositionTolerance) || !Toolbox::IsFinite(Settings.SpeedTolerance) || !Toolbox::IsFinite(Settings.MaxForce) || !Toolbox::IsFinite(Coordinate) || !Toolbox::IsFinite(Speed) || !Toolbox::IsFinite(Seconds) || Seconds <= 0 || Settings.MaxSpeed < 0 || Settings.SlowdownDistance <= 0 || Settings.PositionTolerance < 0 || Settings.SpeedTolerance < 0 || Settings.MaxForce < 0)
	{
		throw Toolbox::FException("Invalid joint target motion input");
	}
	if (Limits.bEnabled && (Settings.TargetTranslation < Limits.LowerTranslation || Settings.TargetTranslation > Limits.UpperTranslation))
	{
		throw Toolbox::FException("Joint target is outside limits");
	}
	// 位置と速度の両条件を満たした場合だけ到達扱いにする。
	const Toolbox::f64 Error = Settings.TargetTranslation - Coordinate;
	const bool bPositionReached = Toolbox::Abs(Error) <= Settings.PositionTolerance;
	FLinearJointTargetCommand Command;
	Command.State = bPositionReached ? (Toolbox::Abs(Speed) <= Settings.SpeedTolerance ? EJointTargetState::Reached : EJointTargetState::Stopping) : (Settings.MaxSpeed > 0 ? EJointTargetState::Driving : EJointTargetState::Stopping);
	Command.Drive = {true, bPositionReached ? 0 : DesiredSpeed_Internal(Error, Settings.MaxSpeed, Settings.SlowdownDistance, Seconds), Settings.MaxForce};
	return Command;
}
} // namespace Dxf
