// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMESAVETICKET_H
#define DXF_GAMESAVETICKET_H
#include "Dxf/GameSaveRequestState.h"
#include "Toolbox/SharedPtr.h"
#include "Toolbox/String.h"
namespace Dxf
{
namespace SavePrivate
{
/**
 * Workerとの受渡しにだけ使うCPU値。
 */
struct FRequestState;
} // namespace SavePrivate
/**
 * 後続要求のあとも元の終端を観察する非所有のゲーム参照を含まないTicket。
 */
class FGameSaveTicket
{
public:
	/**
	 * 空のTicket。取消し済みとして読める。
	 */
	FGameSaveTicket() = default;
	/**
	 * @param State 要求窓口だけが発行する内部値。
	 */
	explicit FGameSaveTicket(Toolbox::TSharedPtr<SavePrivate::FRequestState> State);
	/**
	 * CPU側と競合せず、現在状態を返す。
	 */
	EGameSaveRequestState GetState() const noexcept;
	/**
	 * 原因の値コピー。未失敗なら空文字列。
	 */
	Toolbox::FString GetReason() const;
	/**
	 * 要求受付の通し番号。保存世代・World世代とは独立。
	 */
	Toolbox::uint64 GetSequence() const noexcept;

private:
	/**
	 * SceneやNative資源を持たない結果所有権。
	 */
	Toolbox::TSharedPtr<SavePrivate::FRequestState> m_State;
};
} // namespace Dxf
#endif
