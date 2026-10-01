// SPDX-License-Identifier: NOASSERTION
#include "Dxf/MechanismComponentValidation.h"
namespace Dxf::GameplayPrivate
{
void ValidateJointFrame(const FJointFrame2D& Value)
{
	if (!Value.LocalAnchor.IsValid() || !Toolbox::IsFinite(Value.LocalAngle))
	{
		throw Toolbox::FException("Invalid joint component frame");
	}
}
bool SameJointFrame(const FJointFrame2D& A, const FJointFrame2D& B) noexcept
{
	return A.LocalAnchor == B.LocalAnchor && A.LocalAngle == B.LocalAngle;
}
void ValidateJointFrame(const FJointFrame3D& Value)
{
	if (!Value.LocalAnchor.IsValid())
	{
		throw Toolbox::FException("Invalid joint component anchor");
	}
	(void)Value.LocalRotation.Normalized();
}
bool SameJointFrame(const FJointFrame3D& A, const FJointFrame3D& B) noexcept
{
	// 入力は検証済み。正規化後のqと-qを同じ回転として扱う。
	const auto Q = A.LocalRotation.Normalized();
	const auto R = B.LocalRotation.Normalized();
	const bool bSame = Q.X == R.X && Q.Y == R.Y && Q.Z == R.Z && Q.W == R.W;
	const bool bOpposite = Q.X == -R.X && Q.Y == -R.Y && Q.Z == -R.Z && Q.W == -R.W;
	return A.LocalAnchor == B.LocalAnchor && (bSame || bOpposite);
}
void ValidateJointLimits(const FAngularJointLimits& Value)
{
	if (!Toolbox::IsFinite(Value.LowerAngle) || !Toolbox::IsFinite(Value.UpperAngle) || Value.LowerAngle > Value.UpperAngle || Value.LowerAngle <= -3.14159265358979323846 || Value.UpperAngle >= 3.14159265358979323846)
	{
		throw Toolbox::FException("Invalid joint component limits or drive");
	}
}
void ValidateJointDrive(const FAngularJointDrive& Value)
{
	if (!Toolbox::IsFinite(Value.TargetAngularSpeed) || !Toolbox::IsFinite(Value.MaxTorque) || Value.MaxTorque < 0)
	{
		throw Toolbox::FException("Invalid joint component limits or drive");
	}
}
void ValidateJointLimits(const FLinearJointLimits& Value)
{
	if (!Toolbox::IsFinite(Value.LowerTranslation) || !Toolbox::IsFinite(Value.UpperTranslation) || Value.LowerTranslation > Value.UpperTranslation)
	{
		throw Toolbox::FException("Invalid joint component limits or drive");
	}
}
void ValidateJointDrive(const FLinearJointDrive& Value)
{
	if (!Toolbox::IsFinite(Value.TargetSpeed) || !Toolbox::IsFinite(Value.MaxForce) || Value.MaxForce < 0)
	{
		throw Toolbox::FException("Invalid joint component limits or drive");
	}
}
Toolbox::FVector2 RotateJointAnchor(Toolbox::f32 Angle, Toolbox::FVector2 Local)
{
	return {static_cast<Toolbox::f32>(Toolbox::Cos(Angle) * Local.X - Toolbox::Sin(Angle) * Local.Y), static_cast<Toolbox::f32>(Toolbox::Sin(Angle) * Local.X + Toolbox::Cos(Angle) * Local.Y)};
}
} // namespace Dxf::GameplayPrivate
