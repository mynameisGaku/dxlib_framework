#pragma once
#include "Dxf/GameObjectComponent.h"
#include "Dxf/RenderContext.h"
#include <stdexcept>

namespace Dxf
{
/** Optional, independently positioned 2D sprite. No transform is imposed on every GameObject. */
class DSpriteRendererComponent : public DGameObjectComponent
{
public:
	explicit DSpriteRendererComponent(FTexture Texture = {}, FVector2 Position = {}, FSpriteDrawOptions Options = {})
		: m_Texture(std::move(Texture)), m_Position(Position), m_Options(Options)
	{
	}
	void SetTexture(FTexture Texture)
	{
		m_Texture = std::move(Texture);
	}
	const FTexture& GetTexture() const noexcept
	{
		return m_Texture;
	}
	FVector2& GetPosition() noexcept
	{
		return m_Position;
	}
	const FVector2& GetPosition() const noexcept
	{
		return m_Position;
	}
	FSpriteDrawOptions& GetOptions() noexcept
	{
		return m_Options;
	}
	const FSpriteDrawOptions& GetOptions() const noexcept
	{
		return m_Options;
	}
protected:
	void OnDraw(FRenderContext& Render) const override
	{
		if (!m_Texture.IsValid())
		{
			return;
		}
		auto Result = Render.Draw(m_Texture, m_Position, m_Options);
		if (!Result)
		{
			// The fixed lifecycle entry translates this exception into TResult.
			throw std::runtime_error(Result.Error().Message);
		}
	}
private:
	FTexture m_Texture;
	FVector2 m_Position;
	FSpriteDrawOptions m_Options;
};
}
