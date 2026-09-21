#pragma once
#include "Dxf/RenderBackend.h"
namespace Dxf
{
/**
 * DxLibによる次元別描画を管理する型。
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
	/**
	 * 2D基本図形を描ける。
	 */
	bool SupportsShapes2D() const noexcept override
	{
		return true;
	}
	/**
	 * カメラ付き基本3D形状を描ける。
	 */
	bool SupportsGeometry3D() const noexcept override
	{
		return true;
	}
	/**
	 * @param Command 画面ピクセル座標の線分。
	 */
	TResult<void> DrawLine2D(const FLineCommand2D& Command) override;
	/**
	 * @param Command 画面ピクセル座標の円。
	 */
	TResult<void> DrawCircle2D(const FCircleCommand2D& Command) override;
	/**
	 * @param Command 画面ピクセル座標の三角形。
	 */
	TResult<void> DrawTriangle2D(const FTriangleCommand2D& Command) override;
	/**
	 * @param View 全描画先に適用する透視ビュー。深度だけを消去する。
	 */
	TResult<void> BeginView3D(const FRenderView3D& View) override;
	/**
	 * @param Geometry CPUでフラット照明を適用した面と線。
	 */
	TResult<void> DrawGeometry3D(const FPreparedGeometry3D& Geometry) override;
	/**
	 * 2D向けの標準状態へ戻す。一般のNative状態スナップショット復元ではない。
	 */
	TResult<void> EndView3D() override;
private:
	/**
	 * 描画色と不透明度をバックエンドへ適用する。
	 * @param Style 描画状態またはその適用結果。
	 * @param bSprite スプライト用の色変調を適用するか。
	 */
	TResult<void> ApplyStyle_Internal(const FDrawStyle& Style, bool bSprite);
	/**
	 * Nativeビューが開始済みか。所有スレッドだけで操作する。
	 */
	bool m_bView3D = false;
};
}
// namespace Dxf
