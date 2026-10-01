// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_JSON_VALUE_H
#define TOOLBOX_JSON_VALUE_H
#include "Toolbox/String.h"
namespace Toolbox
{
/**
 * JSONの値の区分。オブジェクトの順序は意味を持たず配列順は保持する。
 */
enum class EJsonKind : uint8
{
	/**
	 * null。
	 */
	Null,
	/**
	 * 真偽値。
	 */
	Boolean,
	/**
	 * 有限の数値。
	 */
	Number,
	/**
	 * 復号済みUTF-8文字列。
	 */
	String,
	/**
	 * 名前付きの値集合。
	 */
	Object,
	/**
	 * 順序付きの値集合。
	 */
	Array
};
/**
 * 文書内の一つの値。子と兄弟は文書内のindex、-1は未指定。
 */
struct FJsonValue
{
	/**
	 * 値の区分。
	 */
	EJsonKind Kind = EJsonKind::Null;
	/**
	 * オブジェクト内の復号済みメンバー名。
	 */
	FString Key;
	/**
	 * 文字列値または数値の原文。埋め込みNULも長さで保持する。
	 */
	FString Text;
	/**
	 * 有限の数値。
	 */
	f64 Number = 0;
	/**
	 * 真偽値。
	 */
	bool Boolean = false;
	/**
	 * 最初の子のindex。
	 */
	int32 FirstChild = -1;
	/**
	 * 親のindex。rootは-1。
	 */
	int32 Parent = -1;
	/**
	 * 次の兄弟のindex。
	 */
	int32 NextSibling = -1;
	/**
	 * 入力上の行、1始まり。
	 */
	uint32 Line = 1;
	/**
	 * 入力上のバイト列、1始まり。
	 */
	uint32 Column = 1;
};
} // namespace Toolbox
#endif
