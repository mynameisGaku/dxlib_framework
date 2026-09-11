#pragma once
#include "Dxf/RenderContext.h"
#include "Dxf/FramePresenter.h"
#include <functional>
namespace Dxf
{
class FRenderSystem2D final : public IRenderControl
{
public:
	explicit FRenderSystem2D(IRenderBackend& Backend);
	FRenderSystem2D(const FRenderSystem2D&) = delete;
	FRenderSystem2D& operator=(const FRenderSystem2D&) = delete;
	FRenderContext& GetContext() noexcept
	{
		return m_Context;
	}
	TResult<void> BeginFrame(int Width, int Height, FColor Color = {0, 0, 0, 255});
	TResult<void> SetRenderTarget(const FRenderTarget& Target) override;
	TResult<void> SetBackBuffer() override;
	TResult<void> ClearTarget(FColor Color) override;
	TResult<void> Flush();
	TResult<void> Native(const std::function<TResult<void>()>& Callback) override;
	TResult<void> EndFrame();
	void CancelFrame() noexcept;
private:
	TResult<void> RestoreTarget_Internal();
	TResult<void> Flush_Internal();
	IRenderBackend* m_pBackend;
	FFramePresenter m_Presenter;
	FRenderQueue2D m_Queue;
	FRenderContext m_Context;
	FRenderTarget m_Target;
	int m_Width = 0;
	int m_Height = 0;
	bool m_bFrame = false;
	bool m_bBusy = false;
};
}
