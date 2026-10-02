// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMESAVEREQUEST_PRIVATE_H
#define DXF_GAMESAVEREQUEST_PRIVATE_H
#include "Dxf/GameSaveTicket.h"
#include "Dxf/SaveStore.h"
#include "Toolbox/Atomic.h"
#include "Toolbox/Mutex.h"
namespace Dxf::SavePrivate
{
// Workerが所有できるCPU値だけの受渡し。
struct FRequestState
{
	// 状態と終端を同じ原子値で扱う。
	Toolbox::TAtomic<Toolbox::uint32> Status{static_cast<Toolbox::uint32>(EGameSaveRequestState::Pending)};
	// 値受渡しと取消しを直列化する。
	Toolbox::FMutex Mutex;
	// 投入順の番号。
	Toolbox::uint64 Sequence = 0;
	// 保存と読込の区別。
	bool Save = false;
	// Task Commitが値受渡しを確認した事実。ファイル公開ではない。
	bool Delivered = false;
	// CPU処理の失敗。
	bool Failed = false;
	// 公開前だけ所有する書込結果とOS排他。
	Toolbox::TOptional<FSaveTransaction> Transaction;
	// 検証済みの読込値。
	FSaveLoadResult Loaded;
	// 確保失敗でも保存できる最初の原因。
	char Reason[1024]{};
};
// 診断の構築のために新たな確保をしない。
void SaveReason(FRequestState& State, const char* Reason) noexcept;
} // namespace Dxf::SavePrivate
#endif
