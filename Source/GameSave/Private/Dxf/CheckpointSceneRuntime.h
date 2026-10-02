// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHECKPOINT_SCENE_RUNTIME_PRIVATE_H
#define DXF_CHECKPOINT_SCENE_RUNTIME_PRIVATE_H
#include "Dxf/CheckpointSceneStatus.h"
#include "Dxf/CheckpointCodec.h"
#include "Dxf/JointTargetMotion.h"
#include "Dxf/SaveCodec.h"
#include "Dxf/SaveError.h"
namespace Dxf::SavePrivate
{
// 次元固有の成分は既存Joint APIが処理する。世代と更新順だけを共用する。
/**
 * Sceneに所有される進行・採取・目標。次元別の数学は既存公開APIへ任せる。
 */
template <typename TScene, typename TCapture, typename TDocument, typename TAngular, typename TLinear>
class TCheckpointSceneRuntime
{
public:
	/**
	 * @param Document 検証済みの再開進行。
	 * @param Limits 採取と進行変更の上限。
	 */
	TCheckpointSceneRuntime(TDocument Document, FCheckpointLimits Limits) : m_Capture(Document, Limits), m_Identity(Document.Identity), m_Payload(Document.GamePayload), m_Limits(Limits), m_Status(Toolbox::MakeShared<FCheckpointSceneStatus>())
	{
		// 再開時だけ構築し、通常Tickでは名前・JSONを再解析しない。
		for (const auto& Joint : Document.Joints)
		{
			if (Joint.AngularTarget || Joint.LinearTarget)
			{
				if (!Joint.bConnected)
				{
					throw FSaveError(ESaveError::InvalidDocument, "Disconnected checkpoint Joint has an active target");
				}
				m_Targets.PushBack({Joint});
			}
		}
		m_Capture.RequestCapture();
	}
	/**
	 * 終了後も読めるCPU観察を返す。
	 */
	FCheckpointSceneTicket Ticket() const noexcept
	{
		return FCheckpointSceneTicket(m_Status);
	}
	/**
	 * このSceneの採取履歴だけを返す。
	 */
	TCapture& Capture() noexcept
	{
		return m_Capture;
	}
	/**
	 * ゲームcodecへ渡す現在の進行を返す。
	 */
	const Toolbox::FString& Payload() const noexcept
	{
		return m_Payload;
	}
	/**
	 * @param Payload ゲームで検証したObject。不正な構造では旧値を保持する。
	 */
	void SetPayload(Toolbox::FString Payload)
	{
		if (m_Status->State == ECheckpointSceneState::Ended)
		{
			throw FSaveError(ESaveError::InvalidDocument, "Checkpoint Scene has ended");
		}
		// 進行変更時だけ検証し、旧値を破壊してから解析しない。
		auto Candidate = m_Identity;
		Candidate.Payload = Payload;
		(void)FSaveCodec::Encode(Candidate, m_Limits.Document);
		m_Payload = Toolbox::Move(Payload);
	}
	/**
	 * 通常Sceneの有効化通知を記録する。
	 */
	void Activate() noexcept
	{
		if (m_Status->State == ECheckpointSceneState::Pending)
		{
			m_Status->State = ECheckpointSceneState::PendingPhysics;
		}
	}
	/**
	 * @param Scene 同一の所有Scene。
	 * @param Context 成功した固定更新。
	 */
	void Complete(TScene& Scene, const FFixedTickContext& Context)
	{
		if (m_Status->State == ECheckpointSceneState::Ended)
		{
			return;
		}
		if (!m_bBound)
		{
			m_Capture.Bind(Scene);
			// 公開先を一度だけ解決し、終了した同名の新個体を探し直さない。
			for (auto& Target : m_Targets)
			{
				const auto& Saved = Target.Settings;
				if (Saved.AngularTarget)
				{
					Target.Angular = Saved.Instance.IsEmpty() ? Scene.GetRevoluteConnection(Saved.Export) : Scene.GetPrefab(Saved.Instance).Get()->GetRevoluteJoint(Saved.Export);
				}
				if (Saved.LinearTarget)
				{
					Target.Linear = Saved.Instance.IsEmpty() ? Scene.GetPrismaticConnection(Saved.Export) : Scene.GetPrefab(Saved.Instance).Get()->GetPrismaticJoint(Saved.Export);
				}
			}
			m_bBound = true;
		}
		// 全ゲームPost反映後の一貫した採取が成功して初めて再開完了候補にする。
		if (m_Capture.CaptureIfRequested(Scene, m_Payload) && m_Status->State == ECheckpointSceneState::PendingPhysics)
		{
			m_Status->State = ECheckpointSceneState::Ready;
		}
		// 速度指示は次の通常PrePhysicsに渡す。座標を直接変更せず追加Stepもしない。
		for (auto& Target : m_Targets)
		{
			if (Target.Settings.AngularTarget && Target.Angular)
			{
				auto* Joint = Target.Angular.Get();
				const auto Observation = Joint->GetObservation();
				if (Observation && !Joint->HasPendingRequest())
				{
					const auto Drive = ComputeJointTargetDrive(*Target.Settings.AngularTarget, Observation->State.Angle, Observation->State.AngularSpeed, Joint->GetDescription().Joint.Limits, Context.DeltaSeconds);
					Joint->RequestDrive(Drive.Drive);
				}
			}
			if (Target.Settings.LinearTarget && Target.Linear)
			{
				auto* Joint = Target.Linear.Get();
				const auto Observation = Joint->GetObservation();
				if (Observation && !Joint->HasPendingRequest())
				{
					const auto Drive = ComputeJointTargetDrive(*Target.Settings.LinearTarget, Observation->State.Translation, Observation->State.TranslationRate, Joint->GetDescription().Joint.Limits, Context.DeltaSeconds);
					Joint->RequestDrive(Drive.Drive);
				}
			}
		}
	}
	/**
	 * 古い世代の参照だけを終了し、確保しない。
	 */
	void Retire() noexcept
	{
		m_Capture.Retire();
		m_Targets.Clear();
		m_Status->State = ECheckpointSceneState::Ended;
	}

private:
	// 目標の種類と元の世代付きComponent参照。
	struct FTarget
	{
		// 保存された有限目標。求解履歴ではない。
		FCheckpointJointState Settings;
		// 元の回転Joint。別種類へcastしない。
		TObjectHandle<TAngular> Angular;
		// 元の直動Joint。失効した新世代へ追従しない。
		TObjectHandle<TLinear> Linear;
	};
	// 既存Collectionの論理履歴と世代参照。
	TCapture m_Capture;
	// 進行JSONの構造検査に使う識別情報。
	FSaveDocument m_Identity;
	// ゲーム側codecが解釈する現在の進行。
	Toolbox::FString m_Payload;
	// 採取時と変更時の有限上限。
	FCheckpointLimits m_Limits;
	// Sceneポインタを含まない終了後の結果。
	Toolbox::TSharedPtr<FCheckpointSceneStatus> m_Status;
	// 構築時にのみ確保する目標と参照。
	Toolbox::TVector<FTarget> m_Targets;
	// 最初の成功固定更新で参照を確定したか。
	bool m_bBound = false;
};
} // namespace Dxf::SavePrivate
#endif
