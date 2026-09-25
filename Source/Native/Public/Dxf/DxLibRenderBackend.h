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
	 * SetDrawAreaで2D命令を切り抜く。
	 */
	bool SupportsClip2D() const noexcept override
	{
		return true;
	}
	/**
	 * 2D命令のクリップを設定する（描画先の範囲との共通部分）。
	 * @param bEnabled 切り抜くか。
	 * @param Rect 描画先の画素の矩形。
	 */
	TResult<void> SetClip2D(bool bEnabled, FIntRect Rect) override;
	/**
	 * 矩形ごとの投影と領域内深度初期化に対応するか。
	 */
	bool SupportsViewports3D() const noexcept override
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
	 * @param View 全描画先または矩形へ適用するビュー。領域内の深度だけを初期化する。
	 */
	TResult<void> BeginView3D(const FRenderView3D& View) override;
	/**
	 * @param Geometry CPUでフラット照明を適用した面と線。
	 */
	TResult<void> DrawGeometry3D(const FPreparedGeometry3D& Geometry) override;
	/**
	 * テクスチャを貼った四角形を描く。
	 */
	bool SupportsTexturedQuads3D() const noexcept override
	{
		return true;
	}
	/**
	 * DrawPolygon3Dで二つの三角形として描く（照明なし。表面だけの指定なら、視点が裏側のときは描かない）。
	 * @param Quad 四角形。
	 */
	TResult<void> DrawTexturedQuad3D(const FTexturedQuad3D& Quad) override;
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
	 * 矩形ビュー開始時に取得した実描画先の幅。
	 */
	Toolbox::int32 m_TargetWidth = 0;
	/**
	 * 矩形ビュー開始時に取得した実描画先の高さ。
	 */
	Toolbox::int32 m_TargetHeight = 0;
	/**
	 * 2Dの描画先の幅（SetTarget・ResetStateで記録。クリップを戻す範囲）。
	 */
	Toolbox::int32 m_Width2D = 0;
	/**
	 * 2Dの描画先の高さ。
	 */
	Toolbox::int32 m_Height2D = 0;
	/**
	 * GPUモデル照明に使う、開始時点のビュー。
	 */
	FRenderView3D m_ModelView;
	/**
	 * このビューで作成したモデル用ライト。-1は未作成。
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
