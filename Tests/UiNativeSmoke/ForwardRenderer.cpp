// SPDX-License-Identifier: NOASSERTION
#include "ForwardRenderer.h"
namespace Dxf::UiSmoke
{
TResult<void> FForwardRenderer::SetTarget(Toolbox::int32 Handle, Toolbox::int32 Width, Toolbox::int32 Height)
{
	return m_Backend.SetTarget(Handle, Width, Height);
}

TResult<void> FForwardRenderer::Clear(FColor Color)
{
	return m_Backend.Clear(Color);
}

TResult<void> FForwardRenderer::ResetState(Toolbox::int32 Width, Toolbox::int32 Height)
{
	return m_Backend.ResetState(Width, Height);
}

TResult<void> FForwardRenderer::DrawSprite(const FSpriteCommand& Value)
{
	return m_Backend.DrawSprite(Value);
}

TResult<void> FForwardRenderer::DrawText(const FTextCommand& Value)
{
	return m_Backend.DrawText(Value);
}

TResult<void> FForwardRenderer::DrawRectangle(const FRectangleCommand& Value)
{
	return m_Backend.DrawRectangle(Value);
}

TResult<void> FForwardRenderer::DrawLine2D(const FLineCommand2D& Value)
{
	return m_Backend.DrawLine2D(Value);
}

TResult<void> FForwardRenderer::DrawCircle2D(const FCircleCommand2D& Value)
{
	return m_Backend.DrawCircle2D(Value);
}

TResult<void> FForwardRenderer::DrawTriangle2D(const FTriangleCommand2D& Value)
{
	return m_Backend.DrawTriangle2D(Value);
}

TResult<void> FForwardRenderer::BeginView3D(const FRenderView3D& Value)
{
	return m_Backend.BeginView3D(Value);
}

TResult<void> FForwardRenderer::DrawGeometry3D(const FPreparedGeometry3D& Value)
{
	return m_Backend.DrawGeometry3D(Value);
}

TResult<void> FForwardRenderer::DrawModel3D(const FModelDraw3D& Value)
{
	return m_Backend.DrawModel3D(Value);
}

TResult<void> FForwardRenderer::DrawTexturedQuad3D(const FTexturedQuad3D& Value)
{
	return m_Backend.DrawTexturedQuad3D(Value);
}

TResult<void> FForwardRenderer::SetClip2D(bool Enabled, FIntRect Rect)
{
	return m_Backend.SetClip2D(Enabled, Rect);
}

TResult<void> FForwardRenderer::EndView3D()
{
	return m_Backend.EndView3D();
}

bool FForwardRenderer::SupportsShapes2D() const noexcept
{
	return m_Backend.SupportsShapes2D();
}

bool FForwardRenderer::SupportsClip2D() const noexcept
{
	return m_Backend.SupportsClip2D();
}

bool FForwardRenderer::SupportsViewports3D() const noexcept
{
	return m_Backend.SupportsViewports3D();
}

bool FForwardRenderer::SupportsGeometry3D() const noexcept
{
	return m_Backend.SupportsGeometry3D();
}

bool FForwardRenderer::SupportsModels3D() const noexcept
{
	return m_Backend.SupportsModels3D();
}

bool FForwardRenderer::SupportsTexturedQuads3D() const noexcept
{
	return m_Backend.SupportsTexturedQuads3D();
}

TResult<void> FForwardRenderer::Present()
{
	if (BeforePresent)
	{
		BeforePresent();
	}
	return m_Backend.Present();
}
} // namespace Dxf::UiSmoke
