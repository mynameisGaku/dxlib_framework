#pragma once
#include "Dxf/RenderBackend.h"
namespace Dxf
{
/**
 * DxLibによる2D描画を管理する型。
 */
class FDxLibRenderBackend final : public IRenderBackend
{
public:
	/**
	 * 描画先と描画範囲を設定する。
	 * @param Handle ハンドル。
	 * @param Width 幅。
	 * @param Height 高さ。
	 */
	TResult<void> SetTarget(Toolbox::int32 Handle, Toolbox::int32 Width, Toolbox::int32 Height) override;
	/**
	 * RGBのみを消去する。Aは255とし、描画先のアルファはDxLibの消去仕様に従う。
	 * @param Color 描画色。
	 */
	TResult<void> Clear(FColor Color) override;
	/**
	 * 2D描画に必要な状態へ戻す。
	 * @param Width 幅。
	 * @param Height 高さ。
	 */
	TResult<void> ResetState(Toolbox::int32 Width, Toolbox::int32 Height) override;
	/**
	 * スプライトの描画命令を処理する。
	 * @param Command 実行する描画命令。
	 */
	TResult<void> DrawSprite(const FSpriteCommand& Command) override;
	/**
	 * 文字列の描画命令を処理する。
	 * @param Command 実行する描画命令。
	 */
	TResult<void> DrawText(const FTextCommand& Command) override;
	/**
	 * 矩形の描画命令を処理する。
	 * @param Command 実行する描画命令。
	 */
	TResult<void> DrawRectangle(const FRectangleCommand& Command) override;
	/**
	 * 描画したフレームを画面へ提示する。
	 */
	TResult<void> Present() override;

private:
	/**
	 * 描画色と不透明度をバックエンドへ適用する。
	 * @param Style 描画状態またはその適用結果。
	 * @param bSprite スプライト用の色変調を適用するか。
	 */
	TResult<void> ApplyStyle_Internal(const FDrawStyle& Style, bool bSprite);
};
} // namespace Dxf
