#pragma once
#include "Dxf/LifecycleObject.h"
namespace Dxf
{
/**
 * GameObjectを持たずに使用でき、ゲーム以外のツールにも使えるシーン。
 */
class DScene : public DLifecycleObject
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	DScene()
	{
		SetTickWhenPaused(true);
	}
	/**
	 * 経過時間の計測器を取得する。
	 */
	FSceneClock& GetClock() noexcept
	{
		return m_Clock;
	}
	/**
	 * 経過時間の計測器を取得する。
	 */
	const FSceneClock& GetClock() const noexcept
	{
		return m_Clock;
	}
	/**
	 * シーンに紐づく音声の識別番号を取得する。
	 */
	Toolbox::uint64 GetAudioScope() const noexcept
	{
		return m_AudioScope;
	}
	/**
	 * 重複を防いでシーン開始を通知する。
	 * @param Context 処理に必要な実行環境。
	 */
	void Enter_Internal(const FSceneActivationContext& Context) noexcept
	{
		if (m_bEntered)
		{
			return;
		}
		m_AudioScope = Context.AudioScope;
		m_bEntered = true;
		OnEnter(Context);
	}
	/**
	 * 開始済みのシーンへ終了を通知する。
	 */
	void Exit_Internal() noexcept
	{
		if (m_bEntered)
		{
			m_bEntered = false;
			OnExit();
		}
	}

protected:
	/**
	 * シーンが有効になったときの処理を行う。
	 */
	virtual void OnEnter(const FSceneActivationContext&) noexcept
	{
	}
	/**
	 * シーンが終了するときの処理を行う。
	 */
	virtual void OnExit() noexcept
	{
	}

private:
	/**
	 * 経過時間の計測器。
	 */
	FSceneClock m_Clock;
	/**
	 * シーンに紐づく音声の識別番号。
	 */
	Toolbox::uint64 m_AudioScope = 0;
	/**
	 * シーンの開始通知を実行済みか。
	 */
	bool m_bEntered = false;
};
} // namespace Dxf
