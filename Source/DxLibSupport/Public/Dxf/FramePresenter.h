#pragma once
#include "Dxf/RenderBackend.h"
namespace Dxf
{
class FFramePresenter
{
public:
	explicit FFramePresenter(IRenderBackend& Backend) : m_pBackend(&Backend)
	{
	}
	TResult<void> Begin(int Width, int Height, FColor ClearColor)
	{
		auto Target = m_pBackend->SetTarget(-1, Width, Height);
		if (!Target)
		{
			return Target;
		}
		auto Reset = m_pBackend->ResetState(Width, Height);
		if (!Reset)
		{
			return Reset;
		}
		return m_pBackend->Clear(ClearColor);
	}
	TResult<void> Present()
	{
		return m_pBackend->Present();
	}
private:
	IRenderBackend* m_pBackend;
};
}
