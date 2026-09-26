// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_TEST_UI_SCREEN_CAPTURE_H
#define DXF_TEST_UI_SCREEN_CAPTURE_H
#include "Dxf/MathTypes.h"
#include "Dxf/NativeHandle.h"
#include "Toolbox/Platform.h"
namespace Dxf::UiSmoke
{
/**
 * Present前の画面（1280x720）をCPUの画像へ一度だけ読み戻し、画素を返す。
 */
class FScreenCapture
{
public:
	/**
	 * 現在の描画先を読み戻す（前の内容は捨てる）。
	 */
	void Capture();
	/**
	 * 読み戻したか。
	 */
	FORCEINLINE bool IsCaptured() const noexcept
	{
		return m_Image.Get() >= 0;
	}
	/**
	 * 画素の色。
	 * @param X 横の画素。
	 * @param Y 縦の画素。
	 */
	FColor Pixel(Toolbox::int32 X, Toolbox::int32 Y) const;
	/**
	 * 読み戻した画像をPNGで保存する。
	 * @param Path 保存先。
	 */
	void Save(const Toolbox::FPath& Path) const;

private:
	/**
	 * CPU側の画像。
	 */
	FNativeHandle m_Image;
};
} // namespace Dxf::UiSmoke
#endif
