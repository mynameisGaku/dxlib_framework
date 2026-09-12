#pragma once
#include "Dxf/Result.h"
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
};
} // namespace Dxf
