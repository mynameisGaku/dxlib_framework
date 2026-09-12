#pragma once
#include "Dxf/Texture.h"
#include "Dxf/Font.h"
#include "Dxf/MathTypes.h"
#include "Toolbox/Variant.h"
namespace Dxf
{
/**
 * 描画色・不透明度・描画順序を管理する型。
 */
struct FDrawStyle
{
	/**
	 * 描画色。
	 */
	FColor Color;
	/**
	 * 不透明度。
	 */
	Toolbox::f32 Opacity = 1.0f;
	/**
	 * 描画レイヤー。
	 */
	Toolbox::int32 Layer = 0;
	/**
	 * 同じレイヤー内の処理順序。
	 */
	Toolbox::int32 Order = 0;
};
/**
 * スプライトの変換と描画設定を管理する型。
 */
struct FSpriteDrawOptions : FDrawStyle
{
	/**
	 * 拡大率。
	 */
	FVector2 Scale{1.0f, 1.0f};
	/**
	 * 回転と拡縮の基準位置。
	 */
	FVector2 Pivot;
	/**
	 * ラジアン単位の回転角。
	 */
	Toolbox::f32 RotationRadians = 0.0f;
	/**
	 * 左右を反転するか。
	 */
	bool bFlipX = false;
	/**
	 * 上下を反転するか。
	 */
	bool bFlipY = false;
};
/**
 * スプライトの描画命令を管理する型。
 */
struct FSpriteCommand
{
	/**
	 * 描画するテクスチャ。
	 */
	FTexture Texture;
	/**
	 * 描画位置。
	 */
	FVector2 Position;
	/**
	 * 処理に適用する設定。
	 */
	FSpriteDrawOptions Options;
};
/**
 * 文字列の描画命令を管理する型。
 */
struct FTextCommand
{
	/**
	 * 文字描画に使うフォント。
	 */
	FFont Font;
	/**
	 * 描画する文字列。
	 */
	Toolbox::FString Text;
	/**
	 * 描画位置。
	 */
	FVector2 Position;
	/**
	 * 処理に適用する設定。
	 */
	FDrawStyle Options;
};
/**
 * 矩形の描画命令を管理する型。
 */
struct FRectangleCommand
{
	/**
	 * 描画する矩形。
	 */
	FIntRect Rectangle;
	/**
	 * 処理に適用する設定。
	 */
	FDrawStyle Options;
};
/**
 * スプライト・文字列・矩形の描画命令。
 */
using FRenderCommand = Toolbox::TVariant<FSpriteCommand, FTextCommand, FRectangleCommand>;
} // namespace Dxf
