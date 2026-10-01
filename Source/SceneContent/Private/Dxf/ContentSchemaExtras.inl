// SPDX-License-Identifier: NOASSERTION
namespace Dxf::ContentPrivate
{
// 表示は資源型と次元を事前に検査する。生成時に代替画像へ逃げない。
template <typename T>
void ReadVisual(FSchemaReader& R, Toolbox::int32 Node, typename T::FPart& Part, const typename T::FDefinition& Definition)
{
	R.Fields(Node, {"kind", "halfExtents", "radius", "color", "asset", "font", "label", "layer", "scale", "animation", "material"});
	auto& V = Part.Visual;
	const auto Kind = R.String(R.Required(Node, "kind"));
	if (Kind == "Box")
	{
		V.Kind = EContentVisualKind::Box;
	}
	else if (Kind == "Sphere" || (T::Dimension == 2 && Kind == "Circle"))
	{
		V.Kind = EContentVisualKind::Sphere;
	}
	else if (Kind == "Texture" && T::Dimension == 2)
	{
		V.Kind = EContentVisualKind::Texture;
		V.Asset = AssetIndex(R, R.Required(Node, "asset"), Definition, EContentAssetKind::Texture);
	}
	else if (Kind == "Model" && T::Dimension == 3)
	{
		V.Kind = EContentVisualKind::Model;
		V.Asset = AssetIndex(R, R.Required(Node, "asset"), Definition, EContentAssetKind::Model);
	}
	else
	{
		R.Fail(Node, "Unknown or wrong-dimension visual kind");
	}
	const auto Extents = R.Find(Node, "halfExtents");
	if ((V.Kind == EContentVisualKind::Sphere && Extents >= 0) || (V.Kind != EContentVisualKind::Sphere && R.Find(Node, "radius") >= 0) || ((V.Kind == EContentVisualKind::Box || V.Kind == EContentVisualKind::Sphere) && R.Find(Node, "asset") >= 0))
	{
		R.Fail(Node, "Fields do not apply to this visual kind");
	}
	if (Extents >= 0)
	{
		if constexpr (T::Dimension == 2)
		{
			const auto E = R.Vector2(Extents);
			V.HalfExtents = {E.X, E.Y, 0.5f};
		}
		else
		{
			V.HalfExtents = R.Vector3(Extents);
		}
	}
	const auto Radius = R.Find(Node, "radius");
	if (Radius >= 0)
	{
		V.Radius = R.Scalar(Radius);
	}
	const auto Color = R.Find(Node, "color");
	if (Color >= 0)
	{
		V.Color = R.Color(Color);
	}
	if (V.HalfExtents.X <= 0 || V.HalfExtents.Y <= 0 || V.HalfExtents.Z <= 0 || V.Radius <= 0)
	{
		R.Fail(Node, "Invalid visual dimensions");
	}
	const auto Font = R.Find(Node, "font");
	if (Font >= 0)
	{
		V.Font = AssetIndex(R, Font, Definition, EContentAssetKind::Font);
		V.Label = R.String(R.Required(Node, "label"));
	}
	else if (R.Find(Node, "label") >= 0)
	{
		R.Fail(Node, "World label requires a Font");
	}
	const auto Layer = R.Find(Node, "layer");
	if (Layer >= 0)
	{
		for (const char C : R.Get(Layer).Text)
		{
			if (C == '.' || C == 'e' || C == 'E')
			{
				R.Fail(Layer, "Drawing layer requires an integer token");
			}
		}
		const auto Value = R.Number(Layer);
		if (Toolbox::Floor(Value) != Value || Value < -2147483648.0 || Value > 2147483647.0)
		{
			R.Fail(Layer, "Invalid drawing layer");
		}
		V.Layer = static_cast<Toolbox::int32>(Value);
	}
	const auto Scale = R.Find(Node, "scale");
	const auto Animation = R.Find(Node, "animation");
	const auto Material = R.Find(Node, "material");
	if (V.Kind != EContentVisualKind::Model && (Scale >= 0 || Animation >= 0 || Material >= 0))
	{
		R.Fail(Node, "Scale, animation and material require Model");
	}
	if (Scale >= 0)
	{
		V.ModelScale = R.Vector3(Scale);
		if (V.ModelScale.X <= 0 || V.ModelScale.Y <= 0 || V.ModelScale.Z <= 0)
		{
			R.Fail(Scale, "Model scale must be positive");
		}
	}
	if (Animation >= 0)
	{
		V.Animation = static_cast<Toolbox::int32>(R.Integer(Animation, 2147483647));
	}
	if (Material >= 0)
	{
		R.Fields(Material, {"lit", "pbr", "metallic", "roughness", "uv"});
		V.Material.bLit = BoolOr(R, Material, "lit", false);
		V.Material.bPbr = BoolOr(R, Material, "pbr", false);
		V.Material.Metallic = static_cast<Toolbox::f32>(NumberOr(R, Material, "metallic", -1));
		V.Material.Roughness = static_cast<Toolbox::f32>(NumberOr(R, Material, "roughness", -1));
		const auto Uv = R.Find(Material, "uv");
		if (Uv >= 0)
		{
			V.Material.BaseColorUv = static_cast<Toolbox::int32>(R.Integer(Uv, 1));
		}
		if ((V.Material.Metallic != -1 && (V.Material.Metallic < 0 || V.Material.Metallic > 1)) || (V.Material.Roughness != -1 && (V.Material.Roughness < 0 || V.Material.Roughness > 1)))
		{
			R.Fail(Material, "Invalid PBR values");
		}
	}
	V.Material.Tint = V.Color;
}
template <typename T>
void ReadParts(FSchemaReader& R, Toolbox::int32 Node, typename T::FDefinition& Definition, FSceneContentLimits Limits)
{
	R.Array(Node);
	for (auto Child = R.Get(Node).FirstChild; Child >= 0; Child = R.Get(Child).NextSibling)
	{
		R.Fields(Child, {"id", "name", "body", "pose", "colliders", "visual"});
		typename T::FPart Part;
		Part.Id = R.Id(R.Required(Child, "id"));
		Unique(R, Child, Definition.Parts, Part.Id);
		const auto Name = R.Find(Child, "name");
		if (Name >= 0)
		{
			Part.DisplayName = R.String(Name);
		}
		const auto Body = R.Find(Child, "body");
		const auto Pose = R.Find(Child, "pose");
		if (Body >= 0)
		{
			ReadBody<T>(R, Body, Part);
			if (Pose >= 0)
			{
				R.Fail(Child, "Body owns its Pose");
			}
		}
		else if (Pose >= 0)
		{
			R.Fields(Pose, {"position", T::Dimension == 2 ? "angle" : "rotation"});
			Part.Body.Position = VectorOr<T>(R, Pose, "position");
			T::SetOrientation(Part, RotationOr<T>(R, Pose));
		}
		const auto Colliders = R.Find(Child, "colliders");
		if (Colliders >= 0)
		{
			ReadColliders<T>(R, Colliders, Part);
		}
		if (Definition.Parts.Size() >= Limits.MaxParts)
		{
			R.Fail(Child, "Part count limit");
		}
		Definition.Parts.PushBack(Toolbox::Move(Part));
	}
}
// 一つのLocal Frameに必要な値以外を受理しない。
template <typename T, typename TFrame>
TFrame ReadLocalFrame(FSchemaReader& R, Toolbox::int32 Node)
{
	R.Fields(Node, {"anchor", T::Dimension == 2 ? "angle" : "rotation"});
	TFrame Frame;
	Frame.LocalAnchor = VectorOr<T>(R, Node, "anchor");
	if constexpr (T::Dimension == 2)
	{
		Frame.LocalAngle = RotationOr<T>(R, Node);
	}
	else
	{
		Frame.LocalRotation = RotationOr<T>(R, Node);
	}
	GameplayPrivate::ValidateJointFrame(Frame);
	return Frame;
}
template <typename TDescription>
void ReadDrive(FSchemaReader& R, Toolbox::int32 Node, TDescription& Description)
{
	if (Node < 0)
	{
		return;
	}
	R.Fields(Node, {"enabled", "speed", "maxForce", "maxTorque"});
	Description.Drive.bEnabled = BoolOr(R, Node, "enabled", false);
	if constexpr (Toolbox::IsSame<decltype(Description.Drive), FAngularJointDrive>)
	{
		if (R.Find(Node, "maxForce") >= 0)
		{
			R.Fail(Node, "Angular drive uses maxTorque");
		}
		Description.Drive.TargetAngularSpeed = NumberOr(R, Node, "speed", 0);
		Description.Drive.MaxTorque = NumberOr(R, Node, "maxTorque", 0);
	}
	else
	{
		if (R.Find(Node, "maxTorque") >= 0)
		{
			R.Fail(Node, "Linear drive uses maxForce");
		}
		Description.Drive.TargetSpeed = NumberOr(R, Node, "speed", 0);
		Description.Drive.MaxForce = NumberOr(R, Node, "maxForce", 0);
	}
	GameplayPrivate::ValidateJointDrive(Description.Drive);
}
template <typename TDescription>
void ReadLimits(FSchemaReader& R, Toolbox::int32 Node, TDescription& Description)
{
	if (Node < 0)
	{
		return;
	}
	R.Fields(Node, {"enabled", "lower", "upper"});
	Description.Limits.bEnabled = BoolOr(R, Node, "enabled", false);
	if constexpr (Toolbox::IsSame<decltype(Description.Limits), FAngularJointLimits>)
	{
		Description.Limits.LowerAngle = NumberOr(R, Node, "lower", -1);
		Description.Limits.UpperAngle = NumberOr(R, Node, "upper", 1);
	}
	else
	{
		Description.Limits.LowerTranslation = NumberOr(R, Node, "lower", -1);
		Description.Limits.UpperTranslation = NumberOr(R, Node, "upper", 1);
	}
	GameplayPrivate::ValidateJointLimits(Description.Limits);
}
template <typename T, typename TDescription>
void ReadFrames(FSchemaReader& R, Toolbox::int32 Node, TDescription& D, const typename T::FPart& A, const typename T::FPart& B)
{
	const auto Common = R.Find(Node, "frame");
	const auto FrameA = R.Find(Node, "frameA");
	const auto FrameB = R.Find(Node, "frameB");
	if (Common >= 0)
	{
		if (FrameA >= 0 || FrameB >= 0)
		{
			R.Fail(Node, "Common and local Frames are exclusive");
		}
		R.Fields(Common, {"space", "anchor", T::Dimension == 2 ? "angle" : "rotation"});
		if (R.String(R.Required(Common, "space")) != R.GetFrameSpace())
		{
			R.Fail(Common, "Common Frame uses wrong coordinate space");
		}
		const auto Anchor = T::Vector(R, R.Required(Common, "anchor"));
		const auto Rotation = RotationOr<T>(R, Common);
		D.FrameA = T::template Frame<decltype(D.FrameA)>(A, Anchor, Rotation);
		D.FrameB = T::template Frame<decltype(D.FrameB)>(B, Anchor, Rotation);
	}
	else
	{
		if (FrameA < 0 || FrameB < 0)
		{
			R.Fail(Node, "Joint requires common or both Local Frames");
		}
		D.FrameA = ReadLocalFrame<T, decltype(D.FrameA)>(R, FrameA);
		D.FrameB = ReadLocalFrame<T, decltype(D.FrameB)>(R, FrameB);
	}
	GameplayPrivate::ValidateJointFrame(D.FrameA);
	GameplayPrivate::ValidateJointFrame(D.FrameB);
}
template <typename T>
void ReadJoints(FSchemaReader& R, Toolbox::int32 Node, typename T::FDefinition& D, FSceneContentLimits Limits)
{
	if (Node < 0)
	{
		return;
	}
	R.Array(Node);
	for (auto Child = R.Get(Node).FirstChild; Child >= 0; Child = R.Get(Child).NextSibling)
	{
		R.Fields(Child, {"id", "kind", "bodyA", "bodyB", "connected", "frame", "frameA", "frameB", "length", "anchorA", "anchorB", "drive", "limits"});
		typename T::FJoint J;
		J.Id = R.Id(R.Required(Child, "id"));
		Unique(R, Child, D.Joints, J.Id);
		Unique(R, Child, D.Parts, J.Id);
		J.BodyA = BodyIndex(R, R.Required(Child, "bodyA"), D);
		J.BodyB = BodyIndex(R, R.Required(Child, "bodyB"), D);
		const auto& A = D.Parts[J.BodyA];
		const auto& B = D.Parts[J.BodyB];
		if (J.BodyA == J.BodyB || (A.Body.Type != EBodyType::Dynamic && B.Body.Type != EBodyType::Dynamic))
		{
			R.Fail(Child, "Joint needs distinct Bodies and a Dynamic endpoint");
		}
		J.bConnect = BoolOr(R, Child, "connected", true);
		const auto Kind = R.String(R.Required(Child, "kind"));
		const auto Drive = R.Find(Child, "drive");
		const auto Limit = R.Find(Child, "limits");
		try
		{
			if (Kind == "Distance")
			{
				J.Kind = EJointKind::Distance;
				J.Distance.Length = R.Number(R.Required(Child, "length"));
				J.Distance.LocalAnchorA = VectorOr<T>(R, Child, "anchorA");
				J.Distance.LocalAnchorB = VectorOr<T>(R, Child, "anchorB");
				if (J.Distance.Length < 0 || Drive >= 0 || Limit >= 0 || R.Find(Child, "frame") >= 0 || R.Find(Child, "frameA") >= 0 || R.Find(Child, "frameB") >= 0)
				{
					R.Fail(Child, "Invalid Distance fields");
				}
			}
			else if (Kind == "Revolute")
			{
				J.Kind = EJointKind::Revolute;
				ReadFrames<T>(R, Child, J.Revolute, A, B);
				ReadDrive(R, Drive, J.Revolute);
				ReadLimits(R, Limit, J.Revolute);
				if constexpr (T::Dimension == 3)
				{
					const auto AxisA = (A.Body.Orientation * J.Revolute.FrameA.LocalRotation).Rotate({0, 0, 1});
					const auto AxisB = (B.Body.Orientation * J.Revolute.FrameB.LocalRotation).Rotate({0, 0, 1});
					if (Toolbox::Dot(AxisA, AxisB) < -0.999999)
					{
						R.Fail(Child, "Antiparallel hinge Frames");
					}
				}
			}
			else if (Kind == "Fixed")
			{
				J.Kind = EJointKind::Fixed;
				ReadFrames<T>(R, Child, J.Fixed, A, B);
				if (Drive >= 0 || Limit >= 0)
				{
					R.Fail(Child, "Fixed has no drive or limits");
				}
			}
			else if (Kind == "Prismatic")
			{
				J.Kind = EJointKind::Prismatic;
				ReadFrames<T>(R, Child, J.Prismatic, A, B);
				ReadDrive(R, Drive, J.Prismatic);
				ReadLimits(R, Limit, J.Prismatic);
			}
			else
			{
				R.Fail(Child, "Unknown Joint kind");
			}
			if (Kind != "Distance" && (R.Find(Child, "length") >= 0 || R.Find(Child, "anchorA") >= 0 || R.Find(Child, "anchorB") >= 0))
			{
				R.Fail(Child, "Distance fields on another kind");
			}
		}
		catch (const FSceneContentError&)
		{
			throw;
		}
		catch (const Toolbox::FException& Error)
		{
			R.Fail(Child, Error.What());
		}
		if (D.Joints.Size() >= Limits.MaxJoints)
		{
			R.Fail(Child, "Joint count limit");
		}
		D.Joints.PushBack(Toolbox::Move(J));
	}
}
template <typename T>
void ReadExports(FSchemaReader& R, Toolbox::int32 Node, typename T::FDefinition& D, FSceneContentLimits Limits)
{
	if (Node < 0)
	{
		return;
	}
	if (R.Get(Node).Kind != Toolbox::EJsonKind::Object)
	{
		R.Fail(Node, "Expected export table");
	}
	for (auto Child = R.Get(Node).FirstChild; Child >= 0; Child = R.Get(Child).NextSibling)
	{
		FContentExportDefinition E;
		E.Id = R.KeyId(Child);
		const auto Ref = R.String(Child);
		bool Found = false;
		for (Toolbox::size_t I = 0; I < D.Parts.Size(); ++I)
		{
			const auto& P = D.Parts[I];
			bool Internal = false;
			for (const char C : P.Id)
			{
				Internal = Internal || C == '/';
			}
			if (Internal)
			{
				continue;
			}
			if (Ref == P.Id + "/body" || Ref == P.Id + "/sensor")
			{
				if (!P.bHasBody)
				{
					R.Fail(Child, "Export target has no Body");
				}
				if (Ref == P.Id + "/sensor")
				{
					bool Sensor = false;
					for (const auto& C : P.Colliders)
					{
						Sensor = Sensor || C.Response == EColliderResponse::Sensor;
					}
					if (!Sensor)
					{
						R.Fail(Child, "Sensor export needs a Sensor collider");
					}
				}
				E.Kind = Ref == P.Id + "/sensor" ? EContentExportKind::Sensor
				         : P.bMover              ? EContentExportKind::KinematicMover
				                                 : EContentExportKind::RigidBody;
				E.Index = static_cast<Toolbox::uint32>(I);
				Found = true;
			}
		}
		for (Toolbox::size_t I = 0; I < D.Joints.Size(); ++I)
		{
			const auto& J = D.Joints[I];
			bool Internal = false;
			for (const char C : J.Id)
			{
				Internal = Internal || C == '/';
			}
			if (Internal)
			{
				continue;
			}
			if (Ref == J.Id)
			{
				E.Kind = J.Kind == EJointKind::Distance   ? EContentExportKind::Distance
				         : J.Kind == EJointKind::Revolute ? EContentExportKind::Revolute
				         : J.Kind == EJointKind::Fixed    ? EContentExportKind::Fixed
				                                          : EContentExportKind::Prismatic;
				E.Index = static_cast<Toolbox::uint32>(I);
				Found = true;
			}
		}
		for (Toolbox::size_t I = 0; I < D.Assets.Size(); ++I)
		{
			const auto& A = D.Assets[I];
			bool Internal = false;
			for (const char C : A.Id)
			{
				Internal = Internal || C == '/';
			}
			if (Internal)
			{
				continue;
			}
			if (Ref == A.Id + "/asset")
			{
				E.Kind = A.Kind == EContentAssetKind::Texture ? EContentExportKind::Texture
				         : A.Kind == EContentAssetKind::Model ? EContentExportKind::Model
				         : A.Kind == EContentAssetKind::Sound ? EContentExportKind::Sound
				                                              : EContentExportKind::Font;
				E.Index = static_cast<Toolbox::uint32>(I);
				Found = true;
			}
		}
		for (const auto& Imported : D.Imports)
		{
			if (Ref == Imported.Id)
			{
				E.Kind = Imported.Kind;
				E.Index = Imported.Index;
				Found = true;
			}
		}
		if (!Found)
		{
			R.Fail(Child, "Unknown or untyped export target");
		}
		D.Exports.PushBack(Toolbox::Move(E));
	}
}
template <typename T, typename TExpansion>
typename T::FDefinition Parse(FSchemaReader& R, const Toolbox::FString& Path, const Toolbox::TVector<FContentParameterValue>& Overrides, FSceneContentLimits Limits, const TExpansion& Expand, const Toolbox::TVector<FContentAssetDefinition>& SharedAssets)
{
	R.Fields(0, {"schema", "kind", "dimension", "parameters", "assets", "parts", "joints", "exports", "children", "metadata"});
	if (R.Integer(R.Required(0, "schema"), 1) != 1 || R.String(R.Required(0, "kind")) != "prefab" || R.Integer(R.Required(0, "dimension"), 3) != T::Dimension)
	{
		R.Fail(0, "Unsupported schema, kind or dimension");
	}
	R.Parameters(R.Find(0, "parameters"), Overrides);
	typename T::FDefinition D;
	D.Path = Path;
	D.Parameters = R.GetParameters();
	D.Assets = SharedAssets;
	ReadAssets(R, R.Find(0, "assets"), D, Limits);
	ReadParts<T>(R, R.Required(0, "parts"), D, Limits);
	const auto Children = R.Find(0, "children");
	if (Children >= 0)
	{
		if (!Expand)
		{
			R.Fail(Children, "Child Prefab references require SceneContentSource");
		}
		Expand(R, Children, D);
	}
	// 自分の表示参照も、子の公開資源が揃ってから解決する。
	Toolbox::size_t PartIndex = 0;
	const auto Parts = R.Required(0, "parts");
	for (auto Part = R.Get(Parts).FirstChild; Part >= 0; Part = R.Get(Part).NextSibling)
	{
		const auto Visual = R.Find(Part, "visual");
		if (Visual >= 0)
		{
			ReadVisual<T>(R, Visual, D.Parts[PartIndex], D);
		}
		++PartIndex;
	}
	ReadJoints<T>(R, R.Find(0, "joints"), D, Limits);
	ReadExports<T>(R, R.Find(0, "exports"), D, Limits);
	return D;
}
} // namespace Dxf::ContentPrivate
