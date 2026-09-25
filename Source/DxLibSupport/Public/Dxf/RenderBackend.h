#pragma once
#include "Dxf/RenderCommands.h"
#include "Dxf/RenderGeometry3D.h"
#include "Dxf/Model.h"
namespace Dxf
{
/**
 * 2D描画のバックエンド窓口を管理する型。
 */
class IRenderBackend
{
public:
	/**
	 * 派生型を含むオブジェクトを安全に終了する。
	 */
	virtual ~IRenderBackend() = default;
	/**
	 * -1でバックバッファを選択する。選択時に描画内容を消去しない。
	 * @param Handle ハンドル。
	 * @param Width 幅。
	 * @param Height 高さ。
	 */
	virtual TResult<void> SetTarget(Toolbox::int32 Handle, Toolbox::int32 Width, Toolbox::int32 Height) = 0;
	/**
	 * 蓄積した内容を消去する。
	 * @param Color 描画色。
	 */
	virtual TResult<void> Clear(FColor Color) = 0;
	/**
	 * 2D描画に必要な状態へ戻す。
	 * @param Width 幅。
	 * @param Height 高さ。
	 */
	virtual TResult<void> ResetState(Toolbox::int32 Width, Toolbox::int32 Height) = 0;
	/**
	 * スプライトの描画命令を処理する。
	 * @param Command 実行する描画命令。
	 */
	virtual TResult<void> DrawSprite(const FSpriteCommand& Command) = 0;
	/**
	 * 文字列の描画命令を処理する。
	 * @param Command 実行する描画命令。
	 */
	virtual TResult<void> DrawText(const FTextCommand& Command) = 0;
	/**
	 * 矩形の描画命令を処理する。
	 * @param Command 実行する描画命令。
	 */
	virtual TResult<void> DrawRectangle(const FRectangleCommand& Command) = 0;
	/**
	 * 追加2D図形を扱えるか。falseの場合、対応関数は失敗する。
	 */
	virtual bool SupportsShapes2D() const noexcept
	{
		return false;
	}
	/**
	 * 2D命令の矩形クリップに対応するか。対応しないBackendへクリップ付きの命令を送ると失敗する（範囲外へ描かない）。
	 */
	virtual bool SupportsClip2D() const noexcept
	{
		return false;
	}
	/**
	 * 以後の2D命令のクリップを設定する（bEnabled=falseで描画先全体へ戻す）。空の矩形は渡さない。
	 * @param bEnabled 切り抜くか。
	 * @param Rect 描画先の画素の矩形（半開区間）。
	 */
	virtual TResult<void> SetClip2D(bool bEnabled, FIntRect Rect)
	{
		(void)bEnabled;
		(void)Rect;
		return Unsupported_Internal();
	}
	/**
	 * 矩形ごとの投影と領域内深度初期化に対応するか。
	 */
	virtual bool SupportsViewports3D() const noexcept
	{
		return false;
	}
	/**
	 * このBackendがワールド座標の基本3D形状を扱えるか。
	 */
	virtual bool SupportsGeometry3D() const noexcept
	{
		return false;
	}
	/**
	 * このBackendが読み込んだモデルを3Dビューへ描画できるか。
	 */
	virtual bool SupportsModels3D() const noexcept
	{
		return false;
	}
	/**
	 * @param Command 描画する2D線分。
	 */
	virtual TResult<void> DrawLine2D(const FLineCommand2D&)
	{
		return Unsupported_Internal();
	}
	/**
	 * @param Command 描画する2D円。
	 */
	virtual TResult<void> DrawCircle2D(const FCircleCommand2D&)
	{
		return Unsupported_Internal();
	}
	/**
	 * @param Command 描画する2D三角形。
	 */
	virtual TResult<void> DrawTriangle2D(const FTriangleCommand2D&)
	{
		return Unsupported_Internal();
	}
	/**
	 * 全描画先または指定矩形で開始し、その領域内の深度を初期化する。
	 * @param View カメラ設定。失敗時もEndView3Dを呼び出す。
	 */
	virtual TResult<void> BeginView3D(const FRenderView3D&)
	{
		return Unsupported_Internal();
	}
	/**
	 * @param Geometry 照明計算済みの描画パケット。
	 */
	virtual TResult<void> DrawGeometry3D(const FPreparedGeometry3D&)
	{
		return Unsupported_Internal();
	}
	/**
	 * 不透明なモデルを1体描画する。BeginView3DとEndView3Dの間で呼ばれる。
	 * 深度を検査・書込みし、描画後に以降の形状が依存する状態を残さない。
	 * @param Model 記録時点の変換と再生状態。
	 */
	/**
	 * テクスチャを貼った四角形に対応するか。
	 */
	virtual bool SupportsTexturedQuads3D() const noexcept
	{
		return false;
	}
	/**
	 * テクスチャを貼った四角形を描く（BeginView3DとEndView3Dの間）。
	 * @param Quad 四角形。
	 */
	virtual TResult<void> DrawTexturedQuad3D(const FTexturedQuad3D& Quad)
	{
		(void)Quad;
		return Unsupported_Internal();
	}
	virtual TResult<void> DrawModel3D(const FModelDraw3D&)
	{
		return Unsupported_Internal();
	}
	/**
	 * 失敗を含むビュー実行後に状態を後始末する。
	 */
	virtual TResult<void> EndView3D()
	{
		return {};
	}
	/**
	 * 描画したフレームを画面へ提示する。
	 */
	virtual TResult<void> Present() = 0;
private:
	/**
	 * 対応していない機能を成功として扱わない。
	 */
	static TResult<void> Unsupported_Internal()
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "Unsupported render capability");
	}
};
}
// namespace Dxf
