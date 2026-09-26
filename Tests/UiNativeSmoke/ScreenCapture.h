// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_TEST_UI_SCREEN_CAPTURE_H
#define DXF_TEST_UI_SCREEN_CAPTURE_H
#include "Dxf/MathTypes.h"
#include "Dxf/NativeHandle.h"
#include "Toolbox/Platform.h"
namespace Dxf::UiSmoke
{
/**
 * Present前の描画先をCPUの画像へ一度だけ読み戻し、画素を返す。
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
	 * 読み戻した描画先の幅と高さ。
	 */
	FORCEINLINE Toolbox::int32 GetWidth() const noexcept
	{
		return m_Width;
	}
	FORCEINLINE Toolbox::int32 GetHeight() const noexcept
	{
		return m_Height;
	}
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
	/**
	 * 読み戻した描画先の寸法。
	 */
	int m_Width = 0;
	int m_Height = 0;
};
} // namespace Dxf::UiSmoke
#endif
