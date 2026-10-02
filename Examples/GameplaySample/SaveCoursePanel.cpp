// SPDX-License-Identifier: NOASSERTION
#include "SaveCoursePanel.h"
#include "SaveCourseGameInstance.h"
#include "Dxf/UiButton.h"
#include "Dxf/InputProfileCodec.h"
#include "SampleHud.h"
namespace Dxf::GameplaySample
{
namespace
{
// enumの保存値とは独立した画面の説明。
const char* StateName(EGameSaveRequestState State)
{
	switch (State)
	{
	case EGameSaveRequestState::Pending:
		return "受付待ち";
	case EGameSaveRequestState::Writing:
		return "書込中";
	case EGameSaveRequestState::Reading:
		return "読込中";
	case EGameSaveRequestState::Prepared:
		return "準備済み（未確定）";
	case EGameSaveRequestState::Publishing:
		return "保存の公開中";
	case EGameSaveRequestState::Committed:
		return "保存成功";
	case EGameSaveRequestState::PublicationUnknown:
		return "公開結果未確定";
	case EGameSaveRequestState::TransitionRequested:
		return "Scene切替／物理登録待ち";
	case EGameSaveRequestState::Completed:
		return "再開完了";
	case EGameSaveRequestState::Failed:
		return "失敗";
	case EGameSaveRequestState::Canceled:
		return "取消し／要求なし";
	case EGameSaveRequestState::Superseded:
		return "後続要求へ置換";
	}
	return "不明";
}
} // namespace
FSaveCoursePanel::FSaveCoursePanel(DScene& Scene, ASaveCourseGameInstance& Game, FAssetService& Assets, Toolbox::uint32 Dimension, Toolbox::TFunction<void()> SpawnCargo, Toolbox::TFunction<void()> DestroyCargo, Toolbox::TFunction<bool(Toolbox::uint32, bool)> Door) : m_pScene(&Scene), m_pGame(&Game), m_Editable(Game.GetSettings()), m_Door(Toolbox::Move(Door)), m_Dimension(Dimension)
{
	// codecの正準化でAction配列が並び替わっても名前から初期選択する。
	for (Toolbox::uint32 Index = 0; Index < m_Editable.Input.Actions.Size(); ++Index)
	{
		if (m_Editable.Input.Actions[Index].Action == "Jump")
		{
			m_Action = Index;
		}
	}
	m_LastSettingsRevision = Game.GetSettingsRevision();
	m_Ui.Initialize(Scene, Assets, [this]()
	                {
		                m_bSplit = !m_bSplit;
	                },
	                Toolbox::Move(SpawnCargo), Toolbox::Move(DestroyCargo), [this](DUiPanel& Panel, FUiScope& Scope)
	                {
		                Build(Panel, Scope);
	                },
	                true);
	m_Ui.SetHudToggleEnabled(false);
	m_Ui.SetUserScale(Game.GetSettings().UiScale);
	Scene.SetInputRouter(this);
}
FSaveCoursePanel::~FSaveCoursePanel()
{
	m_pScene->SetInputRouter(nullptr);
}
void FSaveCoursePanel::Invoke(Toolbox::TFunction<void()> Action)
{
	try
	{
		Action();
		m_Error.Clear();
	}
	catch (const Toolbox::FException& Error)
	{
		m_Error = Error.What();
	}
	m_bDirty = true;
}
void FSaveCoursePanel::Apply(FUserSettings Settings)
{
	m_pGame->RequestSettings(Settings);
	m_Editable = Toolbox::Move(Settings);
	m_bDirty = true;
}
void FSaveCoursePanel::Build(DUiPanel& Panel, FUiScope& Scope)
{
	// UI部品は既存Rootが所有し、購読だけをScene寿命へつなぐ。
	auto Add = [&](const char* Label, Toolbox::TFunction<void()> Action)
	{
		auto Button = Panel.CreateChild<DUiButton>(Label);
		Button.Get()->SetHeight(FUiLength::Fixed(24));
		Scope.Add(Button.Get()->OnClicked().Subscribe([this, Action = Toolbox::Move(Action)]()
		                                              {
			                                              Invoke(Action);
		                                              }));
	};
	Panel.CreateChild<DUiLabel>("チェックポイント／独立設定");
	for (Toolbox::uint32 Index = 0; Index < 2; ++Index)
	{
		for (Toolbox::uint32 Open = 0; Open < 2; ++Open)
		{
			const auto Label = Toolbox::FString(Index == 0 ? "doorA " : "doorB ") + (Open == 0 ? "を閉じる" : "を開く");
			Add(Label.CStr(), [this, Index, Open]()
			    {
				    if (!m_Door(Index, Open != 0))
				    {
					    throw Toolbox::FException("扉は未解錠、または物理登録待ちです。");
				    }
			    });
		}
	}
	Add("New Game 2D", [this]()
	    {
		    m_pGame->RequestNewGame(2);
	    });
	Add("New Game 3D", [this]()
	    {
		    m_pGame->RequestNewGame(3);
	    });
	Add("従来のC++コースへ", [this]()
	    {
		    m_pGame->RequestLegacyCourse(m_Dimension);
	    });
	Add("Continue（autosave）", [this]()
	    {
		    m_pGame->RequestRetry(m_Dimension);
	    });
	Add("手動スロットを次へ（1／2／3）", [this]()
	    {
		    m_Slot = (m_Slot + 1) % 3;
	    });
	Add("Save（選択したスロット）", [this]()
	    {
		    const auto& Slot = m_pGame->GetSlots()[m_Slot];
		    if (Slot.bOccupied)
		    {
			    m_PendingSlot = Slot.Name;
			    RequireSample(m_Overwrite.Get()->Open());
		    }
		    else
		    {
			    m_pGame->RequestSave(Slot.Name);
		    }
	    });
	Add("Load（選択したスロット）", [this]()
	    {
		    m_pGame->RequestLoad(m_Dimension, m_pGame->GetSlots()[m_Slot].Name);
	    });
	Add("Checkpoint Retry", [this]()
	    {
		    m_pGame->RequestRetry(m_Dimension);
	    });
	Add("Cancel（公開／切替前）", [this]()
	    {
		    m_pGame->RequestCancel();
	    });
	Add("スロット状態を再確認", [this]()
	    {
		    m_pGame->RequestSlotRefresh();
	    });
	Add("未確定の進行保存を再確認", [this]()
	    {
		    m_pGame->RequestPublicationRetry(false);
	    });
	m_Status = Panel.CreateChild<DUiLabel>();
	m_Status.Get()->SetHeight(FUiLength::Content());
	m_ActionLabel = Panel.CreateChild<DUiLabel>(Toolbox::FString("再割当対象: ") + m_Editable.Input.Actions[m_Action].Action);
	Add("再割当するActionを次へ", [this]()
	    {
		    m_Rebind.Cancel();
		    m_bRebinding = false;
		    m_Action = (m_Action + 1) % static_cast<Toolbox::uint32>(m_Editable.Input.Actions.Size());
		    m_ActionLabel.Get()->SetText(Toolbox::FString("再割当対象: ") + m_Editable.Input.Actions[m_Action].Action);
	    });
	Add("Keyboardを採取", [this]()
	    {
		    BeginRebind(EInputBindingDevice::Keyboard, 0);
	    });
	Add("Mouseを採取", [this]()
	    {
		    BeginRebind(EInputBindingDevice::Mouse, 0);
	    });
	for (Toolbox::uint32 Slot = 0; Slot < 4; ++Slot)
	{
		const auto Label = Toolbox::FString("Pad ") + Toolbox::ToString(Slot + 1) + " を採取";
		Add(Label.CStr(), [this, Slot]()
		    {
			    BeginRebind(EInputBindingDevice::Gamepad, Slot);
		    });
	}
	Add("再割当の採取を取消す", [this]()
	    {
		    m_Rebind.Cancel();
		    m_bRebinding = false;
	    });
	Panel.CreateChild<DUiLabel>("音量（適用と保存は別）");
	m_Volume = Panel.CreateChild<DUiSlider>();
	m_Volume.Get()->SetRange(0, 1, .05);
	m_Volume.Get()->SetValue(m_Editable.Volume);
	m_Volume.Get()->SetHeight(FUiLength::Fixed(24));
	Scope.Add(m_Volume.Get()->OnValueChanged().Subscribe([this](Toolbox::f64 Value)
	                                                     {
		                                                     Invoke([this, Value]()
		                                                            {
			                                                            auto Settings = m_Editable;
			                                                            Settings.Volume = static_cast<Toolbox::f32>(Value);
			                                                            Apply(Toolbox::Move(Settings));
		                                                            });
	                                                     }));
	Panel.CreateChild<DUiLabel>("UI倍率（描画とクリック判定）");
	m_Scale = Panel.CreateChild<DUiSlider>();
	m_Scale.Get()->SetRange(.25, 8, .25);
	m_Scale.Get()->SetValue(m_Editable.UiScale);
	m_Scale.Get()->SetHeight(FUiLength::Fixed(24));
	Scope.Add(m_Scale.Get()->OnValueChanged().Subscribe([this](Toolbox::f64 Value)
	                                                    {
		                                                    Invoke([this, Value]()
		                                                           {
			                                                           m_Editable.UiScale = static_cast<Toolbox::f32>(Value);
			                                                           m_bScalePending = true;
		                                                           });
	                                                    }));
	Add("設定を保存／保存失敗の再試行", [this]()
	    {
		    m_pGame->RequestSettingsSave();
	    });
	Add("未確定の設定保存を再確認", [this]()
	    {
		    m_pGame->RequestPublicationRetry(true);
	    });
	Add("入力・音量・UI倍率を既定へ（未保存）", [this]()
	    {
		    m_Rebind.Cancel();
		    m_bRebinding = false;
		    m_bScalePending = false;
		    Apply(FSaveCourseInput::Defaults());
		    m_Volume.Get()->SetValue(m_Editable.Volume);
		    m_Scale.Get()->SetValue(m_Editable.UiScale);
	    });
	m_Overwrite = Panel.GetRoot()->Create<DUiPopup>();
	auto Confirm = Panel.GetRoot()->Create<DUiPanel>(EUiStackMode::Vertical, 10);
	Confirm.Get()->SetWidth(FUiLength::Fixed(380));
	Confirm.Get()->SetHeight(FUiLength::Content());
	Confirm.Get()->SetAlign(EUiAlign::Center, EUiAlign::Center);
	Confirm.Get()->SetPadding(FUiThickness::All(16));
	Confirm.Get()->CreateChild<DUiLabel>("選択した系列だけを次世代保存しますか？");
	auto Yes = Confirm.Get()->CreateChild<DUiButton>("保存する");
	auto No = Confirm.Get()->CreateChild<DUiButton>("取消す");
	Yes.Get()->SetHeight(FUiLength::Fixed(32));
	No.Get()->SetHeight(FUiLength::Fixed(32));
	Scope.Add(Yes.Get()->OnClicked().Subscribe([this]()
	                                           {
		                                           Invoke([this]()
		                                                  {
			                                                  m_pGame->RequestSave(m_PendingSlot);
			                                                  m_Overwrite.Get()->Close();
		                                                  });
	                                           }));
	Scope.Add(No.Get()->OnClicked().Subscribe([this]()
	                                          {
		                                          m_Overwrite.Get()->Close();
	                                          }));
	Scope.Add(m_Overwrite.Get()->OnClosed().Subscribe([this]()
	                                                  {
		                                                  m_PendingSlot.Clear();
	                                                  }));
	m_Overwrite.Get()->SetContent(Confirm.Cast<DUiElement>());
}
void FSaveCoursePanel::BeginRebind(EInputBindingDevice Device, Toolbox::uint32 PadSlot)
{
	m_bRebinding = m_Rebind.Begin(Device, PadSlot, m_Input);
	if (!m_bRebinding)
	{
		throw Toolbox::FException("Input is unfocused or selected Pad is disconnected");
	}
	m_Error = "対象装置を一度解放し、新しい入力を一つ押してください。Escapeで取消し。";
}
FInputSnapshot FSaveCoursePanel::RouteInput(const FTickContext& Context)
{
	m_Input = Context.Input;
	if (m_LastSettingsRevision != m_pGame->GetSettingsRevision())
	{
		const auto Selected = m_Editable.Input.Actions[m_Action].Action;
		m_Editable = m_pGame->GetSettings();
		for (Toolbox::uint32 Index = 0; Index < m_Editable.Input.Actions.Size(); ++Index)
		{
			if (m_Editable.Input.Actions[Index].Action == Selected)
			{
				m_Action = Index;
			}
		}
		m_Volume.Get()->SetValue(m_Editable.Volume);
		m_Scale.Get()->SetValue(m_Editable.UiScale);
		m_LastSettingsRevision = m_pGame->GetSettingsRevision();
		m_bDirty = true;
	}
	m_Ui.SetUserScale(m_pGame->GetSettings().UiScale);
	const auto Routed = m_Ui.RouteInput(Context);
	if (m_bScalePending && !m_Scale.Get()->IsPressed())
	{
		Invoke([this]()
		       {
			       Apply(m_Editable);
		       });
		m_bScalePending = false;
	}
	const bool bWasRebinding = m_bRebinding;
	if (m_bRebinding)
	{
		if (!m_Ui.IsSettingsOpen() || Context.Input.WasPressed(EKey::Escape))
		{
			m_Rebind.Cancel();
		}
		else
		{
			m_Rebind.Poll(Context.Input);
		}
		FInputBindingDescription Input;
		if (m_Rebind.TryGetBinding(Input))
		{
			Invoke([this, Input]()
			       {
				       auto Candidate = m_Editable;
				       Candidate.Input.Actions[m_Action].Bindings.Clear();
				       Candidate.Input.Actions[m_Action].Bindings.PushBack(Input);
				       FInputProfileCodec::Validate(Candidate.Input, FSaveCourseInput::Policy());
				       Apply(Toolbox::Move(Candidate));
			       });
			m_bRebinding = false;
		}
		else if (m_Rebind.GetState() == EInputRebindState::Canceled)
		{
			m_bRebinding = false;
			m_Error = "再割当を取り消しました。現在の設定は保持しています。";
		}
		m_bDirty = true;
	}
	if (!m_Ui.IsSettingsOpen() && m_Overwrite.Get()->IsOpen())
	{
		m_Overwrite.Get()->Close();
	}
	UpdateStatus();
	// 採取完了の押下も同Frameのゲームへ配送しない。適用時はGameInstanceが元入力で抑止する。
	return bWasRebinding ? FInputSnapshot{} : Routed;
}
void FSaveCoursePanel::UpdateStatus()
{
	const auto Ticket = m_pGame->GetSaveTicket();
	const auto State = Ticket.GetState();
	const auto SettingsState = m_pGame->GetSettingsTicket().GetState();
	const bool Capturing = m_pGame->IsCapturing();
	const bool SettingsDirty = m_pGame->IsSettingsDirty();
	if (!m_bDirty && State == m_LastState && Ticket.GetSequence() == m_LastSequence && SettingsState == m_LastSettingsState && Capturing == m_bLastCapturing && SettingsDirty == m_bLastSettingsDirty && m_LastSlotRevision == m_pGame->GetSlotRevision() && m_LastDiagnostic == m_pGame->GetDiagnostic())
	{
		return;
	}
	m_LastSlotRevision = m_pGame->GetSlotRevision();
	m_LastDiagnostic = m_pGame->GetDiagnostic();
	m_LastState = State;
	m_LastSequence = Ticket.GetSequence();
	m_LastSettingsState = SettingsState;
	m_bLastCapturing = Capturing;
	m_bLastSettingsDirty = SettingsDirty;
	m_bDirty = false;
	m_Text = Toolbox::FString(Capturing ? "成功固定更新の採取待ち" : StateName(State));
	m_Text += SettingsDirty ? " / 設定は未保存" : " / 設定保存済み";
	m_Text += Toolbox::FString(" / 設定保存要求: ") + StateName(SettingsState);
	m_Text += "\n選択: " + m_pGame->GetSlots()[m_Slot].Name;
	for (const auto& Slot : m_pGame->GetSlots())
	{
		m_Text += "\n" + Slot.Name + ": " + Slot.Status;
		if (!Slot.Reason.IsEmpty())
		{
			m_Text += " (" + Slot.Reason + ")";
		}
	}
	if (State == EGameSaveRequestState::Failed)
	{
		m_Text += "\n" + Ticket.GetReason();
	}
	if (SettingsState == EGameSaveRequestState::Failed)
	{
		m_Text += "\n設定保存: " + m_pGame->GetSettingsTicket().GetReason();
	}
	m_Text += "\n" + m_pGame->GetDiagnostic();
	m_Text += "\n" + m_Error;
	m_Status.Get()->SetText(m_Text);
}
void FSaveCoursePanel::Draw(FRenderContext& Render)
{
	m_Ui.Draw(Render, "Save course: WASD move / Jump action / E door / G cargo / F1 menu");
}
bool FSaveCoursePanel::IsSplit() const noexcept
{
	return m_bSplit;
}
Toolbox::int32 FSaveCoursePanel::GetWidth() const noexcept
{
	return m_Ui.GetWidth();
}
Toolbox::int32 FSaveCoursePanel::GetHeight() const noexcept
{
	return m_Ui.GetHeight();
}
} // namespace Dxf::GameplaySample
