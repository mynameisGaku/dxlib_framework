#pragma once
#include "Dxf/RenderQueue2D.h"
#include "Dxf/RenderControl.h"
namespace Dxf
{
class FRenderContext
{
public:
	explicit FRenderContext(FRenderQueue2D& Queue, IRenderControl* Control = nullptr) : m_pQueue(&Queue), m_pControl(Control)
	{
	}
	TResult<void> Draw(FTexture Texture, FVector2 Position, const FSpriteDrawOptions& Options = {})
	{
		return m_pQueue->Submit(FSpriteCommand{std::move(Texture), Position, Options});
	}
	TResult<void> DrawText(FFont Font, std::string Text, FVector2 Position, const FDrawStyle& Options = {})
	{
		return m_pQueue->Submit(FTextCommand{std::move(Font), std::move(Text), Position, Options});
	}
	TResult<void> FillRectangle(FIntRect Rectangle, const FDrawStyle& Options = {})
	{
		return m_pQueue->Submit(FRectangleCommand{Rectangle, Options});
	}
	TResult<void> SetRenderTarget(const FRenderTarget& Target)
	{
		return m_pControl ? m_pControl->SetRenderTarget(Target) : MissingControl_Internal();
	}
	TResult<void> SetBackBuffer()
	{
		return m_pControl ? m_pControl->SetBackBuffer() : MissingControl_Internal();
	}
	TResult<void> ClearTarget(FColor Color)
	{
		return m_pControl ? m_pControl->ClearTarget(Color) : MissingControl_Internal();
	}
	TResult<void> Native(const std::function<TResult<void>()>& Callback)
	{
		return m_pControl ? m_pControl->Native(Callback) : MissingControl_Internal();
	}
private:
	static TResult<void> MissingControl_Internal()
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "This context has no immediate render control");
	}
	FRenderQueue2D* m_pQueue;
	IRenderControl* m_pControl;
};
}
