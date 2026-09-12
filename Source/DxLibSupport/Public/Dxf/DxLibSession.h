#pragma once
#include "Dxf/Platform.h"
namespace Dxf
{
/**
 * プラットフォームはセッションより長く存続させ、他のリソース所有者は先に終了する。
 */
class FDxLibSession
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Platform OSとウィンドウ機能の呼び出し先。
	 */
	explicit FDxLibSession(IPlatform& Platform) : m_pPlatform(&Platform)
	{
	}
	/**
	 * 所有する状態を終了し、必要なリソースを解放する。
	 */
	~FDxLibSession()
	{
		Shutdown();
	}
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	FDxLibSession(const FDxLibSession&) = delete;
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	FDxLibSession& operator=(const FDxLibSession&) = delete;
	/**
	 * 使用に必要な初期化を行う。
	 * @param Settings 初期化に使用する設定。
	 */
	TResult<void> Initialize(const FWindowSettings& Settings)
	{
		if (m_bInitialized)
		{
			return TResult<void>::Failure(EErrorCode::InvalidState, "Session already initialized");
		}
		if (Settings.Width <= 0 || Settings.Height <= 0)
		{
			return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid window size");
		}
		// 処理結果。
		auto Result = m_pPlatform->Initialize(Settings);
		m_bInitialized = static_cast<bool>(Result);
		return Result;
	}
	/**
	 * 管理する処理とリソースを順序どおり終了する。
	 */
	void Shutdown() noexcept
	{
		if (m_bInitialized)
		{
			m_bInitialized = false;
			m_pPlatform->Shutdown();
		}
	}
	/**
	 * 初期化が完了しているかを調べる。
	 */
	FORCEINLINE bool IsInitialized() const noexcept
	{
		return m_bInitialized;
	}

private:
	/**
	 * OSとウィンドウ機能の呼び出し先。
	 */
	IPlatform* m_pPlatform;
	/**
	 * 初期化が完了しているか。
	 */
	bool m_bInitialized = false;
};
} // namespace Dxf
