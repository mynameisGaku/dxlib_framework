#pragma once
#include "Dxf/RenderCommands.h"
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
	 * 描画したフレームを画面へ提示する。
	 */
	virtual TResult<void> Present() = 0;
};
} // namespace Dxf
