// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_IMAGE_H
#define DXF_UI_IMAGE_H
#include "Dxf/UiElement.h"
#include "Dxf/Texture.h"
namespace Dxf
{
/**
 * 画像の配置方法。Coverは範囲に切り抜く。
 */
enum class EUiImageFit : Toolbox::uint8
{
	Contain,
	Cover,
	Stretch
};
/**
 * 画像を所有参照で保持する部品。未設定は空、設定済み資源の失効は描画側で失敗する。
 */
class DUiImage : public DUiElement
{
public:
	explicit DUiImage(FTexture Texture = {});
	void SetTexture(FTexture Texture);
	void SetFit(EUiImageFit Fit);
	FORCEINLINE const FTexture& GetTexture() const noexcept
	{
		return m_Texture;
	}

	FORCEINLINE EUiImageFit GetFit() const noexcept
	{
		return m_Fit;
	}

	FUiRect GetImageRect() const noexcept;

protected:
	/**
	 * 内容の測定結果を親へ返す。
	 * @param Context 測定の窓口。
	 * @param Available 割り当て可能な寸法。
	 */
	FUiSize OnMeasure(FUiLayoutContext& Context, FUiSize Available) override;
	/**
	 * 現在の表示値から描画命令を記録する。
	 * @param Context 論理座標の描画窓口。
	 */
	void OnDraw(FUiDrawContext& Context) const override;

public:
	/**
	 * 画像解像度検査に用いる、hover等を含む最大表示倍率。
	 */
	void SetInspectionPeakScale(Toolbox::f32 Scale);
	/**
	 * 最大表示倍率。
	 */
	FORCEINLINE Toolbox::f32 GetInspectionPeakScale() const noexcept
	{
		return m_PeakScale;
	}

private:
	/**
	 * 検査用の最大倍率。
	 */
	Toolbox::f32 m_PeakScale = 1;
	FTexture m_Texture;
	EUiImageFit m_Fit = EUiImageFit::Contain;
};
} // namespace Dxf
// namespace Dxf
#endif
