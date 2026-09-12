#include "Dxf/RenderQueue2D.h"
#include "Dxf/GuardValue.h"
#include "Dxf/Utf8.h"
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
			if (!Detail::IsValidNativeString_Internal(Value.Text, true))
			{
				return TResult<void>::Failure(EErrorCode::InvalidArgument, "Text must be UTF-8 without embedded NUL");
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
}
