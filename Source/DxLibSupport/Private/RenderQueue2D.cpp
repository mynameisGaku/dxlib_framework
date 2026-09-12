#include "Dxf/RenderQueue2D.h"
#include "Dxf/GuardValue.h"
#include "Dxf/Utf8.h"
#include "Toolbox/Algorithm.h"
#include "Toolbox/Utility.h"
namespace Dxf
{
namespace
{
// 不正な実行状態を示すエラーを生成する。
TResult<void> StateError_Internal()
{
	return TResult<void>::Failure(EErrorCode::InvalidState, "Invalid or reentrant render operation");
}
// 不透明度が有効な範囲かを検証する。
// @param Style 描画状態またはその適用結果。
bool ValidStyle_Internal(const FDrawStyle& Style)
{
	return Toolbox::IsFinite(Style.Opacity) && Style.Opacity >= 0.0f && Style.Opacity <= 1.0f;
}
// 座標成分が有限値かを調べる。
// @param Value 処理対象の値。
bool Finite_Internal(FVector2 Value)
{
	return Toolbox::IsFinite(Value.X) && Toolbox::IsFinite(Value.Y);
}
} // namespace
// 描画命令のリソースと数値を検証する。
// @param Command 実行する描画命令。
TResult<void> FRenderQueue2D::Validate_Internal(const FRenderCommand& Command) const
{
	return Toolbox::Visit(
	    [&](const auto& Value) -> TResult<void>
	    {
		    if (!ValidStyle_Internal(Value.Options))
		    {
			    return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid draw style");
		    }
		    // 現在の描画命令の実体型。
		    using T = Toolbox::TDecay<decltype(Value)>;
		    if constexpr (Toolbox::IsSame<T, FSpriteCommand>)
		    {
			    if (!Value.Texture.IsValid())
			    {
				    return TResult<void>::Failure(EErrorCode::InvalidState, "Texture was invalidated");
			    }
			    if (Value.Texture.GetNativeHandle_Internal() == m_Target)
			    {
				    return TResult<void>::Failure(EErrorCode::InvalidArgument, "Render target feedback is forbidden");
			    }
			    if (!Finite_Internal(Value.Position) || !Finite_Internal(Value.Options.Scale) ||
			        !Finite_Internal(Value.Options.Pivot) || !Toolbox::IsFinite(Value.Options.RotationRadians))
			    {
				    return TResult<void>::Failure(EErrorCode::InvalidArgument, "Nonfinite sprite transform");
			    }
		    }
		    else if constexpr (Toolbox::IsSame<T, FTextCommand>)
		    {
			    if (!Value.Font.IsValid())
			    {
				    return TResult<void>::Failure(EErrorCode::InvalidState, "Font was invalidated");
			    }
			    if (!Detail::IsValidNativeString_Internal(Value.Text, true))
			    {
				    return TResult<void>::Failure(EErrorCode::InvalidArgument,
				                                  "Text must be UTF-8 without embedded NUL");
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
	    },
	    Command);
}
// 検証した描画命令をキューへ追加する。
// @param Command 実行する描画命令。
TResult<void> FRenderQueue2D::Submit(FRenderCommand Command)
{
	if (!m_bAccepting || m_bExecuting)
	{
		return StateError_Internal();
	}
	// 入力内容の検証結果。
	auto Validation = Validate_Internal(Command);
	if (!Validation)
	{
		return Validation;
	}
	m_Commands.PushBack(Toolbox::Move(Command));
	return {};
}
// 順序を整えて描画命令を実行する。
// @param Backend ネイティブ処理の呼び出し先。
TResult<void> FRenderQueue2D::Execute_Internal(IRenderBackend& Backend)
{
	if (m_bExecuting)
	{
		return StateError_Internal();
	}
	// 処理終了時に状態を戻すガード。
	TGuardValue Guard(m_bExecuting, true);
	// 実行待ちの描画命令。
	Toolbox::TVector<FRenderCommand> Commands;
	Commands.Swap(m_Commands);
	// 検索または入力のキー。
	auto Key = [](const FRenderCommand& Command)
	{
		return Toolbox::Visit(
		    [](const auto& Value)
		    {
			    return Toolbox::TPair(Value.Options.Layer, Value.Options.Order);
		    },
		    Command);
	};
	Toolbox::StableSort(Commands.Begin(), Commands.End(),
	                    [&](const auto& A, const auto& B)
	                    {
		                    return Key(A) < Key(B);
	                    });
	// 実行する描画命令を順に処理する。
	for (const auto& Command : Commands)
	{
		// 入力内容の検証結果。
		auto Validation = Validate_Internal(Command);
		if (!Validation)
		{
			return Validation;
		}
		// 処理結果。
		auto Result = Toolbox::Visit(
		    [&](const auto& Value) -> TResult<void>
		    {
			    // 現在の描画命令の実体型。
			    using T = Toolbox::TDecay<decltype(Value)>;
			    if constexpr (Toolbox::IsSame<T, FSpriteCommand>)
			    {
				    return Backend.DrawSprite(Value);
			    }
			    else if constexpr (Toolbox::IsSame<T, FTextCommand>)
			    {
				    return Backend.DrawText(Value);
			    }
			    else
			    {
				    return Backend.DrawRectangle(Value);
			    }
		    },
		    Command);
		if (!Result)
		{
			return Result;
		}
	}
	return {};
}
} // namespace Dxf
