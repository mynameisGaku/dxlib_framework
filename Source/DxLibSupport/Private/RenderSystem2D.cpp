#include "Dxf/RenderSystem2D.h"
#include "Dxf/GuardValue.h"
#include <exception>

namespace Dxf
{
namespace
{
template <typename TFunction>
TResult<void> CallBackend_Internal(TFunction&& Function)
{
	try
	{
		return Function();
	}
	catch (const std::exception& Error)
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, Error.what());
	}
	catch (...)
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "Unknown render backend exception");
	}
}
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
	auto Result = CallBackend_Internal([&]
	{
		return m_Presenter.Begin(Width, Height, Color);
	});
	if (!Result)
	{
		return Result;
	}
	m_Width = Width;
	m_Height = Height;
	m_Target = {};
	m_bFrame = true;
	m_FrameError.reset();
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
	auto Set = CallBackend_Internal([&]
	{
		return m_pBackend->SetTarget(Handle, Width, Height);
	});
	if (!Set)
	{
		return Set;
	}
	auto Reset = CallBackend_Internal([&]
	{
		return m_pBackend->ResetState(Width, Height);
	});
	if (!Reset)
	{
		return Reset;
	}
	m_Queue.SetTarget_Internal(Handle);
	return {};
}
TResult<void> FRenderSystem2D::FailFrame_Internal(const FError& Error)
{
	// Rendering may already have changed pixels. Keep the first failure until the
	// frame is ended or cancelled; a later successful operation cannot undo it.
	if (!m_FrameError)
	{
		m_FrameError = Error;
	}
	m_Queue.SetAccepting_Internal(false);
	m_Queue.Clear_Internal();
	return TResult<void>::Failure(*m_FrameError);
}
TResult<void> FRenderSystem2D::Flush_Internal()
{
	if (m_FrameError)
	{
		return TResult<void>::Failure(*m_FrameError);
	}
	auto Result = CallBackend_Internal([&]() -> TResult<void>
	{
		if (m_Target.AsTexture().GetResource_Internal() && !m_Target.IsValid())
		{
			return TResult<void>::Failure(EErrorCode::InvalidState, "Target was invalidated");
		}
		return m_Queue.Execute_Internal(*m_pBackend);
	});
	return Result ? Result : FailFrame_Internal(Result.Error());
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
	if (!Flushed)
	{
		return Flushed;
	}
	auto Cleared = CallBackend_Internal([&]
	{
		return m_pBackend->Clear(Color);
	});
	return Cleared ? Cleared : FailFrame_Internal(Cleared.Error());
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
	// Restoration is attempted even after the callback fails. Never resume drawing
	// after either failure, even when the caller ignores Native's result.
	auto Restored = RestoreTarget_Internal();
	if (!Result)
	{
		return FailFrame_Internal(Result.Error());
	}
	if (!Restored)
	{
		return FailFrame_Internal(Restored.Error());
	}
	m_Queue.SetAccepting_Internal(true);
	return {};
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
	auto Presented = CallBackend_Internal([&]
	{
		return m_Presenter.Present();
	});
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
	m_FrameError.reset();
	m_bFrame = false;
}
}
