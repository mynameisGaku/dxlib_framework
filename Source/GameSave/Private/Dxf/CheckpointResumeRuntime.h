// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHECKPOINT_RESUME_RUNTIME_PRIVATE_H
#define DXF_CHECKPOINT_RESUME_RUNTIME_PRIVATE_H
#include "Dxf/CheckpointRestore.h"
#include "Dxf/AssetService.h"
#include "Dxf/SaveError.h"
#include "Dxf/GuardValue.h"
#include "Toolbox/Thread.h"
namespace Dxf::SavePrivate
{
// CPU結果と所有側資源の採用を分離し、Sceneを独自所有しない。
/**
 * 所有側の再開処理だけを共用する。Scene実体とNative資源は既存所有者へ渡す。
 */
template <typename TCatalog, typename TDocument, typename TPrepared, typename TScene>
class TCheckpointResumeRuntime
{
public:
	/**
	 * @param Saves 保存要求。@param Scenes 切替先。@param Assets 資源所有者。
	 * @param Catalog 信頼する定義。@param RestoreProgress 純粋なゲーム変換。
	 * @param CreateScene C++構築フック。@param Limits 有限上限。
	 */
	TCheckpointResumeRuntime(FGameSaveRequest& Saves, FSceneNavigator& Scenes, FAssetService& Assets, TCatalog Catalog, Toolbox::TFunction<TDocument(const TDocument&)> RestoreProgress, Toolbox::TFunction<Toolbox::TUniquePtr<TScene>(TPrepared, TDocument)> CreateScene, FCheckpointLimits Limits) : m_Saves(Saves), m_Scenes(Scenes), m_Assets(Assets), m_Catalog(Toolbox::Move(Catalog)), m_RestoreProgress(Toolbox::Move(RestoreProgress)), m_CreateScene(Toolbox::Move(CreateScene)), m_Limits(Limits)
	{
		if (!m_RestoreProgress)
		{
			throw FSaveError(ESaveError::InvalidDocument, "Checkpoint resume requires an explicit pure game recipe");
		}
	}
	/**
	 * @param Slot 保存系列。不正な受付では旧観察を保持する。
	 */
	FGameSaveTicket Load(Toolbox::FString Slot)
	{
		RequireOwner();
		if (m_bRetired)
		{
			throw FSaveError(ESaveError::InvalidDocument, "Checkpoint resume owner retired");
		}
		// 旧要求は新しい受付が成功してから置換する。
		auto Ticket = m_Saves.RequestLoad(Toolbox::Move(Slot), m_Catalog.Identity);
		m_Ticket = Ticket;
		m_SceneTicket = {};
		return Ticket;
	}
	/**
	 * 未公開の自分の要求だけを取り消す。
	 */
	bool Cancel()
	{
		RequireOwner();
		if (!OwnsCurrent())
		{
			return false;
		}
		return m_Saves.Cancel();
	}
	/**
	 * 既存Scene境界を再入せず、最新結果だけを採用する。
	 */
	void Poll()
	{
		RequireOwner();
		if (m_bRetired || !OwnsCurrent())
		{
			return;
		}
		TGuardValue Guard(m_bPolling, true);
		m_Saves.Poll();
		const auto Sequence = m_Ticket.GetSequence();
		if (m_Ticket.GetState() == EGameSaveRequestState::TransitionRequested)
		{
			const auto State = m_SceneTicket.GetState();
			if (State == ECheckpointSceneState::Ready)
			{
				m_Saves.MarkResumeCompleted(Sequence);
			}
			else if (State == ECheckpointSceneState::Ended)
			{
				m_Saves.MarkResumeFailed(Sequence, "Checkpoint candidate ended before required Ready");
			}
			return;
		}
		if (m_Ticket.GetState() != EGameSaveRequestState::Prepared || m_Saves.GetAcceptedSequence() != Sequence)
		{
			return;
		}
		try
		{
			const auto& Loaded = m_Saves.GetLoaded();
			TDocument Document;
			if constexpr (Toolbox::IsSame<TDocument, FCheckpointDocument2D>)
			{
				Document = FCheckpointCodec::Decode2D(*Loaded.Document, m_Limits);
			}
			else
			{
				Document = FCheckpointCodec::Decode3D(*Loaded.Document, m_Limits);
			}
			// 未許可Prefabや不正参照は、ゲームの純粋な変換を呼ぶ前にも拒否する。
			(void)FCheckpointRestore::Build(Document, m_Catalog, m_Limits);
			Document = m_RestoreProgress(Document);
			auto Plan = FCheckpointRestore::Build(Document, m_Catalog, m_Limits);
			if (!OwnsCurrent() || m_Saves.GetAcceptedSequence() != Sequence)
			{
				return;
			}
			auto Prepared = PrepareScene(Toolbox::Move(Plan.Scene), m_Assets);
			auto Scene = m_CreateScene ? m_CreateScene(Toolbox::Move(Prepared), Toolbox::Move(Document)) : Toolbox::MakeUnique<TScene>(Toolbox::Move(Prepared), Toolbox::Move(Document), m_Limits);
			if (!Scene)
			{
				throw FSaveError(ESaveError::InvalidDocument, "Checkpoint construction hook returned no Scene");
			}
			m_SceneTicket = Scene->GetResumeTicket();
			if (!OwnsCurrent() || m_Saves.GetAcceptedSequence() != Sequence)
			{
				return;
			}
			const auto Requested = m_Scenes.RequestChange(Toolbox::Move(Scene));
			if (!Requested)
			{
				throw Toolbox::FException(Requested.Error().Message);
			}
			// 準備・RequestChange成功は再開完了ではない。Commitは既存Application境界が行う。
			m_Saves.MarkTransitionRequested(Sequence);
		}
		catch (const Toolbox::FException& Error)
		{
			if (OwnsCurrent() && m_Saves.GetAcceptedSequence() == Sequence)
			{
				m_Saves.MarkResumeFailed(Sequence, Error.What());
			}
		}
		catch (...)
		{
			if (OwnsCurrent() && m_Saves.GetAcceptedSequence() == Sequence)
			{
				m_Saves.MarkResumeFailed(Sequence, "Unexpected checkpoint preparation exception");
			}
		}
	}
	/**
	 * 安全な所有側だけで終端を確定する。Sceneを巻き戻さない。
	 */
	bool Retire() noexcept
	{
		if (m_bPolling || Toolbox::FThread::CurrentThreadId() != m_Owner || m_Scenes.IsDispatching())
		{
			return false;
		}
		if (m_bRetired)
		{
			return true;
		}
		try
		{
			// Task/Commit内の自己待機を拒否し、destructorから例外を出さない。
			(void)m_Saves.GetAcceptedSequence();
		}
		catch (...)
		{
			return false;
		}
		if (OwnsCurrent())
		{
			if (m_Ticket.GetState() == EGameSaveRequestState::TransitionRequested)
			{
				m_Saves.MarkResumeFailed(m_Ticket.GetSequence(), "Checkpoint resume owner retired before completion");
			}
			else
			{
				m_Saves.Cancel();
			}
		}
		m_bRetired = true;
		return true;
	}

private:
	/**
	 * Task・Scene通知・再入からの採用を拒否する。
	 */
	void RequireOwner() const
	{
		if (m_bPolling || Toolbox::FThread::CurrentThreadId() != m_Owner || m_Scenes.IsDispatching())
		{
			throw Toolbox::FException("Checkpoint resume requires owner update outside Scene dispatch");
		}
		(void)m_Saves.GetAcceptedSequence();
	}
	/**
	 * 元の要求が未終端であることを確認する。
	 */
	bool OwnsCurrent() const
	{
		// 借用先の現在要求と自分の観察を区別する。Completedなどの古い結果は採用しない。
		return m_Ticket.GetSequence() != 0 && m_Ticket.GetState() != EGameSaveRequestState::Superseded && m_Ticket.GetState() != EGameSaveRequestState::Canceled && m_Ticket.GetState() != EGameSaveRequestState::Failed && m_Ticket.GetState() != EGameSaveRequestState::Completed;
	}
	// GameInstanceなどが先に構築し、最後に破棄する借用先。
	FGameSaveRequest& m_Saves;
	// Sceneの唯一の切替窓口。
	FSceneNavigator& m_Scenes;
	// Native資源を既存経路で所有するサービス。
	FAssetService& m_Assets;
	// 起動側が渡す不変の許可カタログ。
	TCatalog m_Catalog;
	// ゲーム固有の値検証と再開レシピ。Worldや音声へ副作用を出さない。
	Toolbox::TFunction<TDocument(const TDocument&)> m_RestoreProgress;
	// スコープされた明示的C++構築フック。ファイルのクラス名を実行しない。
	Toolbox::TFunction<Toolbox::TUniquePtr<TScene>(TPrepared, TDocument)> m_CreateScene;
	// 保存に適用する有限上限。
	FCheckpointLimits m_Limits;
	// 自分が要求した読込の状態。
	FGameSaveTicket m_Ticket;
	// 生ポインタや世代の再検索を使わないScene観察。
	FCheckpointSceneTicket m_SceneTicket;
	// 所有スレッドだけを許す。
	Toolbox::uint64 m_Owner = Toolbox::FThread::CurrentThreadId();
	// Poll再入を拒否する。
	bool m_bPolling = false;
	// 終了後に古い結果を再採用しない。
	bool m_bRetired = false;
};
} // namespace Dxf::SavePrivate
#endif
