#pragma once
#include "Dxf/AssetBackend.h"
namespace Dxf
{
/**
 * DxLibによるフォントの生成と解放を管理する型。
 */
class FDxLibFontBackend final : public IFontBackend
{
public:
	/**
	 * ネイティブフォントを生成する。
	 * @param Options 処理に適用する設定。
	 */
	TResult<Toolbox::int32> CreateFont(const FFontOptions& Options) override;
	/**
	 * ネイティブフォントを解放する。
	 * @param Handle ハンドル。
	 */
	void DeleteFont(Toolbox::int32 Handle) noexcept override;
	/**
	 * GetDrawStringWidthToHandleで一行の描画幅を計測する（描画と同じUTF-8の文字コード設定）。
	 * @param Handle フォントのハンドル。
	 * @param Text UTF-8の文字列。
	 */
	TResult<Toolbox::int32> MeasureTextWidth(Toolbox::int32 Handle, const Toolbox::FString& Text) override;
	/**
	 * GetFontLineSpaceToHandleで行の送りを取得する。
	 * @param Handle フォントのハンドル。
	 */
	TResult<Toolbox::int32> GetFontLineHeight(Toolbox::int32 Handle) override;
};
} // namespace Dxf
