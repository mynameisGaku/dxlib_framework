// SPDX-License-Identifier: NOASSERTION
#include "Dxf/ContentSchemaReader.h"
#include "Dxf/MechanismComponentValidation.h"
namespace Dxf::ContentPrivate
{
// 次元ごとの数学と値型だけを分ける。所有と参照検査は共通。
struct FSchema2D
{
	using FDefinition = FPrefabDefinition2D;
	using FPart = FContentPartDefinition2D;
	using FJoint = FContentJointDefinition2D;
	using FVector = Toolbox::FVector2;
	using FRotation = Toolbox::f32;
	static constexpr Toolbox::uint32 Dimension = 2;
	static FVector Vector(FSchemaReader& Reader, Toolbox::int32 Index)
	{
		return Reader.Vector2(Index);
	}
	static FRotation Rotation(FSchemaReader& Reader, Toolbox::int32 Index)
	{
		return Reader.Scalar(Index);
	}
	static FVector Rotate(FRotation Rotation, FVector Vector)
	{
		return GameplayPrivate::RotateJointAnchor(Rotation, Vector);
	}
	static FRotation Orientation(const FPart& Part)
	{
		return Part.Body.Angle;
	}
	static void SetOrientation(FPart& Part, FRotation Rotation)
	{
		Part.Body.Angle = Rotation;
	}
	static FVector Local(const FPart& Part, FVector Point)
	{
		return Rotate(-Part.Body.Angle, Point - Part.Body.Position);
	}
	template <typename TFrame>
	static TFrame Frame(const FPart& Part, FVector Anchor, FRotation Angle)
	{
		return {Local(Part, Anchor), Angle - Part.Body.Angle};
	}
};
struct FSchema3D
{
	using FDefinition = FPrefabDefinition3D;
	using FPart = FContentPartDefinition3D;
	using FJoint = FContentJointDefinition3D;
	using FVector = Toolbox::FVector3;
	using FRotation = Toolbox::FQuaternion;
	static constexpr Toolbox::uint32 Dimension = 3;
	static FVector Vector(FSchemaReader& Reader, Toolbox::int32 Index)
	{
		return Reader.Vector3(Index);
	}
	static FRotation Rotation(FSchemaReader& Reader, Toolbox::int32 Index)
	{
		return Reader.Rotation(Index);
	}
	static FVector Rotate(FRotation Rotation, FVector Vector)
	{
		return Rotation.Rotate(Vector);
	}
	static FRotation Orientation(const FPart& Part)
	{
		return Part.Body.Orientation;
	}
	static void SetOrientation(FPart& Part, FRotation Rotation)
	{
		Part.Body.Orientation = Rotation;
	}
	static FVector Local(const FPart& Part, FVector Point)
	{
		return Part.Body.Orientation.Conjugate().Rotate(Point - Part.Body.Position);
	}
	template <typename TFrame>
	static TFrame Frame(const FPart& Part, FVector Anchor, FRotation Rotation)
	{
		return {Local(Part, Anchor), (Part.Body.Orientation.Conjugate() * Rotation).Normalized()};
	}
};
// 少数のオプション値を読む。未指定だけが既定値を使える。
Toolbox::f64 NumberOr(FSchemaReader& R, Toolbox::int32 Object, const char* Key, Toolbox::f64 Default)
{
	const auto Index = R.Find(Object, Key);
	return Index < 0 ? Default : R.Number(Index);
}
bool BoolOr(FSchemaReader& R, Toolbox::int32 Object, const char* Key, bool Default)
{
	const auto Index = R.Find(Object, Key);
	return Index < 0 ? Default : R.Boolean(Index);
}
template <typename T>
typename T::FVector VectorOr(FSchemaReader& R, Toolbox::int32 Object, const char* Key, typename T::FVector Default = {})
{
	const auto Index = R.Find(Object, Key);
	return Index < 0 ? Default : T::Vector(R, Index);
}
template <typename T>
typename T::FRotation RotationOr(FSchemaReader& R, Toolbox::int32 Object, typename T::FRotation Default = {})
{
	const auto Index = R.Find(Object, T::Dimension == 2 ? "angle" : "rotation");
	return Index < 0 ? Default : T::Rotation(R, Index);
}
// 複数の表で同名を許さない。公開先の型を暗黙に選ばない。
template <typename T>
void Unique(FSchemaReader& R, Toolbox::int32 Node, const Toolbox::TVector<T>& Values, const Toolbox::FString& Id)
{
	for (const auto& Value : Values)
	{
		if (Value.Id == Id)
		{
			R.Fail(Node, "Duplicate logical ID");
		}
	}
}
template <typename T>
Toolbox::uint32 BodyIndex(FSchemaReader& R, Toolbox::int32 Node, const T& Definition)
{
	const auto Ref = R.String(Node);
	for (const char C : Ref)
	{
		if (C == '/')
		{
			for (const auto& E : Definition.Imports)
			{
				if (E.Id == Ref)
				{
					if (E.Kind != EContentExportKind::RigidBody && E.Kind != EContentExportKind::KinematicMover)
					{
						R.Fail(Node, "Child export is not a Body");
					}
					return E.Index;
				}
			}
			R.Fail(Node, "Unknown public child Body export");
		}
	}
	R.Id(Node);
	for (Toolbox::size_t Index = 0; Index < Definition.Parts.Size(); ++Index)
	{
		if (Definition.Parts[Index].Id == Ref)
		{
			if (!Definition.Parts[Index].bHasBody)
			{
				R.Fail(Node, "Reference is not a Body");
			}
			return static_cast<Toolbox::uint32>(Index);
		}
	}
	R.Fail(Node, "Unknown Body reference");
}
template <typename T>
Toolbox::int32 AssetIndex(FSchemaReader& R, Toolbox::int32 Node, const T& Definition, EContentAssetKind Kind)
{
	const auto Ref = R.Get(Node).Kind == Toolbox::EJsonKind::Object ? R.Id(Node) : R.String(Node);
	for (const auto& E : Definition.Imports)
	{
		if (E.Id == Ref)
		{
			const auto Expected = Kind == EContentAssetKind::Texture ? EContentExportKind::Texture
			                      : Kind == EContentAssetKind::Model ? EContentExportKind::Model
			                      : Kind == EContentAssetKind::Sound ? EContentExportKind::Sound
			                                                         : EContentExportKind::Font;
			if (E.Kind != Expected)
			{
				R.Fail(Node, "Child resource export type mismatch");
			}
			return static_cast<Toolbox::int32>(E.Index);
		}
	}
	R.Id(Node);
	for (Toolbox::size_t Index = 0; Index < Definition.Assets.Size(); ++Index)
	{
		if (Definition.Assets[Index].Id == Ref)
		{
			if (Definition.Assets[Index].Kind != Kind)
			{
				R.Fail(Node, "Asset type mismatch");
			}
			return static_cast<Toolbox::int32>(Index);
		}
	}
	R.Fail(Node, "Unknown asset key");
}
template <typename T>
void ReadAssets(FSchemaReader& R, Toolbox::int32 Node, T& Definition, FSceneContentLimits Limits)
{
	if (Node < 0)
	{
		return;
	}
	if (R.Get(Node).Kind != Toolbox::EJsonKind::Object)
	{
		R.Fail(Node, "Expected asset table");
	}
	for (auto Child = R.Get(Node).FirstChild; Child >= 0; Child = R.Get(Child).NextSibling)
	{
		R.Fields(Child, {"kind", "path", "use3D", "premultipliedAlpha", "storage", "family", "size", "thickness", "antialias", "targetUnitMeters", "samplesPerSecond"});
		FContentAssetDefinition Asset;
		Asset.Id = R.KeyId(Child);
		Unique(R, Child, Definition.Assets, Asset.Id);
		const auto Kind = R.String(R.Required(Child, "kind"));
		if (Kind == "Texture")
		{
			R.Fields(Child, {"kind", "path", "use3D", "premultipliedAlpha"});
			Asset.Kind = EContentAssetKind::Texture;
			Asset.Texture.bUse3D = BoolOr(R, Child, "use3D", true);
			Asset.Texture.bPremultipliedAlpha = BoolOr(R, Child, "premultipliedAlpha", false);
		}
		else if (Kind == "Model")
		{
			R.Fields(Child, {"kind", "path", "targetUnitMeters", "samplesPerSecond"});
			Asset.Kind = EContentAssetKind::Model;
			Asset.Model.TargetUnitMeters = NumberOr(R, Child, "targetUnitMeters", 0);
			if (Asset.Model.TargetUnitMeters < 0)
			{
				R.Fail(Child, "Negative model unit");
			}
			const auto Samples = R.Find(Child, "samplesPerSecond");
			if (Samples >= 0)
			{
				Asset.Model.SamplesPerSecond = R.Integer(Samples, 1000);
				if (Asset.Model.SamplesPerSecond == 0)
				{
					R.Fail(Samples, "Samples must be positive");
				}
			}
		}
		else if (Kind == "Sound")
		{
			R.Fields(Child, {"kind", "path", "storage"});
			Asset.Kind = EContentAssetKind::Sound;
			const auto Storage = R.Find(Child, "storage");
			if (Storage >= 0)
			{
				const auto Value = R.String(Storage);
				if (Value != "Memory" && Value != "Stream")
				{
					R.Fail(Storage, "Unknown sound storage");
				}
				Asset.Sound.Storage = Value == "Stream" ? ESoundStorage::Stream : ESoundStorage::Memory;
			}
		}
		else if (Kind == "Font")
		{
			R.Fields(Child, {"kind", "family", "size", "thickness", "antialias", "premultipliedAlpha"});
			Asset.Kind = EContentAssetKind::Font;
			const auto Family = R.Find(Child, "family");
			if (Family >= 0)
			{
				Asset.Font.Family = R.String(Family);
			}
			const auto Size = R.Find(Child, "size");
			const auto Thickness = R.Find(Child, "thickness");
			if (Size >= 0)
			{
				Asset.Font.Size = static_cast<Toolbox::int32>(R.Integer(Size, 512));
			}
			if (Thickness >= 0)
			{
				Asset.Font.Thickness = static_cast<Toolbox::int32>(R.Integer(Thickness, 100));
			}
			Asset.Font.bAntialias = BoolOr(R, Child, "antialias", true);
			Asset.Font.bPremultipliedAlpha = BoolOr(R, Child, "premultipliedAlpha", false);
			if (Asset.Font.Size <= 0 || Asset.Font.Family.IsEmpty())
			{
				R.Fail(Child, "Invalid font conditions");
			}
		}
		else
		{
			R.Fail(Child, "Unknown asset kind");
		}
		if (Asset.Kind != EContentAssetKind::Font)
		{
			Asset.Path = R.Path(R.Required(Child, "path"));
		}
		else if (R.Find(Child, "path") >= 0)
		{
			R.Fail(Child, "Font uses family rather than a path");
		}
		if (Definition.Assets.Size() >= Limits.MaxAssets)
		{
			R.Fail(Child, "Asset count limit");
		}
		Definition.Assets.PushBack(Toolbox::Move(Asset));
	}
}
template <typename T>
void ReadBody(FSchemaReader& R, Toolbox::int32 Node, typename T::FPart& Part)
{
	R.Fields(Node, {"type", "position", "angle", "rotation", "velocity", "angularVelocity", "mass", "inertia", "linearDamping", "angularDamping", "gravityScale", "continuous", "allowSleep", "adapter"});
	Part.bHasBody = true;
	const auto Type = R.String(R.Required(Node, "type"));
	if (Type != "Static" && Type != "Dynamic" && Type != "Kinematic")
	{
		R.Fail(Node, "Unknown Body type");
	}
	Part.Body.Type = Type == "Static"    ? EBodyType::Static
	                 : Type == "Dynamic" ? EBodyType::Dynamic
	                                     : EBodyType::Kinematic;
	const auto Adapter = R.Find(Node, "adapter");
	if (Adapter >= 0)
	{
		const auto Name = R.String(Adapter);
		if (Name != "RigidBody" && Name != "KinematicMover")
		{
			R.Fail(Adapter, "Unknown Body adapter");
		}
		Part.bMover = Name == "KinematicMover";
		if (Part.bMover && Part.Body.Type != EBodyType::Kinematic)
		{
			R.Fail(Adapter, "Mover requires Kinematic Body");
		}
		if (Part.bMover)
		{
			// 初版のMoverは静止初期Poseだけ。無視する速度や質量条件を受理しない。
			R.Fields(Node, {"type", "position", T::Dimension == 2 ? "angle" : "rotation", "adapter"});
		}
	}
	Part.Body.Position = VectorOr<T>(R, Node, "position");
	T::SetOrientation(Part, RotationOr<T>(R, Node));
	Part.Body.Velocity = VectorOr<T>(R, Node, "velocity");
	const auto Angular = R.Find(Node, "angularVelocity");
	if (Angular >= 0)
	{
		if constexpr (T::Dimension == 2)
		{
			Part.Body.AngularVelocity = R.Scalar(Angular);
		}
		else
		{
			Part.Body.AngularVelocity = R.Vector3(Angular);
		}
	}
	const auto Mass = R.Find(Node, "mass");
	if (Mass >= 0)
	{
		Part.Body.Mass = R.Scalar(Mass);
	}
	const auto Inertia = R.Find(Node, "inertia");
	if constexpr (T::Dimension == 2)
	{
		if (Inertia >= 0)
		{
			Part.Body.Inertia = R.Scalar(Inertia);
		}
		if (Part.Body.Inertia <= 0 || R.Find(Node, "rotation") >= 0)
		{
			R.Fail(Node, "Invalid 2D inertia or 3D rotation field");
		}
	}
	else
	{
		if (Inertia >= 0)
		{
			Part.Body.DiagonalInertia = R.Vector3(Inertia);
		}
		if (Part.Body.DiagonalInertia.X <= 0 || Part.Body.DiagonalInertia.Y <= 0 || Part.Body.DiagonalInertia.Z <= 0 || R.Find(Node, "angle") >= 0)
		{
			R.Fail(Node, "Invalid 3D inertia or 2D angle field");
		}
	}
	Part.Body.LinearDamping = static_cast<Toolbox::f32>(NumberOr(R, Node, "linearDamping", 0));
	Part.Body.AngularDamping = static_cast<Toolbox::f32>(NumberOr(R, Node, "angularDamping", 0));
	Part.Body.GravityScale = static_cast<Toolbox::f32>(NumberOr(R, Node, "gravityScale", 1));
	Part.Body.bUseContinuous = BoolOr(R, Node, "continuous", false);
	Part.Body.bAllowSleep = BoolOr(R, Node, "allowSleep", true);
	if (Part.Body.Mass <= 0 || !Toolbox::IsFinite(Part.Body.LinearDamping) || Part.Body.LinearDamping < 0 || !Toolbox::IsFinite(Part.Body.AngularDamping) || Part.Body.AngularDamping < 0 || !Toolbox::IsFinite(Part.Body.GravityScale))
	{
		R.Fail(Node, "Invalid Body mass or damping");
	}
}
template <typename T>
void ReadColliders(FSchemaReader& R, Toolbox::int32 Node, typename T::FPart& Part)
{
	R.Array(Node);
	if (!Part.bHasBody)
	{
		R.Fail(Node, "Collider requires a Body");
	}
	for (auto Child = R.Get(Node).FirstChild; Child >= 0; Child = R.Get(Child).NextSibling)
	{
		R.Fields(Child, {"shape", "center", "halfExtents", "angle", "rotation", "radius", "start", "end", "response", "friction", "restitution", "category", "mask", "queryCategory"});
		// 型参照から独立した値を作る。
		typename Toolbox::TRemoveReference<decltype(Part.Colliders[0])>::Type Collider;
		const auto Shape = R.String(R.Required(Child, "shape"));
		const auto Center = VectorOr<T>(R, Child, "center");
		const auto Rotation = RotationOr<T>(R, Child);
		if (Shape == "Box")
		{
			R.Fields(Child, {"shape", "center", "halfExtents", T::Dimension == 2 ? "angle" : "rotation", "response", "friction", "restitution", "category", "mask", "queryCategory"});
			const auto Extents = T::Vector(R, R.Required(Child, "halfExtents"));
			if (Extents.X <= 0 || Extents.Y <= 0)
			{
				R.Fail(Child, "Box dimensions must be positive");
			}
			if constexpr (T::Dimension == 2)
			{
				Toolbox::FOrientedBox2D Box;
				Box.Center = Center;
				Box.HalfExtents = Extents;
				Box.Angle = Rotation;
				Collider.Shape = Box;
			}
			else
			{
				if (Extents.Z <= 0)
				{
					R.Fail(Child, "Box dimensions must be positive");
				}
				Toolbox::FOBB Box;
				Box.Center = Center;
				Box.HalfExtents = Extents;
				Box.Axes = {Rotation.Rotate({1, 0, 0}), Rotation.Rotate({0, 1, 0}), Rotation.Rotate({0, 0, 1})};
				Collider.Shape = Box;
			}
		}
		else if (Shape == "Sphere" || (T::Dimension == 2 && Shape == "Circle"))
		{
			R.Fields(Child, {"shape", "center", "radius", "response", "friction", "restitution", "category", "mask", "queryCategory"});
			const auto Radius = R.Scalar(R.Required(Child, "radius"));
			if (Radius <= 0)
			{
				R.Fail(Child, "Radius must be positive");
			}
			if constexpr (T::Dimension == 2)
			{
				Collider.Shape = Toolbox::FCircle2D{Center, Radius};
			}
			else
			{
				Collider.Shape = Toolbox::FSphere{Center, Radius};
			}
		}
		else if (Shape == "Capsule")
		{
			R.Fields(Child, {"shape", "start", "end", "radius", "response", "friction", "restitution", "category", "mask", "queryCategory"});
			const auto Start = T::Vector(R, R.Required(Child, "start"));
			const auto End = T::Vector(R, R.Required(Child, "end"));
			const auto Radius = R.Scalar(R.Required(Child, "radius"));
			if (Radius <= 0)
			{
				R.Fail(Child, "Capsule radius must be positive");
			}
			if constexpr (T::Dimension == 2)
			{
				const Toolbox::FCapsule2D Capsule{Start, End, Radius};
				if (!Toolbox::IsValid(Capsule))
				{
					R.Fail(Child, "Invalid capsule bounds");
				}
				Collider.Shape = Capsule;
			}
			else
			{
				const Toolbox::FCapsule Capsule{Start, End, Radius};
				if (!Toolbox::IsValid(Capsule))
				{
					R.Fail(Child, "Invalid capsule bounds");
				}
				Collider.Shape = Capsule;
			}
		}
		else
		{
			R.Fail(Child, "Unknown collider shape");
		}
		const auto Response = R.Find(Child, "response");
		if (Response >= 0)
		{
			const auto Name = R.String(Response);
			if (Name != "Solid" && Name != "Sensor")
			{
				R.Fail(Response, "Unknown collider response");
			}
			Collider.Response = Name == "Sensor" ? EColliderResponse::Sensor : EColliderResponse::Solid;
		}
		Collider.Friction = static_cast<Toolbox::f32>(NumberOr(R, Child, "friction", 0.5));
		Collider.Restitution = static_cast<Toolbox::f32>(NumberOr(R, Child, "restitution", 0));
		if (!Toolbox::IsFinite(Collider.Friction) || Collider.Friction < 0 || Collider.Restitution < 0 || Collider.Restitution > 1)
		{
			R.Fail(Child, "Invalid collider material");
		}
		const auto Category = R.Find(Child, "category");
		const auto Mask = R.Find(Child, "mask");
		const auto Query = R.Find(Child, "queryCategory");
		if (Category >= 0)
		{
			Collider.Collision.Category = R.Integer(Category, 0xffffffffu);
		}
		if (Mask >= 0)
		{
			Collider.Collision.Mask = R.Integer(Mask, 0xffffffffu);
		}
		if (Query >= 0)
		{
			Collider.QueryCategory = R.Integer(Query, 0xffffffffu);
		}
		Part.Colliders.PushBack(Toolbox::Move(Collider));
	}
}
} // namespace Dxf::ContentPrivate
