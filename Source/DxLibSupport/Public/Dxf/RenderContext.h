// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_RENDER_CONTEXT_H
#define DXF_RENDER_CONTEXT_H
#include "Dxf/Render2DContext.h"
#include "Dxf/Render3DContext.h"
#include "Dxf/RenderControl.h"
namespace Dxf
{
/**
 * 次元別の描画窓口とパス制御を束ねる。Draw・DrawText等の旧入口は提供しない。
 * Get2D/Get3Dで得た窓口はこのContextより長く保存しない。
 */
class FRenderContext
{
public:
	/**
	 * @param Queue 2Dキュー。
	 * @param Control パス制御。
	 * @param Jobs 借用する共有実行器。
	 */
	explicit FRenderContext(FRenderQueue2D& Queue, IRenderControl* Control = nullptr, Toolbox::FJobSystem* Jobs = nullptr)
	: m_Access(Queue), m_Draw2D(Queue, m_Access), m_Draw3D(m_Access), m_pControl(Control)
	{
		m_Access.m_pJobs = Jobs;
	}
	/**
	 * 内部の借用関係を複製しない。
	 */
	FRenderContext(const FRenderContext&) = delete;
	FRenderContext& operator=(const FRenderContext&) = delete;
	/**
	 * 画面座標の2D描画入口。
	 */
	FORCEINLINE FRender2DContext& Get2D() noexcept
	{
		return m_Draw2D;
	}
	/**
	 * ワールド座標の3D描画入口。
	 */
	FORCEINLINE FRender3DContext& Get3D() noexcept
	{
		return m_Draw3D;
	}
	/**
	 * @param Jobs Applicationが所有する共有実行器。停止前に全使用が完了していること。
	 */
	TResult<void> SetExecutionJobs_Internal(Toolbox::FJobSystem* Jobs)
	{
		if (!m_Access.IsAllowed())
		{
			return MissingControl_Internal();
		}
		m_Access.m_pJobs = Jobs;
		return {};
	}
	/**
	 * 所有スレッドと共通生成区間を検査する。
	 */
	FORCEINLINE bool IsOwnerOperationAllowed_Internal() const noexcept
	{
		return m_Access.IsAllowed();
	}
	/**
	 * @param Backend 3D命令を実行するBackend。
	 */
	TResult<void> Execute3D_Internal(IRenderBackend& Backend)
	{
		return m_Draw3D.Execute_Internal(Backend);
	}
	/**
	 * フレームを中断するときの未実行3D命令の解放。
	 */
	void Clear3D_Internal() noexcept
	{
		m_Draw3D.Clear_Internal();
	}
	/**
	 * 未実行の3D命令があるか。3D後の2D状態復元が必要かの判定に使う。
	 */
	FORCEINLINE bool HasPending3D_Internal() const noexcept
	{
		return m_Draw3D.HasCommands_Internal();
	}
	/**
	 * 描画先のテクスチャを設定する。
	 * @param Target 描画先またはその設定結果。
	 */
	TResult<void> SetRenderTarget(const FRenderTarget& Target)
	{
		if (!m_Access.IsAllowed())
		{
			return MissingControl_Internal();
		}
		return m_pControl ? m_pControl->SetRenderTarget(Target) : MissingControl_Internal();
	}
	/**
	 * 画面のバックバッファを設定する。
	 */
	TResult<void> SetBackBuffer()
	{
		if (!m_Access.IsAllowed())
		{
			return MissingControl_Internal();
		}
		return m_pControl ? m_pControl->SetBackBuffer() : MissingControl_Internal();
	}
	/**
	 * 現在の描画先を指定色で消去する。
	 * @param Color 描画色。
	 */
	TResult<void> ClearTarget(FColor Color)
	{
		if (!m_Access.IsAllowed())
		{
			return MissingControl_Internal();
		}
		return m_pControl ? m_pControl->ClearTarget(Color) : MissingControl_Internal();
	}
	/**
	 * ネイティブ処理を呼び出し、描画状態を復元する。
	 * @param Callback 利用者が指定した処理。
	 */
	TResult<void> Native(const Toolbox::TFunction<TResult<void>()>& Callback)
	{
		if (!m_Access.IsAllowed())
		{
			return MissingControl_Internal();
		}
		return m_pControl ? m_pControl->Native(Callback) : MissingControl_Internal();
	}
private:
	/**
	 * 不正なパス制御を失敗として返す。
	 */
	static TResult<void> MissingControl_Internal()
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Render pass control unavailable");
	}
	/**
	 * 二次元・三次元共通の所有境界。
	 */
	FRenderAccess m_Access;
	/**
	 * 2D専用の借用窓口。
	 */
	FRender2DContext m_Draw2D;
	/**
	 * 3D命令を所有する窓口。
	 */
	FRender3DContext m_Draw3D;
	/**
	 * パスを実行する所有者。
	 */
	IRenderControl* m_pControl;
};
}
#endif
