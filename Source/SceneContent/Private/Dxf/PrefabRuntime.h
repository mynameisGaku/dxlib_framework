// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_PREFABRUNTIME_H
#define DXF_CONTENT_PREFABRUNTIME_H
#include "Dxf/PrefabInstance2D.h"
#include "Dxf/PrefabInstance3D.h"
#include "Dxf/PrePhysicsStep.h"
#include "Dxf/PostPhysicsStep.h"
#include "Dxf/ContentVisuals.h"
#include "Toolbox/Utility.h"
namespace Dxf::ContentPrivate
{
struct FRuntime2D
{
	using FDefinition = FPrefabDefinition2D;
	using FPrepared = FPreparedPrefab2D;
	using FPlacement = FPrefabSpawnOptions2D;
	using FBody = DRigidBody2DComponent;
	using FMover = DKinematicMover2DComponent;
	using FCollider = DCollider2DComponent;
	using FListener = DContactListener2DComponent;
	using FReference = FPhysicsBodyReference2D;
	using FWorld = FPhysicsWorld2D;
	using FMoverDescription = FKinematicMoverDescription2D;
	using FDistance = DDistanceJoint2DComponent;
	using FDistanceDescription = FDistanceJointComponentDescription2D;
	using FRevolute = DRevoluteJoint2DComponent;
	using FRevoluteDescription = FRevoluteJointComponentDescription2D;
	using FFixed = DFixedJoint2DComponent;
	using FFixedDescription = FFixedJointComponentDescription2D;
	using FPrismatic = DPrismaticJoint2DComponent;
	using FPrismaticDescription = FPrismaticJointComponentDescription2D;
	static FWorld* World(const FFixedTickContext& C)
	{
		return C.Physics2D;
	}
	static void Place(FBodyDescription2D& B, const FPlacement& P)
	{
		const auto C = static_cast<Toolbox::f32>(Toolbox::Cos(P.Rotation));
		const auto S = static_cast<Toolbox::f32>(Toolbox::Sin(P.Rotation));
		const auto Rotate = [C, S](Toolbox::FVector2 V)
		{
			return Toolbox::FVector2{C * V.X - S * V.Y, S * V.X + C * V.Y};
		};
		B.Position = P.Position + Rotate(B.Position);
		B.Velocity = Rotate(B.Velocity);
		B.Angle += P.Rotation;
		if (!B.Position.IsValid() || !B.Velocity.IsValid() || !Toolbox::IsFinite(B.Angle))
		{
			throw Toolbox::FException("Prefab placement overflow");
		}
	}
	static void Pose(FMoverDescription& M, const FBodyDescription2D& B)
	{
		M.Pose = {B.Position, B.Angle};
	}
	static void Validate(const FPlacement& P)
	{
		if (!P.Position.IsValid() || !Toolbox::IsFinite(P.Rotation))
		{
			throw Toolbox::FException("Invalid prefab placement");
		}
	}
};
struct FRuntime3D
{
	using FDefinition = FPrefabDefinition3D;
	using FPrepared = FPreparedPrefab3D;
	using FPlacement = FPrefabSpawnOptions3D;
	using FBody = DRigidBody3DComponent;
	using FMover = DKinematicMover3DComponent;
	using FCollider = DCollider3DComponent;
	using FListener = DContactListener3DComponent;
	using FReference = FPhysicsBodyReference3D;
	using FWorld = FPhysicsWorld3D;
	using FMoverDescription = FKinematicMoverDescription3D;
	using FDistance = DDistanceJoint3DComponent;
	using FDistanceDescription = FDistanceJointComponentDescription3D;
	using FRevolute = DRevoluteJoint3DComponent;
	using FRevoluteDescription = FRevoluteJointComponentDescription3D;
	using FFixed = DFixedJoint3DComponent;
	using FFixedDescription = FFixedJointComponentDescription3D;
	using FPrismatic = DPrismaticJoint3DComponent;
	using FPrismaticDescription = FPrismaticJointComponentDescription3D;
	static FWorld* World(const FFixedTickContext& C)
	{
		return C.Physics3D;
	}
	static void Place(FBodyDescription3D& B, const FPlacement& P)
	{
		const auto Q = P.Rotation.Normalized();
		B.Position = P.Position + Q.Rotate(B.Position);
		B.Velocity = Q.Rotate(B.Velocity);
		B.AngularVelocity = Q.Rotate(B.AngularVelocity);
		B.Orientation = (Q * B.Orientation).Normalized();
		if (!B.Position.IsValid() || !B.Velocity.IsValid() || !B.AngularVelocity.IsValid())
		{
			throw Toolbox::FException("Prefab placement overflow");
		}
	}
	static void Pose(FMoverDescription& M, const FBodyDescription3D& B)
	{
		M.Pose = {B.Position, B.Orientation};
	}
	static void Validate(const FPlacement& P)
	{
		if (!P.Position.IsValid())
		{
			throw Toolbox::FException("Invalid prefab placement");
		}
		(void)P.Rotation.Normalized();
	}
};

// 所有は既存Object/Componentに残し、対応表と準備済み定義だけを保持する。
template <typename T>
class TPrefabRuntime
{
public:
	TPrefabRuntime(typename T::FPrepared Prepared, typename T::FPlacement Placement)
	    : m_Prepared(Toolbox::Move(Prepared)), m_Placement(Placement)
	{
		if (!m_Prepared.Definition || !m_Prepared.Resources)
		{
			throw Toolbox::FException("Prefab is not prepared");
		}
		T::Validate(Placement);
		for (const auto& Part : Definition().Parts)
		{
			auto Body = Part.Body;
			T::Place(Body, Placement);
		}
	}
	const typename T::FDefinition& Definition() const noexcept
	{
		return *m_Prepared.Definition;
	}
	EPrefabInstanceState State() const noexcept
	{
		if (m_State == EPrefabInstanceState::Ready && m_pWorld != nullptr)
		{
			for (const auto& P : m_Parts)
			{
				if (P.Reference && !P.Reference->IsAvailable_Internal(*m_pWorld))
				{
					return EPrefabInstanceState::EndpointLost;
				}
				for (const auto& Collider : P.Colliders)
				{
					if (!Collider || !Collider.Get()->GetColliderId())
					{
						return EPrefabInstanceState::EndpointLost;
					}
				}
			}
			for (Toolbox::size_t I = 0; I < m_Joints.Size(); ++I)
			{
				const auto& J = m_Joints[I];
				const auto Kind = Definition().Joints[I].Kind;
				if ((Kind == EJointKind::Distance && !J.Distance) || (Kind == EJointKind::Revolute && !J.Revolute) || (Kind == EJointKind::Fixed && !J.Fixed) || (Kind == EJointKind::Prismatic && !J.Prismatic))
				{
					return EPrefabInstanceState::EndpointLost;
				}
				if (J.Distance && J.Distance.Get()->GetConnectionState() == EJointConnection::EndpointLost)
				{
					return EPrefabInstanceState::EndpointLost;
				}
				if (J.Revolute && J.Revolute.Get()->GetConnectionState() == EJointConnection::EndpointLost)
				{
					return EPrefabInstanceState::EndpointLost;
				}
				if (J.Fixed && J.Fixed.Get()->GetConnectionState() == EJointConnection::EndpointLost)
				{
					return EPrefabInstanceState::EndpointLost;
				}
				if (J.Prismatic && J.Prismatic.Get()->GetConnectionState() == EJointConnection::EndpointLost)
				{
					return EPrefabInstanceState::EndpointLost;
				}
			}
		}
		return m_State;
	}
	void Initialize(DGameObject& Owner, FAssetService& Assets)
	{
		const auto& D = Definition();
		m_Parts.Resize(D.Parts.Size());
		m_Joints.Resize(D.Joints.Size());
		m_Visuals.Reserve(D.Parts.Size());
		for (const auto& Part : D.Parts)
		{
			m_Visuals.Add(Part.Visual, *m_Prepared.Resources, Assets);
		}
		for (Toolbox::size_t I = 0; I < D.Parts.Size(); ++I)
		{
			const auto& Source = D.Parts[I];
			auto& Part = m_Parts[I];
			if (!Source.bHasBody)
			{
				continue;
			}
			auto Body = Source.Body;
			T::Place(Body, m_Placement);
			if (Source.bMover)
			{
				typename T::FMoverDescription Description;
				T::Pose(Description, Body);
				Description.Colliders = Source.Colliders;
				Part.Mover = Add<typename T::FMover>(Owner, Toolbox::Move(Description));
				Part.Reference = T::FReference::FromKinematicMover(Part.Mover);
			}
			else
			{
				Part.Rigid = Add<typename T::FBody>(Owner, Body);
				Part.Reference = T::FReference::FromRigidBody(Part.Rigid);
				Part.Colliders.Reserve(Source.Colliders.Size());
				for (const auto& Collider : Source.Colliders)
				{
					Part.Colliders.PushBack(Add<typename T::FCollider>(Owner, Part.Rigid, Collider));
				}
			}
			bool Sensor = false;
			for (const auto& C : Source.Colliders)
			{
				Sensor = Sensor || C.Response == EColliderResponse::Sensor;
			}
			if (Sensor)
			{
				Part.Listener = Add<typename T::FListener>(Owner, false);
			}
		}
		for (Toolbox::size_t I = 0; I < D.Joints.Size(); ++I)
		{
			const auto& Source = D.Joints[I];
			if (Source.BodyA >= m_Parts.Size() || Source.BodyB >= m_Parts.Size() || !m_Parts[Source.BodyA].Reference || !m_Parts[Source.BodyB].Reference)
			{
				throw Toolbox::FException("Invalid prefab joint endpoints");
			}
			auto& Joint = m_Joints[I];
			const auto A = *m_Parts[Source.BodyA].Reference;
			const auto B = *m_Parts[Source.BodyB].Reference;
			switch (Source.Kind)
			{
			case EJointKind::Distance:
				Joint.Distance = AddJoint<typename T::FDistance, typename T::FDistanceDescription>( Owner, A, B, Source.Distance, Source.bConnect);
				break;
			case EJointKind::Revolute:
				Joint.Revolute = AddJoint<typename T::FRevolute, typename T::FRevoluteDescription>( Owner, A, B, Source.Revolute, Source.bConnect);
				break;
			case EJointKind::Fixed:
				Joint.Fixed = AddJoint<typename T::FFixed, typename T::FFixedDescription>(Owner, A, B, Source.Fixed, Source.bConnect);
				break;
			case EJointKind::Prismatic:
				Joint.Prismatic = AddJoint<typename T::FPrismatic, typename T::FPrismaticDescription>( Owner, A, B, Source.Prismatic, Source.bConnect);
				break;
			default:
				throw Toolbox::FException("Unknown prefab joint kind");
			}
		}
		m_State = RequiresPhysics() ? EPrefabInstanceState::PendingPhysics : EPrefabInstanceState::Ready;
	}
	bool RequiresPhysics() const noexcept
	{
		for (const auto& Part : Definition().Parts)
		{
			if (Part.bHasBody)
			{
				return true;
			}
		}
		return !Definition().Joints.IsEmpty();
	}
	void Tick(const FTickContext& Context)
	{
		if (!Context.Time.bPaused)
		{
			m_Visuals.Advance(Context.Time.DeltaSeconds);
		}
	}
	void Draw(FRenderContext& Render, const FContentView2D& View = {}) const
	{
		const auto Current = State();
		if (Current == EPrefabInstanceState::Destroyed || Current == EPrefabInstanceState::EndpointLost || Current == EPrefabInstanceState::PendingInitialization)
		{
			throw Toolbox::FException("Prefab content is unavailable for drawing");
		}
		if constexpr (Toolbox::IsSame<T, FRuntime2D>)
		{
			if (!IsValidContentView2D(View))
			{
				throw Toolbox::FException("Invalid Content 2D view");
			}
		}
		for (Toolbox::size_t I = 0; I < Definition().Parts.Size(); ++I)
		{
			const auto& Source = Definition().Parts[I];
			if (Source.Visual.Kind == EContentVisualKind::None)
			{
				continue;
			}
			auto Pose = Source.Body;
			T::Place(Pose, m_Placement);
			const auto& Part = m_Parts[I];
			if constexpr (Toolbox::IsSame<T, FRuntime2D>)
			{
				if (Part.Rigid && Part.Rigid.Get()->HasBody())
				{
					Pose.Position = Part.Rigid.Get()->GetRenderPosition();
					Pose.Angle = Part.Rigid.Get()->GetRenderAngle();
				}
				if (Part.Mover && Part.Mover.Get()->GetBodyId())
				{
					Pose.Position = Part.Mover.Get()->GetRenderPosition();
					Pose.Angle = Part.Mover.Get()->GetRenderRotation();
				}
				try
				{
					m_Visuals.Draw2D(I, Source.Visual, *m_Prepared.Resources, Pose.Position, Pose.Angle, View, Render);
				}
				catch (const Toolbox::FException& Error)
				{
					throw Toolbox::FException("parts/" + Source.Id + ": " + Error.What());
				}
			}
			else
			{
				if (Part.Rigid && Part.Rigid.Get()->HasBody())
				{
					Pose.Position = Part.Rigid.Get()->GetRenderPosition();
					Pose.Orientation = Part.Rigid.Get()->GetRenderOrientation();
				}
				if (Part.Mover && Part.Mover.Get()->GetBodyId())
				{
					Pose.Position = Part.Mover.Get()->GetRenderPosition();
					Pose.Orientation = Part.Mover.Get()->GetRenderRotation();
				}
				try
				{
					m_Visuals.Draw3D(I, Source.Visual, *m_Prepared.Resources, Pose.Position, Pose.Orientation, Render);
				}
				catch (const Toolbox::FException& Error)
				{
					throw Toolbox::FException("parts/" + Source.Id + ": " + Error.What());
				}
			}
		}
	}
	void Begin(const FFixedTickContext& C)
	{
		if (State() == EPrefabInstanceState::EndpointLost)
		{
			m_State = EPrefabInstanceState::EndpointLost;
		}
		else
		{
			m_State = EPrefabInstanceState::PendingPhysics;
		}
		auto* World = T::World(C);
		if (World == nullptr || C.PrePhysicsStep == nullptr || C.PostPhysicsStep == nullptr)
		{
			throw Toolbox::FException("Prefab requires a matching PhysicsScene");
		}
		if (m_pWorld != nullptr && m_pWorld != World)
		{
			throw Toolbox::FException("Prefab world changed");
		}
		m_pWorld = World;
	}
	void BeforeStep()
	{
		for (auto& P : m_Parts)
		{
			if (!P.Listener || P.bWatching)
			{
				continue;
			}
			const auto Result = P.Reference->Resolve_Internal(*m_pWorld);
			if (!Result)
			{
				throw Toolbox::FException(Result.Error().Message);
			}
			if (Result.Value())
			{
				P.Listener.Get()->WatchBody(*Result.Value());
				P.bWatching = true;
			}
		}
	}
	void AfterStep()
	{
		if (m_State == EPrefabInstanceState::EndpointLost)
		{
			return;
		}
		for (const auto& P : m_Parts)
		{
			if (P.Reference && !P.Reference->IsAvailable_Internal(*m_pWorld))
			{
				return;
			}
			for (const auto& C : P.Colliders)
			{
				if (!C || !C.Get()->GetColliderId())
				{
					m_State = EPrefabInstanceState::EndpointLost;
					return;
				}
			}
		}
		for (Toolbox::size_t I = 0; I < m_Joints.Size(); ++I)
		{
			const auto& J = m_Joints[I];
			const auto Kind = Definition().Joints[I].Kind;
			if ((Kind == EJointKind::Distance && !J.Distance) || (Kind == EJointKind::Revolute && !J.Revolute) || (Kind == EJointKind::Fixed && !J.Fixed) || (Kind == EJointKind::Prismatic && !J.Prismatic))
			{
				m_State = EPrefabInstanceState::EndpointLost;
				return;
			}
			if (!Definition().Joints[I].bConnect || m_bPhysicsReady)
			{
				continue;
			}
			if ((J.Distance && !J.Distance.Get()->GetObservation()) || (J.Revolute && !J.Revolute.Get()->GetObservation()) || (J.Fixed && !J.Fixed.Get()->GetObservation()) || (J.Prismatic && !J.Prismatic.Get()->GetObservation()))
			{
				return;
			}
		}
		m_State = EPrefabInstanceState::Ready;
		m_bPhysicsReady = true;
	}
	void Shutdown() noexcept
	{
		m_Visuals.Clear();
		m_State = EPrefabInstanceState::Destroyed;
		m_pWorld = nullptr;
	}
	const FContentExportDefinition& Export(Toolbox::FStringView Name, EContentExportKind Kind) const
	{
		if (m_State == EPrefabInstanceState::PendingInitialization || m_State == EPrefabInstanceState::Destroyed || State() == EPrefabInstanceState::EndpointLost)
		{
			throw Toolbox::FException("Prefab export is not available");
		}
		for (const auto& E : Definition().Exports)
		{
			if (E.Id == Name)
			{
				if (E.Kind != Kind)
				{
					throw Toolbox::FException("Prefab export kind mismatch");
				}
				return E;
			}
		}
		throw Toolbox::FException("Prefab export does not exist");
	}
	TObjectHandle<typename T::FBody> Rigid(Toolbox::FStringView N) const
	{
		return Require(m_Parts[PartExport(N, EContentExportKind::RigidBody)].Rigid);
	}
	TObjectHandle<typename T::FMover> Mover(Toolbox::FStringView N) const
	{
		return Require(m_Parts[PartExport(N, EContentExportKind::KinematicMover)].Mover);
	}
	TObjectHandle<typename T::FListener> Listener(Toolbox::FStringView N) const
	{
		return Require(m_Parts[PartExport(N, EContentExportKind::Sensor)].Listener);
	}
	TObjectHandle<typename T::FDistance> Distance(Toolbox::FStringView N) const
	{
		return Require(m_Joints[JointExport(N, EContentExportKind::Distance)].Distance);
	}
	TObjectHandle<typename T::FRevolute> Revolute(Toolbox::FStringView N) const
	{
		return Require(m_Joints[JointExport(N, EContentExportKind::Revolute)].Revolute);
	}
	TObjectHandle<typename T::FFixed> Fixed(Toolbox::FStringView N) const
	{
		return Require(m_Joints[JointExport(N, EContentExportKind::Fixed)].Fixed);
	}
	TObjectHandle<typename T::FPrismatic> Prismatic(Toolbox::FStringView N) const
	{
		return Require(m_Joints[JointExport(N, EContentExportKind::Prismatic)].Prismatic);
	}
	const FSound& Sound(Toolbox::FStringView N) const
	{
		return m_Prepared.Resources->GetSound(Export(N, EContentExportKind::Sound).Index);
	}
	const FTexture& Texture(Toolbox::FStringView N) const
	{
		return m_Prepared.Resources->GetTexture(Export(N, EContentExportKind::Texture).Index);
	}
	const FModel& Model(Toolbox::FStringView N) const
	{
		return m_Prepared.Resources->GetModel(Export(N, EContentExportKind::Model).Index);
	}
	const FFont& Font(Toolbox::FStringView N) const
	{
		return m_Prepared.Resources->GetFont(Export(N, EContentExportKind::Font).Index);
	}

private:
	template <typename C, typename... A>
	static TObjectHandle<C> Add(DGameObject& Owner, A&&... Args)
	{
		auto Result = Owner.AddComponent<C>(Toolbox::Forward<A>(Args)...);
		if (!Result)
		{
			throw Toolbox::FException(Result.Error().Message);
		}
		return Result.Value();
	}
	template <typename C, typename D, typename J>
	static TObjectHandle<C> AddJoint(DGameObject& Owner, typename T::FReference A, typename T::FReference B, const J& Joint, bool Connect)
	{
		auto Result = Add<C>(Owner, D{A, B, Joint});
		if (!Connect)
		{
			Result.Get()->RequestDisconnect();
		}
		return Result;
	}
	template <typename C>
	static TObjectHandle<C> Require(TObjectHandle<C> H)
	{
		if (!H)
		{
			throw Toolbox::FException("Prefab export expired");
		}
		return H;
	}
	Toolbox::uint32 PartExport(Toolbox::FStringView N, EContentExportKind K) const
	{
		const auto I = Export(N, K).Index;
		if (I >= m_Parts.Size())
		{
			throw Toolbox::FException("Invalid prefab part export index");
		}
		return I;
	}
	Toolbox::uint32 JointExport(Toolbox::FStringView N, EContentExportKind K) const
	{
		const auto I = Export(N, K).Index;
		if (I >= m_Joints.Size())
		{
			throw Toolbox::FException("Invalid prefab joint export index");
		}
		return I;
	}
	struct FPartHandles
	{
		TObjectHandle<typename T::FBody> Rigid;
		TObjectHandle<typename T::FMover> Mover;
		TObjectHandle<typename T::FListener> Listener;
		Toolbox::TOptional<typename T::FReference> Reference;
		Toolbox::TVector<TObjectHandle<typename T::FCollider>> Colliders;
		bool bWatching = false;
	};
	struct FJointHandles
	{
		TObjectHandle<typename T::FDistance> Distance;
		TObjectHandle<typename T::FRevolute> Revolute;
		TObjectHandle<typename T::FFixed> Fixed;
		TObjectHandle<typename T::FPrismatic> Prismatic;
	};
	typename T::FPrepared m_Prepared;
	typename T::FPlacement m_Placement;
	Toolbox::TVector<FPartHandles> m_Parts;
	Toolbox::TVector<FJointHandles> m_Joints;
	typename T::FWorld* m_pWorld = nullptr;
	bool m_bPhysicsReady = false;
	EPrefabInstanceState m_State = EPrefabInstanceState::PendingInitialization;
	FContentVisuals m_Visuals;
};
class FPrefabRuntime2D : public TPrefabRuntime<FRuntime2D>
{
public:
	using TPrefabRuntime::TPrefabRuntime;
};
class FPrefabRuntime3D : public TPrefabRuntime<FRuntime3D>
{
public:
	using TPrefabRuntime::TPrefabRuntime;
};
} // namespace Dxf::ContentPrivate
#endif
