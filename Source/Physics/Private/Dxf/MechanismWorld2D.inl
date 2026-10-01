// SPDX-License-Identifier: NOASSERTION
// FImpl内でのみ使用するWorld境界。共通数学へBodyの所有は渡さない。
static PhysicsPrivate::FMechanismBody ReadMechanismBody_Internal(const FBodyRecord2D& Body)
{
	PhysicsPrivate::FMechanismBody Out;
	Out.Position = {Body.Position.X, Body.Position.Y, 0};
	Out.Velocity = {Body.Velocity.X, Body.Velocity.Y, 0};
	Out.Angular = {0, 0, Body.AngularVelocity};
	Out.Rotation = {0, 0, Toolbox::Sin(Body.Angle * 0.5), Toolbox::Cos(Body.Angle * 0.5)};
	Out.InverseInertia = {0, 0, Body.InverseInertia};
	Out.InverseMass = Body.InverseMass;
	if (Body.Type != EBodyType::Dynamic || Body.bSleeping)
	{
		Out.InverseMass = 0;
		Out.InverseInertia = {};
	}
	return Out;
}
static void WriteMechanismBody_Internal(FBodyRecord2D& Body, const PhysicsPrivate::FMechanismBody& Value, bool bPosition)
{
	// 共有Static／Kinematicと休止中Bodyにはゼロの書込もしない。
	if (Body.Type != EBodyType::Dynamic || Body.bSleeping)
	{
		return;
	}
	if (bPosition)
	{
		// Bodyの公開精度で表現できない結果を成功Stepとして書かない。
		if (!Toolbox::IsFinite(Value.Position.X) || Toolbox::Abs(Value.Position.X) > Toolbox::TNumericLimits<Toolbox::f32>::Max() || !Toolbox::IsFinite(Value.Position.Y) || Toolbox::Abs(Value.Position.Y) > Toolbox::TNumericLimits<Toolbox::f32>::Max())
		{
			throw Toolbox::FException("Mechanism position exceeds body precision");
		}
		Body.Position = {Toolbox::f32(Value.Position.X), Toolbox::f32(Value.Position.Y)};
		const Toolbox::f64 Current = Toolbox::f64(Body.Angle);
		Body.Angle = Toolbox::f32(Current + PhysicsPrivate::Principal(2 * Toolbox::Atan2(Value.Rotation.Z, Value.Rotation.W) - Current));
	}
	else
	{
		if (!Toolbox::IsFinite(Value.Velocity.X) || Toolbox::Abs(Value.Velocity.X) > Toolbox::TNumericLimits<Toolbox::f32>::Max() || !Toolbox::IsFinite(Value.Velocity.Y) || Toolbox::Abs(Value.Velocity.Y) > Toolbox::TNumericLimits<Toolbox::f32>::Max() || !Toolbox::IsFinite(Value.Angular.Z) || Toolbox::Abs(Value.Angular.Z) > Toolbox::TNumericLimits<Toolbox::f32>::Max())
		{
			throw Toolbox::FException("Mechanism velocity exceeds body precision");
		}
		Body.Velocity = {Toolbox::f32(Value.Velocity.X), Toolbox::f32(Value.Velocity.Y)};
		Body.AngularVelocity = Toolbox::f32(Value.Angular.Z);
	}
}
PhysicsPrivate::FMechanismRows MechanismRows_Internal(const FJointRecord2D& Joint, const FBodyRecord2D& A, const FBodyRecord2D& B) const
{
	return PhysicsPrivate::BuildMechanismRows(Joint.Kind, true, Joint.Mechanism, ReadMechanismBody_Internal(A), ReadMechanismBody_Internal(B), JointSlice);
}
bool MechanismNeedsWake_Internal(const FJointRecord2D& Joint, const FBodyRecord2D& A, const FBodyRecord2D& B, Toolbox::f64 LinearTolerance) const
{
	const auto Rows = MechanismRows_Internal(Joint, A, B);
	const Toolbox::f64 AngularTolerance = Toolbox::Max(Toolbox::f64(Sleep.AngularSpeedLimit) * JointSlice, 1e-6);
	for (Toolbox::size_t Index = 0; Index < 6; ++Index)
	{
		const auto& R = Rows.Values[Index];
		const Toolbox::f64 PositionTolerance = Index < 3 ? LinearTolerance : AngularTolerance;
		const Toolbox::f64 SpeedTolerance = Index < 3 ? Sleep.LinearSpeedLimit : Sleep.AngularSpeedLimit;
		if (R.bUsed && (Toolbox::Abs(R.Error) > PositionTolerance || Toolbox::Abs(PhysicsPrivate::Rate(R, ReadMechanismBody_Internal(A), ReadMechanismBody_Internal(B))) > SpeedTolerance))
		{
			return true;
		}
	}
	const Toolbox::f64 Tolerance = Joint.Kind == EJointKind::Revolute ? AngularTolerance : LinearTolerance;
	if (Rows.Values[7].bUsed && Rows.Values[7].bPosition && Toolbox::Abs(Rows.Values[7].Error) > Tolerance)
	{
		return true;
	}
	const Toolbox::f64 SpeedTolerance = Joint.Kind == EJointKind::Revolute ? Sleep.AngularSpeedLimit : Sleep.LinearSpeedLimit;
	return Joint.Mechanism.bMotor && Joint.Mechanism.Maximum > 0 && Toolbox::Abs(Rows.Rate - Joint.Mechanism.Target) > SpeedTolerance;
}
void WarmMechanism_Internal(Toolbox::size_t Slot)
{
	const auto& Joint = Joints[Slot];
	auto& Cache = JointSolveStates[Slot].Mechanism;
	auto& A = Resolve_Internal(Joint.BodyA);
	auto& B = Resolve_Internal(Joint.BodyB);
	auto WorkA = ReadMechanismBody_Internal(A);
	auto WorkB = ReadMechanismBody_Internal(B);
	const auto Rows = MechanismRows_Internal(Joint, A, B);
	if (Cache.Side != Rows.Side)
	{
		Cache.Impulses[7] = 0;
	}
	Cache.Side = Rows.Side;
	for (Toolbox::size_t Index = 0; Index < 8; ++Index)
	{
		const auto& R = Rows.Values[Index];
		Cache.Impulses[Index] = R.bUsed ? Toolbox::Clamp(Cache.Impulses[Index], R.Lower, R.Upper) : 0;
		PhysicsPrivate::ApplyRow(R, WorkA, WorkB, Cache.Impulses[Index], false);
	}
	WriteMechanismBody_Internal(A, WorkA, false);
	WriteMechanismBody_Internal(B, WorkB, false);
}
void SolveMechanism_Internal(Toolbox::size_t Slot)
{
	const auto& Joint = Joints[Slot];
	auto& A = Resolve_Internal(Joint.BodyA);
	auto& B = Resolve_Internal(Joint.BodyB);
	auto WorkA = ReadMechanismBody_Internal(A);
	auto WorkB = ReadMechanismBody_Internal(B);
	const auto Rows = MechanismRows_Internal(Joint, A, B);
	auto& Cache = JointSolveStates[Slot].Mechanism;
	for (Toolbox::size_t Index = 0; Index < 8; ++Index)
	{
		PhysicsPrivate::SolveMechanismRow(Rows.Values[Index], WorkA, WorkB, Cache.Impulses[Index]);
	}
	WriteMechanismBody_Internal(A, WorkA, false);
	WriteMechanismBody_Internal(B, WorkB, false);
}
void CorrectMechanismPositions_Internal()
{
	// Poseの修正ごとにFrame・Jacobian・有効質量を組み直す。
	for (Toolbox::size_t Slot = 0; Slot < Joints.Size(); ++Slot)
	{
		const auto& Joint = Joints[Slot];
		if (!Joint.bAlive || Joint.Kind == EJointKind::Distance)
		{
			continue;
		}
		auto& A = Resolve_Internal(Joint.BodyA);
		auto& B = Resolve_Internal(Joint.BodyB);
		for (Toolbox::uint32 Pass = 0; Pass < 8; ++Pass)
		{
			for (Toolbox::size_t Index = 0; Index < 8; ++Index)
			{
				auto WorkA = ReadMechanismBody_Internal(A);
				auto WorkB = ReadMechanismBody_Internal(B);
				const auto Rows = PhysicsPrivate::BuildMechanismRows(Joint.Kind, true, Joint.Mechanism, WorkA, WorkB, JointSlice);
				const auto& R = Rows.Values[Index];
				if (!R.bUsed || !R.bPosition)
				{
					continue;
				}
				const Toolbox::f64 K = PhysicsPrivate::EffectiveMass(R, WorkA, WorkB);
				if (K == 0)
				{
					continue;
				}
				if (!Toolbox::IsFinite(K) || K < 0)
				{
					throw Toolbox::FException("Singular mechanism position row");
				}
				const Toolbox::f64 Correction = Toolbox::Clamp(0.3 * R.Error, -0.05, 0.05);
				PhysicsPrivate::ApplyRow(R, WorkA, WorkB, -Correction / K, true);
				WriteMechanismBody_Internal(A, WorkA, true);
				WriteMechanismBody_Internal(B, WorkB, true);
			}
		}
	}
}
static void ValidateMechanism_Internal(EJointKind Kind, const PhysicsPrivate::FMechanismSettings& S)
{
	if (!Toolbox::IsFinite(S.Lower) || !Toolbox::IsFinite(S.Upper) || S.Lower > S.Upper || !Toolbox::IsFinite(S.Target) || !Toolbox::IsFinite(S.Maximum) || S.Maximum < 0)
	{
		throw Toolbox::FException("Invalid mechanism limits or drive");
	}
	if (Kind == EJointKind::Revolute && (S.Lower <= -3.14159265358979323846 || S.Upper >= 3.14159265358979323846))
	{
		throw Toolbox::FException("Revolute limit crosses principal angle boundary");
	}
}
FJointId2D CreateMechanism_Internal(EJointKind Kind, FBodyId2D IdA, FBodyId2D IdB, PhysicsPrivate::FMechanismSettings Settings)
{
	const auto& A = Resolve_Internal(IdA);
	const auto& B = Resolve_Internal(IdB);
	if (IdA == IdB || (A.Type != EBodyType::Dynamic && B.Type != EBodyType::Dynamic))
	{
		throw Toolbox::FException("Mechanism needs different bodies and a dynamic endpoint");
	}
	ValidateMechanism_Internal(Kind, Settings);
	FJointRecord2D Record;
	Record.Kind = Kind;
	Record.bAlive = true;
	Record.BodyA = IdA;
	Record.BodyB = IdB;
	Record.Mechanism = Settings;
	// 登録前にFrameの退化を調べる。失敗してもslotを公開しない。
	(void)MechanismRows_Internal(Record, A, B);
	return RegisterJoint_Internal(Record);
}
FJointRecord2D& ResolveMechanism_Internal(FJointId2D Id, EJointKind Kind)
{
	auto& Record = ResolveJoint_Internal(Id);
	if (Record.Kind != Kind)
	{
		throw Toolbox::FException("Wrong mechanism joint kind");
	}
	return Record;
}
void UpdateMechanismSettings_Internal(FJointId2D Id, EJointKind Kind, PhysicsPrivate::FMechanismSettings Value)
{
	auto& Record = ResolveMechanism_Internal(Id, Kind);
	ValidateMechanism_Internal(Kind, Value);
	auto& Old = Record.Mechanism;
	const bool bDriveChanged = Old.bMotor != Value.bMotor || Old.Target != Value.Target || Old.Maximum != Value.Maximum;
	const bool bLimitChanged = Old.bLimit != Value.bLimit || Old.Lower != Value.Lower || Old.Upper != Value.Upper;
	if (!bDriveChanged && !bLimitChanged)
	{
		return;
	}
	if (bDriveChanged)
	{
		Record.MechanismCache.Impulses[6] = 0;
	}
	if (bLimitChanged)
	{
		Record.MechanismCache.Impulses[7] = 0;
		Record.MechanismCache.Side = EJointLimitState::Disabled;
	}
	Old = Value;
	auto& A = Resolve_Internal(Record.BodyA);
	auto& B = Resolve_Internal(Record.BodyB);
	if (A.Type == EBodyType::Dynamic)
	{
		Wake_Internal(A);
	}
	if (B.Type == EBodyType::Dynamic)
	{
		Wake_Internal(B);
	}
}
