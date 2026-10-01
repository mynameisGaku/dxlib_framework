// SPDX-License-Identifier: NOASSERTION
static PhysicsPrivate::FMechanismSettings ConvertMechanismFrames_Internal(const FJointFrame2D& A, const FJointFrame2D& B)
{
	if (!A.LocalAnchor.IsValid() || !B.LocalAnchor.IsValid())
	{
		throw Toolbox::FException("Invalid mechanism local anchor");
	}
	PhysicsPrivate::FMechanismSettings S;
	if (!Toolbox::IsFinite(A.LocalAngle) || !Toolbox::IsFinite(B.LocalAngle))
	{
		throw Toolbox::FException("Invalid mechanism frame angle");
	}
	S.AnchorA = {A.LocalAnchor.X, A.LocalAnchor.Y, 0};
	S.AnchorB = {B.LocalAnchor.X, B.LocalAnchor.Y, 0};
	S.RotationA = {0, 0, Toolbox::Sin(A.LocalAngle * 0.5), Toolbox::Cos(A.LocalAngle * 0.5)};
	S.RotationB = {0, 0, Toolbox::Sin(B.LocalAngle * 0.5), Toolbox::Cos(B.LocalAngle * 0.5)};
	return S;
}
EJointKind FPhysicsWorld2D::GetJointKind(FJointId2D Id) const
{
	return m_pImpl->ResolveJoint_Internal(Id).Kind;
}
FJointId2D FPhysicsWorld2D::CreateRevoluteJoint(FBodyId2D A, FBodyId2D B, const FRevoluteJointDescription2D& Description)
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
FRevoluteJointState2D FPhysicsWorld2D::GetRevoluteJoint(FJointId2D Id) const
{
	const auto& J = m_pImpl->ResolveMechanism_Internal(Id, EJointKind::Revolute);
	const auto Rows = m_pImpl->MechanismRows_Internal(J, m_pImpl->Resolve_Internal(J.BodyA), m_pImpl->Resolve_Internal(J.BodyB));
	FRevoluteJointState2D State;
	State.AnchorError = Rows.AnchorError;
	State.AxisAlignmentError = Rows.AngularError;
	State.Angle = Rows.Coordinate;
	State.AngularSpeed = Rows.Rate;
	State.LimitState = Rows.Side;
	State.Drive = {J.Mechanism.bMotor, J.Mechanism.Target, J.Mechanism.Maximum};
	return State;
}
FRevoluteJointDescription2D FPhysicsWorld2D::MakeRevoluteJointDescription(FBodyId2D A, FBodyId2D B, Toolbox::FVector2 Anchor, Toolbox::f64 Angle) const
{
	if (!Anchor.IsValid())
	{
		throw Toolbox::FException("Invalid world joint anchor");
	}
	FRevoluteJointDescription2D Out;
	const auto PoseA = FImpl::ReadMechanismBody_Internal(m_pImpl->Resolve_Internal(A));
	const auto PoseB = FImpl::ReadMechanismBody_Internal(m_pImpl->Resolve_Internal(B));
	if (!Toolbox::IsFinite(Angle))
	{
		throw Toolbox::FException("Invalid world joint angle");
	}
	const PhysicsPrivate::FMechanismVector World{Anchor.X, Anchor.Y, 0};
	const auto LocalA = PhysicsPrivate::Rotate(PhysicsPrivate::Conjugate(PoseA.Rotation), PhysicsPrivate::Subtract(World, PoseA.Position));
	const auto LocalB = PhysicsPrivate::Rotate(PhysicsPrivate::Conjugate(PoseB.Rotation), PhysicsPrivate::Subtract(World, PoseB.Position));
	Out.FrameA.LocalAnchor = {Toolbox::f32(LocalA.X), Toolbox::f32(LocalA.Y)};
	Out.FrameB.LocalAnchor = {Toolbox::f32(LocalB.X), Toolbox::f32(LocalB.Y)};
	Out.FrameA.LocalAngle = static_cast<Toolbox::f32>(PhysicsPrivate::Principal(Angle - GetAngle(A)));
	Out.FrameB.LocalAngle = static_cast<Toolbox::f32>(PhysicsPrivate::Principal(Angle - GetAngle(B)));
	// 公開float精度へ写したLocal Frameも検証し、表現不能な差を返さない。
	(void)ConvertMechanismFrames_Internal(Out.FrameA, Out.FrameB);
	return Out;
}
void FPhysicsWorld2D::SetRevoluteJointDrive(FJointId2D Id, const FAngularJointDrive& Value)
{
	auto S = m_pImpl->ResolveMechanism_Internal(Id, EJointKind::Revolute).Mechanism;
	S.bMotor = Value.bEnabled;
	S.Target = Value.TargetAngularSpeed;
	S.Maximum = Value.MaxTorque;
	m_pImpl->UpdateMechanismSettings_Internal(Id, EJointKind::Revolute, S);
}
void FPhysicsWorld2D::SetRevoluteJointLimits(FJointId2D Id, const FAngularJointLimits& Value)
{
	auto S = m_pImpl->ResolveMechanism_Internal(Id, EJointKind::Revolute).Mechanism;
	S.bLimit = Value.bEnabled;
	S.Lower = Value.LowerAngle;
	S.Upper = Value.UpperAngle;
	m_pImpl->UpdateMechanismSettings_Internal(Id, EJointKind::Revolute, S);
}
FJointId2D FPhysicsWorld2D::CreateFixedJoint(FBodyId2D A, FBodyId2D B, const FFixedJointDescription2D& Description)
{
	auto S = ConvertMechanismFrames_Internal(Description.FrameA, Description.FrameB);
	return m_pImpl->CreateMechanism_Internal(EJointKind::Fixed, A, B, S);
}
FFixedJointState2D FPhysicsWorld2D::GetFixedJoint(FJointId2D Id) const
{
	const auto& J = m_pImpl->ResolveMechanism_Internal(Id, EJointKind::Fixed);
	const auto Rows = m_pImpl->MechanismRows_Internal(J, m_pImpl->Resolve_Internal(J.BodyA), m_pImpl->Resolve_Internal(J.BodyB));
	FFixedJointState2D State;
	State.AnchorError = Rows.AnchorError;
	State.OrientationError = Rows.AngularError;
	return State;
}
FFixedJointDescription2D FPhysicsWorld2D::MakeFixedJointDescription(FBodyId2D A, FBodyId2D B, Toolbox::FVector2 Anchor, Toolbox::f64 Angle) const
{
	if (!Anchor.IsValid())
	{
		throw Toolbox::FException("Invalid world joint anchor");
	}
	FFixedJointDescription2D Out;
	const auto PoseA = FImpl::ReadMechanismBody_Internal(m_pImpl->Resolve_Internal(A));
	const auto PoseB = FImpl::ReadMechanismBody_Internal(m_pImpl->Resolve_Internal(B));
	if (!Toolbox::IsFinite(Angle))
	{
		throw Toolbox::FException("Invalid world joint angle");
	}
	const PhysicsPrivate::FMechanismVector World{Anchor.X, Anchor.Y, 0};
	const auto LocalA = PhysicsPrivate::Rotate(PhysicsPrivate::Conjugate(PoseA.Rotation), PhysicsPrivate::Subtract(World, PoseA.Position));
	const auto LocalB = PhysicsPrivate::Rotate(PhysicsPrivate::Conjugate(PoseB.Rotation), PhysicsPrivate::Subtract(World, PoseB.Position));
	Out.FrameA.LocalAnchor = {Toolbox::f32(LocalA.X), Toolbox::f32(LocalA.Y)};
	Out.FrameB.LocalAnchor = {Toolbox::f32(LocalB.X), Toolbox::f32(LocalB.Y)};
	Out.FrameA.LocalAngle = static_cast<Toolbox::f32>(PhysicsPrivate::Principal(Angle - GetAngle(A)));
	Out.FrameB.LocalAngle = static_cast<Toolbox::f32>(PhysicsPrivate::Principal(Angle - GetAngle(B)));
	// 公開float精度へ写したLocal Frameも検証し、表現不能な差を返さない。
	(void)ConvertMechanismFrames_Internal(Out.FrameA, Out.FrameB);
	return Out;
}
FJointId2D FPhysicsWorld2D::CreatePrismaticJoint(FBodyId2D A, FBodyId2D B, const FPrismaticJointDescription2D& Description)
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
FPrismaticJointState2D FPhysicsWorld2D::GetPrismaticJoint(FJointId2D Id) const
{
	const auto& J = m_pImpl->ResolveMechanism_Internal(Id, EJointKind::Prismatic);
	const auto Rows = m_pImpl->MechanismRows_Internal(J, m_pImpl->Resolve_Internal(J.BodyA), m_pImpl->Resolve_Internal(J.BodyB));
	FPrismaticJointState2D State;
	State.AnchorError = Rows.AnchorError;
	State.OrientationError = Rows.AngularError;
	State.Translation = Rows.Coordinate;
	State.TranslationRate = Rows.Rate;
	State.LimitState = Rows.Side;
	State.Drive = {J.Mechanism.bMotor, J.Mechanism.Target, J.Mechanism.Maximum};
	return State;
}
FPrismaticJointDescription2D FPhysicsWorld2D::MakePrismaticJointDescription(FBodyId2D A, FBodyId2D B, Toolbox::FVector2 Anchor, Toolbox::f64 Angle) const
{
	if (!Anchor.IsValid())
	{
		throw Toolbox::FException("Invalid world joint anchor");
	}
	FPrismaticJointDescription2D Out;
	const auto PoseA = FImpl::ReadMechanismBody_Internal(m_pImpl->Resolve_Internal(A));
	const auto PoseB = FImpl::ReadMechanismBody_Internal(m_pImpl->Resolve_Internal(B));
	if (!Toolbox::IsFinite(Angle))
	{
		throw Toolbox::FException("Invalid world joint angle");
	}
	const PhysicsPrivate::FMechanismVector World{Anchor.X, Anchor.Y, 0};
	const auto LocalA = PhysicsPrivate::Rotate(PhysicsPrivate::Conjugate(PoseA.Rotation), PhysicsPrivate::Subtract(World, PoseA.Position));
	const auto LocalB = PhysicsPrivate::Rotate(PhysicsPrivate::Conjugate(PoseB.Rotation), PhysicsPrivate::Subtract(World, PoseB.Position));
	Out.FrameA.LocalAnchor = {Toolbox::f32(LocalA.X), Toolbox::f32(LocalA.Y)};
	Out.FrameB.LocalAnchor = {Toolbox::f32(LocalB.X), Toolbox::f32(LocalB.Y)};
	Out.FrameA.LocalAngle = static_cast<Toolbox::f32>(PhysicsPrivate::Principal(Angle - GetAngle(A)));
	Out.FrameB.LocalAngle = static_cast<Toolbox::f32>(PhysicsPrivate::Principal(Angle - GetAngle(B)));
	// 公開float精度へ写したLocal Frameも検証し、表現不能な差を返さない。
	(void)ConvertMechanismFrames_Internal(Out.FrameA, Out.FrameB);
	return Out;
}
void FPhysicsWorld2D::SetPrismaticJointDrive(FJointId2D Id, const FLinearJointDrive& Value)
{
	auto S = m_pImpl->ResolveMechanism_Internal(Id, EJointKind::Prismatic).Mechanism;
	S.bMotor = Value.bEnabled;
	S.Target = Value.TargetSpeed;
	S.Maximum = Value.MaxForce;
	m_pImpl->UpdateMechanismSettings_Internal(Id, EJointKind::Prismatic, S);
}
void FPhysicsWorld2D::SetPrismaticJointLimits(FJointId2D Id, const FLinearJointLimits& Value)
{
	auto S = m_pImpl->ResolveMechanism_Internal(Id, EJointKind::Prismatic).Mechanism;
	S.bLimit = Value.bEnabled;
	S.Lower = Value.LowerTranslation;
	S.Upper = Value.UpperTranslation;
	m_pImpl->UpdateMechanismSettings_Internal(Id, EJointKind::Prismatic, S);
}
