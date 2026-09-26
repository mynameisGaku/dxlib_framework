// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_TEST_UI_DISPLAY_SCENE_H
#define DXF_TEST_UI_DISPLAY_SCENE_H
#include "Dxf/Scene.h"
#include "Dxf/UiAssetTextService.h"
#include "Dxf/UiButton.h"
#include "Dxf/UiSceneHost.h"
#include "Toolbox/UniquePtr.h"
namespace Dxf::UiSmoke
{
/**
 * 表示先の確認に使う決定の欄。
 */
enum class EDisplayButton : Toolbox::uint8
{
	/**
	 * 左のViewport。
	 */
	Left,
	/**
	 * 右のViewport。
	 */
	Right,
	/**
	 * 2Dワールドのパネル。
	 */
	Panel2D,
	/**
	 * 不透明な3Dのパネル。
	 */
	Panel3D,
	/**
	 * 欄の数。
	 */
	Count
};

/**
 * 全画面・左右のViewport（幅641／639、倍率1／2）・2Dワールドのパネル・3Dの不透明／透明のパネルを一つのHostで表示する確認用のScene。
 * 期待する画素の位置は、下の定数とProjectWorldToScreenから独立に求める。
 */
class DDisplayScene final : public DScene
{
public:
	/**
	 * 画面の消去色（黒い縁を検出できるように黒以外にする）。
	 */
	static constexpr FColor ClearColor{30, 40, 50, 255};
	/**
	 * 左右のViewportの画素の範囲。
	 */
	static constexpr FUiPixelRect LeftRect{0, 560, 641, 720};
	static constexpr FUiPixelRect RightRect{641, 560, 1280, 720};
	/**
	 * 左右のViewportの背景と、論理(10,10)〜(30,30)の目印の色。
	 */
	static constexpr FColor LeftColor{60, 90, 200, 255};
	static constexpr FColor RightColor{200, 90, 60, 255};
	static constexpr FColor MarkerColor{255, 255, 0, 255};
	/**
	 * 全画面の目印（右上、Viewportの境界をまたぐもの、3Dの上）の色。
	 */
	static constexpr FColor OverlayColor{250, 120, 10, 255};
	/**
	 * 実画像（Assets/player.bmp、64x64）を2倍に引き伸ばして描く画面の範囲。
	 */
	static constexpr FUiPixelRect ImageRect{40, 400, 168, 528};
	/**
	 * 実画像のプロジェクトの根からの相対パス。
	 */
	static constexpr const char* ImagePath = "Assets/player.bmp";
	/**
	 * 2Dワールドのパネルの色。
	 */
	static constexpr FColor Panel2DColor{20, 180, 160, 255};
	/**
	 * 不透明な3Dのパネルの色。
	 */
	static constexpr FColor OpaqueColor{220, 60, 120, 255};
	/**
	 * 奥の透明なパネルの半透明の帯・不透明の帯・文字の色（乗算前の値）。
	 */
	static constexpr FColor FarSemiColor{255, 220, 0, 128};
	static constexpr FColor FarOpaqueColor{0, 200, 0, 255};
	static constexpr FColor FarTextColor{255, 255, 255, 255};
	/**
	 * 手前の透明なパネルの色（乗算前の値）。
	 */
	static constexpr FColor NearColor{0, 64, 255, 96};
	/**
	 * 3DのView（画面全体、1280x720）。
	 */
	static FRenderView3D MakeView() noexcept;
	/**
	 * 2Dワールドのパネル（画面の(740,20)〜(900,80)に映る）。
	 */
	static FUiWorldPanel2D MakePanel2D() noexcept;
	/**
	 * 不透明な3Dのパネル（x=-8〜-4、y=0〜3、z=0）。
	 */
	static FUiWorldPanel3D MakeOpaquePanel() noexcept;
	/**
	 * 奥の透明な3Dのパネル（x=0〜8、y=0〜3、z=0、512x192）。
	 * 行はv=0〜64が透明（文字だけ）、64〜160が半透明、160〜192が不透明。
	 */
	static FUiWorldPanel3D MakeFarPanel() noexcept;
	/**
	 * 手前の透明な3Dのパネル（x=4〜7、y=-1.5〜1、z=-2、全面が半透明）。
	 */
	static FUiWorldPanel3D MakeNearPanel() noexcept;
	/**
	 * 手前の不透明な箱（不透明なパネルの一部を隠す）。
	 */
	static Toolbox::FOBB MakeFrontBox() noexcept;
	/**
	 * パネルの後に描く箱（奥のパネルの透明な行の後ろ）。
	 */
	static Toolbox::FOBB MakeLateBox() noexcept;
	/**
	 * 3Dのパネルを描くか（描かないフレームを基準の背景にする）。
	 */
	bool bDrawPanels3D = true;
	/**
	 * 決定の回数。
	 * @param Button 欄。
	 */
	FORCEINLINE Toolbox::int32 GetClicks(EDisplayButton Button) const noexcept
	{
		return m_Clicks[static_cast<Toolbox::size_t>(Button)];
	}

protected:
	/**
	 * ルートと表示先を作る。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
	/**
	 * パネルの中間画像→3D→2DのUIの順に描く。
	 */
	void OnDraw(FRenderContext& Render) const override;

private:
	/**
	 * 全面の決定ボタン（枠なし、指定の背景）を作る。
	 * @param Root ルート。
	 * @param Color 背景色。
	 * @param Button 数える欄。
	 */
	TResult<void> AddButton_Internal(FUiRoot& Root, FColor Color, EDisplayButton Button);
	/**
	 * 絶対位置の色の部品（入力を受けない）を作る。
	 * @param Root ルート。
	 * @param Rect 論理単位の位置と寸法。
	 * @param Color 背景色。
	 */
	static TResult<void> AddPatch_Internal(FUiRoot& Root, FUiRect Rect, FColor Color);
	/**
	 * 新しいルートを作る。
	 */
	Toolbox::TUniquePtr<FUiRoot> MakeRoot_Internal() const;
	/**
	 * 文字の窓口。
	 */
	Toolbox::TUniquePtr<FUiAssetTextService> m_pText;
	/**
	 * 全画面・左右のViewport・2Dワールド・3D（不透明・奥・手前）のルート。
	 */
	Toolbox::TUniquePtr<FUiRoot> m_pOverlay;
	Toolbox::TUniquePtr<FUiRoot> m_pLeft;
	Toolbox::TUniquePtr<FUiRoot> m_pRight;
	Toolbox::TUniquePtr<FUiRoot> m_pPanel2D;
	Toolbox::TUniquePtr<FUiRoot> m_pOpaque;
	Toolbox::TUniquePtr<FUiRoot> m_pFar;
	Toolbox::TUniquePtr<FUiRoot> m_pNear;
	/**
	 * 仲介（ルートより後に宣言し、先に破棄する）。
	 */
	mutable FUiSceneHost m_Host;
	/**
	 * ボタンの決定の購読。
	 */
	Toolbox::TVector<FUiSubscription> m_Subscriptions;
	/**
	 * 欄ごとの決定の回数。
	 */
	Toolbox::int32 m_Clicks[static_cast<Toolbox::size_t>(EDisplayButton::Count)] = {};
};
} // namespace Dxf::UiSmoke
#endif
