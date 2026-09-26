// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_SAMPLE_SHELL_H
#define DXF_UI_SAMPLE_SHELL_H
#include "UiSampleState.h"
#include "Dxf/UiSceneHost.h"
#include "Dxf/UiAssetTextService.h"
#include "Dxf/UiStyleResource.h"
#include "Dxf/UiPopup.h"
#include "Dxf/UiLabel.h"
#include "Dxf/Sound.h"
namespace Dxf::UiSample
{
/**
 * 共通画面とScene操作を接続するサンプル固有の仲介。Physics本体やUI部品の実装は持たない。
 */
class FUiSampleShell final : public IInputRouter
{
public:
	explicit FUiSampleShell(Toolbox::TSharedPtr<FUiSampleState> State);
	TResult<void> Initialize(DScene& Scene, FAssetService& Assets, bool bTitle, bool b3D,
	                         Toolbox::TFunction<Toolbox::FString()> ReadStatus = {});
	FInputSnapshot RouteInput(const FTickContext& Context) override;
	TResult<void> PreparePanels(FRenderContext& Render);
	TResult<void> DrawPanels(FRenderContext& Render, const FRenderView3D& View);
	TResult<void> Draw(FRenderContext& Render);
	void SetWorldMarker(Toolbox::FVector3 Center);
	FUiRoot& GetRoot() const noexcept
	{
		return *m_pRoot;
	}

	FUiSceneHost& GetHost() noexcept
	{
		return m_Host;
	}

	bool IsPaused() const noexcept
	{
		return m_bPaused;
	}

	Toolbox::TSharedPtr<FUiSampleState> GetState() const noexcept
	{
		return m_pState;
	}

private:
	Toolbox::TSharedPtr<FUiSampleState> m_pState;
	Toolbox::TUniquePtr<FUiAssetTextService> m_pText;
	Toolbox::TUniquePtr<FUiRoot> m_pRoot;
	Toolbox::TUniquePtr<FUiRoot> m_pViewHud;
	Toolbox::TUniquePtr<FUiRoot> m_pWorldRoot;
	FUiSceneHost m_Host;
	FUiStyleResource m_Styles;
	FUiScope m_Scope;
	TUiRef<DUiPopup> m_Pause;
	TUiRef<DUiPopup> m_Settings;
	Toolbox::TArray<TUiRef<DUiLabel>, 2> m_Markers;
	Toolbox::TVector<FUiDisplayId> m_ChangingDisplays;
	FUiDisplayId m_Screen = 0;
	FSound m_Sound;
	Toolbox::FPath m_StylePath;
	Toolbox::TFunction<Toolbox::FString()> m_ReadStatus;
	DScene* m_pScene = nullptr;
	FAssetService* m_pAssets = nullptr;
	bool m_bTitle = false;
	bool m_b3D = false;
	bool m_bPaused = false;
	bool m_bLastSplit = false;
	Toolbox::f64 m_LastScale = -1;
	void RefreshDisplays();
	void ExecuteAction(const FTickContext& Context);
};
} // namespace Dxf::UiSample
#endif
