#pragma once
#include "Dxf/Texture.h"
#include "Dxf/MathTypes.h"
#include "Dxf/Result.h"
#include "Toolbox/Function.h"
namespace Dxf
{
/**
 * 描画の即時制御だけを公開する。アセット・シーン管理・デバイスの所有は扱わない。
 */
class IRenderControl
{
public:
	/**
	 * 派生型を含むオブジェクトを安全に終了する。
	 */
	virtual ~IRenderControl() = default;
	/**
	 * 描画先のテクスチャを設定する。
	 * @param Target 描画先またはその設定結果。
	 */
	virtual TResult<void> SetRenderTarget(const FRenderTarget& Target) = 0;
	/**
	 * 画面のバックバッファを設定する。
	 */
	virtual TResult<void> SetBackBuffer() = 0;
	/**
	 * 現在の描画先を指定色で消去する。
	 * @param Color 描画色。
	 */
	virtual TResult<void> ClearTarget(FColor Color) = 0;
	/**
	 * ネイティブ処理を呼び出し、描画状態を復元する。
	 * @param Callback 利用者が指定した処理。
	 */
	virtual TResult<void> Native(const Toolbox::TFunction<TResult<void>()>& Callback) = 0;
};
} // namespace Dxf
