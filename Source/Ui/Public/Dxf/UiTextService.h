// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_TEXT_SERVICE_H
#define DXF_UI_TEXT_SERVICE_H
#include "Dxf/Font.h"
#include "Dxf/Result.h"
#include "Toolbox/String.h"
namespace Dxf
{
/**
 * 描画・計測に使うフォントの識別（字体名と画素の大きさ）。
 */
struct FUiFontKey
{
	/**
	 * 字体名。
	 */
	Toolbox::FString Family;
	/**
	 * 画素の大きさ。
	 */
	Toolbox::int32 PixelSize = 20;
	/**
	 * 乗算済みアルファの文字の画像か（透明な中間画像へ描く表示面）。寸法は同じ。
	 */
	bool bPremultipliedAlpha = false;
	/**
	 * 値が同じか。
	 */
	bool operator==(const FUiFontKey& Other) const noexcept
	{
		return PixelSize == Other.PixelSize && bPremultipliedAlpha == Other.bPremultipliedAlpha &&
		       Family == Other.Family;
	}
};

/**
 * UIがフォントの取得と文字の計測に使う窓口。描画に使うフォントと同じ資源・大きさで計測する。
 * 実装はフォント資源の寿命を保つ（返したフォントを、ルートが使う間は解放しない）。
 */
class IUiTextService
{
public:
	virtual ~IUiTextService() = default;
	/**
	 * 字体名と画素の大きさのフォントを取得する。
	 * @param Key フォントの識別。
	 */
	virtual TResult<FFont> ResolveFont(const FUiFontKey& Key) = 0;
	/**
	 * 一行の文字列の描画幅（画素）。改行文字は含めないこと。
	 * @param Font 描画と同じフォント。
	 * @param Text UTF-8の文字列。
	 */
	virtual TResult<Toolbox::int32> MeasureWidth(const FFont& Font, const Toolbox::FString& Text) = 0;
	/**
	 * 行の送り（画素）。複数行はこの間隔で並べる。
	 * @param Font 描画と同じフォント。
	 */
	virtual TResult<Toolbox::int32> GetLineHeight(const FFont& Font) = 0;
};
} // namespace Dxf
#endif
