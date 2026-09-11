#pragma once
#include "Dxf/Texture.h"
#include "Dxf/Font.h"
#include "Dxf/MathTypes.h"
#include <variant>
namespace Dxf
{
struct FDrawStyle
{
	FColor Color;
	float Opacity = 1.0f;
	int Layer = 0;
	int Order = 0;
};
struct FSpriteDrawOptions : FDrawStyle
{
	FVector2 Scale{1.0f, 1.0f};
	FVector2 Pivot;
	float RotationRadians = 0.0f;
	bool bFlipX = false;
	bool bFlipY = false;
};
struct FSpriteCommand
{
	FTexture Texture;
	FVector2 Position;
	FSpriteDrawOptions Options;
};
struct FTextCommand
{
	FFont Font;
	std::string Text;
	FVector2 Position;
	FDrawStyle Options;
};
struct FRectangleCommand
{
	FIntRect Rectangle;
	FDrawStyle Options;
};
using FRenderCommand = std::variant<FSpriteCommand, FTextCommand, FRectangleCommand>;
}
