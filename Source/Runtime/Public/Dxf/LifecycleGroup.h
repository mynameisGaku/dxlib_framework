#pragma once
#include "Dxf/Contexts.h"
namespace Dxf
{
class FRenderContext;
/**
 * 子の初期化・更新・終了通知を管理する型。
 */
class ILifecycleGroup
{
public:
	/**
	 * 派生型を含むオブジェクトを安全に終了する。
	 */
	virtual ~ILifecycleGroup() = default;
	/**
	 * 利用者の処理を呼ぶ前に今回の変更対象を固定する。
	 */
	virtual void FreezeBoundary_Internal() = 0;
	/**
	 * 更新境界でシーンの変更を反映する。
	 * @param Context 処理に必要な実行環境。
	 */
	virtual TResult<void> CommitBoundary_Internal(const FInitContext& Context) = 0;
	/**
	 * 更新対象へフレーム更新を通知する。
	 * @param Context 処理に必要な実行環境。
	 */
	virtual TResult<void> Tick_Internal(const FTickContext& Context) = 0;
	/**
	 * 更新対象へ固定時間更新を通知する。
	 * @param Context 処理に必要な実行環境。
	 */
	virtual TResult<void> FixedTick_Internal(const FFixedTickContext& Context) = 0;
	/**
	 * 対象の描画を要求する。
	 * @param Context 処理に必要な実行環境。
	 */
	virtual TResult<void> Draw_Internal(FRenderContext& Context) = 0;
	/**
	 * 利用者の処理や削除を行わず、子孫全体を停止要求済みにする。
	 */
	virtual void RequestStop_Internal() noexcept = 0;
	/**
	 * 管理する処理とリソースを順序どおり終了する。
	 */
	virtual void Shutdown_Internal() noexcept = 0;
};
} // namespace Dxf
