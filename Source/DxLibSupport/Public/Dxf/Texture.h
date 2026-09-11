#pragma once
#include "Dxf/ResourceRegistry.h"
namespace Dxf
{
struct FTextureMetadata
{
	int Width = 0;
	int Height = 0;
	bool bRenderTarget = false;
};
using FTextureResource = TResourceRecord<FTextureMetadata>;
class FTexture
{
public:
	FTexture() = default;
	explicit FTexture(std::shared_ptr<FTextureResource> Resource) : m_pResource(std::move(Resource))
	{
	}
	bool IsValid() const noexcept
	{
		return GetNativeHandle_Internal() >= 0;
	}
	int GetWidth() const noexcept
	{
		return m_pResource ? m_pResource->GetMetadata().Width : 0;
	}
	int GetHeight() const noexcept
	{
		return m_pResource ? m_pResource->GetMetadata().Height : 0;
	}
	int GetNativeHandle_Internal() const noexcept
	{
		return m_pResource ? m_pResource->GetHandle_Internal() : -1;
	}
	const std::shared_ptr<FTextureResource>& GetResource_Internal() const noexcept
	{
		return m_pResource;
	}
private:
	std::shared_ptr<FTextureResource> m_pResource;
};
class FRenderTarget
{
public:
	FRenderTarget() = default;
	explicit FRenderTarget(std::shared_ptr<FTextureResource> Resource) : m_Texture(std::move(Resource))
	{
	}
	bool IsValid() const noexcept
	{
		return m_Texture.IsValid() && m_Texture.GetResource_Internal()->GetMetadata().bRenderTarget;
	}
	const FTexture& AsTexture() const noexcept
	{
		return m_Texture;
	}
	int GetWidth() const noexcept
	{
		return m_Texture.GetWidth();
	}
	int GetHeight() const noexcept
	{
		return m_Texture.GetHeight();
	}
private:
	FTexture m_Texture;
};
}
