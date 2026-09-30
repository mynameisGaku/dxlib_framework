// SPDX-License-Identifier: NOASSERTION
#include "JointConsumer.h"
#include "Dxf/DistanceJointComponent2D.h"
#include "Dxf/DistanceJointComponent3D.h"
#include "Dxf/PhysicsScene2D.h"
#include "Dxf/PhysicsScene3D.h"
#include "Dxf/InputStateTracker.h"
#include "Dxf/RenderContext.h"
namespace
{
// 契約違反はSceneとApplicationの既存エラー経路で返す。
void RequireJoint(bool Condition, const char* Message)
{
	if (!Condition)
	{
		throw Toolbox::FException(Message);
	}
}
// Worldが存在する間のComponent終了を記録する。
template <typename TJoint>
class TObservedJoint final : public TJoint
{
public:
	// 接続設定と、Scene終了まで有効な結果の保存先。
	template <typename TDescription>
	TObservedJoint(TDescription Description, FJointConsumerResult& Result) : TJoint(Description), m_Result(Result)
	{
	}

protected:
	// 製品の終了処理の後に、公開読み取りが失効したことを調べる。
	void OnDeinitialize() noexcept override
	{
		TJoint::OnDeinitialize();
		m_Result.bShutdown = !this->GetJointId() && !this->GetObservation();
	}

private:
	// 呼び出し側が所有する結果。
	FJointConsumerResult& m_Result;
};
// 2Dの実Componentを公開Sceneへ組み込む。
class DConsumer2D final : public Dxf::DGameObject
{
public:
	// 所有Sceneと終了まで有効な結果。
	DConsumer2D(Dxf::DPhysicsScene2D& Scene, FJointConsumerResult& Result) : m_pScene(&Scene), m_Result(Result)
	{
	}

protected:
	Dxf::TResult<void> OnInitialize(const Dxf::FInitContext&) override
	{
		Dxf::FBodyDescription2D Description;
		Description.Type = Dxf::EBodyType::Static;
		Description.Position = {0, 3};
		m_A = AddComponent<Dxf::DRigidBody2DComponent>(Description).Value();
		Description.Type = Dxf::EBodyType::Dynamic;
		Description.Position = {0, 1};
		m_B = AddComponent<Dxf::DRigidBody2DComponent>(Description).Value();
		m_Description.BodyA = Dxf::FPhysicsBodyReference2D::FromRigidBody(m_A);
		m_Description.BodyB = Dxf::FPhysicsBodyReference2D::FromRigidBody(m_B);
		m_Description.Joint.Length = 2;
		m_Joint = AddComponent<TObservedJoint<Dxf::DDistanceJoint2DComponent>>(m_Description, m_Result).Value().Cast<Dxf::DDistanceJoint2DComponent>();
		return {};
	}
	void OnTick(const Dxf::FTickContext&) override
	{
		++m_Frame;
		// 今回調べる接続Component。
		auto* Joint = m_Joint.Get();
		if (m_Frame == 3)
		{
			RequireJoint(static_cast<bool>(Joint->GetObservation()), "external joint initial observation");
			m_First = *Joint->GetJointId();
			m_OldBody = m_B.Get()->GetBodyId();
		}
		if (m_Frame == 10)
		{
			Joint->RequestDisconnect();
		}
		if (m_Frame == 12)
		{
			RequireJoint(Joint->GetConnectionState() == Dxf::EDistanceJointConnection::Disconnected && !Joint->GetJointId(), "external joint disconnect");
			Joint->RequestConnect(m_Description);
		}
		if (m_Frame == 16)
		{
			RequireJoint(Joint->GetJointId() && *Joint->GetJointId() != m_First, "external joint reconnect generation");
			RequireJoint(static_cast<bool>(RemoveComponent(m_B)), "external body removal");
		}
		if (m_Frame == 20)
		{
			RequireJoint(!m_B.Get() && !m_pScene->GetPhysicsWorld().IsAlive(m_OldBody) && Joint->GetConnectionState() == Dxf::EDistanceJointConnection::EndpointLost, "external endpoint loss");
			Dxf::FBodyDescription2D Replacement;
			Replacement.Position = {0, 1};
			m_B = AddComponent<Dxf::DRigidBody2DComponent>(Replacement).Value();
			m_Description.BodyB = Dxf::FPhysicsBodyReference2D::FromRigidBody(m_B);
			Joint->RequestConnect(m_Description);
		}
		if (m_Frame == 26)
		{
			RequireJoint(Joint->GetObservation() && m_B.Get()->GetBodyId() != m_OldBody, "external endpoint regeneration");
			Joint->RequestConnect(m_Description);
			m_First = *Joint->GetJointId();
		}
		if (m_Frame == 40)
		{
			RequireJoint(Joint->GetJointId() && *Joint->GetJointId() == m_First && Toolbox::Abs(Joint->GetObservation()->Error) < 0.05, "external steady joint");
			m_Result.bComplete = true;
		}
	}
	void OnDraw(Dxf::FRenderContext& Render) const override
	{
		// 接続のA側。
		Toolbox::FVector2 A;
		// 接続のB側。
		Toolbox::FVector2 B;
		if (m_Joint.Get() == nullptr || !m_Joint.Get()->GetRenderAnchors(A, B))
		{
			return;
		}
		Dxf::FDrawStyle Style;
		Style.Color = {170, 105, 230, 255};
		RequireJoint(static_cast<bool>(Render.Get2D().FillCircle({640 + B.X * 80, 540 - B.Y * 80}, 18, Style)), "external joint weight draw");
		Style.Color = {255, 230, 100, 255};
		RequireJoint(static_cast<bool>(Render.Get2D().DrawLine({640 + A.X * 80, 540 - A.Y * 80}, {640 + B.X * 80, 540 - B.Y * 80}, Style)), "external joint guide draw");
		m_Result.RenderWeight = {B.X, B.Y, 0};
	}

private:
	// WorldはSceneが所有し、このObjectより長く有効。
	Dxf::DPhysicsScene2D* m_pScene;
	// 終了後も呼び出し側に残る結果。
	FJointConsumerResult& m_Result;
	// 接続先を所有するComponentの世代付き参照。
	Dxf::TObjectHandle<Dxf::DRigidBody2DComponent> m_A;
	Dxf::TObjectHandle<Dxf::DRigidBody2DComponent> m_B;
	Dxf::TObjectHandle<Dxf::DDistanceJoint2DComponent> m_Joint;
	// 明示的な再接続設定。
	Dxf::FDistanceJointComponentDescription2D m_Description;
	// 旧登録と再利用を区別するID。
	Dxf::FJointId2D m_First;
	Dxf::FBodyId2D m_OldBody;
	// 固定更新後のフレーム番号。
	Toolbox::int32 m_Frame = 0;
};
// 既存Sceneの固定更新・終了契約をそのまま使う。
class DConsumerScene2D final : public Dxf::DPhysicsScene2D
{
public:
	explicit DConsumerScene2D(FJointConsumerResult& Result) : m_Result(Result)
	{
	}

protected:
	Dxf::TResult<void> OnInitialize(const Dxf::FInitContext&) override
	{
		if (!Spawn<DConsumer2D>(*this, m_Result))
		{
			return Dxf::TResult<void>::Failure(Dxf::EErrorCode::InvalidState, "external joint scene");
		}
		return {};
	}

private:
	// 呼び出し側が所有する保存先。
	FJointConsumerResult& m_Result;
};
// 3Dの実Componentを公開Sceneへ組み込む。
class DConsumer3D final : public Dxf::DGameObject
{
public:
	// 所有Sceneと終了まで有効な結果。
	DConsumer3D(Dxf::DPhysicsScene3D& Scene, FJointConsumerResult& Result) : m_pScene(&Scene), m_Result(Result)
	{
	}

protected:
	Dxf::TResult<void> OnInitialize(const Dxf::FInitContext&) override
	{
		Dxf::FBodyDescription3D Description;
		Description.Type = Dxf::EBodyType::Static;
		Description.Position = {0, 3, 0};
		m_A = AddComponent<Dxf::DRigidBody3DComponent>(Description).Value();
		Description.Type = Dxf::EBodyType::Dynamic;
		Description.Position = {0, 1, 0};
		m_B = AddComponent<Dxf::DRigidBody3DComponent>(Description).Value();
		m_Description.BodyA = Dxf::FPhysicsBodyReference3D::FromRigidBody(m_A);
		m_Description.BodyB = Dxf::FPhysicsBodyReference3D::FromRigidBody(m_B);
		m_Description.Joint.Length = 2;
		m_Joint = AddComponent<TObservedJoint<Dxf::DDistanceJoint3DComponent>>(m_Description, m_Result).Value().Cast<Dxf::DDistanceJoint3DComponent>();
		return {};
	}
	void OnTick(const Dxf::FTickContext&) override
	{
		++m_Frame;
		// 今回調べる接続Component。
		auto* Joint = m_Joint.Get();
		if (m_Frame == 3)
		{
			RequireJoint(static_cast<bool>(Joint->GetObservation()), "external joint initial observation");
			m_First = *Joint->GetJointId();
			m_OldBody = m_B.Get()->GetBodyId();
		}
		if (m_Frame == 10)
		{
			Joint->RequestDisconnect();
		}
		if (m_Frame == 12)
		{
			RequireJoint(Joint->GetConnectionState() == Dxf::EDistanceJointConnection::Disconnected && !Joint->GetJointId(), "external joint disconnect");
			Joint->RequestConnect(m_Description);
		}
		if (m_Frame == 16)
		{
			RequireJoint(Joint->GetJointId() && *Joint->GetJointId() != m_First, "external joint reconnect generation");
			RequireJoint(static_cast<bool>(RemoveComponent(m_B)), "external body removal");
		}
		if (m_Frame == 20)
		{
			RequireJoint(!m_B.Get() && !m_pScene->GetPhysicsWorld().IsAlive(m_OldBody) && Joint->GetConnectionState() == Dxf::EDistanceJointConnection::EndpointLost, "external endpoint loss");
			Dxf::FBodyDescription3D Replacement;
			Replacement.Position = {0, 1, 0};
			m_B = AddComponent<Dxf::DRigidBody3DComponent>(Replacement).Value();
			m_Description.BodyB = Dxf::FPhysicsBodyReference3D::FromRigidBody(m_B);
			Joint->RequestConnect(m_Description);
		}
		if (m_Frame == 26)
		{
			RequireJoint(Joint->GetObservation() && m_B.Get()->GetBodyId() != m_OldBody, "external endpoint regeneration");
			Joint->RequestConnect(m_Description);
			m_First = *Joint->GetJointId();
		}
		if (m_Frame == 40)
		{
			RequireJoint(Joint->GetJointId() && *Joint->GetJointId() == m_First && Toolbox::Abs(Joint->GetObservation()->Error) < 0.05, "external steady joint");
			m_Result.bComplete = true;
		}
	}
	void OnDraw(Dxf::FRenderContext& Render) const override
	{
		// 接続のA側。
		Toolbox::FVector3 A;
		// 接続のB側。
		Toolbox::FVector3 B;
		if (m_Joint.Get() == nullptr || !m_Joint.Get()->GetRenderAnchors(A, B))
		{
			return;
		}
		RequireJoint(static_cast<bool>(Render.Get3D().SetView(GetJointConsumerView())), "external joint view");
		Dxf::FDrawStyle3D Style;
		Style.Color = {170, 105, 230, 255};
		RequireJoint(static_cast<bool>(Render.Get3D().DrawSphere({B, 0.25f}, Style, 16)), "external joint weight draw");
		Style.Color = {255, 230, 100, 255};
		RequireJoint(static_cast<bool>(Render.Get3D().DrawLine(A, B, Style)), "external joint guide draw");
		m_Result.RenderWeight = B;
	}

private:
	// WorldはSceneが所有し、このObjectより長く有効。
	Dxf::DPhysicsScene3D* m_pScene;
	// 終了後も呼び出し側に残る結果。
	FJointConsumerResult& m_Result;
	// 接続先を所有するComponentの世代付き参照。
	Dxf::TObjectHandle<Dxf::DRigidBody3DComponent> m_A;
	Dxf::TObjectHandle<Dxf::DRigidBody3DComponent> m_B;
	Dxf::TObjectHandle<Dxf::DDistanceJoint3DComponent> m_Joint;
	// 明示的な再接続設定。
	Dxf::FDistanceJointComponentDescription3D m_Description;
	// 旧登録と再利用を区別するID。
	Dxf::FJointId3D m_First;
	Dxf::FBodyId3D m_OldBody;
	// 固定更新後のフレーム番号。
	Toolbox::int32 m_Frame = 0;
};
// 既存Sceneの固定更新・終了契約をそのまま使う。
class DConsumerScene3D final : public Dxf::DPhysicsScene3D
{
public:
	explicit DConsumerScene3D(FJointConsumerResult& Result) : m_Result(Result)
	{
	}

protected:
	Dxf::TResult<void> OnInitialize(const Dxf::FInitContext&) override
	{
		if (!Spawn<DConsumer3D>(*this, m_Result))
		{
			return Dxf::TResult<void>::Failure(Dxf::EErrorCode::InvalidState, "external joint scene");
		}
		return {};
	}

private:
	// 呼び出し側が所有する保存先。
	FJointConsumerResult& m_Result;
};
} // namespace
Dxf::FRenderView3D GetJointConsumerView()
{
	Dxf::FRenderView3D View;
	View.Eye = {0, 3, -8};
	View.Target = {0, 1.5f, 0};
	return View;
}
Toolbox::TUniquePtr<Dxf::DScene> MakeJointConsumer2D(FJointConsumerResult& Result)
{
	return Toolbox::MakeUnique<DConsumerScene2D>(Result);
}
Toolbox::TUniquePtr<Dxf::DScene> MakeJointConsumer3D(FJointConsumerResult& Result)
{
	return Toolbox::MakeUnique<DConsumerScene3D>(Result);
}
Toolbox::int32 RunJointConsumer(Dxf::FAssetService& Assets, Dxf::FAudioPlayer& Audio)
{
	for (Toolbox::int32 Dimension = 0; Dimension < 2; ++Dimension)
	{
		// 呼び出しの成否または記録先。
		FJointConsumerResult Result;
		Dxf::FSceneNavigator Navigator(Assets, Audio);
		Dxf::FInputStateTracker Input;
		Dxf::FFrameTime Time;
		Time.DeltaSeconds = 1.0 / 60;
		Time.UnscaledDeltaSeconds = Time.DeltaSeconds;
		if (!Navigator.RequestChange(Dimension == 0 ? MakeJointConsumer2D(Result) : MakeJointConsumer3D(Result)) || !Navigator.Commit())
		{
			return 501 + Dimension;
		}
		for (Toolbox::int32 Frame = 0; Frame < 50; ++Frame)
		{
			if (!Navigator.CommitObjects() || !Navigator.Tick(Time, Input.GetSnapshot()))
			{
				return 503 + Dimension;
			}
		}
		Navigator.Shutdown();
		if (!Result.bComplete || !Result.bShutdown)
		{
			return 505 + Dimension;
		}
	}
	return 0;
}
