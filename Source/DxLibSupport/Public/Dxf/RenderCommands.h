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
	/**
	 * 命令ごとのクリップ矩形（描画先の画素。左・上を含み、右・下を含まない）。
	 * 値で保存するため、レイヤーの並べ替えの後も同じ範囲に切り抜く。
	 */
	FIntRect ClipRect{};
	/**
	 * ClipRectで切り抜くか。空の矩形は何も描かない（描画先全体とは扱わない）。
	 */
	bool bClip = false;
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
	/**
	 * falseなら外周だけ描く。
	 */
	bool bFilled = true;
};
/**
 * 2D線分。画面ピクセル単位。
 */
struct FLineCommand2D
{
	/**
	 * 始点。
	 */
	FVector2 Start;
	/**
	 * 終点。
	 */
	FVector2 End;
	/**
	 * 色と描画順。
	 */
	FDrawStyle Options;
};
/**
 * 2D円。画面ピクセル単位。
 */
struct FCircleCommand2D
{
	/**
	 * 中心。
	 */
	FVector2 Center;
	/**
	 * 非負の半径。
	 */
	Toolbox::f32 Radius = 0;
	/**
	 * 中身を塗るか。
	 */
	bool bFilled = false;
	/**
	 * 色と描画順。
	 */
	FDrawStyle Options;
};
/**
 * 2D三角形。画面ピクセル単位。
 */
struct FTriangleCommand2D
{
	/**
	 * 三頂点。
	 */
	FVector2 A;
	FVector2 B;
	FVector2 C;
	/**
	 * 中身を塗るか。
	 */
	bool bFilled = false;
	/**
	 * 色と描画順。
	 */
	FDrawStyle Options;
};
/**
 * 2D描画命令。3Dの命令を混在させない。
 */
using FRenderCommand = Toolbox::TVariant<FSpriteCommand, FTextCommand, FRectangleCommand, FLineCommand2D, FCircleCommand2D, FTriangleCommand2D>;
}
// namespace Dxf
