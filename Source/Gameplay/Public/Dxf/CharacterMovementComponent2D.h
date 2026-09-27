#pragma once
#include "Dxf/CharacterMovement2D.h"
#include "Dxf/CharacterMovementDescription2D.h"
#include "Dxf/GameObject.h"
#include "Dxf/GameObjectComponent.h"
#include "Dxf/PrePhysicsStep.h"
#include "Dxf/RigidBodyComponent2D.h"
namespace Dxf
{
/**
 * 2Dキャラクターの移動（反復滑り・接地・坂・段差・重力・ジャンプ）をDPhysicsScene2Dの固定更新へ接続する。
 * 入力はデバイスに依存しない要求（SetMoveInput・RequestJump）で受け、同じ固定更新のすべての登録の後、物理Stepの直前に
 * StepCharacterで1回だけ進める。位置はこのComponentだけが決める（Worldの速度積分やSolverでは動かさない）。
 * 自分のBodyを登録する場合は、所有オブジェクトに一つのKinematicのBodyと、Settings.Shapeの形（円、または中心線が
 * Upに沿うカプセル）のColliderを作り、破棄時に解放する。形状の設定を変えると、次の固定更新でColliderの形状を合わせる
 * （IDは変えない）。高さを安全に変える場合はTrySetCapsuleHalfHeightを使う。
 * 同じオブジェクトのDRigidBody2DComponentとは位置の決定権が重なるため、同時には使えない（最初の固定更新で例外）。
 * 動く床への追従や、剛体との押し合いは扱わない（現在の姿勢を障害物として扱うだけ）。
 */
class DCharacterMovement2DComponent : public DGameObjectComponent, private IPrePhysicsStep
{
public:
	/**
	 * 初期条件を受け取り、登録前の状態を構築する。設定は最初の固定更新で検査する。
	 * @param Description 初期条件。
	 */
	explicit DCharacterMovement2DComponent(FCharacterMovementDescription2D Description = {})
	    : m_Description(Description)
	{
		m_State.Center = Description.Center;
		m_PrevCenter = Description.Center;
		m_CurrentCenter = Description.Center;
		// 一時停止中のジャンプ要求を破棄するため、一時停止中もOnTickを受ける（移動は固定更新だけで行う）。
		SetTickWhenPaused(true);
	}
	/**
	 * 希望する水平方向の移動を設定する（次に変更するまで続く）。長さ1で最大速度、1を超える長さは1へ縮める。
	 * @param Move 移動の方向と強さ（Upに直交する成分だけを使う）。
	 */
	void SetMoveInput(Toolbox::FVector2 Move)
	{
		if (!Move.IsValid())
		{
			throw Toolbox::FException("Invalid 2D character move input");
		}
		m_MoveInput = Move;
	}
	/**
	 * 次に実行される固定更新で一度だけジャンプを試みる。固定更新が0回のフレームでは次のフレームへ持ち越し、
	 * 同じフレームの複数の固定更新でも一度しか使わない。一時停止中は破棄する。
	 */
	FORCEINLINE void RequestJump() noexcept
	{
		m_bJumpRequested = true;
	}
	/**
	 * 中心を直接移し、速度・足元・補間履歴を初期化する。登録したBodyも移す。
	 * @param Center 新しい円の中心。
	 */
	void Teleport(Toolbox::FVector2 Center)
	{
		if (!Center.IsValid())
		{
			throw Toolbox::FException("Invalid 2D character teleport");
		}
		m_State = {};
		m_State.Center = Center;
		m_PrevCenter = Center;
		m_CurrentCenter = Center;
		m_Alpha = 0;
		if (m_bHasBody && m_pWorld != nullptr)
		{
			m_pWorld->SetBodyTransform(m_Body, Center, 0);
		}
	}
	/**
	 * カプセルの半高を、足元（中心線の下端の円の底）を保って変える。伸ばした形状がSolidのColliderと重なる（低い天井の
	 * 下など）場合と、まだ固定更新でWorldを受け取っていない場合は変えずにfalseを返す（形状・中心・Colliderは元のまま）。
	 * 変えた場合は、中心・補間の両端（同じ量だけずらし、足元の描画を連続させる）・登録したBodyの位置とColliderの形状
	 * （ID・世代は同じで、接触・Triggerの組は続く）を同時に更新する。足元の状態は足元が動かないため保つ。
	 * Settings.ShapeがCapsuleでない、または半高が有限・非負でない場合はToolbox::FException。
	 * @param HalfHeight 新しい半高。
	 */
	bool TrySetCapsuleHalfHeight(Toolbox::f64 HalfHeight)
	{
		if (m_Description.Settings.Shape != ECharacterShape::Capsule || !Toolbox::IsFinite(HalfHeight) ||
		    HalfHeight < 0)
		{
			throw Toolbox::FException("Invalid 2D character capsule half height");
		}
		if (m_pWorld == nullptr)
		{
			return false;
		}
		Toolbox::TOptional<FBodyId2D> Self;
		if (m_bHasBody)
		{
			Self = m_Body;
		}
		const FCharacterResize2D Result = ResizeCharacterCapsule(*m_pWorld, m_State.Center, m_Description.Settings,
		                                                         HalfHeight, Self, m_Description.Filter);
		if (!Result.bResized)
		{
			return false;
		}
		FCharacterMoveSettings2D Settings = m_Description.Settings;
		Settings.HalfHeight = HalfHeight;
		// Worldの形状と位置を先に更新し、成功してからComponentの状態を変える。
		if (m_bHasBody)
		{
			m_pWorld->SetColliderShape(m_Collider, LocalShape_Internal(Settings));
			m_pWorld->SetBodyTransform(m_Body, Result.Center, 0);
			m_ColliderSettings = Settings;
		}
		const Toolbox::FVector2 Shift = Result.Center - m_State.Center;
		m_Description.Settings = Settings;
		m_State.Center = Result.Center;
		m_PrevCenter = m_PrevCenter + Shift;
		m_CurrentCenter = m_CurrentCenter + Shift;
		return true;
	}
	/**
	 * 設定を変更する（次の固定更新で検査して使う）。
	 * @param Settings 新しい設定。
	 */
	FORCEINLINE void SetSettings(const FCharacterMoveSettings2D& Settings) noexcept
	{
		m_Description.Settings = Settings;
	}
	/**
	 * 現在の設定を返す。
	 */
	FORCEINLINE const FCharacterMoveSettings2D& GetSettings() const noexcept
	{
		return m_Description.Settings;
	}
	/**
	 * 直近の固定更新後の円の中心を返す。
	 */
	FORCEINLINE Toolbox::FVector2 GetCenter() const noexcept
	{
		return m_State.Center;
	}
	/**
	 * 直近の固定更新後の速度を返す。
	 */
	FORCEINLINE Toolbox::FVector2 GetVelocity() const noexcept
	{
		return m_State.Velocity;
	}
	/**
	 * 直近の固定更新後の足元を返す。
	 */
	FORCEINLINE const FCharacterGround2D& GetGround() const noexcept
	{
		return m_State.Ground;
	}
	/**
	 * 歩ける床に立っているかを返す。
	 */
	FORCEINLINE bool IsGrounded() const noexcept
	{
		return m_State.Ground.State == ECharacterGroundState::Walkable;
	}
	/**
	 * 直近の固定更新の結果（着地・離地・天井・段差などの変化と停止理由）を返す。GetStepCount()が0なら初期値。
	 */
	FORCEINLINE const FCharacterStepResult2D& GetLastStep() const noexcept
	{
		return m_LastStep;
	}
	/**
	 * これまでに進めた固定更新の回数を返す。
	 */
	FORCEINLINE Toolbox::int64 GetStepCount() const noexcept
	{
		return m_StepCount;
	}
	/**
	 * 描画用の補間した中心を返す（直前と直近の固定更新の間）。
	 */
	Toolbox::FVector2 GetRenderCenter() const noexcept
	{
		const Toolbox::f32 Alpha = static_cast<Toolbox::f32>(m_Alpha);
		return m_PrevCenter * (1 - Alpha) + m_CurrentCenter * Alpha;
	}
	/**
	 * 登録したBodyのIDを返す。登録していなければ空。
	 */
	FORCEINLINE Toolbox::TOptional<FBodyId2D> GetBodyId() const noexcept
	{
		if (!m_bHasBody)
		{
			return {};
		}
		return m_Body;
	}

protected:
	/**
	 * Worldを確認し、必要なら自分のBodyを登録して、物理Stepの直前の移動を予約する。
	 * @param Context 固定更新の実行環境。
	 */
	void OnFixedTick(const FFixedTickContext& Context) override
	{
		if (Context.Physics2D == nullptr || Context.PrePhysicsStep == nullptr)
		{
			throw Toolbox::FException("DCharacterMovement2DComponent requires DPhysicsScene2D");
		}
		if (m_pWorld != nullptr && m_pWorld != Context.Physics2D)
		{
			throw Toolbox::FException("2D character world changed");
		}
		m_pWorld = Context.Physics2D;
		if (!m_bChecked)
		{
			DGameObject* Owner = GetOwner();
			if (Owner != nullptr && Owner->FindComponent<DRigidBody2DComponent>().Get() != nullptr)
			{
				throw Toolbox::FException(
				    "DCharacterMovement2DComponent cannot share an object with DRigidBody2DComponent");
			}
			m_bChecked = true;
		}
		if (m_Description.bRegisterBody && !m_bHasBody)
		{
			FBodyDescription2D Body;
			Body.Type = EBodyType::Kinematic;
			Body.Position = m_State.Center;
			m_Body = m_pWorld->CreateBody(Body);
			m_bHasBody = true;
			FColliderDescription2D Collider;
			Collider.Shape = LocalShape_Internal(m_Description.Settings);
			Collider.QueryCategory = m_Description.BodyQueryCategory;
			m_Collider = m_pWorld->AttachCollider(m_Body, Collider);
			m_ColliderSettings = m_Description.Settings;
		}
		Context.PrePhysicsStep->Enqueue(*this);
	}
	/**
	 * 一時停止中は未使用のジャンプ要求を破棄する。
	 * @param Context フレーム更新の実行環境。
	 */
	void OnTick(const FTickContext& Context) override
	{
		if (Context.Time.bPaused)
		{
			m_bJumpRequested = false;
		}
	}
	/**
	 * 登録したBodyを解放する。
	 */
	void OnDeinitialize() noexcept override
	{
		if (m_bHasBody && m_pWorld != nullptr)
		{
			m_pWorld->DestroyBody(m_Body);
		}
		m_bHasBody = false;
		m_pWorld = nullptr;
	}

private:
	/**
	 * 同じ固定更新のすべての登録の後に、1回だけ移動を計算して反映する。計算が成功するまで状態は変えない。
	 * @param Context 同じ固定更新の実行環境。
	 */
	void OnPrePhysicsStep_Internal(const FFixedTickContext& Context) override
	{
		if (IsDestroyRequested() || m_pWorld == nullptr)
		{
			return;
		}
		FCharacterMoveInput2D Input;
		Input.Move = m_MoveInput;
		Input.bJump = m_bJumpRequested;
		Toolbox::TOptional<FBodyId2D> Self;
		if (m_bHasBody)
		{
			Self = m_Body;
			// 形状の設定が変わっていれば、Colliderの形状を合わせる（IDは変えない）。
			if (!SameShape_Internal(m_ColliderSettings, m_Description.Settings))
			{
				m_pWorld->SetColliderShape(m_Collider, LocalShape_Internal(m_Description.Settings));
				m_ColliderSettings = m_Description.Settings;
			}
		}
		const FCharacterStepResult2D Result = StepCharacter(*m_pWorld, m_Description.Settings, m_State, Input,
		                                                    Context.DeltaSeconds, Self, m_Description.Filter);
		// ここから反映する（例外時は以前の状態とジャンプ要求を保つ）。
		m_bJumpRequested = false;
		m_PrevCenter = m_State.Center;
		m_State = Result.State;
		m_CurrentCenter = m_State.Center;
		m_LastStep = Result;
		++m_StepCount;
		m_Alpha = Context.InterpolationAlpha;
		if (m_bHasBody)
		{
			m_pWorld->SetBodyTransform(m_Body, m_State.Center, 0);
		}
	}
	/**
	 * 設定の形（Round／Capsule・半径・半高・Up）に合う、Body相対のColliderの形状。Bodyの姿勢は常に単位なので、
	 * カプセルの中心線はUpに沿う。
	 * @param Settings 形状を決める設定。
	 */
	static decltype(FColliderDescription2D::Shape) LocalShape_Internal(const FCharacterMoveSettings2D& Settings)
	{
		if (Settings.Shape != ECharacterShape::Capsule)
		{
			return Toolbox::FCircle2D{{}, Settings.Radius};
		}
		const Toolbox::f64 Length = Toolbox::Sqrt(Toolbox::f64(Toolbox::Dot(Settings.Up, Settings.Up)));
		if (!(Length > 0) || !Toolbox::IsFinite(Settings.HalfHeight) || Settings.HalfHeight < 0)
		{
			throw Toolbox::FException("Invalid 2D character capsule");
		}
		const Toolbox::FVector2 Axis = Settings.Up * static_cast<Toolbox::f32>(Settings.HalfHeight / Length);
		return Toolbox::FCapsule2D{-Axis, Axis, Settings.Radius};
	}
	/**
	 * 二つの設定のColliderの形が同じか（Round／Capsule・半径・半高・Up）。
	 * @param A 一つ目。
	 * @param B 二つ目。
	 */
	static bool SameShape_Internal(const FCharacterMoveSettings2D& A, const FCharacterMoveSettings2D& B) noexcept
	{
		return A.Shape == B.Shape && A.Radius == B.Radius && A.HalfHeight == B.HalfHeight && A.Up == B.Up;
	}
	/**
	 * 初期条件と設定。
	 */
	FCharacterMovementDescription2D m_Description;
	/**
	 * 現在の位置・速度・足元。
	 */
	FCharacterState2D m_State;
	/**
	 * 直近の固定更新の結果。
	 */
	FCharacterStepResult2D m_LastStep;
	/**
	 * 進めた固定更新の回数。
	 */
	Toolbox::int64 m_StepCount = 0;
	/**
	 * 希望する移動（次に変更するまで続く）。
	 */
	Toolbox::FVector2 m_MoveInput;
	/**
	 * 未使用のジャンプ要求。
	 */
	bool m_bJumpRequested = false;
	/**
	 * 同じオブジェクトの剛体との併用を確認したか。
	 */
	bool m_bChecked = false;
	/**
	 * 自分のBodyを登録したか。
	 */
	bool m_bHasBody = false;
	/**
	 * 登録したBody。
	 */
	FBodyId2D m_Body;
	/**
	 * 登録したCollider。
	 */
	FColliderId2D m_Collider;
	/**
	 * 登録したColliderの形を決めた設定（形が変わったかの比較に使う）。
	 */
	FCharacterMoveSettings2D m_ColliderSettings;
	/**
	 * 参照する物理ワールド。所有しない。
	 */
	FPhysicsWorld2D* m_pWorld = nullptr;
	/**
	 * 補間開始点の中心。
	 */
	Toolbox::FVector2 m_PrevCenter;
	/**
	 * 補間終了点の中心。
	 */
	Toolbox::FVector2 m_CurrentCenter;
	/**
	 * 描画補間用の残余割合。
	 */
	Toolbox::f64 m_Alpha = 0;
};
} // namespace Dxf
