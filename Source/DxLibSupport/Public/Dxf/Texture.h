#pragma once
#include "Dxf/ResourceRegistry.h"
namespace Dxf
{
/**
 * テクスチャの寸法と描画先の属性を管理する型。
 */
struct FTextureMetadata
{
	/**
	 * 幅。
	 */
	Toolbox::int32 Width = 0;
	/**
	 * 高さ。
	 */
	Toolbox::int32 Height = 0;
	/**
	 * 描画先として使用できるテクスチャか。
	 */
	bool bRenderTarget = false;
	/**
	 * 乗算済みアルファの画像か（読込の設定による）。描画先テクスチャは描いた方法に従うため偽。
	 */
	bool bPremultipliedAlpha = false;
};
/**
 * ネイティブテクスチャと寸法を保持するレコード。
 */
using FTextureResource = TResourceRecord<FTextureMetadata>;
/**
 * 描画するテクスチャを管理する型。
 */
class FTexture
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	FTexture() = default;
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	explicit FTexture(Toolbox::TSharedPtr<FTextureResource> Resource) : m_pResource(Toolbox::Move(Resource))
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
	 * 幅を取得する。
	 */
	Toolbox::int32 GetWidth() const noexcept
	{
		return m_pResource ? m_pResource->GetMetadata().Width : 0;
	}
	/**
	 * 高さを取得する。
	 */
	Toolbox::int32 GetHeight() const noexcept
	{
		return m_pResource ? m_pResource->GetMetadata().Height : 0;
	}
	/**
	 * 乗算済みアルファの画像か（FTextureLoadOptions::bPremultipliedAlphaで読んだ画像）。
	 */
	bool IsPremultipliedAlpha() const noexcept
	{
		return m_pResource && m_pResource->GetMetadata().bPremultipliedAlpha;
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
	FORCEINLINE const Toolbox::TSharedPtr<FTextureResource>& GetResource_Internal() const noexcept
	{
		return m_pResource;
	}

private:
	/**
	 * 共有するリソース。
	 */
	Toolbox::TSharedPtr<FTextureResource> m_pResource;
};
/**
 * 描画先のテクスチャを管理する型。
 */
class FRenderTarget
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	FRenderTarget() = default;
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	explicit FRenderTarget(Toolbox::TSharedPtr<FTextureResource> Resource) : m_Texture(Toolbox::Move(Resource))
	{
	}
	/**
	 * 検証に成功したかを調べる。
	 */
	bool IsValid() const noexcept
	{
		return m_Texture.IsValid() && m_Texture.GetResource_Internal()->GetMetadata().bRenderTarget;
	}
	/**
	 * 描画先を読み取り用テクスチャとして参照する。
	 */
	FORCEINLINE const FTexture& AsTexture() const noexcept
	{
		return m_Texture;
	}
	/**
	 * 幅を取得する。
	 */
	Toolbox::int32 GetWidth() const noexcept
	{
		return m_Texture.GetWidth();
	}
	/**
	 * 高さを取得する。
	 */
	Toolbox::int32 GetHeight() const noexcept
	{
		return m_Texture.GetHeight();
	}

private:
	/**
	 * 描画するテクスチャ。
	 */
	FTexture m_Texture;
};
} // namespace Dxf
