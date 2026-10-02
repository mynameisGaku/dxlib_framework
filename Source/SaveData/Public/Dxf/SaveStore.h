// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAVESTORE_H
#define DXF_SAVESTORE_H
#include "Dxf/SaveLoadResult.h"
#include "Dxf/SaveTransaction.h"
namespace Dxf
{
/**
 * ProjectRootから独立した任意リンクの保存窓口。Scene・Physics・Nativeを所有しない。
 */
class FSaveStore
{
public:
	/**
	 * @param Root 完全修飾した専用領域。不正時はCWDへ切り替えず例外。
	 * @param Limits 文書上限。
	 */
	explicit FSaveStore(Toolbox::FPath Root, FSaveLimits Limits = {});
	/**
	 * @param ApplicationId ユーザー領域を分けるASCII識別子。OS取得失敗は例外。
	 */
	static Toolbox::FPath DefaultRoot(Toolbox::FStringView ApplicationId);
	/**
	 * @param Slot パスではないASCII識別子。読取も非待機のslot排他で整合させる。
	 */
	FSaveLoadResult Load(Toolbox::FStringView Slot) const;
	/**
	 * 確定面があるslot名だけを返す。最大128件。temp・lock・不正名は含めない。
	 * 一覧は瞬間の値であり、その後のLoadでも排他と完全性を検査する。
	 */
	Toolbox::TVector<Toolbox::FString> ListSlots() const;
	/**
	 * @param Slot 保存系列。
	 * @param Document 世代はStoreが割り当てる。公開前の結果を返す。
	 */
	FSaveTransaction Stage(Toolbox::FString Slot, FSaveDocument Document) const;
	/**
	 * @param Slot 保存系列。
	 * @param Document 論理値。全工程成功後の世代を返す同期経路。
	 */
	Toolbox::uint64 Save(Toolbox::FString Slot, FSaveDocument Document) const;
	/**
	 * 起動時に確定した保存領域を返す。保存文書から変更できない。
	 */
	const Toolbox::FPath& GetRoot() const noexcept;
	/**
	 * 読込後の純粋移行にも適用する、起動時に確定した上限。
	 */
	FSaveLimits GetLimits() const noexcept;

private:
	/**
	 * CWD・Assetsから独立した不変領域。
	 */
	Toolbox::FPath m_Root;
	/**
	 * 各処理で再利用する有限上限。
	 */
	FSaveLimits m_Limits;
};
} // namespace Dxf
#endif
