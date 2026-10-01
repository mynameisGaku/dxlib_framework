// SPDX-License-Identifier: NOASSERTION
static PhysicsPrivate::FMechanismSettings ConvertMechanismFrames_Internal(const FJointFrame3D& A, const FJointFrame3D& B)
{
	if (!A.LocalAnchor.IsValid() || !B.LocalAnchor.IsValid())
	{
		throw Toolbox::FException("Invalid mechanism local anchor");
	}
	PhysicsPrivate::FMechanismSettings S;
	S.AnchorA = {A.LocalAnchor.X, A.LocalAnchor.Y, A.LocalAnchor.Z};
	S.AnchorB = {B.LocalAnchor.X, B.LocalAnchor.Y, B.LocalAnchor.Z};
	S.RotationA = PhysicsPrivate::Normalize({A.LocalRotation.X, A.LocalRotation.Y, A.LocalRotation.Z, A.LocalRotation.W});
	S.RotationB = PhysicsPrivate::Normalize({B.LocalRotation.X, B.LocalRotation.Y, B.LocalRotation.Z, B.LocalRotation.W});
	return S;
}
EJointKind FPhysicsWorld3D::GetJointKind(FJointId3D Id) const
{
	return m_pImpl->ResolveJoint_Internal(Id).Kind;
}
FJointId3D FPhysicsWorld3D::CreateRevoluteJoint(FBodyId3D A, FBodyId3D B, const FRevoluteJointDescription3D& Description)
{
	auto S = ConvertMechanismFrames_Internal(Description.FrameA, Description.FrameB);
	S.bLimit = Description.Limits.bEnabled;
	S.Lower = Description.Limits.LowerAngle;
	S.Upper = Description.Limits.UpperAngle;
	S.bMotor = Description.Drive.bEnabled;
	S.Target = Description.Drive.TargetAngularSpeed;
	S.Maximum = Description.Drive.MaxTorque;
	return m_pImpl->CreateMechanism_Internal(EJointKind::Revolute, A, B, S);
}
FRevoluteJointState3D FPhysicsWorld3D::GetRevoluteJoint(FJointId3D Id) const
{
	const auto& J = m_pImpl->ResolveMechanism_Internal(Id, EJointKind::Revolute);
	const auto Rows = m_pImpl->MechanismRows_Internal(J, m_pImpl->Resolve_Internal(J.BodyA), m_pImpl->Resolve_Internal(J.BodyB));
	FRevoluteJointState3D State;
	State.AnchorError = Rows.AnchorError;
	State.AxisAlignmentError = Rows.AngularError;
	State.Angle = Rows.Coordinate;
	State.AngularSpeed = Rows.Rate;
	State.LimitState = Rows.Side;
	State.Drive = {J.Mechanism.bMotor, J.Mechanism.Target, J.Mechanism.Maximum};
	return State;
}
FRevoluteJointDescription3D FPhysicsWorld3D::MakeRevoluteJointDescription(FBodyId3D A, FBodyId3D B, Toolbox::FVector3 Anchor, Toolbox::FQuaternion Rotation) const
{
	if (!Anchor.IsValid())
	{
		throw Toolbox::FException("Invalid world joint anchor");
	}
	FRevoluteJointDescription3D Out;
	const auto PoseA = FImpl::ReadMechanismBody_Internal(m_pImpl->Resolve_Internal(A));
	const auto PoseB = FImpl::ReadMechanismBody_Internal(m_pImpl->Resolve_Internal(B));
	const auto Q = PhysicsPrivate::Normalize({Rotation.X, Rotation.Y, Rotation.Z, Rotation.W});
	const PhysicsPrivate::FMechanismVector World{Anchor.X, Anchor.Y, Anchor.Z};
	const auto LocalA = PhysicsPrivate::Rotate(PhysicsPrivate::Conjugate(PoseA.Rotation), PhysicsPrivate::Subtract(World, PoseA.Position));
	const auto LocalB = PhysicsPrivate::Rotate(PhysicsPrivate::Conjugate(PoseB.Rotation), PhysicsPrivate::Subtract(World, PoseB.Position));
	const auto QA = PhysicsPrivate::Multiply(PhysicsPrivate::Conjugate(PoseA.Rotation), Q);
	const auto QB = PhysicsPrivate::Multiply(PhysicsPrivate::Conjugate(PoseB.Rotation), Q);
	Out.FrameA.LocalAnchor = {Toolbox::f32(LocalA.X), Toolbox::f32(LocalA.Y), Toolbox::f32(LocalA.Z)};
	Out.FrameB.LocalAnchor = {Toolbox::f32(LocalB.X), Toolbox::f32(LocalB.Y), Toolbox::f32(LocalB.Z)};
	Out.FrameA.LocalRotation = {Toolbox::f32(QA.X), Toolbox::f32(QA.Y), Toolbox::f32(QA.Z), Toolbox::f32(QA.W)};
	Out.FrameB.LocalRotation = {Toolbox::f32(QB.X), Toolbox::f32(QB.Y), Toolbox::f32(QB.Z), Toolbox::f32(QB.W)};
	// 公開float精度へ写したLocal Frameも検証し、表現不能な差を返さない。
	(void)ConvertMechanismFrames_Internal(Out.FrameA, Out.FrameB);
	return Out;
}
void FPhysicsWorld3D::SetRevoluteJointDrive(FJointId3D Id, const FAngularJointDrive& Value)
{
	auto S = m_pImpl->ResolveMechanism_Internal(Id, EJointKind::Revolute).Mechanism;
	S.bMotor = Value.bEnabled;
	S.Target = Value.TargetAngularSpeed;
	S.Maximum = Value.MaxTorque;
	m_pImpl->UpdateMechanismSettings_Internal(Id, EJointKind::Revolute, S);
}
void FPhysicsWorld3D::SetRevoluteJointLimits(FJointId3D Id, const FAngularJointLimits& Value)
{
	auto S = m_pImpl->ResolveMechanism_Internal(Id, EJointKind::Revolute).Mechanism;
	S.bLimit = Value.bEnabled;
	S.Lower = Value.LowerAngle;
	S.Upper = Value.UpperAngle;
	m_pImpl->UpdateMechanismSettings_Internal(Id, EJointKind::Revolute, S);
}
FJointId3D FPhysicsWorld3D::CreateFixedJoint(FBodyId3D A, FBodyId3D B, const FFixedJointDescription3D& Description)
{
	auto S = ConvertMechanismFrames_Internal(Description.FrameA, Description.FrameB);
	return m_pImpl->CreateMechanism_Internal(EJointKind::Fixed, A, B, S);
}
FFixedJointState3D FPhysicsWorld3D::GetFixedJoint(FJointId3D Id) const
{
	const auto& J = m_pImpl->ResolveMechanism_Internal(Id, EJointKind::Fixed);
	const auto Rows = m_pImpl->MechanismRows_Internal(J, m_pImpl->Resolve_Internal(J.BodyA), m_pImpl->Resolve_Internal(J.BodyB));
	FFixedJointState3D State;
	State.AnchorError = Rows.AnchorError;
	State.OrientationError = Rows.AngularError;
	return State;
}
FFixedJointDescription3D FPhysicsWorld3D::MakeFixedJointDescription(FBodyId3D A, FBodyId3D B, Toolbox::FVector3 Anchor, Toolbox::FQuaternion Rotation) const
{
	if (!Anchor.IsValid())
	{
		throw Toolbox::FException("Invalid world joint anchor");
	}
	FFixedJointDescription3D Out;
	const auto PoseA = FImpl::ReadMechanismBody_Internal(m_pImpl->Resolve_Internal(A));
	const auto PoseB = FImpl::ReadMechanismBody_Internal(m_pImpl->Resolve_Internal(B));
	const auto Q = PhysicsPrivate::Normalize({Rotation.X, Rotation.Y, Rotation.Z, Rotation.W});
	const PhysicsPrivate::FMechanismVector World{Anchor.X, Anchor.Y, Anchor.Z};
	const auto LocalA = PhysicsPrivate::Rotate(PhysicsPrivate::Conjugate(PoseA.Rotation), PhysicsPrivate::Subtract(World, PoseA.Position));
	const auto LocalB = PhysicsPrivate::Rotate(PhysicsPrivate::Conjugate(PoseB.Rotation), PhysicsPrivate::Subtract(World, PoseB.Position));
	const auto QA = PhysicsPrivate::Multiply(PhysicsPrivate::Conjugate(PoseA.Rotation), Q);
	const auto QB = PhysicsPrivate::Multiply(PhysicsPrivate::Conjugate(PoseB.Rotation), Q);
	Out.FrameA.LocalAnchor = {Toolbox::f32(LocalA.X), Toolbox::f32(LocalA.Y), Toolbox::f32(LocalA.Z)};
	Out.FrameB.LocalAnchor = {Toolbox::f32(LocalB.X), Toolbox::f32(LocalB.Y), Toolbox::f32(LocalB.Z)};
	Out.FrameA.LocalRotation = {Toolbox::f32(QA.X), Toolbox::f32(QA.Y), Toolbox::f32(QA.Z), Toolbox::f32(QA.W)};
	Out.FrameB.LocalRotation = {Toolbox::f32(QB.X), Toolbox::f32(QB.Y), Toolbox::f32(QB.Z), Toolbox::f32(QB.W)};
	// 公開float精度へ写したLocal Frameも検証し、表現不能な差を返さない。
	(void)ConvertMechanismFrames_Internal(Out.FrameA, Out.FrameB);
	return Out;
}
FJointId3D FPhysicsWorld3D::CreatePrismaticJoint(FBodyId3D A, FBodyId3D B, const FPrismaticJointDescription3D& Description)
{
	auto S = ConvertMechanismFrames_Internal(Description.FrameA, Description.FrameB);
	S.bLimit = Description.Limits.bEnabled;
	S.Lower = Description.Limits.LowerTranslation;
	S.Upper = Description.Limits.UpperTranslation;
	S.bMotor = Description.Drive.bEnabled;
	S.Target = Description.Drive.TargetSpeed;
	S.Maximum = Description.Drive.MaxForce;
	return m_pImpl->CreateMechanism_Internal(EJointKind::Prismatic, A, B, S);
}
FPrismaticJointState3D FPhysicsWorld3D::GetPrismaticJoint(FJointId3D Id) const
{
	const auto& J = m_pImpl->ResolveMechanism_Internal(Id, EJointKind::Prismatic);
	const auto Rows = m_pImpl->MechanismRows_Internal(J, m_pImpl->Resolve_Internal(J.BodyA), m_pImpl->Resolve_Internal(J.BodyB));
	FPrismaticJointState3D State;
	State.AnchorError = Rows.AnchorError;
	State.OrientationError = Rows.AngularError;
	State.Translation = Rows.Coordinate;
	State.TranslationRate = Rows.Rate;
	State.LimitState = Rows.Side;
	State.Drive = {J.Mechanism.bMotor, J.Mechanism.Target, J.Mechanism.Maximum};
	return State;
}
FPrismaticJointDescription3D FPhysicsWorld3D::MakePrismaticJointDescription(FBodyId3D A, FBodyId3D B, Toolbox::FVector3 Anchor, Toolbox::FQuaternion Rotation) const
{
	if (!Anchor.IsValid())
	{
		throw Toolbox::FException("Invalid world joint anchor");
	}
	FPrismaticJointDescription3D Out;
	const auto PoseA = FImpl::ReadMechanismBody_Internal(m_pImpl->Resolve_Internal(A));
	const auto PoseB = FImpl::ReadMechanismBody_Internal(m_pImpl->Resolve_Internal(B));
	const auto Q = PhysicsPrivate::Normalize({Rotation.X, Rotation.Y, Rotation.Z, Rotation.W});
	const PhysicsPrivate::FMechanismVector World{Anchor.X, Anchor.Y, Anchor.Z};
	const auto LocalA = PhysicsPrivate::Rotate(PhysicsPrivate::Conjugate(PoseA.Rotation), PhysicsPrivate::Subtract(World, PoseA.Position));
	const auto LocalB = PhysicsPrivate::Rotate(PhysicsPrivate::Conjugate(PoseB.Rotation), PhysicsPrivate::Subtract(World, PoseB.Position));
	const auto QA = PhysicsPrivate::Multiply(PhysicsPrivate::Conjugate(PoseA.Rotation), Q);
	const auto QB = PhysicsPrivate::Multiply(PhysicsPrivate::Conjugate(PoseB.Rotation), Q);
	Out.FrameA.LocalAnchor = {Toolbox::f32(LocalA.X), Toolbox::f32(LocalA.Y), Toolbox::f32(LocalA.Z)};
	Out.FrameB.LocalAnchor = {Toolbox::f32(LocalB.X), Toolbox::f32(LocalB.Y), Toolbox::f32(LocalB.Z)};
	Out.FrameA.LocalRotation = {Toolbox::f32(QA.X), Toolbox::f32(QA.Y), Toolbox::f32(QA.Z), Toolbox::f32(QA.W)};
	Out.FrameB.LocalRotation = {Toolbox::f32(QB.X), Toolbox::f32(QB.Y), Toolbox::f32(QB.Z), Toolbox::f32(QB.W)};
	// 公開float精度へ写したLocal Frameも検証し、表現不能な差を返さない。
	(void)ConvertMechanismFrames_Internal(Out.FrameA, Out.FrameB);
	return Out;
}
void FPhysicsWorld3D::SetPrismaticJointDrive(FJointId3D Id, const FLinearJointDrive& Value)
{
	auto S = m_pImpl->ResolveMechanism_Internal(Id, EJointKind::Prismatic).Mechanism;
	S.bMotor = Value.bEnabled;
	S.Target = Value.TargetSpeed;
	S.Maximum = Value.MaxForce;
	m_pImpl->UpdateMechanismSettings_Internal(Id, EJointKind::Prismatic, S);
}
void FPhysicsWorld3D::SetPrismaticJointLimits(FJointId3D Id, const FLinearJointLimits& Value)
{
	auto S = m_pImpl->ResolveMechanism_Internal(Id, EJointKind::Prismatic).Mechanism;
	S.bLimit = Value.bEnabled;
	S.Lower = Value.LowerTranslation;
	S.Upper = Value.UpperTranslation;
	m_pImpl->UpdateMechanismSettings_Internal(Id, EJointKind::Prismatic, S);
}
