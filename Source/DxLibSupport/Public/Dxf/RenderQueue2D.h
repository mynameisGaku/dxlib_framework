#pragma once
#include "Dxf/RenderBackend.h"
#include "Dxf/GuardValue.h"
#include "Toolbox/JobSystem.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * 構築した所有スレッドで描画命令を検証・順序付けする。Job本体からの操作は拒否する。
 * 並列生成中の入力資源は呼出し側が保持し、Registryの変更は完了後に行う。
 */
class FRenderQueue2D
{
public:
	/**
	 * 現在のOSスレッドをキューの所有者として記録する。
	 */
	FRenderQueue2D() = default;
	/**
	 * コンテキストから参照されるキューの所有権を複製しない。
	 */
	FRenderQueue2D(const FRenderQueue2D&) = delete;
	/**
	 * 所有スレッドと参照元を保持するためコピー代入を禁止する。
	 */
	FRenderQueue2D& operator=(const FRenderQueue2D&) = delete;
	/**
	 * 所有スレッドかつJob・並列生成の外側で、状態にアクセス可能か返す。
	 * 不正なスレッドでは変更可能なフラグを読み取らず拒否する。
	 */
	FORCEINLINE bool IsOwnerOperationAllowed_Internal() const noexcept
	{
		return Toolbox::FThread::CurrentThreadId() == m_OwnerThreadId &&
		       !Toolbox::FJobSystem::IsExecutingJob() && !m_bGenerating;
	}
	/**
	 * 検証した描画命令をキューへ追加する。
	 * @param Command 実行する描画命令。
	 */
	TResult<void> Submit(FRenderCommand Command);
	/**
	 * 描画命令を並列生成して一括で追加する。呼び出し側で完了まで待つ。
	 * 各添字は専用領域へ一度だけ書き込み、入力順に統合する。
	 * 一つでも生成・検証に失敗したらキューを変更しない。受付中の空範囲は成功する。
	 * 生成中は資源の寿命を呼び出し側で保ち、所有スレッド以外からRegistryを
	 * 変更しないこと。最終検証は所有スレッドで行う。
	 * @param Jobs 生成の実行に借用するJob System。
	 * @param Count 生成する命令数。
	 * @param Generate 添字と専用領域を受け取る処理。同じ捕捉を並列参照するため共有状態を変更しない。
	 * @param MinimumBatch 一つのJobへまとめる最小要素数。
	 */
	template <typename F>
	TResult<void> SubmitGenerated(Toolbox::FJobSystem& Jobs, Toolbox::size_t Count, F&& Generate,
	                             Toolbox::size_t MinimumBatch = 16)
	{
		if (!IsOwnerOperationAllowed_Internal() || !m_bAccepting || m_bExecuting)
		{
			return TResult<void>::Failure(EErrorCode::InvalidState, "Invalid or reentrant render operation");
		}
		if (Count == 0)
		{
			return {};
		}
		// 件数加算と各配列のバイト数がsize_tを超える入力は確保前に拒否する。
		const Toolbox::size_t Maximum = Toolbox::TNumericLimits<Toolbox::size_t>::Max();
		if (Count > Maximum / sizeof(FRenderCommand) - m_Commands.Size() ||
		    Count > Maximum / sizeof(TResult<void>))
		{
			return TResult<void>::Failure(EErrorCode::InvalidArgument, "Render batch size overflow");
		}
		// バッファと生成用捕捉の破棄が終わるまで再入を拒否する。
		TGuardValue Generating(m_bGenerating, true);
		try
		{
			// 添字ごとの専用命令領域。
			Toolbox::TVector<FRenderCommand> Slots(Count);
			// 添字ごとの生成結果。
			Toolbox::TVector<TResult<void>> Outcomes(Count);
			auto GenerateOne = [&](Toolbox::size_t Index)
			{
				Outcomes[Index] = Generate(Index, Slots[Index]);
			};
			if (!Toolbox::ParallelFor(Jobs, Count, GenerateOne, MinimumBatch))
			{
				return TResult<void>::Failure(EErrorCode::UserException, "Parallel command generation failed");
			}
			for (Toolbox::size_t Index = 0; Index < Count; ++Index)
			{
				if (!Outcomes[Index])
				{
					return Outcomes[Index];
				}
			}
			for (Toolbox::size_t Index = 0; Index < Count; ++Index)
			{
				auto Validation = Validate_Internal(Slots[Index]);
				if (!Validation)
				{
					return Validation;
				}
			}
			// Reserve成功後の移動が例外を投げないことを型の変更時にも保証する。
			static_assert(__is_nothrow_constructible(FRenderCommand, FRenderCommand&&));
			m_Commands.Reserve(m_Commands.Size() + Count);
			for (Toolbox::size_t Index = 0; Index < Count; ++Index)
			{
				m_Commands.PushBack(Toolbox::Move(Slots[Index]));
			}
			return {};
		}
		catch (const Toolbox::FException& Error)
		{
			return TResult<void>::Failure(EErrorCode::UserException, Error.What());
		}
		catch (...)
		{
			return TResult<void>::Failure(EErrorCode::UserException, "Render batch preparation failed");
		}
	}
	/**
	 * 順序を整えて描画命令を実行する。
	 * @param Backend ネイティブ処理の呼び出し先。
	 */
	TResult<void> Execute_Internal(IRenderBackend& Backend);
	/**
	 * 描画先と描画範囲を設定する。
	 * @param Target 描画先またはその設定結果。
	 */
	FORCEINLINE void SetTarget_Internal(Toolbox::int32 Target) noexcept
	{
		if (!IsOwnerOperationAllowed_Internal() || m_bExecuting)
		{
			return;
		}
		m_Target = Target;
	}
	/**
	 * 描画命令を受け付けるかを設定する。
	 * @param bAccepting 新しい要求を受け付けるか。
	 */
	FORCEINLINE void SetAccepting_Internal(bool bAccepting) noexcept
	{
		if (!IsOwnerOperationAllowed_Internal() || m_bExecuting)
		{
			return;
		}
		m_bAccepting = bAccepting;
	}
	/**
	 * 蓄積した内容を消去する。
	 */
	void Clear_Internal() noexcept
	{
		if (!IsOwnerOperationAllowed_Internal() || m_bExecuting)
		{
			return;
		}
		m_Commands.Clear();
	}

private:
	/**
	 * 構築時から変わらない所有OSスレッド番号。
	 */
	const Toolbox::uint64 m_OwnerThreadId = Toolbox::FThread::CurrentThreadId();
	/**
	 * 生成バッファの確保から解放まで、所有スレッドで再入を防ぐ。
	 */
	bool m_bGenerating = false;
	/**
	 * 描画命令のリソースと数値を検証する。
	 * @param Command 実行する描画命令。
	 */
	TResult<void> Validate_Internal(const FRenderCommand& Command) const;
	/**
	 * 実行待ちの描画命令。
	 */
	Toolbox::TVector<FRenderCommand> m_Commands;
	/**
	 * 描画先またはその設定結果。
	 */
	Toolbox::int32 m_Target = -1;
	/**
	 * 描画命令を受け付けるか。
	 */
	bool m_bAccepting = false;
	/**
	 * 描画命令を実行しているか。
	 */
	bool m_bExecuting = false;
};
} // namespace Dxf
