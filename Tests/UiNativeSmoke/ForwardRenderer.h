// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_TEST_UI_FORWARD_RENDERER_H
#define DXF_TEST_UI_FORWARD_RENDERER_H
#include "Dxf/RenderBackend.h"
#include "Toolbox/Function.h"
namespace Dxf::UiSmoke
{
/**
 * 描画はすべて本物のBackendへ委譲し、Present直前だけ検査を差し込む試験用アダプター。
 */
class FForwardRenderer final : public IRenderBackend
{
public:
	explicit FForwardRenderer(IRenderBackend& Backend) : m_Backend(Backend)
	{
	}
	Toolbox::TFunction<void()> BeforePresent;
	TResult<void> SetTarget(Toolbox::int32 Handle, Toolbox::int32 Width, Toolbox::int32 Height) override;
	TResult<void> Clear(FColor Color) override;
	TResult<void> ResetState(Toolbox::int32 Width, Toolbox::int32 Height) override;
	TResult<void> DrawSprite(const FSpriteCommand& Value) override;
	TResult<void> DrawText(const FTextCommand& Value) override;
	TResult<void> DrawRectangle(const FRectangleCommand& Value) override;
	TResult<void> DrawLine2D(const FLineCommand2D& Value) override;
	TResult<void> DrawCircle2D(const FCircleCommand2D& Value) override;
	TResult<void> DrawTriangle2D(const FTriangleCommand2D& Value) override;
	TResult<void> BeginView3D(const FRenderView3D& Value) override;
	TResult<void> DrawGeometry3D(const FPreparedGeometry3D& Value) override;
	TResult<void> DrawModel3D(const FModelDraw3D& Value) override;
	TResult<void> DrawTexturedQuad3D(const FTexturedQuad3D& Value) override;
	TResult<void> SetClip2D(bool Enabled, FIntRect Rect) override;
	TResult<void> EndView3D() override;
	bool SupportsShapes2D() const noexcept override;
	bool SupportsClip2D() const noexcept override;
	bool SupportsViewports3D() const noexcept override;
	bool SupportsGeometry3D() const noexcept override;
	bool SupportsModels3D() const noexcept override;
	bool SupportsTexturedQuads3D() const noexcept override;
	TResult<void> Present() override;

private:
	IRenderBackend& m_Backend;
};
} // namespace Dxf::UiSmoke
#endif
