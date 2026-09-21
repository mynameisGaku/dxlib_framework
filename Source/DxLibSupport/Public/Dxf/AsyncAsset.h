// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_ASYNC_ASSET_H
#define DXF_ASYNC_ASSET_H
#include "Dxf/Texture.h"
#include "Dxf/Sound.h"
#include "Toolbox/Atomic.h"
#include "Toolbox/Mutex.h"
#include "Toolbox/Optional.h"
#include "Toolbox/SharedPtr.h"
namespace Dxf
{
class FAssetService;
class FTaskDispatcher;
struct FTaskScope;
/**
 * 非同期テクスチャ要求の結果。完了前や破棄後は失敗を返す。
 * Dispatcherの破棄後は未完了のまま残るため、無期限に待たないこと。
 */
class FAsyncTexture
{
public:
	/**
	 * 空の要求結果を作る。
	 */
	FAsyncTexture() = default;
	/**
	 * 共有状態を解放する。
	 */
	~FAsyncTexture();
	/**
	 * 結果が確定しているか調べる。
	 */
	bool IsReady() noexcept;
	/**
	 * 確定した結果を複製する。未確定なら失敗を返す。
	 */
	TResult<FTexture> Take();
	/**
	 * 共有状態を複製する。
	 */
	FAsyncTexture(const FAsyncTexture&) = default;
	/**
	 * 共有状態を代入する。
	 */
	FAsyncTexture& operator=(const FAsyncTexture&) = default;

private:
	friend class FAssetService;
	/**
	 * 共有する要求の進行状態。
	 */
	struct FState;
	/**
	 * 共有する要求の進行状態。
	 */
	Toolbox::TSharedPtr<FState> m_State;
};
/**
 * 非同期音声要求の結果。完了前や破棄後は失敗を返す。
 * Dispatcherの破棄後は未完了のまま残るため、無期限に待たないこと。
 */
class FAsyncSound
{
public:
	/**
	 * 空の要求結果を作る。
	 */
	FAsyncSound() = default;
	/**
	 * 共有状態を解放する。
	 */
	~FAsyncSound();
	/**
	 * 結果が確定しているか調べる。
	 */
	bool IsReady() noexcept;
	/**
	 * 確定した結果を複製する。未確定なら失敗を返す。
	 */
	TResult<FSound> Take();
	/**
	 * 共有状態を複製する。
	 */
	FAsyncSound(const FAsyncSound&) = default;
	/**
	 * 共有状態を代入する。
	 */
	FAsyncSound& operator=(const FAsyncSound&) = default;

private:
	friend class FAssetService;
	/**
	 * 共有する要求の進行状態。
	 */
	struct FState;
	/**
	 * 共有する要求の進行状態。
	 */
	Toolbox::TSharedPtr<FState> m_State;
};
} // namespace Dxf
#endif
