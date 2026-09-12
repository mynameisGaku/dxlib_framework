#pragma once
#include "Dxf/AssetBackend.h"
#include "Dxf/ResourceRegistry.h"
namespace Dxf
{
/**
 * ネイティブフォントと設定を保持するレコード。
 */
using FFontResource = TResourceRecord<FFontOptions>;
/**
 * 文字描画に使うフォントを管理する型。
 */
class FFont
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	FFont() = default;
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	explicit FFont(Toolbox::TSharedPtr<FFontResource> Resource) : m_pResource(Toolbox::Move(Resource))
	{
	}
	/**
	 * 検証に成功したかを調べる。
	 */
	bool IsValid() const noexcept
	{
		return GetNativeHandle_Internal() >= 0;
	}
	/**
	 * ネイティブAPIのリソース識別値を取得する。
	 */
	Toolbox::int32 GetNativeHandle_Internal() const noexcept
	{
		return m_pResource ? m_pResource->GetHandle_Internal() : -1;
	}
	/**
	 * 共有するリソースを取得する。
	 */
	FORCEINLINE const Toolbox::TSharedPtr<FFontResource>& GetResource_Internal() const noexcept
	{
		return m_pResource;
	}

private:
	/**
	 * 共有するリソース。
	 */
	Toolbox::TSharedPtr<FFontResource> m_pResource;
};
} // namespace Dxf
