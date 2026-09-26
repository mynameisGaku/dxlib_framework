// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_HOST_STATE_H
#define DXF_UI_HOST_STATE_H
#include "Dxf/UiHostSettings.h"
#include "Dxf/UiRootHandle.h"
namespace Dxf::Detail
{
class FUiHostState
{
public:
	/**
	 * @param Settings 設定。
	 */
	explicit FUiHostState(FUiSceneHostSettings Settings = {});
	/**
	 * 接続したSceneから外す。
	 */
	~FUiHostState() = default;
	void Shutdown() noexcept;
	FUiHostState(const FUiHostState&) = delete;
	FUiHostState& operator=(const FUiHostState&) = delete;
	/**
	 * Sceneの入力の仲介として接続する（以後、Scene・子・固定更新はUIを除いた入力を受け取る）。
	 * @param Scene Scene（この仲介より長く生存する場合は、破棄の前に外すこと）。
	 */
	/**
	 * Sceneから外す。
	 */
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
	 * 3Dのパネルを見るViewを設定する（入力の判定に使う。描画でViewを渡したときにも更新する）。
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
	FInputSnapshot RouteInput(const FTickContext& Context);
	/**
	 * 3Dのパネルの描画先テクスチャへUIを描く（Sceneの描画の最初に呼ぶ。描画先を切り替え、画面へ戻す）。
	 * @param Render 描画の窓口。
	 */
	TResult<void> RenderWorldPanelTextures(FRenderContext& Render);
	/**
	 * 3Dのパネルを平面として描く（Viewを設定した後、そのViewの描画として呼ぶ。複数のViewでも木の更新は増えない）。
	 * @param Render 描画の窓口。
	 * @param View 描くView（入力の判定にも使う）。
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
	FORCEINLINE const FUiRoutingResult& GetLastRouting() const noexcept
	{
		return m_LastRouting;
	}
	/**
	 * 表示先の数。
	 */
	FORCEINLINE Toolbox::size_t GetDisplayCount() const noexcept
	{
		return m_Displays.Size();
	}
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

private:
	/**
	 * 表示先の一件。
	 */
	struct FDisplay
	{
		/**
		 * 識別。
		 */
		FUiDisplayId Id = 0;
		/**
		 * 種類。
		 */
		EUiDisplayKind Kind = EUiDisplayKind::Screen;
		/**
		 * ルート（所有しない）。
		 */
		FUiRootHandle Root;
		const FUiRoot* RootIdentity = nullptr;
		/**
		 * 設定。
		 */
		FUiDisplayOptions Options;
		/**
		 * Viewportの矩形。
		 */
		FUiPixelRect Rect;
		/**
		 * 2Dのパネル。
		 */
		FUiWorldPanel2D Panel2D;
		/**
		 * 3Dのパネル。
		 */
		FUiWorldPanel3D Panel3D;
		/**
		 * 3Dのパネルの描画先テクスチャ。
		 */
		FRenderTarget Texture;
		/**
		 * 3Dのパネルの描画先を作る資源管理。
		 */
		FAssetService* pAssets = nullptr;
	};
	/**
	 * 表示先の表示面。
	 * @param Display 表示先。
	 */
	FUiSurface MakeSurface_Internal(const FDisplay& Display) const noexcept;
	/**
	 * 画面の点を表示先の論理座標へ変える（範囲外・背面・遮蔽ならfalse）。
	 * @param Display 表示先。
	 * @param Screen 画面の画素の点。
	 * @param Out 論理座標。
	 */
	bool MapPointer_Internal(const FDisplay& Display, FVector2 Screen, FVector2& Out, Toolbox::f64* Distance = nullptr,
	                         bool bCapture = false) const;
	/**
	 * 表示先を探す。
	 * @param Id 識別。
	 */
	FDisplay* Find_Internal(FUiDisplayId Id) noexcept;
	/**
	 * 表示先を探す。
	 * @param Id 識別。
	 */
	const FDisplay* Find_Internal(FUiDisplayId Id) const noexcept;
	/**
	 * 表示先を加える。
	 * @param Display 表示先。
	 */
	FUiDisplayId Add_Internal(FDisplay Display);
	/**
	 * 設定。
	 */
	FUiSceneHostSettings m_Settings;
	/**
	 * 接続したScene。
	 */
	bool m_bStopped = false;
	bool m_bRouting = false;
	bool m_bDrawing = false;
	/**
	 * 表示先（登録順）。
	 */
	Toolbox::TVector<FDisplay> m_Displays;
	/**
	 * 次の識別。
	 */
	FUiDisplayId m_NextId = 1;
	/**
	 * ポインターを固定している表示先（押し始めてから離すまで）。
	 */
	FUiDisplayId m_PointerOwner = 0;
	// 有限な画面線分を作れない間のキャプチャ位置。原点への偽ドラッグを送らない。
	FVector2 m_LastPointerPosition;
	/**
	 * 3Dのパネルを見るView。
	 */
	Toolbox::TVector<FRenderView3D> m_Views3D;
	/**
	 * 最後に描画したViewの画面の大きさ。
	 */
	Toolbox::int32 m_ScreenWidth = 1280;
	/**
	 * 同上の高さ。
	 */
	Toolbox::int32 m_ScreenHeight = 720;
	/**
	 * UIが持っているキー。
	 */
	Toolbox::TArray<bool, static_cast<Toolbox::size_t>(EKey::Count)> m_OwnedKeys{};
	/**
	 * UIが持っているマウスのボタン。
	 */
	Toolbox::TArray<bool, static_cast<Toolbox::size_t>(EMouseButton::Count)> m_OwnedMouse{};
	/**
	 * UIが持っているパッドのボタン。
	 */
	Toolbox::TArray<Toolbox::TArray<bool, 16>, 4> m_OwnedPad{};
	/**
	 * 前のフレームの方向の操作（スティック）の状態（押下の判定に使う）。
	 */
	Toolbox::TArray<Toolbox::TArray<bool, static_cast<Toolbox::size_t>(EUiNavigationCommand::Count)>, 4>
	    m_PreviousNavigation{};
	/**
	 * ゲームへ渡す入力の前回と今回。
	 */
	FInputStateTracker m_Filtered;
	Toolbox::TArray<bool, 4> m_OwnedStick{};
	bool m_bSeeded = false;
	/**
	 * 最後の仲介の結果。
	 */
	FUiRoutingResult m_LastRouting;
	/**
	 * 描画の列（毎フレーム使い回す）。
	 */
	FUiDrawList m_DrawList;
	/**
	 * 最後に仲介したフレーム（同じフレームの二度目は同じ結果を返す）。
	 */
	Toolbox::uint64 m_LastFrame = Toolbox::TNumericLimits<Toolbox::uint64>::Max();
};
} // namespace Dxf::Detail
// namespace Dxf::Detail
#endif
