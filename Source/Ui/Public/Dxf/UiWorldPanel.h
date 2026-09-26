// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_WORLD_PANEL_H
#define DXF_UI_WORLD_PANEL_H
#include "Dxf/RenderView3D.h"
#include "Dxf/UiSurface.h"
#include "Toolbox/Function.h"
#include "Toolbox/Optional.h"
#include "Toolbox/Vector3.h"
namespace Dxf
{
/**
 * 2D世界から画面への変換（拡大と平行移動。Y上向きの世界にも対応する）。
 */
struct FUiWorldToScreen2D
{
	/**
	 * 世界の原点が映る画面の画素の位置。
	 */
	FVector2 Origin;
	/**
	 * 世界の1単位あたりの画素数（正）。
	 */
	Toolbox::f32 PixelsPerUnit = 1;
	/**
	 * 世界のYが上向きか（画面は下向き）。
	 */
	bool bYUp = true;
	/**
	 * 世界の点を画面の画素へ変える。
	 * @param World 世界の点。
	 */
	FORCEINLINE FVector2 ToScreen(FVector2 World) const noexcept
	{
		return {Origin.X + World.X * PixelsPerUnit, bYUp ? Origin.Y - World.Y * PixelsPerUnit : Origin.Y + World.Y * PixelsPerUnit};
	}
};

/**
 * 2D世界の矩形のパネル。パネルの論理座標は、左上を原点に高さ基準（表示先の設定のReferenceHeight）で拡縮する。
 */
struct FUiWorldPanel2D
{
	/**
	 * パネルの左上の世界の位置（Y上向きの世界では、上端のY）。
	 */
	FVector2 WorldTopLeft;
	/**
	 * パネルの世界の大きさ（幅・高さ、正）。
	 */
	FVector2 WorldSize{4, 2};
	/**
	 * 世界から画面への変換（カメラ・Viewportに合わせて毎フレーム更新する）。
	 */
	FUiWorldToScreen2D Transform;
	/**
	 * パネルを切り抜く画面の範囲（Viewport等。空は画面全体）。
	 */
	FUiPixelRect ScreenClip;
	/**
	 * パネルが映る画面の画素の矩形。
	 */
	FUiPixelRect GetPixelRect() const noexcept;
};

/**
 * 3Dのパネルの中間画像の合成。
 */
enum class EUiPanelComposition : Toolbox::uint8
{
	/**
	 * 不透明な背景（Background）へ描き、平面全体を不透明に貼る（従来の既定）。
	 */
	Opaque,
	/**
	 * 透明な中間画像へ乗算済みアルファで描き、画素ごとの透明度で背後と合成する。
	 * α=0の画素は深度を書かず背後の物体を隠さない。半透明の画素は、同じViewの先に描いた物体とだけ合成する
	 * （パネルは同じViewの物体の後に描き、透明なパネル同士は奥から順に描く。交差する透明な形状の厳密な合成は対象外）。
	 * 文字は乗算済みのフォント、画像は乗算済みで読んだ画像（FTextureLoadOptions::bPremultipliedAlpha）に限る。
	 * 入力は描画の透明度に依らず、平面の範囲で受ける（透明な画素のクリック透過はしない）。
	 */
	Transparent
};

/**
 * 3D世界の平面のパネル（平行四辺形）。同じルートをテクスチャへ描き、その画像を平面へ貼る。
 * 表面は右（左上→右上）と下（左上→左下）の外積の向き。既定では表面からの入力だけを受ける。
 */
struct FUiWorldPanel3D
{
	/**
	 * 左上の角。
	 */
	Toolbox::FVector3 TopLeft{-1, 1, 0};
	/**
	 * 右上の角。
	 */
	Toolbox::FVector3 TopRight{1, 1, 0};
	/**
	 * 左下の角（右下は 右上＋左下−左上）。
	 */
	Toolbox::FVector3 BottomLeft{-1, 0, 0};
	/**
	 * 描画先テクスチャの幅（画素）。
	 */
	Toolbox::int32 TextureWidth = 512;
	/**
	 * 描画先テクスチャの高さ（画素）。
	 */
	Toolbox::int32 TextureHeight = 256;
	/**
	 * 裏面からの入力も受けるか（描画は両面）。
	 */
	bool bDoubleSided = false;
	/**
	 * 深度の扱い（Scene: 奥行きで他の物体と前後する）。
	 */
	EDepthMode3D Depth = EDepthMode3D::TestAndWrite;
	/**
	 * 入力の遮蔽の判定（任意）。視点側の点からパネルの交点までの有限線分に遮るものがあればtrueを返す。
	 * 設定しない場合、見えないパネル（手前の物体に隠れた部分）へのクリックを防ぐ保証はない。
	 */
	Toolbox::TFunction<bool(Toolbox::FVector3, Toolbox::FVector3)> IsOccluded;
	/**
	 * 中間テクスチャの不透明な背景。Aは255に限る。部品の半透明はこの背景へ合成する。
	 * 透明なRenderTargetの合成規約を既存Rendererへ暗黙に追加しない。
	 */
	FColor Background{0, 0, 0, 255};
	/**
	 * 中間画像の合成（Opaqueは上のBackground、Transparentは透明な中間画像）。
	 */
	EUiPanelComposition Composition = EUiPanelComposition::Opaque;
	/**
	 * 右下の角。
	 */
	FORCEINLINE Toolbox::FVector3 BottomRight() const noexcept
	{
		return TopRight + BottomLeft - TopLeft;
	}
};

/**
 * 有限線分とパネルの交点を、パネルの正規化座標（左上0・右下1）で求める。
 * 表面以外（bDoubleSidedでない裏面）・平行・線分の範囲外・パネルの外はなし。
 * @param Panel パネル。
 * @param Start 線分の始点（視点側）。
 * @param End 線分の終点。
 * @param OutWorld 交点（世界）。
 * @param bAllowOutside キャプチャ配送専用。trueなら平面外のUVも返す。背面・近遠範囲の条件は維持する。
 */
Toolbox::TOptional<FVector2> IntersectUiWorldPanel3D(const FUiWorldPanel3D& Panel, Toolbox::FVector3 Start,
                                                     Toolbox::FVector3 End, Toolbox::FVector3* OutWorld = nullptr,
                                                     bool bAllowOutside = false) noexcept;
} // namespace Dxf
#endif
