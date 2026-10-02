// SPDX-License-Identifier: NOASSERTION
#include "SaveCourseGameInstance.h"
#include "SaveCourseRecipes.h"
#include "SaveCourseScene2D.h"
#include "SaveCourseScene3D.h"
#include "Dxf/CheckpointResumeRequest2D.h"
#include "Dxf/CheckpointResumeRequest3D.h"
#include "Dxf/CheckpointCodec.h"
#include "Dxf/UserSettingsCodec.h"
#include "Dxf/SaveError.h"
namespace Dxf::GameplaySample
{
namespace
{
// 新しい要求へ移る前に、現在の公開・再開境界を確認する。
bool IsWorking(EGameSaveRequestState State) noexcept
{
	return State == EGameSaveRequestState::Pending || State == EGameSaveRequestState::Writing || State == EGameSaveRequestState::Reading || State == EGameSaveRequestState::Prepared || State == EGameSaveRequestState::Publishing || State == EGameSaveRequestState::PublicationUnknown || State == EGameSaveRequestState::TransitionRequested;
}
// 保存から任意の次元を推測しない。
void RequireDimension(Toolbox::uint32 Dimension)
{
	if (Dimension != 2 && Dimension != 3)
	{
		throw Toolbox::FException("Save course dimension must be 2 or 3");
	}
}
} // namespace
struct ASaveCourseGameInstance::FImpl
{
	// 明示Rootの保存面だけを扱うサービス。
	FSaveStore Store;
	// 実読込値を保持する許可カタログ。以後のディスク変更を暗黙採用しない。
	FCheckpointCatalog2D Catalog2D;
	// 同じ意味の3D許可値。
	FCheckpointCatalog3D Catalog3D;
	// Sceneと進行とは別に保持する現在設定。
	FUserSettings Settings = FSaveCourseInput::Defaults();
	// 次の所有側境界へ渡す候補設定。
	Toolbox::TOptional<FUserSettings> PendingSettings;
	// Applicationが所有する資源窓口。Workerへ渡さない。
	FAssetService* Assets = nullptr;
	// ゲーム進行のScope付きCPU要求。
	Toolbox::TUniquePtr<FGameSaveRequest> Saves;
	// 設定はゲーム進行と別の要求・スロット系列。
	Toolbox::TUniquePtr<FGameSaveRequest> SettingsSaves;
	// 通常Navigatorへ渡す次元別adapter。
	Toolbox::TUniquePtr<FCheckpointResumeRequest2D> Resume2D;
	// 3Dの純粋変換と必須Ready確認。
	Toolbox::TUniquePtr<FCheckpointResumeRequest3D> Resume3D;
	// 最新の進行要求の終端を保持する値。
	FGameSaveTicket Ticket;
	// 現在設定の保存結果。
	FGameSaveTicket SettingsTicket;
	// 採取要求を出した元Sceneの観察。終了後に新Sceneへ追従しない。
	FCheckpointSceneTicket CaptureSource;
	// autosaveの重複をSceneごとに抑止する元世代。
	FCheckpointSceneTicket AutoSource;
	// 書込前採取を待つ手動系列。
	Toolbox::FString SaveSlot;
	// 次の通常更新で読み込む系列。
	Toolbox::FString LoadSlot;
	// コア要求を出す前の失敗を含む表示用値。
	Toolbox::FString Diagnostic;
	// 未受付New Gameの次元。0なら要求なし。
	Toolbox::uint32 NewDimension = 0;
	// 最新ロードの次元を固定する。
	Toolbox::uint32 LoadDimension = 0;
	// 保存に渡した設定の変更番号。
	Toolbox::uint64 SavedSettingsRevision = 0;
	// 現在の設定変更番号。
	Toolbox::uint64 SettingsRevision = 0;
	// 同じcheckpoint進行でautosaveを繰り返さない。
	Toolbox::uint64 AttemptedAutoRevision = 0;
	// 新しい採取がない通常Frameでは進行JSONを再解析しない。
	Toolbox::uint64 ObservedCaptureStep = 0;
	// Dispatcherを借りるか。新Worker poolを作らない。
	bool bAsync = true;
	// 採取の依頼を元Sceneへ一度送ったか。
	bool bCaptureStarted = false;
	// UIで適用した設定は保存成功まで未保存。
	bool bSettingsDirty = true;
	// 次の所有側更新の独立設定保存。
	bool bSaveSettings = false;
	// 次の更新で公開前取消しを実行する。
	bool bCancel = false;
	// 現在のロードadapter。0は進行保存や要求なし。
	Toolbox::uint32 ActiveResume = 0;
	FImpl(Toolbox::FPath Root, const FSceneContentSource& Source, bool Async) : Store(Toolbox::Move(Root)), Catalog2D(FSaveCourseRecipes::Load2D(Source)), Catalog3D(FSaveCourseRecipes::Load3D(Source)), bAsync(Async)
	{
	}
	// 依存先はApplicationが長く生存する。通常更新からだけ要求を構築する。
	void Ensure(const FTickContext& Context)
	{
		if (Saves || Context.Scenes == nullptr)
		{
			return;
		}
		Saves = Toolbox::MakeUnique<FGameSaveRequest>(Store, bAsync ? Context.Tasks : nullptr, bAsync ? Context.TaskScope : FTaskScope{});
		SettingsSaves = Toolbox::MakeUnique<FGameSaveRequest>(Store, bAsync ? Context.Tasks : nullptr, bAsync ? Context.TaskScope : FTaskScope{});
		Resume2D = Toolbox::MakeUnique<FCheckpointResumeRequest2D>(*Saves, *Context.Scenes, *Assets, Catalog2D, [](const FCheckpointDocument2D& Document)
		                                                           {
			                                                           return FSaveCourseRecipes::Restore(Document);
		                                                           },
		                                                           [this](FPreparedScene2D Prepared, FCheckpointDocument2D Document) -> Toolbox::TUniquePtr<ACheckpointScene2D>
		                                                           {
			                                                           return Toolbox::MakeUnique<ASaveCourseScene2D>(Toolbox::Move(Prepared), Toolbox::Move(Document), PreparePrefab(Catalog2D.Prefabs.Back().Definition, *Assets), Settings);
		                                                           });
		Resume3D = Toolbox::MakeUnique<FCheckpointResumeRequest3D>(*Saves, *Context.Scenes, *Assets, Catalog3D, [](const FCheckpointDocument3D& Document)
		                                                           {
			                                                           return FSaveCourseRecipes::Restore(Document);
		                                                           },
		                                                           [this](FPreparedScene3D Prepared, FCheckpointDocument3D Document) -> Toolbox::TUniquePtr<ACheckpointScene3D>
		                                                           {
			                                                           return Toolbox::MakeUnique<ASaveCourseScene3D>(Toolbox::Move(Prepared), Toolbox::Move(Document), PreparePrefab(Catalog3D.Prefabs.Back().Definition, *Assets), Settings);
		                                                           });
	}
	// 成功固定更新の文書だけを使う。GameInstanceが先に更新されても一つ後の境界を待つ。
	template <typename TScene>
	void PollCapture(TScene& Scene)
	{
		auto& Capture = Scene.GetCheckpointCapture();
		if (!SaveSlot.IsEmpty())
		{
			if (!bCaptureStarted)
			{
				if (Scene.GetResumeTicket().GetState() != ECheckpointSceneState::Ready)
				{
					return;
				}
				CaptureSource = Scene.GetResumeTicket();
				if (!Scene.GetClock().IsPaused())
				{
					Capture.RequestCapture();
				}
				bCaptureStarted = true;
			}
			if (Capture.GetLastCapture() && (Scene.GetClock().IsPaused() || !Capture.HasCaptureRequest()))
			{
				Ticket = Saves->RequestSave(SaveSlot, FCheckpointCodec::Encode(*Capture.GetLastCapture()));
				SaveSlot.Clear();
				bCaptureStarted = false;
				ActiveResume = 0;
			}
			return;
		}
		// autosaveは同じ進行のStayでは要求しない。失敗は明示Retryまで再試行しない。
		if (AutoSource.GetState() == ECheckpointSceneState::Ended)
		{
			AutoSource = Scene.GetResumeTicket();
			AttemptedAutoRevision = Scene.GetProgress().Revision;
			ObservedCaptureStep = 0;
		}
		const auto& Last = Capture.GetLastCapture();
		if (Last && Capture.GetCapturedStep() != ObservedCaptureStep && !Capture.HasCaptureRequest() && !IsWorking(Ticket.GetState()))
		{
			ObservedCaptureStep = Capture.GetCapturedStep();
			const auto Progress = FSaveCourseProgressCodec::Decode(Last->GamePayload);
			if (Progress.Checkpoint == "north" && Progress.Revision > AttemptedAutoRevision)
			{
				Ticket = Saves->RequestSave("autosave", FCheckpointCodec::Encode(*Last));
				AttemptedAutoRevision = Progress.Revision;
				ActiveResume = 0;
			}
		}
	}
	// 自分の要求とCPU観察だけを終端へ確定する。
	void Retire() noexcept
	{
		Resume2D.Reset();
		Resume3D.Reset();
		SettingsSaves.Reset();
		Saves.Reset();
		Assets = nullptr;
	}
};
ASaveCourseGameInstance::ASaveCourseGameInstance(Toolbox::FPath SaveRoot, FSceneContentSource Content, bool bAsync) : m_pImpl(Toolbox::MakeUnique<FImpl>(Toolbox::Move(SaveRoot), Content, bAsync))
{
	SetTickWhenPaused(true);
}
ASaveCourseGameInstance::~ASaveCourseGameInstance()
{
	Shutdown_Internal();
}
void ASaveCourseGameInstance::RequestNewGame(Toolbox::uint32 Dimension)
{
	RequireDimension(Dimension);
	m_pImpl->LoadSlot.Clear();
	m_pImpl->SaveSlot.Clear();
	m_pImpl->bCaptureStarted = false;
	m_pImpl->NewDimension = Dimension;
}
void ASaveCourseGameInstance::RequestSave(Toolbox::FString Slot)
{
	if (Slot.IsEmpty())
	{
		throw Toolbox::FException("Save course slot must not be empty");
	}
	if (!m_pImpl->SaveSlot.IsEmpty() || IsWorking(m_pImpl->Ticket.GetState()))
	{
		throw Toolbox::FException("Save course request is busy");
	}
	m_pImpl->Diagnostic.Clear();
	m_pImpl->SaveSlot = Toolbox::Move(Slot);
	m_pImpl->bCaptureStarted = false;
}
void ASaveCourseGameInstance::RequestLoad(Toolbox::uint32 Dimension, Toolbox::FString Slot)
{
	RequireDimension(Dimension);
	if (Slot.IsEmpty())
	{
		throw Toolbox::FException("Save course slot must not be empty");
	}
	m_pImpl->NewDimension = 0;
	m_pImpl->SaveSlot.Clear();
	m_pImpl->bCaptureStarted = false;
	m_pImpl->LoadDimension = Dimension;
	m_pImpl->LoadSlot = Toolbox::Move(Slot);
}
void ASaveCourseGameInstance::RequestRetry(Toolbox::uint32 Dimension)
{
	RequestLoad(Dimension, "autosave");
}
void ASaveCourseGameInstance::RequestCancel() noexcept
{
	m_pImpl->bCancel = true;
}
void ASaveCourseGameInstance::RequestSettings(FUserSettings Settings)
{
	(void)FUserSettingsCodec::Encode(Settings, FSaveCourseInput::Policy(), "dxfGameplaySave");
	m_pImpl->PendingSettings = Toolbox::Move(Settings);
}
void ASaveCourseGameInstance::RequestSettingsSave() noexcept
{
	m_pImpl->bSaveSettings = true;
}
const FUserSettings& ASaveCourseGameInstance::GetSettings() const noexcept
{
	return m_pImpl->Settings;
}
bool ASaveCourseGameInstance::IsSettingsDirty() const noexcept
{
	return m_pImpl->bSettingsDirty;
}
bool ASaveCourseGameInstance::IsCapturing() const noexcept
{
	return !m_pImpl->SaveSlot.IsEmpty();
}
FGameSaveTicket ASaveCourseGameInstance::GetSaveTicket() const
{
	return m_pImpl->Ticket;
}
FGameSaveTicket ASaveCourseGameInstance::GetSettingsTicket() const
{
	return m_pImpl->SettingsTicket;
}
const Toolbox::FString& ASaveCourseGameInstance::GetDiagnostic() const noexcept
{
	return m_pImpl->Diagnostic;
}
TResult<void> ASaveCourseGameInstance::OnInitialize(const FInitContext& Context)
{
	m_pImpl->Assets = &Context.Assets;
	try
	{
		const auto Loaded = m_pImpl->Store.Load("settings");
		if (Loaded.Document)
		{
			m_pImpl->Settings = FUserSettingsCodec::Decode(*Loaded.Document, FSaveCourseInput::Policy(), "dxfGameplaySave");
			m_pImpl->bSettingsDirty = false;
			m_pImpl->Diagnostic = Loaded.RecoveryReason;
		}
	}
	catch (const Toolbox::FException& Error)
	{
		// 初回設定の失敗は明示した既定値と診断を保持し、ゲーム進行へ混ぜない。
		m_pImpl->Diagnostic = Error.What();
	}
	return {};
}
void ASaveCourseGameInstance::OnTick(const FTickContext& Context)
{
	auto& I = *m_pImpl;
	I.Ensure(Context);
	if (!I.Saves || Context.Scenes == nullptr)
	{
		return;
	}
	if (I.bCancel)
	{
		I.SaveSlot.Clear();
		I.LoadSlot.Clear();
		I.NewDimension = 0;
		I.bCaptureStarted = false;
		const bool Canceled = I.ActiveResume == 2 ? I.Resume2D->Cancel() : I.ActiveResume == 3 ? I.Resume3D->Cancel()
		                                                                                       : I.Saves->Cancel();
		if (!Canceled)
		{
			I.Diagnostic = "公開またはScene切替の後の要求は取り消せません。";
		}
		I.bCancel = false;
	}
	// New Gameも遅れた旧ロードより先に受付する。切替済み候補は既存境界まで待つ。
	if (I.NewDimension != 0 && I.ActiveResume != 0 && I.Ticket.GetState() != EGameSaveRequestState::TransitionRequested)
	{
		(void)(I.ActiveResume == 2 ? I.Resume2D->Cancel() : I.Resume3D->Cancel());
	}
	// 受付Frameには再開計画を採用しない。同期経路でも取消し・後続要求の境界を残す。
	bool bAcceptedLoad = false;
	// 新しい読込を先に受付し、遅れた旧CPU結果をこのFrameでSceneへ採用しない。
	if (!I.LoadSlot.IsEmpty() && I.NewDimension == 0)
	{
		const auto State = I.Ticket.GetState();
		if (!IsWorking(State) || (I.ActiveResume != 0 && State != EGameSaveRequestState::TransitionRequested))
		{
			try
			{
				I.Ticket = I.LoadDimension == 2 ? I.Resume2D->RequestLoad(I.LoadSlot) : I.Resume3D->RequestLoad(I.LoadSlot);
				I.ActiveResume = I.LoadDimension;
				I.LoadSlot.Clear();
				I.Diagnostic.Clear();
				bAcceptedLoad = true;
			}
			catch (const Toolbox::FException& Error)
			{
				I.Diagnostic = Error.What();
				I.LoadSlot.Clear();
			}
		}
	}
	if (I.ActiveResume == 2 && !bAcceptedLoad)
	{
		I.Resume2D->Poll();
	}
	else if (I.ActiveResume == 3 && !bAcceptedLoad)
	{
		I.Resume3D->Poll();
	}
	else if (I.ActiveResume == 0)
	{
		I.Saves->Poll();
	}
	I.SettingsSaves->Poll();
	if (I.SettingsTicket.GetState() == EGameSaveRequestState::Committed && I.SavedSettingsRevision == I.SettingsRevision)
	{
		I.bSettingsDirty = false;
	}
	try
	{
		// 通常Scene更新より先に元Snapshotで全体適用する。GameInstanceはActionを更新しない。
		if (I.PendingSettings)
		{
			if (I.SettingsRevision == Toolbox::TNumericLimits<Toolbox::uint64>::Max())
			{
				throw Toolbox::FException("Settings revision overflow");
			}
			if (auto* Scene = Context.Scenes->GetCurrent())
			{
				if (auto* C2 = Scene->TryCast<ASaveCourseScene2D>())
				{
					C2->ApplySettings(*I.PendingSettings, Context.Input);
				}
				if (auto* C3 = Scene->TryCast<ASaveCourseScene3D>())
				{
					C3->ApplySettings(*I.PendingSettings, Context.Input);
				}
			}
			I.Settings = Toolbox::Move(*I.PendingSettings);
			I.PendingSettings.Reset();
			++I.SettingsRevision;
			I.bSettingsDirty = true;
		}
		if (I.bSaveSettings && !IsWorking(I.SettingsTicket.GetState()))
		{
			I.SettingsTicket = I.SettingsSaves->RequestSave("settings", FUserSettingsCodec::Encode(I.Settings, FSaveCourseInput::Policy(), "dxfGameplaySave"));
			I.SavedSettingsRevision = I.SettingsRevision;
			I.bSaveSettings = false;
		}
		if (I.NewDimension != 0 || !I.LoadSlot.IsEmpty())
		{
			if (IsWorking(I.Ticket.GetState()))
			{
				I.Diagnostic = "保存または再開の確定を待ってください。";
				return;
			}
			I.Diagnostic.Clear();
			I.SaveSlot.Clear();
			I.bCaptureStarted = false;
			if (I.NewDimension != 0)
			{
				TResult<void> Requested;
				if (I.NewDimension == 2)
				{
					const auto Initial = FSaveCourseRecipes::NewGame(I.Catalog2D);
					const auto Plan = FCheckpointRestore::Build(Initial, I.Catalog2D);
					Requested = Context.Scenes->RequestChange<ASaveCourseScene2D>(PrepareScene(Plan.Scene, *I.Assets), Initial, PreparePrefab(I.Catalog2D.Prefabs.Back().Definition, *I.Assets), I.Settings);
				}
				else
				{
					const auto Initial = FSaveCourseRecipes::NewGame(I.Catalog3D);
					const auto Plan = FCheckpointRestore::Build(Initial, I.Catalog3D);
					Requested = Context.Scenes->RequestChange<ASaveCourseScene3D>(PrepareScene(Plan.Scene, *I.Assets), Initial, PreparePrefab(I.Catalog3D.Prefabs.Back().Definition, *I.Assets), I.Settings);
				}
				if (!Requested)
				{
					throw Toolbox::FException(Requested.Error().Message);
				}
				I.NewDimension = 0;
			}
			else
			{
				I.Ticket = I.LoadDimension == 2 ? I.Resume2D->RequestLoad(I.LoadSlot) : I.Resume3D->RequestLoad(I.LoadSlot);
				I.ActiveResume = I.LoadDimension;
				I.LoadSlot.Clear();
			}
		}
		if (I.bCaptureStarted && I.CaptureSource.GetState() == ECheckpointSceneState::Ended)
		{
			I.SaveSlot.Clear();
			I.bCaptureStarted = false;
			I.Diagnostic = "採取前に元Sceneが終了しました。保存は変更していません。";
		}
		if (auto* Scene = Context.Scenes->GetCurrent())
		{
			if (auto* C2 = Scene->TryCast<ASaveCourseScene2D>())
			{
				I.PollCapture(*C2);
			}
			else if (auto* C3 = Scene->TryCast<ASaveCourseScene3D>())
			{
				I.PollCapture(*C3);
			}
			else if (!I.SaveSlot.IsEmpty())
			{
				throw Toolbox::FException("No active save course Scene");
			}
		}
	}
	catch (const Toolbox::FException& Error)
	{
		// 準備・要求採用の失敗だけを表示する。SceneのFixedTick例外はここへ入らない。
		I.Diagnostic = Error.What();
		I.NewDimension = 0;
		I.LoadSlot.Clear();
		I.SaveSlot.Clear();
		I.bCaptureStarted = false;
		I.PendingSettings.Reset();
		I.bSaveSettings = false;
	}
}
void ASaveCourseGameInstance::OnDeinitialize() noexcept
{
	m_pImpl->Retire();
}
} // namespace Dxf::GameplaySample
