#pragma once
#include "Dxf/Result.h"
#include "Dxf/WindowState.h"
#include "Toolbox/String.h"
namespace Dxf
{
/**
 * ウィンドウのタイトルと表示設定を管理する型。
 */
struct FWindowSettings
{
	/**
	 * ウィンドウのタイトル。
	 */
	Toolbox::FString Title = "dxlib_framework";
	/**
	 * 幅。
	 */
	Toolbox::int32 Width = 1280;
	/**
	 * 高さ。
	 */
	Toolbox::int32 Height = 720;
	/**
	 * ウィンドウモードを使用するか。
	 */
	bool bWindowed = true;
	/**
	 * 垂直同期を使用するか。
	 */
	bool bVSync = true;
	/**
	 * 利用者によるウィンドウの拡縮の扱い（既定は拡縮しない）。
	 */
	EWindowResizeMode Resize = EWindowResizeMode::Fixed;
	/**
	 * 最小化の間、ゲームの更新を止めるか（既定は止めず、描画だけを省く）。
	 * 止める場合も、復帰時に止めていた時間を固定更新の追い付きへ変えない。Sceneの一時停止の状態は変えない。
	 */
	bool bPauseWhenMinimized = false;
};
/**
 * OSとウィンドウ機能の呼び出し先を管理する型。
 */
class IPlatform
{
public:
	/**
	 * 派生型を含むオブジェクトを安全に終了する。
	 */
	virtual ~IPlatform() = default;
	/**
	 * 失敗時は実装側で途中まで行った初期化を取り消す。
	 * @param Settings 初期化に使用する設定。
	 */
	virtual TResult<void> Initialize(const FWindowSettings& Settings) = 0;
	/**
	 * 管理する処理とリソースを順序どおり終了する。
	 */
	virtual void Shutdown() noexcept = 0;
	/**
	 * 成功値がfalseの場合は正常終了を要求する。
	 */
	virtual TResult<bool> PumpEvents() = 0;
	/**
	 * フレームの境界でウィンドウの状態を確定する（Applicationが各フレームのOSイベントの直後に呼ぶ）。
	 * 描画先の寸法の変更が必要ならここで行い、描画中の命令がない時点で反映する。
	 * 既定は状態を取得できない実装（bKnown=false）で、起動時の寸法で描き続ける。
	 */
	virtual TResult<FWindowState> SyncWindow()
	{
		return TResult<FWindowState>::Success(FWindowState{});
	}
	/**
	 * そのフレームの更新が求めた操作（マウスの捕捉・カーソルの形）を反映する。
	 * 既定は何もしない（捕捉は取得できず、カーソルは標準のまま。UIの操作の失敗ではない）。
	 * 捕捉は自アプリのウィンドウだけで、取得できない場合は要求を取り消して結果を次のSyncWindowで返す。
	 * @param Requests そのフレームの要求。
	 */
	virtual TResult<void> ApplyRequests(const FPlatformRequests& Requests)
	{
		(void)Requests;
		return {};
	}
};
} // namespace Dxf
