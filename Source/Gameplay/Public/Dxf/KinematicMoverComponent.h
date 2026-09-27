#pragma once
#include "Dxf/GameObject.h"
#include "Dxf/GameObjectComponent.h"
#include "Toolbox/Function.h"
#include "Toolbox/Optional.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * Kinematicの物体（動く床・リフト・回転床・扉）の位置と向き。
 * @tparam TTraits 次元の型と操作。
 */
template <typename TTraits> struct TKinematicPose
{
	/**
	 * 重心の位置。
	 */
	typename TTraits::FVector Position;
	/**
	 * 向き（2Dはラジアン角、3Dは単位四元数）。
	 */
	typename TTraits::FRotation Rotation = TTraits::IdentityRotation();
};
/**
 * Kinematicの物体の登録内容。
 * @tparam TTraits 次元の型と操作。
 */
template <typename TTraits> struct TKinematicMoverDescription
{
	/**
	 * 取り付ける形状（重心からの相対。区分・衝突フィルターもそのまま使う）。
	 */
	Toolbox::TVector<typename TTraits::FColliderDescription> Colliders;
	/**
	 * 初期の位置と向き。
	 */
	TKinematicPose<TTraits> Pose;
	/**
	 * 通常の移動で許す最大の速さ（メートル毎秒、有限な正値）。超える目標は、この速さで目標へ向かう（保守的に遅れる）。
	 */
	Toolbox::f64 MaxLinearSpeed = 20;
	/**
	 * 通常の移動で許す最大の角速度（ラジアン毎秒、有限な正値）。1回の固定更新の回転は最大でπ未満にする。
	 */
	Toolbox::f64 MaxAngularSpeed = 6.283185307179586;
};
/**
 * Kinematicの物体を固定更新で動かすComponent。自分のBody・Colliderを作り、位置・向きの決定権を持つ（剛体Componentと併用しない）。
 * 各固定更新で、次の物理Stepの後に目標の位置・向きへ着く速度・角速度を設定するだけで、位置は物理Stepが一度だけ積分する
 * （先に位置を動かしてから同じ速度でもう一度積分する二重の移動をしない）。速度はこのComponentの固定更新で決まり、
 * キャラクター移動は物理Stepの直前（全ての固定更新の後）にその速度を参照するため、登録順に依存しない。
 * 目標は時刻の関数（SetPath）か一回の目標（SetTarget）で与える。速度・角速度の上限を超える目標には上限で向かう（通過のための瞬間移動はしない）。
 * 瞬間移動はTeleportだけで、乗っている物体を運ばない（速度0で置き直す）。
 * @tparam TTraits 次元の型と操作（FKinematicMoverTraits2D／FKinematicMoverTraits3D）。
 */
