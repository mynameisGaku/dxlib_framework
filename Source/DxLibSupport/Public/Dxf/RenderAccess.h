// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_RENDER_ACCESS_H
#define DXF_RENDER_ACCESS_H
#include "Dxf/RenderQueue2D.h"
namespace Dxf
{
/**
 * 所有スレッドと2D/3D共通の再入境界。
 */
class FRenderAccess
{
public:
	/**
	 * @param Queue フレームの受付状態を持つ既存の2Dキュー。
	 */
	explicit FRenderAccess(FRenderQueue2D& Queue) : m_pQueue(&Queue)
	{
	}
	/**
	 * 所有スレッド以外では変更可能な状態を読まない。
	 */
	FORCEINLINE bool IsAllowed() const noexcept
	{
		return m_pQueue->IsScopeOperationAllowed_Internal() && !m_bBusy;
	}
	/**
	 * 描画フレーム中だけ命令を受け付ける。
	 */
	FORCEINLINE bool IsAccepting() const noexcept
	{
		return IsAllowed() && m_pQueue->IsAccepting_Internal();
	}
	/**
	 * 並列生成とBackend実行の再入を抑止する状態。内部専用。
	 */
	bool m_bBusy = false;
	/**
	 * 共有実行器。呼び出し中および未退役Jobより長く存続する借用。
	 */
	Toolbox::FJobSystem* m_pJobs = nullptr;
private:
	/**
	 * 受付状態を所有するキュー。
	 */
	FRenderQueue2D* m_pQueue;
};
}
#endif
