// SPDX-License-Identifier: NOASSERTION
#include "Dxf/SceneContentValidation.h"
#include "Dxf/SceneContentError.h"
#include "Dxf/MechanismComponentValidation.h"
#include "Toolbox/ProjectPaths.h"
#include "Toolbox/Platform.h"
#include "Dxf/ContentViews.h"
namespace Dxf::ContentPrivate
{
// 解決済みの値でも、利用者のC++による変更を信頼して範囲外参照しない。
void Require(bool Value, const Toolbox::FString& Path, const Toolbox::FString& Location, const char* Reason)
{
	if (!Value)
	{
		throw FSceneContentError({Path, {}, Location, Reason, 0, 0});
	}
}
bool Name(Toolbox::FStringView Text, FSceneContentLimits Limits, bool Expanded)
{
	Toolbox::size_t Segment = 0;
	if (Text.IsEmpty() || Text.Size() > Limits.Json.MaxStringBytes)
	{
		return false;
	}
	for (Toolbox::size_t Byte = 0; Byte < Text.Size(); ++Byte)
	{
		const char C = Text[Byte];
		if (Expanded && C == '/')
		{
			if (Segment == 0 || Segment > Limits.MaxIdBytes)
			{
				return false;
			}
			Segment = 0;
			continue;
		}
		const bool Letter = (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || C == '_';
		if (!(Letter || (Segment > 0 && ((C >= '0' && C <= '9') || C == '-'))))
		{
			return false;
		}
		++Segment;
	}
	return Segment > 0 && Segment <= Limits.MaxIdBytes;
}
bool Text(const Toolbox::FString& Value, FSceneContentLimits Limits)
{
	if (Value.Size() > Limits.Json.MaxStringBytes)
	{
		return false;
	}
	for (const char C : Value)
	{
		if (C == 0)
		{
			return false;
		}
	}
	try
	{
		(void)Toolbox::ToWide(Value);
	}
	catch (const Toolbox::FException&)
	{
		return false;
	}
	return true;
}
bool RelativePath(Toolbox::FStringView Path)
{
	if (Path.IsEmpty() || Path[0] == '/' || Path[0] == '\\')
	{
		return false;
	}
	Toolbox::size_t Start = 0;
	Toolbox::size_t Depth = 0;
	for (Toolbox::size_t I = 0; I <= Path.Size(); ++I)
	{
		if (I < Path.Size() && (Path[I] == ':' || Path[I] == 0))
		{
			return false;
		}
		if (I == Path.Size() || Path[I] == '/' || Path[I] == '\\')
		{
			const auto Length = I - Start;
			if (Length == 2 && Path[Start] == '.' && Path[Start + 1] == '.')
			{
				if (Depth == 0)
				{
					return false;
				}
				--Depth;
			}
			else if (Length > 0 && !(Length == 1 && Path[Start] == '.'))
			{
				++Depth;
			}
			Start = I + 1;
		}
	}
	return Depth > 0;
}
void Body(const FBodyDescription2D& B)
{
	if (!Toolbox::IsFinite(B.Angle) || !Toolbox::IsFinite(B.AngularVelocity) || !Toolbox::IsFinite(B.Inertia) || B.Inertia <= 0)
	{
		throw Toolbox::FException("Invalid 2D Body rotation or inertia");
	}
}
void Body(const FBodyDescription3D& B)
{
	if (!Toolbox::IsFinite(B.Orientation.X) || !Toolbox::IsFinite(B.Orientation.Y) || !Toolbox::IsFinite(B.Orientation.Z) || !Toolbox::IsFinite(B.Orientation.W) || (Toolbox::f64(B.Orientation.X) * B.Orientation.X + Toolbox::f64(B.Orientation.Y) * B.Orientation.Y + Toolbox::f64(B.Orientation.Z) * B.Orientation.Z + Toolbox::f64(B.Orientation.W) * B.Orientation.W) <= 0 || !B.AngularVelocity.IsValid() || !B.DiagonalInertia.IsValid() || B.DiagonalInertia.X <= 0 || B.DiagonalInertia.Y <= 0 || B.DiagonalInertia.Z <= 0)
	{
		throw Toolbox::FException("Invalid 3D Body rotation or inertia");
	}
}
bool Shape(const Toolbox::FCircle2D& S)
{
	return Toolbox::IsValid(S) && S.Radius > 0;
}
bool Shape(const Toolbox::FOrientedBox2D& S)
{
	return Toolbox::IsValid(S) && S.HalfExtents.X > 0 && S.HalfExtents.Y > 0;
}
bool Shape(const Toolbox::FCapsule2D& S)
{
	return Toolbox::IsValid(S) && S.Radius > 0;
}
bool Shape(const Toolbox::FSphere& S)
{
	return S.Center.IsValid() && Toolbox::IsFinite(S.Radius) && S.Radius > 0;
}
bool Shape(const Toolbox::FCapsule& S)
{
	return Toolbox::IsValid(S) && S.Radius > 0;
}
bool Shape(const Toolbox::FOBB& S)
{
	if (!S.Center.IsValid() || !S.HalfExtents.IsValid() || S.HalfExtents.X <= 0 || S.HalfExtents.Y <= 0 || S.HalfExtents.Z <= 0)
	{
		return false;
	}
	for (Toolbox::uint32 I = 0; I < 3; ++I)
	{
		if (!S.Axes[I].IsValid() || Toolbox::Abs(Toolbox::Dot(S.Axes[I], S.Axes[I]) - 1) > .0001)
		{
			return false;
		}
		for (Toolbox::uint32 J = 0; J < I; ++J)
		{
			if (Toolbox::Abs(Toolbox::Dot(S.Axes[I], S.Axes[J])) > .0001)
			{
				return false;
			}
		}
	}
	return Toolbox::Dot(Toolbox::Cross(S.Axes[0], S.Axes[1]), S.Axes[2]) > .9999;
}
template <typename T>
void Frames(const T& J)
{
	GameplayPrivate::ValidateJointFrame(J.FrameA);
	GameplayPrivate::ValidateJointFrame(J.FrameB);
}
template <typename T>
void Definition(const T& D, FSceneContentLimits L)
{
	Require(D.Parts.Size() <= L.MaxParts && D.Joints.Size() <= L.MaxJoints && D.Assets.Size() <= L.MaxAssets && D.ExpandedPrefabCount > 0 && D.ExpandedPrefabCount <= L.MaxParts, D.Path, "$", "Definition count limit");
	for (Toolbox::size_t I = 0; I < D.Parts.Size(); ++I)
	{
		const auto& P = D.Parts[I];
		const auto Location = "parts/" + P.Id;
		Require(Name(P.Id, L, true), D.Path, Location, "Invalid part ID");
		for (Toolbox::size_t J = 0; J < I; ++J)
		{
			Require(D.Parts[J].Id != P.Id, D.Path, Location, "Duplicate part ID");
		}
		const auto& B = P.Body;
		try
		{
			Body(B);
			Require(B.Type == EBodyType::Dynamic || B.Type == EBodyType::Static || B.Type == EBodyType::Kinematic, D.Path, Location, "Unknown Body type");
			Require(B.Position.IsValid() && B.Velocity.IsValid() && Toolbox::IsFinite(B.Mass) && B.Mass > 0 && Toolbox::IsFinite(B.LinearDamping) && B.LinearDamping >= 0 && Toolbox::IsFinite(B.AngularDamping) && B.AngularDamping >= 0 && Toolbox::IsFinite(B.GravityScale), D.Path, Location, "Invalid Body values");
			Require(!P.bMover || (P.bHasBody && B.Type == EBodyType::Kinematic), D.Path, Location, "Mover requires Kinematic Body");
			Require(P.bHasBody || P.Colliders.IsEmpty(), D.Path, Location, "Collider requires Body");
			for (const auto& C : P.Colliders)
			{
				bool Valid = false;
				C.Shape.Visit(
				    [&Valid](const auto& S)
				    {
					    Valid = Shape(S);
				    });
				Require(Valid && Toolbox::IsFinite(C.Friction) && C.Friction >= 0 && Toolbox::IsFinite(C.Restitution) && C.Restitution >= 0 && C.Restitution <= 1 && (C.Response == EColliderResponse::Solid || C.Response == EColliderResponse::Sensor), D.Path, Location, "Invalid Collider values");
			}
		}
		catch (const FSceneContentError&)
		{
			throw;
		}
		catch (const Toolbox::FException& E)
		{
			throw FSceneContentError({D.Path, P.Id, Location, E.What(), 0, 0});
		}
		Require(Text(P.DisplayName, L), D.Path, Location, "Invalid display name UTF-8 or length");
		const auto& V = P.Visual;
		Require(Text(V.Label, L), D.Path, Location, "Invalid label UTF-8 or length");
		Require(V.Animation >= -1 && V.Material.BaseColorUv >= -1 && V.Material.BaseColorUv <= 1 && Toolbox::IsFinite(V.Material.Metallic) && Toolbox::IsFinite(V.Material.Roughness) && (V.Material.Metallic == -1 || (V.Material.Metallic >= 0 && V.Material.Metallic <= 1)) && (V.Material.Roughness == -1 || (V.Material.Roughness >= 0 && V.Material.Roughness <= 1)), D.Path, Location, "Invalid model animation or material");
		Require(V.HalfExtents.IsValid() && V.HalfExtents.X > 0 && V.HalfExtents.Y > 0 && V.HalfExtents.Z > 0 && Toolbox::IsFinite(V.Radius) && V.Radius > 0 && V.ModelScale.IsValid() && V.ModelScale.X > 0 && V.ModelScale.Y > 0 && V.ModelScale.Z > 0, D.Path, Location, "Invalid visual dimensions");
		Require(V.Kind >= EContentVisualKind::None && V.Kind <= EContentVisualKind::Model, D.Path, Location, "Unknown visual kind");
		if (V.Kind == EContentVisualKind::Texture || V.Kind == EContentVisualKind::Model)
		{
			const auto Kind =
			    V.Kind == EContentVisualKind::Texture ? EContentAssetKind::Texture : EContentAssetKind::Model;
			Require(V.Asset >= 0 && static_cast<Toolbox::size_t>(V.Asset) < D.Assets.Size() && D.Assets[V.Asset].Kind == Kind, D.Path, Location, "Visual resource index or type mismatch");
			if constexpr (Toolbox::IsSame<T, FPrefabDefinition2D>)
			{
				Require(V.Kind != EContentVisualKind::Model, D.Path, Location, "Model visual requires 3D");
			}
			else
			{
				Require(V.Kind != EContentVisualKind::Texture, D.Path, Location, "Texture visual requires 2D");
			}
		}
		if (V.Font >= 0)
		{
			Require(static_cast<Toolbox::size_t>(V.Font) < D.Assets.Size() && D.Assets[V.Font].Kind == EContentAssetKind::Font, D.Path, Location, "Font index or type mismatch");
		}
	}
	for (Toolbox::size_t I = 0; I < D.Joints.Size(); ++I)
	{
		const auto& J = D.Joints[I];
		const auto Location = "joints/" + J.Id;
		Require(Name(J.Id, L, true), D.Path, Location, "Invalid Joint ID");
		for (Toolbox::size_t Other = 0; Other < I; ++Other)
		{
			Require(D.Joints[Other].Id != J.Id, D.Path, Location, "Duplicate Joint ID");
		}
		for (const auto& P : D.Parts)
		{
			Require(P.Id != J.Id, D.Path, Location, "Joint and part ID collision");
		}
		Require(J.BodyA < D.Parts.Size() && J.BodyB < D.Parts.Size() && J.BodyA != J.BodyB, D.Path, Location, "Invalid Joint indices");
		const auto& A = D.Parts[J.BodyA];
		const auto& B = D.Parts[J.BodyB];
		Require(A.bHasBody && B.bHasBody && (A.Body.Type == EBodyType::Dynamic || B.Body.Type == EBodyType::Dynamic), D.Path, Location, "Joint requires two Bodies and Dynamic endpoint");
		try
		{
			switch (J.Kind)
			{
			case EJointKind::Distance:
				Require(J.Distance.LocalAnchorA.IsValid() && J.Distance.LocalAnchorB.IsValid() && Toolbox::IsFinite(J.Distance.Length) && J.Distance.Length >= 0, D.Path, Location, "Invalid Distance values");
				break;
			case EJointKind::Revolute:
				Frames(J.Revolute);
				GameplayPrivate::ValidateJointDrive(J.Revolute.Drive);
				GameplayPrivate::ValidateJointLimits(J.Revolute.Limits);
				if constexpr (Toolbox::IsSame<T, FPrefabDefinition3D>)
				{
					const auto AxisA =
					    (A.Body.Orientation * J.Revolute.FrameA.LocalRotation).Normalized().Rotate({0, 0, 1});
					const auto AxisB =
					    (B.Body.Orientation * J.Revolute.FrameB.LocalRotation).Normalized().Rotate({0, 0, 1});
					Require(Toolbox::Dot(AxisA, AxisB) >= -.999999, D.Path, Location, "Antiparallel hinge Frames");
				}
				break;
			case EJointKind::Fixed:
				Frames(J.Fixed);
				break;
			case EJointKind::Prismatic:
				Frames(J.Prismatic);
				GameplayPrivate::ValidateJointDrive(J.Prismatic.Drive);
				GameplayPrivate::ValidateJointLimits(J.Prismatic.Limits);
				break;
			default:
				Require(false, D.Path, Location, "Unknown Joint kind");
			}
		}
		catch (const FSceneContentError&)
		{
			throw;
		}
		catch (const Toolbox::FException& E)
		{
			throw FSceneContentError({D.Path, J.Id, Location, E.What(), 0, 0});
		}
	}
	for (Toolbox::size_t I = 0; I < D.Assets.Size(); ++I)
	{
		const auto& A = D.Assets[I];
		Require(Text(A.Path, L) && Text(A.Font.Family, L), D.Path, "assets/" + A.Id, "Invalid asset path or Font UTF-8");
		const auto Location = "assets/" + A.Id;
		Require(Name(A.Id, L, true), D.Path, Location, "Invalid asset ID");
		for (Toolbox::size_t J = 0; J < I; ++J)
		{
			Require(D.Assets[J].Id != A.Id, D.Path, Location, "Duplicate asset ID");
		}
		Require(A.Kind >= EContentAssetKind::Texture && A.Kind <= EContentAssetKind::Font, D.Path, Location, "Unknown resource kind");
		Require(A.Kind == EContentAssetKind::Font || RelativePath(A.Path), D.Path, Location, "Resource path must stay relative to ProjectRoot");
		if (A.Kind == EContentAssetKind::Font)
		{
			Require(!A.Font.Family.IsEmpty() && A.Font.Size > 0 && A.Font.Thickness > 0, D.Path, Location, "Invalid Font conditions");
		}
		Require(Toolbox::IsFinite(A.Model.TargetUnitMeters) && A.Model.TargetUnitMeters >= 0 && A.Model.SamplesPerSecond > 0 && A.Model.SamplesPerSecond <= 1000, D.Path, Location, "Invalid Model conditions");
	}
	const auto Exports = [&](const auto& Table, bool Imports)
	{
		for (Toolbox::size_t I = 0; I < Table.Size(); ++I)
		{
			const auto& E = Table[I];
			const auto Location = "exports/" + E.Id;
			Require(Name(E.Id, L, Imports), D.Path, Location, "Invalid export ID");
			for (Toolbox::size_t J = 0; J < I; ++J)
			{
				Require(Table[J].Id != E.Id, D.Path, Location, "Duplicate export ID");
			}
			if (E.Kind == EContentExportKind::RigidBody || E.Kind == EContentExportKind::KinematicMover || E.Kind == EContentExportKind::Sensor)
			{
				Require(E.Index < D.Parts.Size() && D.Parts[E.Index].bHasBody, D.Path, Location, "Export Body index mismatch");
				const auto& P = D.Parts[E.Index];
				Require(E.Kind == EContentExportKind::Sensor || P.bMover == (E.Kind == EContentExportKind::KinematicMover), D.Path, Location, "Export Body adapter mismatch");
				if (E.Kind == EContentExportKind::Sensor)
				{
					bool Sensor = false;
					for (const auto& C : P.Colliders)
					{
						Sensor = Sensor || C.Response == EColliderResponse::Sensor;
					}
					Require(Sensor, D.Path, Location, "Sensor export requires Sensor collider");
				}
			}
			else if (E.Kind >= EContentExportKind::Distance && E.Kind <= EContentExportKind::Prismatic)
			{
				const auto Kind = E.Kind == EContentExportKind::Distance   ? EJointKind::Distance
				                  : E.Kind == EContentExportKind::Revolute ? EJointKind::Revolute
				                  : E.Kind == EContentExportKind::Fixed    ? EJointKind::Fixed
				                                                           : EJointKind::Prismatic;
				Require(E.Index < D.Joints.Size() && D.Joints[E.Index].Kind == Kind, D.Path, Location, "Export Joint kind or index mismatch");
			}
			else
			{
				Require(E.Kind >= EContentExportKind::Texture && E.Kind <= EContentExportKind::Font, D.Path, Location, "Unknown export kind");
				const auto Kind = E.Kind == EContentExportKind::Texture ? EContentAssetKind::Texture
				                  : E.Kind == EContentExportKind::Model ? EContentAssetKind::Model
				                  : E.Kind == EContentExportKind::Sound ? EContentAssetKind::Sound
				                                                        : EContentAssetKind::Font;
				Require(E.Index < D.Assets.Size() && D.Assets[E.Index].Kind == Kind, D.Path, Location, "Export resource kind or index mismatch");
			}
		}
	};
	Exports(D.Exports, false);
	Exports(D.Imports, true);
}
void Place(FContentPartDefinition2D& P, const FPrefabSpawnOptions2D& Placement)
{
	if (!Placement.Position.IsValid() || !Toolbox::IsFinite(Placement.Rotation))
	{
		throw Toolbox::FException("Invalid Scene placement");
	}
	P.Body.Position = Placement.Position + GameplayPrivate::RotateJointAnchor(Placement.Rotation, P.Body.Position);
	P.Body.Velocity = GameplayPrivate::RotateJointAnchor(Placement.Rotation, P.Body.Velocity);
	P.Body.Angle += Placement.Rotation;
}
void Place(FContentPartDefinition3D& P, const FPrefabSpawnOptions3D& Placement)
{
	if (!Placement.Position.IsValid())
	{
		throw Toolbox::FException("Invalid Scene placement");
	}
	const auto Q = Placement.Rotation.Normalized();
	P.Body.Position = Placement.Position + Q.Rotate(P.Body.Position);
	P.Body.Velocity = Q.Rotate(P.Body.Velocity);
	P.Body.AngularVelocity = Q.Rotate(P.Body.AngularVelocity);
	P.Body.Orientation = (Q * P.Body.Orientation.Normalized()).Normalized();
}
template <typename TScene, typename TPrefab>
void Scene(const TScene& S, FSceneContentLimits L)
{
	Require(S.Prefabs.Size() == S.Placements.Size() && S.Prefabs.Size() == S.Instances.Size() && S.Prefabs.Size() <= L.MaxParts, S.Path, "$", "Scene table mismatch or Instance limit");
	Require(S.Gravity.IsValid() && S.ViewCount > 0 && S.ViewCount <= 2, S.Path, "$", "Invalid Scene gravity or view count");
	for (Toolbox::uint32 I = 0; I < S.ViewCount; ++I)
	{
		if constexpr (Toolbox::IsSame<TScene, FSceneDefinition2D>)
		{
			Require(IsValidContentView2D(S.Views[I]), S.Path, "views", "Invalid 2D view");
		}
		else
		{
			Require(IsValidRenderView3D(S.Views[I]), S.Path, "views", "Invalid 3D view");
		}
	}
	(void)Toolbox::FFixedStepScheduler(S.FixedUpdate);
	TPrefab AssetTable;
	AssetTable.Path = S.Path;
	AssetTable.Assets = S.Assets;
	Definition(AssetTable, L);
	Toolbox::size_t Parts = 0;
	Toolbox::size_t Joints = S.Connections.Size();
	Require(Joints <= L.MaxJoints, S.Path, "connections", "Scene Joint limit");
	for (Toolbox::size_t I = 0; I < S.Prefabs.Size(); ++I)
	{
		Require(Name(S.Instances[I], L, false), S.Path, "instances", "Invalid Instance ID");
		for (Toolbox::size_t J = 0; J < I; ++J)
		{
			Require(S.Instances[I] != S.Instances[J], S.Path, "instances", "Duplicate Instance ID");
		}
		const auto& P = S.Prefabs[I];
		Definition(P, L);
		Require(P.Parts.Size() <= L.MaxParts - Parts && P.Joints.Size() <= L.MaxJoints - Joints, S.Path, "instances", "Expanded Scene limit");
		Parts += P.Parts.Size();
		Joints += P.Joints.Size();
		// 配置後のoverflowも、資源要求やSpawnより前に検査する。
		for (auto Part : P.Parts)
		{
			Place(Part, S.Placements[I]);
			Body(Part.Body);
			Require(Part.Body.Position.IsValid() && Part.Body.Velocity.IsValid(), S.Path, "placement", "Scene placement overflow");
		}
	}
	for (Toolbox::size_t I = 0; I < S.Connections.Size(); ++I)
	{
		const auto& C = S.Connections[I];
		for (Toolbox::size_t J = 0; J < I; ++J)
		{
			Require(S.Connections[J].Joint.Id != C.Joint.Id, S.Path, "connections", "Duplicate connection ID");
		}
		TPrefab Pair;
		Pair.Path = S.Path;
		const auto Resolve = [&](const FContentSceneEndpoint& E)
		{
			Require(E.Instance < S.Prefabs.Size(), S.Path, "connections", "Connection Instance index mismatch");
			const auto& P = S.Prefabs[E.Instance];
			bool Published = false;
			for (const auto& Candidate : P.Exports)
			{
				Published = Published || (Candidate.Id == E.Export.Id && Candidate.Kind == E.Export.Kind && Candidate.Index == E.Export.Index);
			}
			Require(Published && (E.Export.Kind == EContentExportKind::RigidBody || E.Export.Kind == EContentExportKind::KinematicMover), S.Path, "connections", "Connection is not a declared Body export");
			auto Part = P.Parts[E.Export.Index];
			Place(Part, S.Placements[E.Instance]);
			Part.Colliders.Clear();
			Part.Visual = {};
			Part.Id = Pair.Parts.IsEmpty() ? "endpointA" : "endpointB";
			Pair.Parts.PushBack(Toolbox::Move(Part));
		};
		Resolve(C.BodyA);
		Resolve(C.BodyB);
		Require(C.BodyA.Instance != C.BodyB.Instance || C.BodyA.Export.Index != C.BodyB.Export.Index, S.Path, "connections", "Connection endpoints refer to same Body");
		auto Joint = C.Joint;
		Joint.BodyA = 0;
		Joint.BodyB = 1;
		Pair.Joints.PushBack(Toolbox::Move(Joint));
		Definition(Pair, L);
	}
}

} // namespace Dxf::ContentPrivate
namespace Dxf
{
void ValidatePrefabDefinition(const FPrefabDefinition2D& Definition, FSceneContentLimits Limits)
{
	ContentPrivate::Definition(Definition, Limits);
}
void ValidatePrefabDefinition(const FPrefabDefinition3D& Definition, FSceneContentLimits Limits)
{
	ContentPrivate::Definition(Definition, Limits);
}
void ValidateSceneDefinition(const FSceneDefinition2D& Definition, FSceneContentLimits Limits)
{
	ContentPrivate::Scene<FSceneDefinition2D, FPrefabDefinition2D>(Definition, Limits);
}
void ValidateSceneDefinition(const FSceneDefinition3D& Definition, FSceneContentLimits Limits)
{
	ContentPrivate::Scene<FSceneDefinition3D, FPrefabDefinition3D>(Definition, Limits);
}

} // namespace Dxf
