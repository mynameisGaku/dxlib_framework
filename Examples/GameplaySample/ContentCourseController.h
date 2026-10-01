// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_CONTENT_COURSE_CONTROLLER_H
#define DXF_SAMPLE_CONTENT_COURSE_CONTROLLER_H
#include "ContentCourseControls.h"
#include "Dxf/GameObject.h"
#include "Dxf/JointTargetMotion.h"
#include "Dxf/AudioPlayer.h"
namespace Dxf::GameplaySample
{
/**
 * 定義から生成された公開先だけを操作するゲーム処理。解放順や物理求解を持たない。
 * @tparam TScene 次元別のContentコース。
 * @tparam TJoint 既存Prismatic Component。
 */
template <typename TScene, typename TJoint>
class TContentCourseController final : public DGameObject
{
public:
	/**
	 * @param Scene Collectionを所有し、このObjectより長く生存するScene。
	 * @param Controls UIが更新する操作値。
	 */
	TContentCourseController(TScene& Scene, FContentCourseControls& Controls) : m_pScene(&Scene), m_pControls(&Controls)
	{
		SetTickWhenPaused(true);
	}

protected:
	/**
	 * @param Context 操作された入力と通常更新。描画回数で実行しない。
	 */
	void OnTick(const FTickContext& Context) override
	{
		if (m_pControls->Selection >= 2)
		{
			throw Toolbox::FException("Content sample selection must be 0 or 1");
		}
		if (Context.Input.WasPressed(EKey::Escape) && Context.Scenes)
		{
			Context.Scenes->RequestQuit();
		}
		if (Context.Input.WasPressed(EKey::P))
		{
			m_pScene->GetClock().SetPaused(!m_pScene->GetClock().IsPaused());
		}
		if (Context.Input.WasPressed(EKey::V))
		{
			m_pControls->bSplit = !m_pControls->bSplit;
		}
		m_pScene->ConfigureViews(m_pControls->bSplit);
		if (m_pScene->GetClock().IsPaused())
		{
			return;
		}
		if (Context.Input.WasPressed(EKey::Space))
		{
			m_pControls->Target = m_pControls->Target > 1 ? 0 : 2;
			m_pControls->bOperate = true;
			m_pControls->bCue = true;
		}
		m_pControls->bSpawn = m_pControls->bSpawn || Context.Input.WasPressed(EKey::G);
		m_pControls->bDestroy = m_pControls->bDestroy || Context.Input.WasPressed(EKey::X);
		if (m_pControls->bDestroy)
		{
			const char* Names[2] = {"doorA", "doorB"};
			const auto Instance = m_pScene->FindCoursePrefab(Names[m_pControls->Selection]);
			if (Instance)
			{
				Instance.Get()->Destroy();
			}
			m_Drives[m_pControls->Selection] = {};
			m_pControls->bDestroy = false;
		}
		if (m_pControls->bSpawn)
		{
			m_pScene->SpawnExtra();
			m_pControls->bSpawn = false;
		}
		for (Toolbox::uint32 I = 0; I < 2; ++I)
		{
			if (!m_Drives[I] && !m_bLost[I])
			{
				const char* Names[2] = {"doorA", "doorB"};
				const auto Instance = m_pScene->FindCoursePrefab(Names[I]);
				if (!Instance)
				{
					m_bLost[I] = true;
					continue;
				}
				if (Instance.Get()->GetState() == EPrefabInstanceState::Ready)
				{
					m_Drives[I] = Instance.Get()->GetPrismaticJoint("doorDrive");
					m_bConnected[I] = m_Drives[I].Get()->GetConnectionState() == EDistanceJointConnection::Connected;
					if (I == 0)
					{
						BindSensor(Instance);
					}
				}
			}
		}
		if (m_pControls->bCue && Context.Audio && m_Drives[0])
		{
			FPlaybackOptions Options;
			Options.Scope = Context.AudioScope;
			const auto Played = Context.Audio->Play(m_pScene->GetPrefab("doorA").Get()->GetSound("openCue"), Options);
			if (!Played)
			{
				throw Toolbox::FException(Played.Error().Message);
			}
			m_pControls->bCue = false;
		}
	}
	/**
	 * ControllerはPrefabより先に登録され、前回成功Postの観察から次の要求を作る。
	 * @param Context 次の固定更新。0回ならこの関数も呼ばれない。
	 */
	void OnFixedTick(const FFixedTickContext& Context) override
	{
		if (!m_pControls->bOperate)
		{
			return;
		}
		const auto Selected = m_pControls->Selection;
		const auto Joint = m_Drives[Selected];
		if (!Joint)
		{
			return;
		}
		if (m_bConnected[Selected] != m_pControls->bConnected)
		{
			if (m_pControls->bConnected)
			{
				Joint.Get()->RequestConnect(Joint.Get()->GetDescription());
			}
			else
			{
				Joint.Get()->RequestDisconnect();
			}
			m_bConnected[Selected] = m_pControls->bConnected;
		}
		const auto Observation = Joint.Get()->GetObservation();
		if (!Observation)
		{
			return;
		}
		FLinearJointTargetSettings Target;
		const auto Limits = Joint.Get()->GetDescription().Joint.Limits;
		Target.TargetTranslation =
		    Limits.bEnabled ? Toolbox::Clamp(m_pControls->Target, Limits.LowerTranslation, Limits.UpperTranslation)
		                    : m_pControls->Target;
		Target.MaxSpeed = m_pControls->Speed;
		Target.MaxForce = m_pControls->Effort;
		auto Command =
		    ComputeJointTargetDrive(Target, Observation->State.Translation, Observation->State.TranslationRate, Joint.Get()->GetDescription().Joint.Limits, Context.DeltaSeconds);
		if (!m_pControls->bRunning)
		{
			Command.Drive.TargetSpeed = 0;
		}
		Joint.Get()->RequestDrive(Command.Drive);
	}
	/**
	 * 捕捉先が解放される前にSensor通知を解除する。
	 */
	void OnDeinitialize() noexcept override
	{
		if (m_Listener)
		{
			m_Listener.Get()->SetHandler({});
		}
	}

private:
	/**
	 * @param Instance 公開Sensorを持つ最初の扉。ゲーム判断だけを通知先へ置く。
	 */
	void BindSensor(typename TScene::FPrefabHandle Instance)
	{
		m_Listener = Instance.Get()->GetContactListener("sensor");
		m_Listener.Get()->SetHandler(
		    [this](const auto& Notice)
		    {
			    // Ready前にBeginが起きた場合は最初のStayでも一度だけ反応する。
			    if (!m_bTriggered && Notice.Kind == EWorldEventKind::Trigger && Notice.Phase != EWorldEventPhase::End)
			    {
				    m_bTriggered = true;
				    m_pControls->bCue = true;
			    }
		    });
	}
	/**
	 * Sceneと操作値はこのObjectを所有する。
	 */
	TScene* m_pScene;
	FContentCourseControls* m_pControls;
	/**
	 * 公開先の解決はReady後に一度だけ。定常で文字列検索しない。
	 */
	TObjectHandle<TJoint> m_Drives[2];
	/**
	 * 通知元のComponentも既存Collectionが所有する。
	 */
	typename TScene::FListenerHandle m_Listener;
	/**
	 * 意図的な破棄や失効から同名新世代へ自動追従しない。
	 */
	bool m_bLost[2] = {};
	/**
	 * 接続要求の同値反復を避ける。
	 */
	bool m_bConnected[2] = {true, true};
	/**
	 * Sensorからのcueを一回だけ受け付ける。
	 */
	bool m_bTriggered = false;
};
} // namespace Dxf::GameplaySample
#endif
