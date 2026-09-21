#pragma once
#include "Dxf/RenderBackend.h"
#include "Toolbox/JobSystem.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * 描画命令の検証と順序付けを管理する型。
 */
class FRenderQueue2D
{
public:
	/**
	 * 検証した描画命令をキューへ追加する。
	 * @param Command 実行する描画命令。
	 */
	TResult<void> Submit(FRenderCommand Command);
	/**
	 * 描画命令を並列生成して一括で追加する。呼び出し側で完了まで待つ。
	 * 各添字は専用領域へ一度だけ書き込み、入力順に統合する。
	 * 一つでも生成・検証に失敗したらキューを変更しない。空の範囲は成功する。
	 * 生成中は資源の寿命を呼び出し側で保ち、所有スレッド以外からRegistryを
	 * 変更しないこと。最終検証は所有スレッドで行う。
	 * @param Jobs 生成の実行に借用するJob System。
	 * @param Count 生成する命令数。
	 * @param Generate 添字と専用領域を受け取り、命令を作る処理。
	 * @param MinimumBatch 一つのJobへまとめる最小要素数。
	 */
	template <typename F>
	TResult<void> SubmitGenerated(Toolbox::FJobSystem& Jobs, Toolbox::size_t Count, F&& Generate,
	                             Toolbox::size_t MinimumBatch = 16)
	{
		if (!m_bAccepting || m_bExecuting)
		{
			return TResult<void>::Failure(EErrorCode::InvalidState, "Invalid or reentrant render operation");
		}
		if (Count == 0)
		{
			return {};
		}
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
		m_Commands.Reserve(m_Commands.Size() + Count);
		for (Toolbox::size_t Index = 0; Index < Count; ++Index)
		{
			m_Commands.PushBack(Toolbox::Move(Slots[Index]));
		}
		return {};
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
		m_Target = Target;
	}
	/**
	 * 描画命令を受け付けるかを設定する。
	 * @param bAccepting 新しい要求を受け付けるか。
	 */
	FORCEINLINE void SetAccepting_Internal(bool bAccepting) noexcept
	{
		m_bAccepting = bAccepting;
	}
	/**
	 * 蓄積した内容を消去する。
	 */
	void Clear_Internal() noexcept
	{
		m_Commands.Clear();
	}

private:
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
