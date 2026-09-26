// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_SCENE_HOST_H
#define DXF_UI_SCENE_HOST_H
#include "Dxf/UiHostSettings.h"
#include "Dxf/UiInspectionResult.h"
namespace Dxf
{
namespace Detail
{
class FUiHostState;
}
/**
 * ゲーム側のUI接続窓口。入力・表示先・描画は独立したPrivate実装へ委譲する。
 * RootとSceneは非所有の生存印で借用し、破棄順に依存しない。所有スレッド専用。
 */
class FUiSceneHost final : public IInputRouter
{
public:
	/**
	 * @param Settings 設定。
	 */
	explicit FUiSceneHost(FUiSceneHostSettings Settings = {});
	/**
	 * 接続したSceneから外す。
	 */
	~FUiSceneHost() override;
	FUiSceneHost(const FUiSceneHost&) = delete;
	FUiSceneHost& operator=(const FUiSceneHost&) = delete;
	/**
	 * Sceneの入力の仲介として接続する（以後、Scene・子・固定更新はUIを除いた入力を受け取る）。
	 * @param Scene 接続先。SceneとHostのどちらが先に破棄されても接続を解除する。
	 */
	void AttachTo(DScene& Scene);
	/**
	 * Sceneから外す。
	 */
	void DetachFromScene() noexcept;
	/**
	 * 全画面の表示先を加える（後から加えたものが前面）。
	 * @param Root ルート。
	 * @param Options 設定。
	 */
	FUiDisplayId AddScreen(FUiRoot& Root, const FUiDisplayOptions& Options = {});
	/**
	 * 分割画面の一つの領域の表示先を加える。
	 * @param Root ルート。
	 * @param Rect 画面の画素の矩形。
	 * @param Options 設定。
	 */
	FUiDisplayId AddViewport(FUiRoot& Root, FUiPixelRect Rect, const FUiDisplayOptions& Options = {});
	/**
	 * 2D世界の矩形のパネルの表示先を加える。
	 * @param Root ルート。
	 * @param Panel パネル（世界の矩形と、世界から画面への変換）。
	 * @param Options 設定。
	 */
	FUiDisplayId AddWorldPanel2D(FUiRoot& Root, const FUiWorldPanel2D& Panel, const FUiDisplayOptions& Options = {});
	/**
	 * 3D世界の平面のパネルの表示先を加える（描画先テクスチャは初回の描画で作る）。
	 * @param Root ルート。
	 * @param Panel パネル（四隅・テクスチャの大きさ・遮蔽の判定）。
	 * @param Assets 描画先テクスチャを作る資源管理（この仲介より長く生存すること）。
	 * @param Options 設定。
	 */
	FUiDisplayId AddWorldPanel3D(FUiRoot& Root, const FUiWorldPanel3D& Panel, FAssetService& Assets,
	                             const FUiDisplayOptions& Options = {});
	/**
	 * 表示先を外す（そのルートのホバー・キャプチャを外す）。
	 * @param Display 表示先。
	 */
	bool Remove(FUiDisplayId Display) noexcept;
	/**
	 * 分割画面の領域を変える。
	 * @param Display 表示先。
	 * @param Rect 画面の画素の矩形。
	 */
	bool SetViewportRect(FUiDisplayId Display, FUiPixelRect Rect) noexcept;
	/**
	 * 2Dのパネルを変える（世界の矩形・世界から画面への変換）。
	 * @param Display 表示先。
	 * @param Panel パネル。
	 */
	bool SetWorldPanel2D(FUiDisplayId Display, const FUiWorldPanel2D& Panel) noexcept;
	/**
	 * 3Dのパネルを変える（四隅・遮蔽の判定。テクスチャの大きさを変えると作り直す）。
	 * @param Display 表示先。
	 * @param Panel パネル。
	 */
	bool SetWorldPanel3D(FUiDisplayId Display, const FUiWorldPanel3D& Panel) noexcept;
	/**
	 * 当該フレームの3D入力に使うView集合を明示する。描画関数からは書き換えない。
	 * @param Views Viewの一覧（複数のカメラで同じパネルを見る場合は複数）。
	 */
	void SetWorldViews3D(const Toolbox::TVector<FRenderView3D>& Views);
	/**
	 * 表示先の設定を変える。
	 * @param Display 表示先。
	 * @param Options 設定。
	 */
	bool SetOptions(FUiDisplayId Display, const FUiDisplayOptions& Options) noexcept;
	/**
	 * 入力プレイヤーの機器を設定する。
	 * @param Player 入力プレイヤー（0〜3）。
	 * @param Devices 機器。
	 */
	void SetPlayerDevices(Toolbox::int32 Player, FUiPlayerDevices Devices) noexcept;
	/**
	 * 操作の割当を設定する。
	 * @param Bindings 割当。
	 */
	void SetNavigationBindings(FUiNavigationBindings Bindings);
	/**
	 * 描画の前の画面の大きさを設定する。
	 * @param Width 幅。
	 * @param Height 高さ。
	 */
	void SetScreenSize(Toolbox::int32 Width, Toolbox::int32 Height) noexcept;
	/**
	 * Sceneの更新の前に入力を処理し、UIが使った操作を除いた入力を返す（そのフレームで一度だけ）。
	 * @param Context Sceneの更新の情報。
	 */
	FInputSnapshot RouteInput(const FTickContext& Context) override;
	/**
	 * 3Dパネルの中間テクスチャへUIを描く。終了時は呼出し前の描画先へ復帰する。
	 * @param Render 描画の窓口。
	 */
	TResult<void> RenderWorldPanelTextures(FRenderContext& Render);
	/**
	 * 3Dのパネルを平面として描く（Viewを設定した後、そのViewの描画として呼ぶ。複数のViewでも木の更新は増えない）。
	 * @param Render 描画の窓口。
	 * @param View 描くView。入力用の集合はSetWorldViews3Dへ別途渡す。
	 */
	TResult<void> DrawWorldPanels3D(FRenderContext& Render, const FRenderView3D& View);
	/**
	 * 全画面・Viewport・2Dのパネルのルートをレイアウトし、2D命令として送る。
	 * @param Render 描画の窓口。
	 */
	TResult<void> Draw(FRenderContext& Render);
	/**
	 * 最後の仲介の結果。
	 */
	FUiRoutingResult GetLastRouting() const noexcept;
	/**
	 * 表示先の数。
	 */
	Toolbox::size_t GetDisplayCount() const noexcept;
	/**
	 * 3Dのパネルの描画先テクスチャ（まだなければ無効）。
	 * @param Display 表示先。
	 */
	FRenderTarget GetWorldPanelTexture(FUiDisplayId Display) const noexcept;
	/**
	 * 表示先の表示面（最後の描画・入力で使ったもの）。
	 * @param Display 表示先。
	 */
	FUiSurface GetSurface(FUiDisplayId Display) const noexcept;
	/**
	 * 表示先を、その表示面で配置して描画命令を作り（描画はしない）、表示先の番号・表示面・追加のクリップ付きで検査する。
	 * 同じルートを複数の表示先へ出す場合も、表示先ごとの表示面で検査する。
	 * @param Display 表示先。
	 * @param MaxIssues 問題数の上限（超過は失敗）。
	 */
	TResult<FUiInspectionResult> InspectDisplay(FUiDisplayId Display, Toolbox::size_t MaxIssues = 4096);

private:
	/**
	 * コールバックから窓口が破棄されても、進行中処理の状態だけを保つ。
	 */
	Toolbox::TSharedPtr<Detail::FUiHostState> m_pState;
	/**
	 * Sceneが所有する接続枠。
	 */
	Toolbox::TWeakPtr<Detail::FInputRouterSlot> m_pConnection;
};
} // namespace Dxf
// namespace Dxf
#endif
