#pragma once
#include "Dxf/Object.h"
#include "Dxf/LifecycleGroup.h"
namespace Dxf
{
/**
 * オブジェクトの初期化と停止の状態を管理する型。
 */
enum class ELifecycleState
{
	/**
	 * 初期化を待機している。
	 */
	Pending,
	/**
	 * 初期化を実行している。
	 */
	Initializing,
	/**
	 * 更新と描画が可能。
	 */
	Active,
	/**
	 * 終了処理を待機している。
	 */
	Stopping,
	/**
	 * 終了処理が完了している。
	 */
	Stopped
};
/**
 * 通知の順序はフレームワークが管理し、利用者はOn系のフックだけを上書きする。
 */
class DLifecycleObject : public DObject
{
public:
	/**
	 * 現在の状態を取得する。
	 */
	FORCEINLINE ELifecycleState GetState() const noexcept
	{
		return m_State;
	}
	/**
	 * 初期化が完了しているかを調べる。
	 */
	FORCEINLINE bool IsInitialized() const noexcept
	{
		return m_State == ELifecycleState::Active;
	}
	/**
	 * 破棄が要求されているかを調べる。
	 */
	FORCEINLINE bool IsDestroyRequested() const noexcept
	{
		return m_bDestroyRequested;
	}
	/**
	 * 更新の優先順位を設定する。
	 * @param Order 同じレイヤー内の処理順序。
	 */
	FORCEINLINE void SetUpdateOrder(Toolbox::int32 Order) noexcept
	{
		m_UpdateOrder = Order;
	}
	/**
	 * 更新の優先順位を取得する。
	 */
	FORCEINLINE Toolbox::int32 GetUpdateOrder() const noexcept
	{
		return m_UpdateOrder;
	}
	/**
	 * 一時停止中も更新するかを設定する。
	 * @param bEnabled 機能を有効にするか。
	 */
	FORCEINLINE void SetTickWhenPaused(bool bEnabled) noexcept
	{
		m_bTickWhenPaused = bEnabled;
	}
	/**
	 * 描画対象として表示するかを設定する。
	 * @param bVisible 描画を有効にするか。
	 */
	FORCEINLINE void SetVisible(bool bVisible) noexcept
	{
		m_bVisible = bVisible;
	}
	/**
	 * 描画対象として表示するかを調べる。
	 */
	FORCEINLINE bool IsVisible() const noexcept
	{
		return m_bVisible;
	}
	/**
	 * ハンドルを通して参照できる状態かを調べる。
	 */
	bool IsHandleAccessible_Internal() const noexcept final
	{
		return !m_bDestroyRequested && m_State != ELifecycleState::Stopped && m_State != ELifecycleState::Stopping;
	}
	/**
	 * 使用に必要な初期化を行う。
	 * @param Context 処理に必要な実行環境。
	 */
	TResult<void> Initialize_Internal(const FInitContext& Context);
	/**
	 * 更新対象へフレーム更新を通知する。
	 * @param Context 処理に必要な実行環境。
	 */
	TResult<void> Tick_Internal(const FTickContext& Context);
	/**
	 * 更新対象へ固定時間更新を通知する。
	 * @param Context 処理に必要な実行環境。
	 */
	TResult<void> FixedTick_Internal(const FFixedTickContext& Context);
	/**
	 * 対象の描画を要求する。
	 * @param Context 処理に必要な実行環境。
	 */
	TResult<void> Draw_Internal(FRenderContext& Context);
	/**
	 * 管理する処理とリソースを順序どおり終了する。
	 */
	void Shutdown_Internal() noexcept;
	/**
	 * 次の処理境界での破棄を要求する。
	 */
	void RequestDestroy_Internal() noexcept
	{
		m_bDestroyRequested = true;
		if (m_pChildren)
		{
			m_pChildren->RequestStop_Internal();
		}
	}
	/**
	 * 生成された順序を設定する。
	 * @param Order 同じレイヤー内の処理順序。
	 */
	FORCEINLINE void SetCreationOrder_Internal(Toolbox::uint64 Order) noexcept
	{
		m_CreationOrder = Order;
	}
	/**
	 * 生成された順序を取得する。
	 */
	FORCEINLINE Toolbox::uint64 GetCreationOrder_Internal() const noexcept
	{
		return m_CreationOrder;
	}
	/**
	 * 子のライフサイクルグループを取得する。
	 */
	FORCEINLINE ILifecycleGroup* GetChildren_Internal() noexcept
	{
		return m_pChildren;
	}

protected:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	DLifecycleObject() = default;
	/**
	 * 派生型固有の初期化を行う。
	 */
	virtual TResult<void> OnInitialize(const FInitContext&)
	{
		return {};
	}
	/**
	 * 現在のフレーム情報で状態を更新する。
	 */
	virtual void OnTick(const FTickContext&)
	{
	}
	/**
	 * 固定時間の刻みで状態を更新する。
	 */
	virtual void OnFixedTick(const FFixedTickContext&)
	{
	}
	/**
	 * 現在の状態を描画する。
	 */
	virtual void OnDraw(FRenderContext&) const
	{
	}
	/**
	 * 初期化を試行した場合は部分失敗でも一度だけ呼ばれる。例外を送出しないこと。
	 */
	virtual void OnDeinitialize() noexcept
	{
	}
	/**
	 * 基盤の呼出し: 自身と子へ渡す入力を選ぶ（既定は受け取った入力をそのまま渡す）。nullptrは受け取った入力を使う。
	 * @param Context 更新の情報。
	 */
	virtual const FInputSnapshot* RouteInput_Internal(const FTickContext& Context)
	{
		(void)Context;
		return nullptr;
	}
	/**
	 * 子のライフサイクルグループを接続する。
	 * @param Children 子のライフサイクルグループ。
	 */
	FORCEINLINE void AttachChildren_Internal(ILifecycleGroup& Children) noexcept
	{
		m_pChildren = &Children;
	}

private:
	/**
	 * 子のライフサイクルグループ。
	 */
	ILifecycleGroup* m_pChildren = nullptr;
	/**
	 * 現在の状態。
	 */
	ELifecycleState m_State = ELifecycleState::Pending;
	/**
	 * 生成された順序。
	 */
	Toolbox::uint64 m_CreationOrder = 0;
	/**
	 * 更新の優先順位。
	 */
	Toolbox::int32 m_UpdateOrder = 0;
	/**
	 * 破棄が要求されているか。
	 */
	bool m_bDestroyRequested = false;
	/**
	 * 一時停止中も更新するか。
	 */
	bool m_bTickWhenPaused = false;
	/**
	 * 描画対象として表示するか。
	 */
	bool m_bVisible = true;
	/**
	 * 処理の実行中か。
	 */
	bool m_bBusy = false;
	/**
	 * 初期化を一度試行したか。
	 */
	bool m_bInitializationAttempted = false;
};
} // namespace Dxf
