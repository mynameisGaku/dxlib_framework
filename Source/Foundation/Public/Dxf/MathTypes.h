#pragma once
#include <cstdint>
namespace Dxf
{
struct FVector2
{
    float X = 0.0f;
    float Y = 0.0f;
};
struct FColor
{
    std::uint8_t R = 255;
    std::uint8_t G = 255;
    std::uint8_t B = 255;
    std::uint8_t A = 255;
};
struct FIntRect
{
    int Left = 0;
    int Top = 0;
    int Right = 0;
    int Bottom = 0;
};
}
