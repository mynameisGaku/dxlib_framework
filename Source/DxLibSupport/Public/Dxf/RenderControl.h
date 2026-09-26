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
	 * 現在の管理対象を保存する。空のFRenderTargetはバックバッファ。
	 * 未対応の外部制御は明示的に失敗する。
	 */
	virtual TResult<FRenderTarget> GetRenderTarget() const
	{
		return TResult<FRenderTarget>::Failure(EErrorCode::InvalidState, "Render target inspection unsupported");
	}
	/**
	 * 現在の描画先を指定色で消去する。
	 * @param Color 描画色。
	 */
	virtual TResult<void> ClearTarget(FColor Color) = 0;
	/**
	 * 保存した対象へ復帰する。失敗フレームでも復帰を試し、元の失敗は維持する。
	 * @param Target 保存した対象。空はバックバッファ。
	 */
	virtual TResult<void> RestoreRenderTarget(const FRenderTarget& Target)
	{
		return Target.AsTexture().GetResource_Internal() ? SetRenderTarget(Target) : SetBackBuffer();
	}
	/**
	 * ネイティブ処理を呼び出し、描画状態を復元する。
	 * @param Callback 利用者が指定した処理。
	 */
	virtual TResult<void> Native(const Toolbox::TFunction<TResult<void>()>& Callback) = 0;
};
} // namespace Dxf
