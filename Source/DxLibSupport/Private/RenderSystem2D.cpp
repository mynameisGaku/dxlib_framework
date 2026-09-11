#include "Dxf/RenderSystem2D.h"
#include "Dxf/GuardValue.h"
#include <exception>

namespace Dxf
{
namespace
{
TResult<void> StateError_Internal()
{
	return TResult<void>::Failure(EErrorCode::InvalidState, "Invalid or reentrant render operation");
}
}
FRenderSystem2D::FRenderSystem2D(IRenderBackend& Backend)
	: m_pBackend(&Backend), m_Presenter(Backend), m_Context(m_Queue, this)
{
}
TResult<void> FRenderSystem2D::BeginFrame(int Width, int Height, FColor Color)
{
	if (m_bBusy || m_bFrame)
	{
		return StateError_Internal();
	}
	if (Width <= 0 || Height <= 0)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid frame dimensions");
	}
	TGuardValue Guard(m_bBusy, true);
	auto Result = m_Presenter.Begin(Width, Height, Color);
	if (!Result)
	{
		return Result;
	}
	m_Width = Width;
	m_Height = Height;
	m_Target = {};
	m_bFrame = true;
	m_Queue.SetTarget_Internal(-1);
	m_Queue.SetAccepting_Internal(true);
	return {};
}
TResult<void> FRenderSystem2D::RestoreTarget_Internal()
{
	const bool bTarget = m_Target.AsTexture().GetResource_Internal() != nullptr;
	if (bTarget && !m_Target.IsValid())
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Target was invalidated");
	}
	const int Width = bTarget ? m_Target.GetWidth() : m_Width;
	const int Height = bTarget ? m_Target.GetHeight() : m_Height;
	const int Handle = bTarget ? m_Target.AsTexture().GetNativeHandle_Internal() : -1;
	auto Set = m_pBackend->SetTarget(Handle, Width, Height);
	if (!Set)
	{
		return Set;
	}
	auto Reset = m_pBackend->ResetState(Width, Height);
	if (!Reset)
	{
		return Reset;
	}
	m_Queue.SetTarget_Internal(Handle);
	return {};
}
TResult<void> FRenderSystem2D::Flush_Internal()
{
	if (m_Target.AsTexture().GetResource_Internal() && !m_Target.IsValid())
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Target was invalidated");
	}
	return m_Queue.Execute_Internal(*m_pBackend);
}
TResult<void> FRenderSystem2D::Flush()
{
	if (!m_bFrame || m_bBusy)
	{
		return StateError_Internal();
	}
	TGuardValue Guard(m_bBusy, true);
	return Flush_Internal();
}
TResult<void> FRenderSystem2D::SetRenderTarget(const FRenderTarget& Target)
{
	if (!m_bFrame || m_bBusy)
	{
		return StateError_Internal();
	}
	if (!Target.IsValid())
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid target");
	}
	TGuardValue Guard(m_bBusy, true);
	auto Flushed = Flush_Internal();
	if (!Flushed)
	{
		return Flushed;
	}
	const auto Previous = m_Target;
	m_Target = Target;
	auto Result = RestoreTarget_Internal();
	if (!Result)
	{
		m_Target = Previous;
		auto Restored = RestoreTarget_Internal();
		if (!Restored)
		{
			m_bFrame = false;
			m_Queue.SetAccepting_Internal(false);
			m_Queue.Clear_Internal();
			m_Target = {};
		}
	}
	return Result;
}
TResult<void> FRenderSystem2D::SetBackBuffer()
{
	if (!m_bFrame || m_bBusy)
	{
		return StateError_Internal();
	}
	TGuardValue Guard(m_bBusy, true);
	auto Flushed = Flush_Internal();
	if (!Flushed)
	{
		return Flushed;
	}
	const auto Previous = m_Target;
	m_Target = {};
	auto Result = RestoreTarget_Internal();
	if (!Result)
	{
		m_Target = Previous;
		auto Restored = RestoreTarget_Internal();
		if (!Restored)
		{
			m_bFrame = false;
			m_Queue.SetAccepting_Internal(false);
			m_Target = {};
		}
	}
	return Result;
}
TResult<void> FRenderSystem2D::ClearTarget(FColor Color)
{
	if (!m_bFrame || m_bBusy)
	{
		return StateError_Internal();
	}
	TGuardValue Guard(m_bBusy, true);
	auto Flushed = Flush_Internal();
	return Flushed ? m_pBackend->Clear(Color) : Flushed;
}
TResult<void> FRenderSystem2D::Native(const std::function<TResult<void>()>& Callback)
{
	if (!m_bFrame || m_bBusy || !Callback)
	{
		return StateError_Internal();
	}
	TGuardValue Guard(m_bBusy, true);
	auto Flushed = Flush_Internal();
	if (!Flushed)
	{
		return Flushed;
	}
	m_Queue.SetAccepting_Internal(false);
	TResult<void> Result;
	try
	{
		Result = Callback();
	}
	catch (const std::exception& Error)
	{
		Result = TResult<void>::Failure(EErrorCode::UserException, Error.what());
	}
	catch (...)
	{
		Result = TResult<void>::Failure(EErrorCode::UserException, "Unknown native callback exception");
	}
	auto Restored = RestoreTarget_Internal();
	m_Queue.SetAccepting_Internal(static_cast<bool>(Restored));
	if (!Restored)
	{
		m_bFrame = false;
		m_Target = {};
		return Restored;
	}
	return Result;
}
TResult<void> FRenderSystem2D::EndFrame()
{
	if (!m_bFrame || m_bBusy)
	{
		return StateError_Internal();
	}
	auto Back = SetBackBuffer();
	if (!Back)
	{
		CancelFrame();
		return Back;
	}
	TGuardValue Guard(m_bBusy, true);
	auto Presented = m_Presenter.Present();
	m_bFrame = false;
	m_Queue.SetAccepting_Internal(false);
	m_Queue.Clear_Internal();
	m_Target = {};
	return Presented;
}
void FRenderSystem2D::CancelFrame() noexcept
{
	if (m_bBusy)
	{
		return;
	}
	m_Queue.Clear_Internal();
	m_Queue.SetAccepting_Internal(false);
	m_Target = {};
	m_bFrame = false;
}
}
