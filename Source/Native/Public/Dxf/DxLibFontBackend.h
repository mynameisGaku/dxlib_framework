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
};
} // namespace Dxf
