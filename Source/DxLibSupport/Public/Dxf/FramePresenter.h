#pragma once
#include "Dxf/RenderBackend.h"
namespace Dxf
{
/**
 * フレーム描画結果の画面提示を管理する型。
 */
class FFramePresenter
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Backend ネイティブ処理の呼び出し先。
	 */
	explicit FFramePresenter(IRenderBackend& Backend) : m_pBackend(&Backend)
	{
	}
	/**
	 * 描画対象を設定してフレームを開始する。
	 * @param Width 幅。
	 * @param Height 高さ。
	 * @param ClearColor 画面を消去する色。
	 */
	TResult<void> Begin(Toolbox::int32 Width, Toolbox::int32 Height, FColor ClearColor)
	{
		// 描画先またはその設定結果。
		auto Target = m_pBackend->SetTarget(-1, Width, Height);
		if (!Target)
		{
			return Target;
		}
		// 既定状態への復元結果。
		auto Reset = m_pBackend->ResetState(Width, Height);
		if (!Reset)
		{
			return Reset;
		}
		return m_pBackend->Clear(ClearColor);
	}
	/**
	 * 描画したフレームを画面へ提示する。
	 */
	TResult<void> Present()
	{
		return m_pBackend->Present();
	}

private:
	/**
	 * ネイティブ処理の呼び出し先。
	 */
	IRenderBackend* m_pBackend;
};
} // namespace Dxf
