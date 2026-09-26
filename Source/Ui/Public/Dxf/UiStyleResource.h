// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_STYLE_RESOURCE_H
#define DXF_UI_STYLE_RESOURCE_H
#include "Dxf/UiRoot.h"
#include "Dxf/UiStyleParser.h"
#include "Toolbox/Platform.h"
namespace Dxf
{
/**
 * 型付きスタイル資源。明示再読込で、接続中のRootへ同じ有効版を一括で渡す。所有スレッド専用。
 */
class FUiStyleResource
{
public:
	/**
	 * @param Root 接続先。Rootの寿命は保持しない。同じRootは二重登録しない。
	 */
	void Attach(FUiRoot& Root);
	/**
	 * @param Root 接続解除先。現在の見た目は変えない。
	 */
	void Detach(const FUiRoot& Root) noexcept;
	/** 全資源の検証と確保に成功してから差し替える。要素・フォーカス・スクロールは維持する。
	 * @param Sources 部品別または集約済みの版1資源。
	 * @param Limits 読込上限。
	 */
	TResult<void> Reload(const Toolbox::TVector<FUiStyleSource>& Sources, const FUiStyleLimits& Limits = {});
	/** 集約済み資源を明示的に再読込する。外部監視は行わない。
	 * @param Path 読むファイル。呼出し元のProjectRoot等から解決したもの。
	 * @param Limits 読込上限。
	 */
	TResult<void> ReloadFile(const Toolbox::FPath& Path, const FUiStyleLimits& Limits = {});
	/**
	 * 最後に成功した資源。未読込は空。
	 */
	FORCEINLINE Toolbox::TSharedPtr<const FUiStyleSheet> Get() const noexcept
	{
		return m_pCurrent;
	}
	/**
	 * 成功した差替え回数。
	 */
	FORCEINLINE Toolbox::uint64 GetRevision() const noexcept
	{
		return m_Revision;
	}

private:
	/**
	 * 接続先の弱い生存印。
	 */
	Toolbox::TVector<FUiRootHandle> m_Roots;
	/**
	 * 最後の有効版。
	 */
	Toolbox::TSharedPtr<const FUiStyleSheet> m_pCurrent;
	/**
	 * 差替え世代。
	 */
	Toolbox::uint64 m_Revision = 0;
};
} // namespace Dxf
#endif
