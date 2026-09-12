#pragma once
#include "Dxf/RenderBackend.h"
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
	 * 順序を整えて描画命令を実行する。
	 * @param Backend ネイティブ処理の呼び出し先。
	 */
	TResult<void> Execute_Internal(IRenderBackend& Backend);
	/**
	 * 描画先と描画範囲を設定する。
	 * @param Target 描画先またはその設定結果。
	 */
	void SetTarget_Internal(Toolbox::int32 Target) noexcept
	{
		m_Target = Target;
	}
	/**
	 * 描画命令を受け付けるかを設定する。
	 * @param bAccepting 新しい要求を受け付けるか。
	 */
	void SetAccepting_Internal(bool bAccepting) noexcept
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
