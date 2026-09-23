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
	 * まだビューを開始していない描画境界を作る。
	 */
	FDxLibRenderBackend() = default;
	/**
	 * ネイティブライトの所有を複製しない。
	 */
	FDxLibRenderBackend(const FDxLibRenderBackend&) = delete;
	/**
	 * ネイティブライトの所有を複製しない。
	 */
	FDxLibRenderBackend& operator=(const FDxLibRenderBackend&) = delete;
	/**
	 * 開始済みのビューがあれば照明と2D状態を復元する。
	 */
	~FDxLibRenderBackend() override;
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
	 * リンクしたDxLibでモデルを扱える構成か。
	 */
	bool SupportsModels3D() const noexcept override;
	/**
	 * @param Model 記録時点の変換と再生状態。クリップと時刻は描画直前に反映する。
	 */
	TResult<void> DrawModel3D(const FModelDraw3D& Model) override;
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
	/**
	 * GPUモデル照明に使う、開始時点のビュー。
	 */
	FRenderView3D m_ModelView;
	/**
	 * このビューで作成した指向性ライト。-1は未作成。
	 */
	Toolbox::int32 m_ModelLight = -1;
	/**
	 * 外部ライトの有効状態を保存済みか。
	 */
	bool m_bSavedLights = false;
	/**
	 * 復元する既定ライトの有効状態。
	 */
	Toolbox::int32 m_DefaultLightEnabled = 0;
	/**
	 * 開始時に有効だった外部ライト。ビュー中だけ無効にする。
	 */
	Toolbox::TVector<Toolbox::int32> m_ExternalLights;
	/**
	 * GPU照明の初回利用時にライトを確保し、ビューの設定を適用する。
	 */
	TResult<void> PrepareModelLight_Internal();
	/**
	 * 作成したライトを破棄し、外部ライトの有効状態を復元する。
	 */
	bool RestoreModelLights_Internal() noexcept;
};
} // namespace Dxf
// namespace Dxf
