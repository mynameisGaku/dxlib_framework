// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_JSON_DOCUMENT_H
#define TOOLBOX_JSON_DOCUMENT_H
#include "Toolbox/JsonValue.h"
#include "Toolbox/JsonLimits.h"
namespace Toolbox
{
/**
 * 復号したJSONの値を所有する。読み取りは外部I/Oやゲーム状態に触れない。
 */
class FJsonDocument
{
public:
	/**
	 * 全入力を読む。不正UTF-8、重複キー、末尾データ、非有限数値、上限は例外。
	 * @param Bytes 長さ付きのUTF-8入力。先頭BOMは一つだけ許す。
	 * @param Limits 入力・構造・文字列の有限上限。
	 */
	static FJsonDocument Parse(FStringView Bytes, FJsonLimits Limits = {});
	/**
	 * 文書内の値を取得する。範囲外は例外。
	 * @param Index 文書内のindex。最上位は0。
	 */
	const FJsonValue& Get(int32 Index) const;
	/**
	 * メンバーを復号後の名前で探す。不存在は-1、Object以外は例外。
	 * @param Object 検索するオブジェクト。
	 * @param Key 復号済みUTF-8の名前。
	 */
	int32 Find(int32 Object, FStringView Key) const;
	/**
	 * 保持する値の数を返す。
	 */
	size_t Size() const noexcept;

private:
	/**
	 * 読み取り時に確定した値の集合。
	 */
	TVector<FJsonValue> m_Values;
};
} // namespace Toolbox
#endif
