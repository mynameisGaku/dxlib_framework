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
}
// namespace
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
		if (Value.Options.bClip &&
		    (Value.Options.ClipRect.Right < Value.Options.ClipRect.Left || Value.Options.ClipRect.Bottom < Value.Options.ClipRect.Top))
		{
			return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid clip rectangle");
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
		else if constexpr (Toolbox::IsSame<T, FRectangleCommand>)
		{
			if (Value.Rectangle.Right < Value.Rectangle.Left || Value.Rectangle.Bottom < Value.Rectangle.Top)
			{
				return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid rectangle");
			}
		}
		else
		{
			auto Pixel = [](FVector2 V)
			{
				// 整数描画APIへの丸めで表現範囲を超えないことを先に検査する。
				return Finite_Internal(V) && Toolbox::Abs(static_cast<Toolbox::f64>(V.X)) <= 2147483000.0 &&
				Toolbox::Abs(static_cast<Toolbox::f64>(V.Y)) <= 2147483000.0;
			};
			bool Valid = false;
			if constexpr (Toolbox::IsSame<T, FLineCommand2D>)
			{
				Valid = Pixel(Value.Start) && Pixel(Value.End);
			}
			else if constexpr (Toolbox::IsSame<T, FCircleCommand2D>)
			{
				Valid = Pixel(Value.Center) && Toolbox::IsFinite(Value.Radius) && Value.Radius >= 0 &&
				static_cast<Toolbox::f64>(Value.Radius) <= 2147483000.0 &&
				Toolbox::Abs(static_cast<Toolbox::f64>(Value.Center.X)) + Value.Radius <= 2147483000.0 &&
				Toolbox::Abs(static_cast<Toolbox::f64>(Value.Center.Y)) + Value.Radius <= 2147483000.0;
			}
			else
			{
				Valid = Pixel(Value.A) && Pixel(Value.B) && Pixel(Value.C);
			}
			if (!Valid)
			{
				return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid 2D shape");
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
	if (!IsOwnerOperationAllowed_Internal() || !m_bAccepting || m_bExecuting)
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
	if (!IsOwnerOperationAllowed_Internal() || m_bExecuting)
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
	}
	);
	// Backendに設定中のクリップ（命令ごとに値で持ち、変わるときだけ設定する）。
	bool bClipActive = false;
	FIntRect ActiveClip{};
	// 途中の失敗でも、クリップを描画先全体へ戻す（最初の失敗を結果として保つ）。
	auto RestoreClip = [&]() -> TResult<void>
	{
		if (!bClipActive)
		{
			return {};
		}
		bClipActive = false;
		try
		{
			return Backend.SetClip2D(false, {});
		}
		catch (const Toolbox::FException& Error)
		{
			return TResult<void>::Failure(EErrorCode::BackendFailure, Error.What());
		}
		catch (...)
		{
			return TResult<void>::Failure(EErrorCode::BackendFailure, "Clip restoration threw");
		}
	};
	try
	{
		// 実行する描画命令を順に処理する。
		for (const auto& Command : Commands)
		{
			// 入力内容の検証結果。
			auto Validation = Validate_Internal(Command);
			if (!Validation)
			{
				(void)RestoreClip();
				return Validation;
			}
			// 命令のクリップ。空の矩形は描かない。
			const FDrawStyle& Style = Toolbox::Visit(
			    [](const auto& Value) -> const FDrawStyle&
			    {
				    return Value.Options;
			    },
			    Command);
			if (Style.bClip)
			{
				if (Style.ClipRect.Right <= Style.ClipRect.Left || Style.ClipRect.Bottom <= Style.ClipRect.Top)
				{
					continue;
				}
				if (!Backend.SupportsClip2D())
				{
					(void)RestoreClip();
					return TResult<void>::Failure(EErrorCode::BackendFailure, "2D clip unsupported");
				}
				const FIntRect& Clip = Style.ClipRect;
				if (!bClipActive || Clip.Left != ActiveClip.Left || Clip.Top != ActiveClip.Top ||
				    Clip.Right != ActiveClip.Right || Clip.Bottom != ActiveClip.Bottom)
				{
					bClipActive = true;
					auto Set = Backend.SetClip2D(true, Clip);
					if (!Set)
					{
						bClipActive = true;
						(void)RestoreClip();
						return Set;
					}
					bClipActive = true;
					ActiveClip = Clip;
				}
			}
			else if (bClipActive)
			{
				auto Restored = RestoreClip();
				if (!Restored)
				{
					return Restored;
				}
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
				    else if constexpr (Toolbox::IsSame<T, FRectangleCommand>)
				    {
					    if (!Value.bFilled && !Backend.SupportsShapes2D())
					    {
						    return TResult<void>::Failure(EErrorCode::BackendFailure, "Outline rectangle unsupported");
					    }
					    return Backend.DrawRectangle(Value);
				    }
				    else
				    {
					    if (!Backend.SupportsShapes2D())
					    {
						    return TResult<void>::Failure(EErrorCode::BackendFailure, "2D shape unsupported");
					    }
					    if constexpr (Toolbox::IsSame<T, FLineCommand2D>)
					    {
						    return Backend.DrawLine2D(Value);
					    }
					    else if constexpr (Toolbox::IsSame<T, FCircleCommand2D>)
					    {
						    return Backend.DrawCircle2D(Value);
					    }
					    else
					    {
						    return Backend.DrawTriangle2D(Value);
					    }
				    }
			    },
			    Command);
			if (!Result)
			{
				(void)RestoreClip();
				return Result;
			}
		}
		return RestoreClip();
	}
	catch (...)
	{
		// 復帰がさらに失敗しても最初に発生した例外を再送する。
		(void)RestoreClip();
		throw;
	}
}
}
// namespace Dxf
