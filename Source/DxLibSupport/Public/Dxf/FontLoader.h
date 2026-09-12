#pragma once
#include "Dxf/Font.h"
namespace Dxf
{
/**
 * フォントの読み込み器を管理する型。
 */
class FFontLoader
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Backend ネイティブ処理の呼び出し先。
	 * @param Registry リソースの登録先。
	 */
	FFontLoader(IFontBackend& Backend, FResourceRegistry& Registry) : m_pBackend(&Backend), m_pRegistry(&Registry)
	{
	}
	/**
	 * 対象のリソースを読み込む。
	 * @param Options 処理に適用する設定。
	 */
	TResult<FFont> Load(const FFontOptions& Options);

private:
	/**
	 * ネイティブ処理の呼び出し先。
	 */
	IFontBackend* m_pBackend;
	/**
	 * リソースの登録先。
	 */
	FResourceRegistry* m_pRegistry;
};
} // namespace Dxf
