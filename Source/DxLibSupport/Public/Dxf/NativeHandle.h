#pragma once
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * 移動だけが可能な所有者。解放処理は有効なハンドルだけを受け取り、例外を送出しない。
 */
class FNativeHandle
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	FNativeHandle() = default;
	/**
	 * ネイティブハンドルを例外なしで解放する関数。
	 */
	using FReleaseFunction = void (*)(void*, Toolbox::int32) noexcept;
	/**
	 * 有効なハンドルには解放処理が必須。Contextは所有者より長く存続させる。
	 * @param Handle ハンドル。
	 * @param Context 処理に必要な実行環境。
	 * @param Release ネイティブ資源を解放する処理。
	 */
	FNativeHandle(Toolbox::int32 Handle, void* Context, FReleaseFunction Release) noexcept
	    : m_Handle(Handle), m_pContext(Context), m_pRelease(Release)
	{
	}
	/**
	 * 所有する状態を終了し、必要なリソースを解放する。
	 */
	~FNativeHandle()
	{
		Reset();
	}
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	FNativeHandle(const FNativeHandle&) = delete;
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	FNativeHandle& operator=(const FNativeHandle&) = delete;
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Other 操作相手。
	 */
	FNativeHandle(FNativeHandle&& Other) noexcept
	    : m_Handle(Toolbox::Exchange(Other.m_Handle, -1)), m_pContext(Other.m_pContext), m_pRelease(Other.m_pRelease)
	{
	}
	/**
	 * 所有する状態を代入する。
	 * @param Other 操作相手。
	 */
	FNativeHandle& operator=(FNativeHandle&& Other) noexcept
	{
		if (this != &Other)
		{
			Reset();
			m_Handle = Toolbox::Exchange(Other.m_Handle, -1);
			m_pContext = Other.m_pContext;
			m_pRelease = Other.m_pRelease;
		}
		return *this;
	}
	/**
	 * 所有するネイティブハンドルの値を取得する。
	 */
	FORCEINLINE Toolbox::int32 Get() const noexcept
	{
		return m_Handle;
	}
	/**
	 * バックエンドの識別ポインターを取得する。
	 */
	FORCEINLINE const void* GetBackendIdentity_Internal() const noexcept
	{
		return m_pContext;
	}
	/**
	 * 保持している状態を初期値へ戻す。
	 */
	void Reset() noexcept
	{
		// ハンドル。
		const Toolbox::int32 Handle = Toolbox::Exchange(m_Handle, -1);
		if (Handle >= 0 && m_pRelease)
		{
			m_pRelease(m_pContext, Handle);
		}
	}

private:
	/**
	 * ハンドル。
	 */
	Toolbox::int32 m_Handle = -1;
	/**
	 * 処理に必要な実行環境。
	 */
	void* m_pContext = nullptr;
	/**
	 * ネイティブ資源を解放する処理。
	 */
	FReleaseFunction m_pRelease = nullptr;
};
} // namespace Dxf
