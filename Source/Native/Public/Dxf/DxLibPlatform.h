#pragma once
#include "Dxf/Platform.h"
namespace Dxf
{
/**
 * DxLibの初期化とウィンドウイベントを管理する型。
 */
class FDxLibPlatform final : public IPlatform
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	FDxLibPlatform() = default;
	/**
	 * 所有する状態を終了し、必要なリソースを解放する。
	 */
	~FDxLibPlatform() override;
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	FDxLibPlatform(const FDxLibPlatform&) = delete;
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	FDxLibPlatform& operator=(const FDxLibPlatform&) = delete;
	/**
	 * 使用に必要な初期化を行う。
	 * @param Settings 初期化に使用する設定。
	 */
	TResult<void> Initialize(const FWindowSettings& Settings) override;
	/**
	 * 管理する処理とリソースを順序どおり終了する。
	 */
	void Shutdown() noexcept override;
	/**
	 * OSイベントを処理し継続可否を返す。
	 */
	TResult<bool> PumpEvents() override;
	/**
	 * クライアント領域・最小化・フォーカス・DPIを取得し、Resizableなら描画先をクライアント領域の寸法へ変える。
	 * 描画先の変更に失敗した場合は以前の状態を保ち、失敗を返す。最小化の間は短く待ち、空回りしない。
	 */
	TResult<FWindowState> SyncWindow() override;
	/**
	 * 自アプリのウィンドウだけのマウスの捕捉（SetCapture／ReleaseCapture）と標準カーソルの形を反映する。
	 * 捕捉を取得できない場合は取り消し、次のSyncWindowで捕捉していないと返す。解除は何度呼んでもよい。
	 */
	TResult<void> ApplyRequests(const FPlatformRequests& Requests) override;

private:
	/**
	 * 初期化が完了しているか。
	 */
	bool m_bInitialized = false;
	/**
	 * 拡縮の扱いと垂直同期（画面モードの変更後に設定し直す）。
	 */
	EWindowResizeMode m_Resize = EWindowResizeMode::Fixed;
	bool m_bVSync = true;
	/**
	 * 前のフレームで確定した状態。
	 */
	FWindowState m_State;
};
} // namespace Dxf
