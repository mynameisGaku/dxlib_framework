// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_PARAMETER_VALUE_H
#define DXF_CONTENT_PARAMETER_VALUE_H
#include "Dxf/MathTypes.h"
#include "Toolbox/Vector2.h"
#include "Toolbox/Vector3.h"
#include "Toolbox/String.h"
namespace Dxf
{
/**
 * 型付き公開値の区分。文字列は資源キーだけで式やコードを実行しない。
 */
enum class EContentParameterKind : Toolbox::uint8
{
	/**
	 * 真偽値。
	 */
	Boolean,
	/**
	 * 有限数値。
	 */
	Number,
	/**
	 * 平面ベクトル。
	 */
	Vector2,
	/**
	 * 立体ベクトル。
	 */
	Vector3,
	/**
	 * RGBA色。
	 */
	Color,
	/**
	 * 型検査される資源キー。
	 */
	Asset
};
/**
 * 名前と型の付いた公開値。使用する値はKindで決まり、定義共有先を変更しない。
 */
struct FContentParameterValue
{
	/**
	 * 公開名。
	 */
	Toolbox::FString Id;
	/**
	 * 宣言された型。
	 */
	EContentParameterKind Kind = EContentParameterKind::Number;
	/**
	 * 真偽の値。
	 */
	bool Boolean = false;
	/**
	 * 有限数値。
	 */
	Toolbox::f64 Number = 0;
	/**
	 * 平面の値。
	 */
	Toolbox::FVector2 Vector2;
	/**
	 * 立体の値。
	 */
	Toolbox::FVector3 Vector3;
	/**
	 * 色の値。
	 */
	FColor Color;
	/**
	 * 資源キーの値。
	 */
	Toolbox::FString Asset;
};
} // namespace Dxf
#endif
