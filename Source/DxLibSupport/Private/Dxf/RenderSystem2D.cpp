#include "Dxf/RenderSystem2D.h"
#include "Dxf/GuardValue.h"
#include "Toolbox/Utility.h"

namespace Dxf
{
namespace
{
// バックエンド処理を呼び出し失敗を結果へ変換する。
// @param Function 各要素に適用する処理。
template <typename TFunction> TResult<void> CallBackend_Internal(TFunction&& Function)
{
	try
	{
		return Function();
	}
	// 呼び出し先の例外を処理結果へ変換する。
	catch (const Toolbox::FException& Error)
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, Error.What());
	}
	catch (...)
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "Unknown render backend exception");
	}
}
// 不正な実行状態を示すエラーを生成する。
TResult<void> StateError_Internal()
{
	return TResult<void>::Failure(EErrorCode::InvalidState, "Invalid or reentrant render operation");
}
} // namespace
// 必要な依存関係を受け取り、初期状態を構築する。
// @param Backend ネイティブ処理の呼び出し先。
FRenderSystem2D::FRenderSystem2D(IRenderBackend& Backend)
    : m_pBackend(&Backend), m_Presenter(Backend), m_Context(m_Queue, this)
{
}
// フレームの描画受付を開始する。
// @param Width 幅。
// @param Height 高さ。
// @param Color 描画色。
TResult<void> FRenderSystem2D::BeginFrame(Toolbox::int32 Width, Toolbox::int32 Height, FColor Color)
{
	if (!m_Queue.IsOwnerOperationAllowed_Internal() || m_bBusy || m_bFrame)
	{
		return StateError_Internal();
	}
	if (Width <= 0 || Height <= 0)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid frame dimensions");
	}
	// 処理終了時に状態を戻すガード。
	TGuardValue Guard(m_bBusy, true);
	// 処理結果。
	auto Result = CallBackend_Internal(
	    [&]
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
	m_FrameError.Reset();
	m_Queue.SetTarget_Internal(-1);
	m_Queue.SetAccepting_Internal(true);
	return {};
}
// 退避した描画先を復元する。
TResult<void> FRenderSystem2D::RestoreTarget_Internal()
{
	// テクスチャを描画先に設定しているか。
	const bool bTarget = m_Target.AsTexture().GetResource_Internal() != nullptr;
	if (bTarget && !m_Target.IsValid())
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Target was invalidated");
	}
	// 幅。
	const Toolbox::int32 Width = bTarget ? m_Target.GetWidth() : m_Width;
	// 高さ。
	const Toolbox::int32 Height = bTarget ? m_Target.GetHeight() : m_Height;
	// ハンドル。
	const Toolbox::int32 Handle = bTarget ? m_Target.AsTexture().GetNativeHandle_Internal() : -1;
	// 描画先の設定結果。
	auto Set = CallBackend_Internal(
	    [&]
	    {
		    return m_pBackend->SetTarget(Handle, Width, Height);
	    });
	if (!Set)
	{
		return Set;
	}
	// 既定状態への復元結果。
	auto Reset = CallBackend_Internal(
	    [&]
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
// エラーを保存して現在の描画フレームを終了する。
// @param Error エラー情報。
TResult<void> FRenderSystem2D::FailFrame_Internal(const FError& Error)
{
	// 描画済み内容は取り消せないため、フレーム終了または中断まで最初のエラーを保持する。
	if (!m_FrameError)
	{
		m_FrameError = Error;
	}
	m_Queue.SetAccepting_Internal(false);
	m_Queue.Clear_Internal();
	return TResult<void>::Failure(*m_FrameError);
}
// 蓄積した描画命令を実行する。
TResult<void> FRenderSystem2D::Flush_Internal()
{
	if (m_FrameError)
	{
		return TResult<void>::Failure(*m_FrameError);
	}
	// 処理結果。
	auto Result = CallBackend_Internal(
	    [&]() -> TResult<void>
	    {
		    if (m_Target.AsTexture().GetResource_Internal() && !m_Target.IsValid())
		    {
			    return TResult<void>::Failure(EErrorCode::InvalidState, "Target was invalidated");
		    }
		    return m_Queue.Execute_Internal(*m_pBackend);
	    });
	return Result ? Result : FailFrame_Internal(Result.Error());
}
// 蓄積した描画命令を実行する。
TResult<void> FRenderSystem2D::Flush()
{
	if (!m_Queue.IsOwnerOperationAllowed_Internal() || !m_bFrame || m_bBusy)
	{
		return StateError_Internal();
	}
	// 処理終了時に状態を戻すガード。
	TGuardValue Guard(m_bBusy, true);
	return Flush_Internal();
}
// 描画先のテクスチャを設定する。
// @param Target 描画先またはその設定結果。
TResult<void> FRenderSystem2D::SetRenderTarget(const FRenderTarget& Target)
{
	if (!m_Queue.IsOwnerOperationAllowed_Internal() || !m_bFrame || m_bBusy)
	{
		return StateError_Internal();
	}
	if (!Target.IsValid())
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid target");
	}
	// 処理終了時に状態を戻すガード。
	TGuardValue Guard(m_bBusy, true);
	// 描画命令実行の結果。
	auto Flushed = Flush_Internal();
	if (!Flushed)
	{
		return Flushed;
	}
	// 前回の状態。
	const auto Previous = m_Target;
	m_Target = Target;
	// 処理結果。
	auto Result = RestoreTarget_Internal();
	if (!Result)
	{
		m_Target = Previous;
		// 描画先復元の結果。
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
// 画面のバックバッファを設定する。
TResult<void> FRenderSystem2D::SetBackBuffer()
{
	if (!m_Queue.IsOwnerOperationAllowed_Internal() || !m_bFrame || m_bBusy)
	{
		return StateError_Internal();
	}
	// 処理終了時に状態を戻すガード。
	TGuardValue Guard(m_bBusy, true);
	// 描画命令実行の結果。
	auto Flushed = Flush_Internal();
	if (!Flushed)
	{
		return Flushed;
	}
	// 前回の状態。
	const auto Previous = m_Target;
	m_Target = {};
	// 処理結果。
	auto Result = RestoreTarget_Internal();
	if (!Result)
	{
		m_Target = Previous;
		// 描画先復元の結果。
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
// 現在の描画先を指定色で消去する。
// @param Color 描画色。
TResult<void> FRenderSystem2D::ClearTarget(FColor Color)
{
	if (!m_Queue.IsOwnerOperationAllowed_Internal() || !m_bFrame || m_bBusy)
	{
		return StateError_Internal();
	}
	// 処理終了時に状態を戻すガード。
	TGuardValue Guard(m_bBusy, true);
	// 描画命令実行の結果。
	auto Flushed = Flush_Internal();
	if (!Flushed)
	{
		return Flushed;
	}
	// 画面消去の結果。
	auto Cleared = CallBackend_Internal(
	    [&]
	    {
		    return m_pBackend->Clear(Color);
	    });
	return Cleared ? Cleared : FailFrame_Internal(Cleared.Error());
}
// ネイティブ処理を呼び出し、描画状態を復元する。
// @param Callback 利用者が指定した処理。
TResult<void> FRenderSystem2D::Native(const Toolbox::TFunction<TResult<void>()>& Callback)
{
	if (!m_Queue.IsOwnerOperationAllowed_Internal() || !m_bFrame || m_bBusy || !Callback)
	{
		return StateError_Internal();
	}
	// 処理終了時に状態を戻すガード。
	TGuardValue Guard(m_bBusy, true);
	// 描画命令実行の結果。
	auto Flushed = Flush_Internal();
	if (!Flushed)
	{
		return Flushed;
	}
	m_Queue.SetAccepting_Internal(false);
	// 処理結果。
	TResult<void> Result;
	try
	{
		Result = Callback();
	}
	// 呼び出し先の例外を処理結果へ変換する。
	catch (const Toolbox::FException& Error)
	{
		Result = TResult<void>::Failure(EErrorCode::UserException, Error.What());
	}
	catch (...)
	{
		Result = TResult<void>::Failure(EErrorCode::UserException, "Unknown native callback exception");
	}
	// コールバックが失敗しても復元を試みる。どちらかが失敗した場合は描画を再開しない。
	//
	// 描画先復元の結果。
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
// 蓄積した描画を実行してフレームを終了する。
TResult<void> FRenderSystem2D::EndFrame()
{
	if (!m_Queue.IsOwnerOperationAllowed_Internal() || !m_bFrame || m_bBusy)
	{
		return StateError_Internal();
	}
	// バックバッファへの復帰結果。
	auto Back = SetBackBuffer();
	if (!Back)
	{
		CancelFrame();
		return Back;
	}
	// 処理終了時に状態を戻すガード。
	TGuardValue Guard(m_bBusy, true);
	// 画面提示の結果。
	auto Presented = CallBackend_Internal(
	    [&]
	    {
		    return m_Presenter.Present();
	    });
	m_bFrame = false;
	m_Queue.SetAccepting_Internal(false);
	m_Queue.Clear_Internal();
	m_Target = {};
	return Presented;
}
// 実行待ちの描画を破棄してフレームを中断する。
void FRenderSystem2D::CancelFrame() noexcept
{
	if (!m_Queue.IsOwnerOperationAllowed_Internal() || m_bBusy)
	{
		return;
	}
	m_Queue.Clear_Internal();
	m_Queue.SetAccepting_Internal(false);
	m_Target = {};
	m_FrameError.Reset();
	m_bFrame = false;
}
} // namespace Dxf
