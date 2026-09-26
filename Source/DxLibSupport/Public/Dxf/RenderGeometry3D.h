// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_RENDER_GEOMETRY_3D_H
#define DXF_RENDER_GEOMETRY_3D_H
#include "Dxf/RenderView3D.h"
#include "Dxf/Result.h"
#include "Dxf/Texture.h"
#include "Toolbox/Array.h"
#include "Toolbox/CollisionShapes.h"
namespace Dxf
{
/**
 * ワールド座標での線分。
 */
struct FLine3D
{
	/**
	 * 始点。
	 */
	Toolbox::FVector3 Start;
	/**
	 * 終点。
	 */
	Toolbox::FVector3 End;
};
/**
 * 反時計回り法線を持つ、ワールド座標の三角形。
 */
struct FTriangle3D
{
	/**
	 * 一番目の頂点。
	 */
	Toolbox::FVector3 A;
	/**
	 * 二番目の頂点。
	 */
	Toolbox::FVector3 B;
	/**
	 * 三番目の頂点。
	 */
	Toolbox::FVector3 C;
};
/**
 * 所有する線分・面群。Nativeハンドルや外部ポインタを含まない。
 */
struct FGeometry3D
{
	/**
	 * メッシュの辺または独立した線。
	 */
	Toolbox::TVector<FLine3D> Lines;
	/**
	 * 塗りつぶす面。三頂点から法線を計算する。
	 */
	Toolbox::TVector<FTriangle3D> Triangles;
};
/**
 * 不透明・透明のScene描画と、調査用の重ね描画を区別する。
 */
enum class ERenderLayer3D
{
	/**
	 * A=255は不透明、0<A<255は奥から手前へ並べる透明パス。
	 */
	Scene,
	/**
	 * Sceneの後に受付順で重ねる。深度を書き込まない。
	 */
	Overlay
};
/**
 * 色・深度・表示層。透明Sceneは深度を検査するが書き込まない。
 * AlwaysはLayerに関係なくOverlayとして受付順で描く。
 */
struct FDrawStyle3D
{
	/**
	 * 面または線の色。Aは不透明度。
	 */
	FColor Color;
	/**
	 * 深度検査・書き込みの方式。
	 */
	EDepthMode3D Depth = EDepthMode3D::TestAndWrite;
	/**
	 * 描画順の所属。デバッグ補助表示はOverlayでSceneと分離できる。
	 */
	ERenderLayer3D Layer = ERenderLayer3D::Scene;
};
/**
 * 並列生成の一入力が所有する3D描画命令。
 */
struct FGeometryCommand3D
{
	/**
	 * ワールド座標の形状。
	 */
	FGeometry3D Geometry;
	/**
	 * 命令の表示設定。
	 */
	FDrawStyle3D Options;
};
/**
 * 表示上書き・照明を適用した三角形。
 */
struct FPreparedTriangle3D
{
	/**
	 * 実際に描く面。
	 */
	FTriangle3D Triangle;
	/**
	 * 照明評価済みの色。
	 */
	FColor Color;
	/**
	 * 深度設定。
	 */
	EDepthMode3D Depth;
	/**
	 * 変換済みの所属層。Surface+Edgesで生成した辺はOverlay。
	 */
	ERenderLayer3D Layer = ERenderLayer3D::Scene;
};
/**
 * 表示上書き後の線分。
 */
struct FPreparedLine3D
{
	/**
	 * 実際に描く線。
	 */
	FLine3D Line;
	/**
	 * 線色。照明は適用しない。
	 */
	FColor Color;
	/**
	 * 深度設定。
	 */
	EDepthMode3D Depth;
	/**
	 * 変換済みの所属層。Surface+Edgesで生成した辺はOverlay。
	 */
	ERenderLayer3D Layer = ERenderLayer3D::Scene;
};
/**
 * Native非依存の描画パケット。計画後は複数命令の同種プリミティブをまとめ得る。
 */
struct FPreparedGeometry3D
{
	/**
	 * 面の受付順。
	 */
	Toolbox::TVector<FPreparedTriangle3D> Triangles;
	/**
	 * 補助線の受付順。面の後に描く。
	 */
	Toolbox::TVector<FPreparedLine3D> Lines;
};
/**
 * @param Geometry 有限値と件数を検証する形状。
 */
/**
 * テクスチャを貼った四角形（UIのパネル等）。頂点は左上・右上・右下・左下の順で、テクスチャの左上(0,0)〜右下(1,1)を対応させる。
 */
struct FTexturedQuad3D
{
	/**
	 * 貼るテクスチャ（描画先テクスチャも可）。
	 */
	FTexture Texture;
	/**
	 * 四隅（左上・右上・右下・左下）。
	 */
	Toolbox::TArray<Toolbox::FVector3, 4> Corners{};
	/**
	 * 乗算色（Aは不透明度）。
	 */
	FColor Tint{255, 255, 255, 255};
	/**
	 * 深度の扱い。
	 */
	EDepthMode3D Depth = EDepthMode3D::TestAndWrite;
	/**
	 * 裏面も描くか（偽なら表面だけ）。
	 */
	bool bDoubleSided = true;
	/**
	 * テクスチャが乗算済みアルファか（透明なUIのパネル）。真なら乗算済みの合成で描き、α=0の画素は深度も書かない。
	 */
	bool bPremultipliedAlpha = false;
};
bool IsValidGeometry3D(const FGeometryCommand3D& Geometry) noexcept;
/**
 * @param Box 描画する箱。Axesは直交する単位軸。
 */
TResult<FGeometry3D> BuildBoxGeometry3D(const Toolbox::FOBB& Box);
/**
 * 経緯線で球を構築する。精度とメモリ予算のため分割数は4〜64、偶数とする。
 * @param Sphere 非負の半径と中心。
 * @param Segments 経度方向の分割数。緯度方向は半分。
 */
TResult<FGeometry3D> BuildSphereGeometry3D(const Toolbox::FSphere& Sphere, Toolbox::uint32 Segments = 16);
/**
 * ビュー上書きから実描画パケットを作る。元の形状と設定は変更しない。
 * @param Command 元の形状と表示設定。
 * @param View 受付時に複写した描画ビュー。
 */
TResult<FPreparedGeometry3D> PrepareGeometry3D(const FGeometryCommand3D& Command, const FRenderView3D& View);
}
#endif
