// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_E_MODEL_LIGHT_TYPE_H
#define DXF_E_MODEL_LIGHT_TYPE_H
namespace Dxf
{
/**
 * モデルのGPU照明で使う光源の形。
 */
enum class EModelLightType
{
	/**
	 * 一定の向きから届く光。
	 */
	Directional,
	/**
	 * 一点から全方向へ届く光。
	 */
	Point,
	/**
	 * 一点から円錐内へ届く光。
	 */
	Spot
};
} // namespace Dxf
#endif
