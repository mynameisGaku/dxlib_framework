#pragma once
#include "Dxf/InputRouter.h"
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
	FORCEINLINE FSceneClock& GetClock() noexcept
	{
		return m_Clock;
	}
	/**
	 * 経過時間の計測器を取得する。
	 */
	FORCEINLINE const FSceneClock& GetClock() const noexcept
	{
		return m_Clock;
	}
	/**
	 * シーンに紐づく音声の識別番号を取得する。
	 */
	FORCEINLINE Toolbox::uint64 GetAudioScope() const noexcept
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
	/**
	 * Sceneの入力の仲介を設定する（nullptrで外す）。双方の寿命を弱参照で区別し、仲介が先に破棄されても安全に外れる。
	 * 設定すると、Scene・子・固定更新は仲介が返した入力を受け取る。
	 * @param Router 仲介。
	 */
	void SetInputRouter(IInputRouter* Router)
	{
		if (Router != nullptr && !m_pInputRoute)
		{
			m_pInputRoute = Toolbox::MakeShared<Detail::FInputRouterSlot>();
		}
		if (m_pInputRoute)
		{
			m_pInputRoute->Router =
			    Router != nullptr ? Router->GetLifetime_Internal() : Toolbox::TWeakPtr<Detail::FInputRouterLifetime>{};
		}
	}
	/**
	 * 入力仲介の接続枠。Scene破棄後は解決しない。
	 */
	Toolbox::TWeakPtr<Detail::FInputRouterSlot> GetInputRouteSlot_Internal() const noexcept
	{
		return Toolbox::TWeakPtr<Detail::FInputRouterSlot>(m_pInputRoute);
	}
	/**
	 * 入力の仲介。
	 */
	FORCEINLINE IInputRouter* GetInputRouter() const noexcept
	{
		const auto Life =
		    m_pInputRoute ? m_pInputRoute->Router.Lock() : Toolbox::TSharedPtr<Detail::FInputRouterLifetime>{};
		return Life ? Life->Router : nullptr;
	}
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
	/**
	 * 仲介があれば、その入力を自身と子へ渡す。
	 * @param Context 更新の情報。
	 */
	const FInputSnapshot* RouteInput_Internal(const FTickContext& Context) override
	{
		IInputRouter* Router = GetInputRouter();
		if (Router == nullptr)
		{
			return nullptr;
		}
		m_RoutedInput = Router->RouteInput(Context);
		return &m_RoutedInput;
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
	/**
	 * 入力の仲介（所有しない）。
	 */
	Toolbox::TSharedPtr<Detail::FInputRouterSlot> m_pInputRoute;
	/**
	 * 仲介から返ったフレームの値。仲介より長く、このSceneの更新中を生存する。
	 */
	FInputSnapshot m_RoutedInput;
};
} // namespace Dxf
