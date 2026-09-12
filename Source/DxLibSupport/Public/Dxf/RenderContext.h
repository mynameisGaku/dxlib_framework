#pragma once
#include "Dxf/RenderQueue2D.h"
#include "Dxf/RenderControl.h"
namespace Dxf
{
/**
 * 描画命令と即時制御の窓口を管理する型。
 */
class FRenderContext
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Queue 描画命令の蓄積先。
	 * @param Control 描画状態を直接制御する窓口。
	 */
	explicit FRenderContext(FRenderQueue2D& Queue, IRenderControl* Control = nullptr)
	    : m_pQueue(&Queue), m_pControl(Control)
	{
	}
	/**
	 * 対象の描画を要求する。
	 * @param Texture 描画するテクスチャ。
	 * @param Position 描画位置。
	 * @param Options 処理に適用する設定。
	 */
	TResult<void> Draw(FTexture Texture, FVector2 Position, const FSpriteDrawOptions& Options = {})
	{
		return m_pQueue->Submit(FSpriteCommand{Toolbox::Move(Texture), Position, Options});
	}
	/**
	 * 文字列の描画命令を処理する。
	 * @param Font 文字描画に使うフォント。
	 * @param Text 描画する文字列。
	 * @param Position 描画位置。
	 * @param Options 処理に適用する設定。
	 */
	TResult<void> DrawText(FFont Font, Toolbox::FString Text, FVector2 Position, const FDrawStyle& Options = {})
	{
		return m_pQueue->Submit(FTextCommand{Toolbox::Move(Font), Toolbox::Move(Text), Position, Options});
	}
	/**
	 * 塗りつぶした矩形の描画を要求する。
	 * @param Rectangle 描画する矩形。
	 * @param Options 処理に適用する設定。
	 */
	TResult<void> FillRectangle(FIntRect Rectangle, const FDrawStyle& Options = {})
	{
		return m_pQueue->Submit(FRectangleCommand{Rectangle, Options});
	}
	/**
	 * 描画先のテクスチャを設定する。
	 * @param Target 描画先またはその設定結果。
	 */
	TResult<void> SetRenderTarget(const FRenderTarget& Target)
	{
		return m_pControl ? m_pControl->SetRenderTarget(Target) : MissingControl_Internal();
	}
	/**
	 * 画面のバックバッファを設定する。
	 */
	TResult<void> SetBackBuffer()
	{
		return m_pControl ? m_pControl->SetBackBuffer() : MissingControl_Internal();
	}
	/**
	 * 現在の描画先を指定色で消去する。
	 * @param Color 描画色。
	 */
	TResult<void> ClearTarget(FColor Color)
	{
		return m_pControl ? m_pControl->ClearTarget(Color) : MissingControl_Internal();
	}
	/**
	 * ネイティブ処理を呼び出し、描画状態を復元する。
	 * @param Callback 利用者が指定した処理。
	 */
	TResult<void> Native(const Toolbox::TFunction<TResult<void>()>& Callback)
	{
		return m_pControl ? m_pControl->Native(Callback) : MissingControl_Internal();
	}

private:
	/**
	 * 描画制御サービス不足を示すエラーを生成する。
	 */
	static TResult<void> MissingControl_Internal()
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "This context has no immediate render control");
	}
	/**
	 * 描画命令の蓄積先。
	 */
	FRenderQueue2D* m_pQueue;
	/**
	 * 描画状態を直接制御する窓口。
	 */
	IRenderControl* m_pControl;
};
} // namespace Dxf
