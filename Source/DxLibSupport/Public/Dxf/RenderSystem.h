#pragma once
#include "Dxf/RenderContext.h"
#include "Dxf/FramePresenter.h"
#include "Toolbox/Function.h"
#include "Toolbox/Optional.h"
namespace Dxf
{
/**
 * フレーム描画と描画先の切り替えを管理する型。
 */
class FRenderSystem final : public IRenderControl
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Backend ネイティブ処理の呼び出し先。
	 */
	explicit FRenderSystem(IRenderBackend& Backend);
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	FRenderSystem(const FRenderSystem&) = delete;
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	FRenderSystem& operator=(const FRenderSystem&) = delete;
	/**
	 * 処理に必要な実行環境を取得する。
	 */
	FORCEINLINE FRenderContext& GetContext() noexcept
	{
		return m_Context;
	}
	/**
	 * @param Jobs Applicationが所有する共有実行器。
	 */
	TResult<void> SetExecutionJobs(Toolbox::FJobSystem& Jobs)
	{
		return m_Context.SetExecutionJobs_Internal(&Jobs);
	}
	/**
	 * フレームの描画受付を開始する。
	 * @param Width 幅。
	 * @param Height 高さ。
	 * @param Color 描画色。
	 */
	TResult<void> BeginFrame(Toolbox::int32 Width, Toolbox::int32 Height, FColor Color = {0, 0, 0, 255});
	/**
	 * 描画先のテクスチャを設定する。
	 * @param Target 描画先またはその設定結果。
	 */
	TResult<void> SetRenderTarget(const FRenderTarget& Target) override;
	/**
	 * 画面のバックバッファを設定する。
	 */
	TResult<void> SetBackBuffer() override;
	/**
	 * 現在の描画先を指定色で消去する。
	 * @param Color 描画色。
	 */
	TResult<void> ClearTarget(FColor Color) override;
	/**
	 * 蓄積した描画命令を実行する。
	 */
	TResult<void> Flush();
	/**
	 * ネイティブ処理を呼び出し、描画状態を復元する。
	 * @param Callback 利用者が指定した処理。
	 */
	TResult<void> Native(const Toolbox::TFunction<TResult<void>()>& Callback) override;
	/**
	 * 蓄積した描画を実行してフレームを終了する。
	 */
	TResult<void> EndFrame();
	/**
	 * 実行待ちの描画を破棄してフレームを中断する。
	 */
	void CancelFrame() noexcept;
private:
	/**
	 * エラーを保存して現在の描画フレームを終了する。
	 * @param Error エラー情報。
	 */
	TResult<void> FailFrame_Internal(const FError& Error);
	/**
	 * 退避した描画先を復元する。
	 */
	TResult<void> RestoreTarget_Internal();
	/**
	 * 蓄積した描画命令を実行する。
	 */
	TResult<void> Flush_Internal();
	/**
	 * ネイティブ処理の呼び出し先。
	 */
	IRenderBackend* m_pBackend;
	/**
	 * 描画内容を画面へ提示する窓口。
	 */
	FFramePresenter m_Presenter;
	/**
	 * 描画命令の蓄積先。
	 */
	FRenderQueue2D m_Queue;
	/**
	 * 処理に必要な実行環境。
	 */
	FRenderContext m_Context;
	/**
	 * 描画先またはその設定結果。
	 */
	FRenderTarget m_Target;
	/**
	 * フレーム処理中のエラー。
	 */
	Toolbox::TOptional<FError> m_FrameError;
	/**
	 * 幅。
	 */
	Toolbox::int32 m_Width = 0;
	/**
	 * 高さ。
	 */
	Toolbox::int32 m_Height = 0;
	/**
	 * フレームの描画を受け付けているか。
	 */
	bool m_bFrame = false;
	/**
	 * 処理の実行中か。
	 */
	bool m_bBusy = false;
};
}
// namespace Dxf
