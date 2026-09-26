// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_ROOT_HANDLE_H
#define DXF_UI_ROOT_HANDLE_H
#include "Toolbox/SharedPtr.h"
namespace Dxf
{
class FUiRoot;
namespace Detail
{
/**
 * 所有スレッドでだけ読むルートの生存印。ルートの寿命そのものは延長しない。
 */
struct FUiRootLifetime
{
	FUiRoot* Owner = nullptr;
};
} // namespace Detail
/**
 * 表示先が借用するルートの弱い参照。破棄開始時から解決しない。
 * Getの結果は次の利用者コールバックまでの借用であり、コールバック後は解決し直す。
 */
class FUiRootHandle
{
public:
	FUiRootHandle() = default;
	explicit FUiRootHandle(Toolbox::TWeakPtr<Detail::FUiRootLifetime> Life) noexcept : m_pLife(Toolbox::Move(Life))
	{
	}
	/**
	 * 現在生存するルート。所有スレッドだけで使う。
	 */
	FORCEINLINE FUiRoot* Get() const noexcept
	{
		const auto Life = m_pLife.Lock();
		return Life ? Life->Owner : nullptr;
	}

private:
	/**
	 * 所有者が保持する生存印。
	 */
	Toolbox::TWeakPtr<Detail::FUiRootLifetime> m_pLife;
};
} // namespace Dxf
#endif
