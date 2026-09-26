// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_INPUT_ROUTER_H
#define DXF_INPUT_ROUTER_H
#include "Dxf/Contexts.h"
#include "Toolbox/SharedPtr.h"
namespace Dxf
{
class IInputRouter;
namespace Detail
{
/**
 * 入力仲介の生存印。所有スレッドだけで解決する。
 */
struct FInputRouterLifetime
{
	IInputRouter* Router = nullptr;
};
/**
 * Sceneが所有する接続枠。外部の仲介は弱参照で安全に切断できる。
 */
struct FInputRouterSlot
{
	Toolbox::TWeakPtr<FInputRouterLifetime> Router;
};
} // namespace Detail
/**
 * Scene更新前の任意の入力仲介。返す値は仲介が通知中に破棄されても有効。
 * RuntimeはUIを知らず、入力を取得し直さない。所有スレッド専用。
 */
class IInputRouter
{
public:
	IInputRouter() : m_pLifetime(Toolbox::MakeShared<Detail::FInputRouterLifetime>())
	{
		m_pLifetime->Router = this;
	}
	virtual ~IInputRouter()
	{
		m_pLifetime->Router = nullptr;
	}
	IInputRouter(const IInputRouter&) = delete;
	IInputRouter& operator=(const IInputRouter&) = delete;
	/**
	 * Sceneの実時間側で一度処理し、子と固定更新に渡す入力を返す。
	 * @param Context 入力の事実とフレーム時刻。
	 */
	virtual FInputSnapshot RouteInput(const FTickContext& Context) = 0;
	/**
	 * Sceneの接続に使う非所有の生存印。
	 */
	Toolbox::TWeakPtr<Detail::FInputRouterLifetime> GetLifetime_Internal() const noexcept
	{
		return Toolbox::TWeakPtr<Detail::FInputRouterLifetime>(m_pLifetime);
	}

private:
	/** 生存中だけ自身を指す。 */
	Toolbox::TSharedPtr<Detail::FInputRouterLifetime> m_pLifetime;
};
} // namespace Dxf
#endif