template <typename TTraits> class TKinematicMoverComponent : public DGameObjectComponent
{
public:
	/**
	 * 位置と向き。
	 */
	using FPose = TKinematicPose<TTraits>;
	/**
	 * BodyのID。
	 */
	using FBodyId = typename TTraits::FBodyId;
	/**
	 * @param Description 登録内容。上限が有限な正値でない場合は例外で通知する。
	 */
	explicit TKinematicMoverComponent(TKinematicMoverDescription<TTraits> Description)
	    : m_Description(Toolbox::Move(Description))
	{
		if (!Toolbox::IsFinite(m_Description.MaxLinearSpeed) || !(m_Description.MaxLinearSpeed > 0) ||
		    !Toolbox::IsFinite(m_Description.MaxAngularSpeed) || !(m_Description.MaxAngularSpeed > 0))
		{
			throw Toolbox::FException("Invalid kinematic mover speed limit");
		}
		m_PrevPose = m_Description.Pose;
	}
	/**
	 * 経過秒数から位置・向きを返す関数を設定する（固定更新の累計秒数。一時停止中は進まない）。空の関数で解除する。
	 * @param Path 経過秒数から、その時刻の位置・向きを返す関数。
	 */
	void SetPath(Toolbox::TFunction<FPose(Toolbox::f64)> Path)
	{
		m_Path = Toolbox::Move(Path);
	}
	/**
	 * 次の固定更新の物理Stepの後に着く目標を一度だけ設定する（経路を設定している間は経路が優先）。
	 * @param Pose 目標の位置・向き。
	 */
	void SetTarget(const FPose& Pose)
	{
		if (!Pose.Position.IsValid())
		{
			throw Toolbox::FException("Invalid kinematic mover target");
		}
		m_Target = Pose;
	}
	/**
	 * 位置・向きを瞬間的に置き直し、速度を0にする。乗っている物体は運ばず、補間の履歴も置き直す。
	 * @param Pose 新しい位置・向き。
	 */
	void Teleport(const FPose& Pose)
	{
		if (!Pose.Position.IsValid())
		{
			throw Toolbox::FException("Invalid kinematic mover teleport");
		}
		m_Description.Pose = Pose;
		m_PrevPose = Pose;
		m_Target.Reset();
		++m_Teleports;
		if (m_bHasBody && m_pWorld != nullptr)
		{
			TTraits::Teleport(*m_pWorld, m_Body, Pose);
		}
	}
	/**
	 * 固定更新の累計秒数（経路の時刻）を設定する。
	 * @param Seconds 新しい累計秒数。
	 */
	void SetElapsedSeconds(Toolbox::f64 Seconds) noexcept
	{
		m_Elapsed = Seconds;
	}
	/**
	 * 固定更新の累計秒数。
	 */
	FORCEINLINE Toolbox::f64 GetElapsedSeconds() const noexcept
	{
		return m_Elapsed;
	}
	/**
	 * 直前の固定更新で、目標が上限を超えて速度・角速度を制限したか。
	 */
	FORCEINLINE bool WasLimited() const noexcept
	{
		return m_bLimited;
	}
	/**
	 * Teleportの回数。
	 */
	FORCEINLINE Toolbox::uint64 GetTeleportCount() const noexcept
	{
		return m_Teleports;
	}
	/**
	 * Body（未作成なら空）。
	 */
	Toolbox::TOptional<FBodyId> GetBodyId() const noexcept
	{
		if (!m_bHasBody)
		{
			return {};
		}
		return m_Body;
	}
	/**
	 * 物理の現在の位置・向き（直前の物理Stepの後）。未作成なら初期の値。
	 */
	FPose GetPose() const
	{
		if (!m_bHasBody || m_pWorld == nullptr)
		{
			return m_Description.Pose;
		}
		return TTraits::GetPose(*m_pWorld, m_Body);
	}
	/**
	 * 描画用の位置（直前の固定更新の開始と物理Stepの後の間を補間する）。
	 */
	typename TTraits::FVector GetRenderPosition() const
	{
		const FPose Current = GetPose();
		const Toolbox::f32 Alpha = static_cast<Toolbox::f32>(m_Alpha);
		return m_PrevPose.Position * (1 - Alpha) + Current.Position * Alpha;
	}
	/**
	 * 描画用の向き（補間する）。
	 */
	typename TTraits::FRotation GetRenderRotation() const
	{
		return TTraits::Interpolate(m_PrevPose.Rotation, GetPose().Rotation, m_Alpha);
	}

protected:
	void OnFixedTick(const FFixedTickContext& Context) override
	{
		typename TTraits::FWorld* World = TTraits::GetWorld(Context);
		if (World == nullptr)
		{
			throw Toolbox::FException(Toolbox::FString("Kinematic mover requires DPhysicsScene") + TTraits::Name);
		}
		if (m_pWorld != nullptr && m_pWorld != World)
		{
			throw Toolbox::FException("Kinematic mover world changed");
		}
		m_pWorld = World;
		if (!m_bHasBody)
		{
			CreateBody_Internal(*World);
		}
		// 物理Stepの開始時の位置・向き（補間の開始点）。
		const FPose Current = TTraits::GetPose(*World, m_Body);
		m_PrevPose = Current;
		m_Alpha = Context.InterpolationAlpha;
		const Toolbox::f64 Seconds = Context.DeltaSeconds;
		// 次の物理Stepの後に着く目標。
		FPose Goal = Current;
		if (m_Path)
		{
			Goal = m_Path(m_Elapsed + Seconds);
		}
		else if (m_Target)
		{
			Goal = *m_Target;
			m_Target.Reset();
		}
		if (!Goal.Position.IsValid())
		{
			throw Toolbox::FException("Kinematic mover path returned an invalid pose");
		}
		m_Elapsed += Seconds;
		// 位置の差を速度に、向きの差（最短の回転）を角速度にする。上限を超えたら上限の大きさで目標へ向かう。
		typename TTraits::FVector Velocity =
		    (Goal.Position - Current.Position) * static_cast<Toolbox::f32>(1.0 / Seconds);
		auto Angular = TTraits::AngularVelocity(Current.Rotation, Goal.Rotation, Seconds);
		m_bLimited = false;
		const Toolbox::f64 Speed = TTraits::Length(Velocity);
		if (Speed > m_Description.MaxLinearSpeed)
		{
			Velocity = Velocity * static_cast<Toolbox::f32>(m_Description.MaxLinearSpeed / Speed);
			m_bLimited = true;
		}
		const Toolbox::f64 Spin = TTraits::AngularSpeed(Angular);
		const Toolbox::f64 MaxSpin = Toolbox::Min(m_Description.MaxAngularSpeed, 3.0 / Seconds);
		if (Spin > MaxSpin)
		{
			Angular = TTraits::ScaleAngular(Angular, MaxSpin / Spin);
			m_bLimited = true;
		}
		TTraits::SetMotion(*World, m_Body, Velocity, Angular);
	}
	void OnDeinitialize() noexcept override
	{
		if (m_bHasBody && m_pWorld != nullptr)
		{
			(void)m_pWorld->DestroyBody(m_Body);
		}
		m_bHasBody = false;
		m_pWorld = nullptr;
	}

private:
	void CreateBody_Internal(typename TTraits::FWorld& World)
	{
		m_Body = World.CreateBody(TTraits::BodyDescription(m_Description.Pose));
		try
		{
			for (Toolbox::size_t Index = 0; Index < m_Description.Colliders.Size(); ++Index)
			{
				(void)World.AttachCollider(m_Body, m_Description.Colliders[Index]);
			}
		}
		catch (...)
		{
			(void)World.DestroyBody(m_Body);
			throw;
		}
		m_bHasBody = true;
	}
	/**
	 * 登録内容（Teleportで初期の位置・向きを更新する）。
	 */
	TKinematicMoverDescription<TTraits> m_Description;
	/**
	 * 経路。
	 */
	Toolbox::TFunction<FPose(Toolbox::f64)> m_Path;
	/**
	 * 一回の目標。
	 */
	Toolbox::TOptional<FPose> m_Target;
	/**
	 * 補間の開始点（直前の固定更新の開始時）。
	 */
	FPose m_PrevPose;
	/**
	 * 補間の割合。
	 */
	Toolbox::f64 m_Alpha = 0;
	/**
	 * 固定更新の累計秒数。
	 */
	Toolbox::f64 m_Elapsed = 0;
	/**
	 * 参照するWorld（所有しない）。
	 */
	typename TTraits::FWorld* m_pWorld = nullptr;
	/**
	 * 自分で作ったBody。
	 */
	FBodyId m_Body;
	bool m_bHasBody = false;
	/**
	 * 直前の固定更新で上限により制限したか。
	 */
	bool m_bLimited = false;
	/**
	 * Teleportの回数。
	 */
	Toolbox::uint64 m_Teleports = 0;
};
} // namespace Dxf
