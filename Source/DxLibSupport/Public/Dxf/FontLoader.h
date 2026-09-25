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
	/**
	 * 一行の文字列の描画幅（画素）を計測する。
	 * @param Font 描画に使うフォント。
	 * @param Text UTF-8の文字列（改行を含まない）。
	 */
	TResult<Toolbox::int32> MeasureTextWidth(const FFont& Font, const Toolbox::FString& Text) const;
	/**
	 * 行の送り（画素）。
	 * @param Font 描画に使うフォント。
	 */
	TResult<Toolbox::int32> GetLineHeight(const FFont& Font) const;

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
