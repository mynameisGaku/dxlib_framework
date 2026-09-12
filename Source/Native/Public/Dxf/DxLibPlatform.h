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

private:
	/**
	 * 初期化が完了しているか。
	 */
	bool m_bInitialized = false;
};
} // namespace Dxf
