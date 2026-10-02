// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_SAVE_COURSE_CONTROLLER_H
#define DXF_SAMPLE_SAVE_COURSE_CONTROLLER_H
#include "SaveCourseInput.h"
#include "SaveCourseProgressCodec.h"
#include "Dxf/GameObject.h"
#include "Dxf/CheckpointSceneState.h"
#include "Dxf/ContactNotice.h"
#include "Dxf/InputProfileCodec.h"
#include "Dxf/UserSettingsCodec.h"
#include "Dxf/JointTargetMotion.h"
#include "Dxf/AudioPlayer.h"
#include "Toolbox/JsonWriter.h"
namespace Dxf::GameplaySample
{
/**
 * Sceneに所有されるゲーム判断。型付き公開先を一度解決し、進行は成功PostPhysics後に反映する。
 * @tparam TScene 対応する既存チェックポイントScene。
 * @tparam TBody 型付きプレイヤーBody Component。
 * @tparam TPrefab Collection所有のPrefab。
 * @tparam TPrepared 所有側で準備した荷物資源。
 * @tparam TInstance 動的個体の論理値。
 * @tparam TJoint 対応する直動Joint Component。
 * @tparam TSensor 対応するContact Listener Component。
 */
template <typename TScene, typename TBody, typename TPrefab, typename TPrepared, typename TInstance, typename TJoint, typename TSensor>
class TSaveCourseController final : public DGameObject
{
public:
	/**
	 * @param Scene 同じCollectionを所有するScene。Workerへ捕捉しない。
	 * @param Cargo 準備済みの許可荷物。初期化中に再生しない。
	 * @param Settings 完全な設定。Scene切替でゲーム進行と混同しない。
	 */
	TSaveCourseController(TScene& Scene, TPrepared Cargo, FUserSettings Settings) : m_Scene(Scene), m_Cargo(Toolbox::Move(Cargo)), m_Settings(Toolbox::Move(Settings)), m_Policy(FSaveCourseInput::Policy()), m_Progress(FSaveCourseProgressCodec::Decode(Scene.GetGamePayload()))
	{
		FInputProfileCodec::Apply(m_Settings.Input, m_Policy, m_Input, {});
		(void)FUserSettingsCodec::Encode(m_Settings, m_Policy, "dxfGameplaySave");
		m_Progress.CollectedKeys.Reserve(2);
		m_bGoalOpen[0] = m_Progress.bDoorOpen[0];
		m_bGoalOpen[1] = m_Progress.bDoorOpen[1];
	}
	/**
	 * 捕捉された購読をmemberの解放より先に終了する。
	 */
	~TSaveCourseController() override
	{
		Shutdown_Internal();
	}
	/**
	 * 現在のゲーム値。採取済み文書とは区別する。
	 */
	const FSaveCourseProgress& GetProgress() const noexcept
	{
		return m_Progress;
	}
	/**
	 * @param Settings 候補の全設定。検証・確保失敗では現Mapと設定を保持する。
	 * @param Current UI消費前の元入力。押し続けは一度解放するまで使わない。
	 */
	void ApplySettings(FUserSettings Settings, const FInputSnapshot& Current)
	{
		(void)FUserSettingsCodec::Encode(Settings, m_Policy, "dxfGameplaySave");
		FInputProfileCodec::Apply(Settings.Input, m_Policy, m_Input, Current);
		m_Settings = Toolbox::Move(Settings);
		m_bJump = false;
		m_bInteract = false;
		m_bSpawn = false;
		m_bVolumeChanged = true;
	}
	/**
	 * @param Routed UIが消費した後のSnapshot。物理計画の前に一度だけ更新する。
	 * @param Raw UIの背後で押し続けている入力を抑止する元Snapshot。
	 * @param bPaused Pause中は未処理のゲーム要求を破棄する。
	 */
	void UpdateInput(const FInputSnapshot& Routed, const FInputSnapshot& Raw, bool bPaused)
	{
		Bind();
		// 消費済みの偽の解放で解除待ちを終えないよう、更新の後に元入力で抑止する。
		m_Input.Update(Routed);
		bool Disconnected = false;
		for (Toolbox::size_t Pad = 0; Pad < 4; ++Pad)
		{
			const bool Connected = Raw.GetRaw().Pads[Pad].bConnected;
			Disconnected = Disconnected || (m_bPadConnected[Pad] && !Connected);
			m_bPadConnected[Pad] = Connected;
		}
		if (IsMasked(Routed, Raw) || bPaused || !Raw.GetRaw().bFocused || Disconnected)
		{
			m_Input.SuppressUntilRelease(Raw);
			m_bJump = false;
			m_bInteract = false;
			m_bSpawn = false;
			return;
		}
		// 固定更新0回でも押下を保持し、最初の通常固定更新だけで消費する。
		m_bJump = m_bJump || m_Input.WasPressed(m_Jump);
		m_bInteract = m_bInteract || m_Input.WasPressed(m_Interact);
		m_bSpawn = m_bSpawn || m_Input.WasPressed(m_Spawn);
	}
	/**
	 * @param Door 0はdoorA、1はdoorB。別個体へ適用しない。
	 * @param bOpen 解錠済みの扉へ有限Motor目標を渡す。未解錠はfalse。
	 */
	bool RequestDoor(Toolbox::uint32 Door, bool bOpen) noexcept
	{
		if (Door >= 2 || !m_Progress.bDoorUnlocked[Door])
		{
			return false;
		}
		m_bGoalOpen[Door] = bOpen;
		return true;
	}
	/**
	 * 次の通常固定更新で一個だけ生成する。同じFrameのView数で増えない。
	 */
	void RequestCargo() noexcept
	{
		m_bSpawn = true;
	}
	/**
	 * @param Context 全PostPhysicsが成功した一貫した境界。World.Stepを追加しない。
	 */
	void Complete(const FFixedTickContext& Context)
	{
		if (!IsReady())
		{
			return;
		}
		for (Toolbox::uint32 Index = 0; Index < 2; ++Index)
		{
			if (m_bKeyTouched[Index] && !HasKey(m_KeyNames[Index]))
			{
				Increment(m_Progress.Revision);
				if (m_Progress.Score > Toolbox::TNumericLimits<Toolbox::uint64>::Max() - 10)
				{
					throw Toolbox::FException("Save course score overflow");
				}
				m_Progress.CollectedKeys.PushBack(m_KeyNames[Index]);
				m_Progress.Score += 10;
				m_Progress.bDoorUnlocked[Index] = true;
				m_Scene.GetCheckpointCapture().Destroy(m_Scene, m_KeyNames[Index]);
				m_bDirty = true;
				m_bCue = true;
			}
			m_bKeyTouched[Index] = false;
			if (m_Doors[Index])
			{
				const auto Observation = m_Doors[Index].Get()->GetObservation();
				if (Observation)
				{
					const auto Command = DoorCommand(Index, Context.DeltaSeconds);
					if (Command.State == EJointTargetState::Reached && m_Progress.bDoorOpen[Index] != m_bGoalOpen[Index])
					{
						Increment(m_Progress.Revision);
						m_Progress.bDoorOpen[Index] = m_bGoalOpen[Index];
						m_bDirty = true;
					}
				}
			}
		}
		if (m_bCheckpointTouched && m_Progress.Checkpoint != "north")
		{
			Increment(m_Progress.Revision);
			m_Progress.Checkpoint = "north";
			m_bDirty = true;
		}
		m_bCheckpointTouched = false;
		if (m_bDirty)
		{
			m_Scene.SetGamePayload(FSaveCourseProgressCodec::Encode(m_Progress));
			m_Scene.GetCheckpointCapture().RequestCapture(m_Progress.Checkpoint);
			m_bDirty = false;
		}
	}

protected:
	/**
	 * @param Context 通常固定更新。Impulseと専任Joint要求だけを使う。
	 */
	void OnFixedTick(const FFixedTickContext& Context) override
	{
		if (!IsReady() || !m_Player || !m_Player.Get()->HasBody())
		{
			return;
		}
		// 入力から力を要求する。Dynamicの座標・速度を代入しない。
		auto Force = m_Player.Get()->GetGamePosition();
		Force = {};
		Force.X = 12 * (static_cast<Toolbox::f32>(m_Input.IsDown(m_Right)) - static_cast<Toolbox::f32>(m_Input.IsDown(m_Left)));
		Force.Y = 12 * (static_cast<Toolbox::f32>(m_Input.IsDown(m_Up)) - static_cast<Toolbox::f32>(m_Input.IsDown(m_Down)));
		if (Force.X != 0 || Force.Y != 0)
		{
			m_Player.Get()->AddForce(Force);
		}
		if (m_bJump)
		{
			auto Impulse = Force;
			Impulse = {};
			Impulse.Y = 2;
			m_Player.Get()->AddImpulse(Impulse);
			m_bJump = false;
		}
		if (m_bInteract)
		{
			(void)RequestDoor(0, !m_bGoalOpen[0]);
			m_bInteract = false;
		}
		if (m_bSpawn)
		{
			SpawnCargo();
			m_bSpawn = false;
		}
		for (Toolbox::uint32 Index = 0; Index < 2; ++Index)
		{
			if (m_Doors[Index] && m_Doors[Index].Get()->GetObservation())
			{
				m_Doors[Index].Get()->RequestDrive(DoorCommand(Index, Context.DeltaSeconds).Drive);
			}
		}
	}
	/**
	 * @param Context Scene有効化後の通常更新。初期化や描画では再生しない。
	 */
	void OnTick(const FTickContext& Context) override
	{
		if (Context.Audio == nullptr || Context.Time.bPaused)
		{
			return;
		}
		if (m_bVolumeChanged)
		{
			for (const auto Playback : m_Playbacks)
			{
				if (Playback)
				{
					const auto Changed = Context.Audio->SetVolume(Playback, m_Settings.Volume);
					if (!Changed)
					{
						throw Toolbox::FException(Changed.Error().Message);
					}
				}
			}
		}
		m_bVolumeChanged = false;
		if (m_bCue && m_DoorPrefabs[0])
		{
			FPlaybackOptions Options;
			Options.Scope = Context.AudioScope;
			Options.Volume = m_Settings.Volume;
			const auto Played = Context.Audio->Play(m_DoorPrefabs[0].Get()->GetSound("openCue"), Options);
			if (!Played)
			{
				throw Toolbox::FException(Played.Error().Message);
			}
			for (auto& Playback : m_Playbacks)
			{
				if (!Playback)
				{
					Playback = Played.Value();
					break;
				}
			}
			m_bCue = false;
		}
	}
	/**
	 * 旧Sensor購読を解除し、同名新世代へ追従しない。確保しない。
	 */
	void OnDeinitialize() noexcept override
	{
		for (auto& Sensor : m_Sensors)
		{
			if (Sensor)
			{
				Sensor.Get()->SetHandler({});
			}
		}
	}

private:
	// 旧SceneのCollection。Sceneが子より長く生存する。
	TScene& m_Scene;
	// 準備済みの荷物。Native参照は所有側だけで扱う。
	TPrepared m_Cargo;
	// 現在適用した設定。保存済みかは別の要求所有者が表示する。
	FUserSettings m_Settings;
	// Sampleの許可Actionと予約入力。
	FInputProfilePolicy m_Policy;
	// 最新Snapshotだけから更新する既存Action Map。
	FInputMap m_Input;
	// 取得済み状態を最初のイベントより先に保持する。
	FSaveCourseProgress m_Progress;
	// 元のプレイヤーComponent。破棄後は再検索しない。
	TObjectHandle<TBody> m_Player;
	// 音の公開先を持つ元の扉個体。
	TObjectHandle<TPrefab> m_DoorPrefabs[2];
	// 個体ごとの直動Joint参照。
	TObjectHandle<TJoint> m_Doors[2];
	// 二つの鍵と安全地点への購読。
	TObjectHandle<TSensor> m_Sensors[3];
	// 取得物の保存上の名前。
	Toolbox::FString m_KeyNames[2] = {"northKey", "southKey"};
	// 起動中の効果音。Scene AudioScopeが終了時に停止する。
	FPlaybackHandle m_Playbacks[2];
	// 定常のMap参照で一時文字列を作らないAction名。
	Toolbox::FString m_Left = "Left";
	// 右移動Action名。
	Toolbox::FString m_Right = "Right";
	// 上移動Action名。
	Toolbox::FString m_Up = "Up";
	// 下移動Action名。
	Toolbox::FString m_Down = "Down";
	// 単発のJump Action名。
	Toolbox::FString m_Jump = "Jump";
	// 解錠後の扉操作Action名。
	Toolbox::FString m_Interact = "Interact";
	// 動的荷物生成Action名。
	Toolbox::FString m_Spawn = "Spawn";
	// 解決は一度だけ。Body登録は後の固定更新でもよい。
	bool m_bBound = false;
	// 次の固定更新まで保留する押下。
	bool m_bJump = false;
	// 固定更新0回でも保持する扉操作の押下。
	bool m_bInteract = false;
	// 次の固定更新で一度だけ消費する生成要求。
	bool m_bSpawn = false;
	// 成功Postで消費するSensor通知。
	bool m_bKeyTouched[2] = {};
	// 安全地点Sensorの成功通知待ち。
	bool m_bCheckpointTouched = false;
	// 現在のゲーム目標。開閉の論理値は到達後だけ確定する。
	bool m_bGoalOpen[2] = {};
	// 次の成功境界で進行JSONを更新する。
	bool m_bDirty = false;
	// 次の通常更新で一度だけ効果音を再生する。
	bool m_bCue = false;
	// 起動済みPlaybackへ元音量から再適用する。
	bool m_bVolumeChanged = false;
	// 切断したPadの保留押下を次の固定更新へ持ち越さない。
	bool m_bPadConnected[4] = {};
	/**
	 * 復号済みの取得物だけを名前で確認する。
	 * @param Key 取得物の論理キー。
	 */
	bool HasKey(Toolbox::FStringView Key) const noexcept
	{
		for (const auto& Saved : m_Progress.CollectedKeys)
		{
			if (Saved == Key)
			{
				return true;
			}
		}
		return false;
	}
	/**
	 * 新しいSceneを探さず、元の成功境界だけを確認する。
	 */
	bool IsReady() const noexcept
	{
		return m_bBound && m_Scene.GetResumeTicket().GetState() == ECheckpointSceneState::Ready;
	}
	/**
	 * 正確な進行カウンターを更新し、上限では例外で拒否する。
	 * @param Counter 更新する進行の通し番号。
	 */
	static void Increment(Toolbox::uint64& Counter)
	{
		if (Counter == Toolbox::TNumericLimits<Toolbox::uint64>::Max())
		{
			throw Toolbox::FException("Save course counter overflow");
		}
		++Counter;
	}
	/**
	 * UIが消費した保持入力を調べる。
	 * @param Routed UI消費後の値。
	 * @param Raw 同じFrameの元入力。
	 */
	static bool IsMasked(const FInputSnapshot& Routed, const FInputSnapshot& Raw) noexcept
	{
		for (Toolbox::size_t Index = 0; Index < static_cast<Toolbox::size_t>(EKey::Count); ++Index)
		{
			const auto Key = static_cast<EKey>(Index);
			if (Raw.IsDown(Key) && !Routed.IsDown(Key))
			{
				return true;
			}
		}
		for (Toolbox::size_t Index = 0; Index < static_cast<Toolbox::size_t>(EMouseButton::Count); ++Index)
		{
			const auto Button = static_cast<EMouseButton>(Index);
			if (Raw.IsMouseDown(Button) && !Routed.IsMouseDown(Button))
			{
				return true;
			}
		}
		for (Toolbox::size_t Pad = 0; Pad < 4; ++Pad)
		{
			for (Toolbox::size_t Button = 0; Button < 16; ++Button)
			{
				if (Raw.IsPadDown(Pad, Button) && !Routed.IsPadDown(Pad, Button))
				{
					return true;
				}
			}
		}
		return false;
	}
	/**
	 * 公開先を初回入力境界で一度解決する。初期化中の副作用はない。
	 */
	void Bind()
	{
		if (m_bBound)
		{
			return;
		}
		m_Player = m_Scene.GetPrefab("player").Get()->GetRigidBody("body");
		m_DoorPrefabs[0] = m_Scene.GetPrefab("doorA");
		m_DoorPrefabs[1] = m_Scene.GetPrefab("doorB");
		for (Toolbox::uint32 Index = 0; Index < 2; ++Index)
		{
			m_Doors[Index] = m_DoorPrefabs[Index].Get()->GetPrismaticJoint("doorDrive");
			if (!HasKey(m_KeyNames[Index]))
			{
				m_Sensors[Index] = m_Scene.GetPrefab(m_KeyNames[Index]).Get()->GetContactListener("sensor");
			}
		}
		m_Sensors[2] = m_Scene.GetPrefab("northCheckpoint").Get()->GetContactListener("sensor");
		for (Toolbox::uint32 Index = 0; Index < 3; ++Index)
		{
			if (m_Sensors[Index])
			{
				m_Sensors[Index].Get()->SetHandler([this, Index](const auto& Notice)
				                                   {
					                                   if (!m_Player || !m_Player.Get()->HasBody() || Notice.Other.Body != m_Player.Get()->GetBodyId() || Notice.Kind != EWorldEventKind::Trigger || Notice.Phase == EWorldEventPhase::End)
					                                   {
						                                   return;
					                                   }
					                                   if (Index < 2)
					                                   {
						                                   m_bKeyTouched[Index] = true;
					                                   }
					                                   else
					                                   {
						                                   m_bCheckpointTouched = true;
					                                   }
				                                   });
			}
		}
		m_bBound = true;
	}
	/**
	 * 有限Motorと既存の目標計算だけを使う。
	 * @param Index 元の扉個体の位置。
	 * @param Seconds 次の通常固定更新の秒数。
	 */
	FLinearJointTargetCommand DoorCommand(Toolbox::uint32 Index, Toolbox::f64 Seconds) const
	{
		const auto* Joint = m_Doors[Index].Get();
		const auto Observation = Joint->GetObservation();
		const auto& Description = Joint->GetDescription().Joint;
		FLinearJointTargetSettings Target;
		Target.TargetTranslation = m_bGoalOpen[Index] ? Description.Limits.UpperTranslation : Description.Limits.LowerTranslation;
		Target.MaxSpeed = .8;
		Target.PositionTolerance = .01;
		Target.SpeedTolerance = .03;
		Target.MaxForce = Description.Drive.MaxForce;
		return ComputeJointTargetDrive(Target, Observation->State.Translation, Observation->State.TranslationRate, Description.Limits, Seconds);
	}
	/**
	 * 動的個体を既存履歴とCollectionへ登録する。確保失敗は通常の実行時失敗として伝える。
	 */
	void SpawnCargo()
	{
		if (m_Progress.SpawnCounter == Toolbox::TNumericLimits<Toolbox::uint64>::Max() || m_Progress.Revision == Toolbox::TNumericLimits<Toolbox::uint64>::Max())
		{
			throw Toolbox::FException("Save course spawn counter overflow");
		}
		TInstance Instance;
		Instance.Key = Toolbox::FString("cargo") + Toolbox::FJsonWriter::Unsigned(m_Progress.SpawnCounter + 1);
		Instance.Prefab = "cargo";
		Instance.bDynamic = true;
		Instance.DefinitionFingerprint = m_Cargo.Definition->ContentFingerprint;
		Instance.Parameters = m_Cargo.Definition->Parameters;
		Instance.Placement.Position.X = 7;
		Instance.Placement.Position.Y = 3;
		const auto Spawned = m_Scene.GetCheckpointCapture().Spawn(m_Scene, Toolbox::Move(Instance), m_Cargo);
		if (!Spawned)
		{
			throw Toolbox::FException(Spawned.Error().Message);
		}
		++m_Progress.SpawnCounter;
		++m_Progress.Revision;
		m_bDirty = true;
	}
};
} // namespace Dxf::GameplaySample
#endif
