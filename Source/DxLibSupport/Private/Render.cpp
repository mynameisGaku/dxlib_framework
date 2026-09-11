#include "Dxf/RenderSystem2D.h"
#include "Dxf/GuardValue.h"
#include <algorithm>
#include <cmath>
#include <tuple>
#include <exception>
namespace Dxf
{
namespace
{
TResult<void> StateError_Internal()
{
	return TResult<void>::Failure(EErrorCode::InvalidState, "Invalid or reentrant render operation");
}
bool ValidStyle_Internal(const FDrawStyle& Style)
{
	return std::isfinite(Style.Opacity) && Style.Opacity >= 0.0f && Style.Opacity <= 1.0f;
}
bool Finite_Internal(FVector2 Value)
{
	return std::isfinite(Value.X) && std::isfinite(Value.Y);
}
}
TResult<void> FRenderQueue2D::Validate_Internal(const FRenderCommand& Command) const
{
	return std::visit([&](const auto& Value) -> TResult<void>
	{
		if (!ValidStyle_Internal(Value.Options))
		{
			return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid draw style");
		}
		using T = std::decay_t<decltype(Value)>;
		if constexpr (std::is_same_v<T, FSpriteCommand>)
		{
			if (!Value.Texture.IsValid())
			{
				return TResult<void>::Failure(EErrorCode::InvalidState, "Texture was invalidated");
			}
			if (Value.Texture.GetNativeHandle_Internal() == m_Target)
			{
				return TResult<void>::Failure(EErrorCode::InvalidArgument, "Render target feedback is forbidden");
			}
			if (!Finite_Internal(Value.Position) || !Finite_Internal(Value.Options.Scale) || !Finite_Internal(Value.Options.Pivot) || !std::isfinite(Value.Options.RotationRadians))
			{
				return TResult<void>::Failure(EErrorCode::InvalidArgument, "Nonfinite sprite transform");
			}
		}
		else if constexpr (std::is_same_v<T, FTextCommand>)
		{
			if (!Value.Font.IsValid())
			{
				return TResult<void>::Failure(EErrorCode::InvalidState, "Font was invalidated");
			}
			if (!Finite_Internal(Value.Position))
			{
				return TResult<void>::Failure(EErrorCode::InvalidArgument, "Nonfinite text position");
			}
		}
		else
		{
			if (Value.Rectangle.Right < Value.Rectangle.Left || Value.Rectangle.Bottom < Value.Rectangle.Top)
			{
				return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid rectangle");
			}
		}
		return {};
	}, Command);
}
TResult<void> FRenderQueue2D::Submit(FRenderCommand Command)
{
	if (!m_bAccepting || m_bExecuting)
	{
		return StateError_Internal();
	}
	auto Validation = Validate_Internal(Command);
	if (!Validation)
	{
		return Validation;
	}
	m_Commands.push_back(std::move(Command));
	return {};
}
TResult<void> FRenderQueue2D::Execute_Internal(IRenderBackend& Backend)
{
	if (m_bExecuting)
	{
		return StateError_Internal();
	}
	TGuardValue Guard(m_bExecuting, true);
	std::vector<FRenderCommand> Commands;
	Commands.swap(m_Commands);
	auto Key = [](const FRenderCommand& Command)
	{
		return std::visit([](const auto& Value)
		{
			return std::pair(Value.Options.Layer, Value.Options.Order);
		}, Command);
	};
	std::stable_sort(Commands.begin(), Commands.end(), [&](const auto& A, const auto& B)
	{
		return Key(A) < Key(B);
	});
	for (const auto& Command : Commands)
	{
		auto Validation = Validate_Internal(Command);
		if (!Validation)
		{
			return Validation;
		}
		auto Result = std::visit([&](const auto& Value) -> TResult<void>
		{
			using T = std::decay_t<decltype(Value)>;
			if constexpr (std::is_same_v<T, FSpriteCommand>)
			{
				return Backend.DrawSprite(Value);
			}
			else if constexpr (std::is_same_v<T, FTextCommand>)
			{
				return Backend.DrawText(Value);
			}
			else
			{
				return Backend.DrawRectangle(Value);
			}
		}, Command);
		if (!Result)
		{
			return Result;
		}
	}
	return {};
}
FRenderSystem2D::FRenderSystem2D(IRenderBackend& Backend)
	: m_pBackend(&Backend), m_Presenter(Backend), m_Context(m_Queue)
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
