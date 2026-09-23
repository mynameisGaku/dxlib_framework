// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_MODEL_CAMERA_INFO_H
#define DXF_MODEL_CAMERA_INFO_H
#include "Dxf/RenderView3D.h"
#include "Dxf/Result.h"
#include "Toolbox/String.h"
namespace Dxf
{
/**
 * FBXの静止カメラ。位置と距離はモデル読込時の座標系・単位。
 */
struct FModelCameraInfo
{
	/**
	 * 接続先ノードの名前。
	 */
	Toolbox::FString Name;
	/**
	 * カメラの位置。
	 */
	Toolbox::FVector3 Eye;
	/**
	 * 注視する向き。
	 */
	Toolbox::FVector3 Forward{0, 0, 1};
	/**
	 * カメラの上方向。
	 */
	Toolbox::FVector3 Up{0, 1, 0};
	/**
	 * 垂直画角（ラジアン）。
	 */
	Toolbox::f32 VerticalFov = 1;
	/**
	 * 正の近接面距離。
	 */
	Toolbox::f32 NearPlane = 0.01f;
	/**
	 * 遠方面距離。
	 */
	Toolbox::f32 FarPlane = 10000;
	/**
	 * 正射影で映る縦の長さ。
	 */
	Toolbox::f32 OrthographicHeight = 1;
	/**
	 * 元ファイルの横幅/高さ。適用先の画面比率へ自動変更しない。
	 */
	Toolbox::f32 AspectRatio = 1;
	/**
	 * 正射影ならtrue。
	 */
	bool bOrthographic = false;
	/**
	 * カメラだけをビューへ適用する。無効値ならビューを変更せず失敗。
	 * 画面の縦横比は描画先を使う。ファイルの画角・正射影の高さを保持する。
	 * @param View 適用先。ライトと調査表示は保持する。
	 */
	TResult<void> ApplyTo(FRenderView3D& View) const;
};
} // namespace Dxf
#endif
