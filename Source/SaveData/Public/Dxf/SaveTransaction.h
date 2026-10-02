// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAVETRANSACTION_H
#define DXF_SAVETRANSACTION_H
#include "Dxf/SaveDocument.h"
#include "Dxf/SaveLimits.h"
#include "Toolbox/Platform.h"
#include "Toolbox/UniquePtr.h"
namespace Dxf
{
/**
 * 公開前の書込と排他を唯一所有する。破棄だけでは公開しない。
 */
class FSaveTransaction
{
public:
	/**
	 * @param Root 明示した絶対保存領域。@param Slot 検証する識別子。@param Document 不変値。@param Limits 有限上限。
	 */
	static FSaveTransaction Prepare(const Toolbox::FPath& Root, Toolbox::FString Slot, FSaveDocument Document, FSaveLimits Limits = {});
	/**
	 * 未公開なら確定面を変えずに排他を解放する。未公開tempは読込候補にならない。
	 */
	~FSaveTransaction() noexcept;
	/**
	 * 排他の複製を禁止する。
	 */
	FSaveTransaction(const FSaveTransaction&) = delete;
	/**
	 * 排他の複製を禁止する。
	 */
	FSaveTransaction& operator=(const FSaveTransaction&) = delete;
	/**
	 * @param Other 書込結果と排他の所有権を移す。
	 */
	FSaveTransaction(FSaveTransaction&& Other) noexcept;
	/**
	 * @param Other 未公開の所有権を移す。旧一時ファイルを公開しない。
	 */
	FSaveTransaction& operator=(FSaveTransaction&& Other) noexcept;
	/**
	 * 最新要求・取消しの照合後、所有側から一度だけ呼ぶ。公開確認後に確定世代を返す。
	 */
	Toolbox::uint64 Publish();
	/**
	 * 公開前はtrue。公開後はfalseを返し、Canceledと偽らない。
	 */
	bool Cancel();
	/**
	 * 確認済みの公開後だけtrue。
	 */
	bool IsCommitted() const noexcept;

private:
	/**
	 * OS排他・temp・検証済み封筒の唯一所有状態。
	 */
	struct FImpl;
	/**
	 * @param Impl 準備済み状態を引き受ける。
	 */
	explicit FSaveTransaction(Toolbox::TUniquePtr<FImpl> Impl);
	/**
	 * 破棄中に確保しない内部所有権。
	 */
	Toolbox::TUniquePtr<FImpl> m_pImpl;
};
} // namespace Dxf
#endif
