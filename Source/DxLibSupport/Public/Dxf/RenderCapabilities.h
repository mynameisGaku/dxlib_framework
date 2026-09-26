// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_RENDER_CAPABILITIES_H
#define DXF_RENDER_CAPABILITIES_H
namespace Dxf
{
/**
 * 描画Backendが宣言した合成の能力（描画の前に確かめるための値）。
 * Backendの宣言をそのまま写した値で、宣言が正しいことまでは保証しない。
 */
struct FRenderCapabilities
{
	/**
	 * 2D命令の乗算済みアルファの合成（EBlendMode2D::PremultipliedAlpha）。
	 */
	bool bPremultipliedBlend2D = false;
	/**
	 * アルファ付きの描画先を透明（A<255）で消去し、アルファを蓄積できるか。
	 */
	bool bAlphaTargetClear = false;
	/**
	 * テクスチャを貼った3Dの四角形。
	 */
	bool bTexturedQuads3D = false;
	/**
	 * テクスチャを貼った3Dの四角形の乗算済みアルファの合成（α=0の画素は深度を書かない）。
	 */
	bool bPremultipliedQuads3D = false;
	/**
	 * 値が同じか。
	 */
	bool operator==(const FRenderCapabilities&) const = default;
};
} // namespace Dxf
#endif
