#pragma once
#include "Dxf/RenderQueue2D.h"
namespace Dxf
{
class FRenderContext
{
public:
    explicit FRenderContext(FRenderQueue2D& Queue) : m_pQueue(&Queue) {}
    TResult<void> Draw(FTexture Texture, FVector2 Position, const FSpriteDrawOptions& Options = {})
    { return m_pQueue->Submit(FSpriteCommand{std::move(Texture), Position, Options}); }
    TResult<void> DrawText(FFont Font, std::string Text, FVector2 Position, const FDrawStyle& Options = {})
    { return m_pQueue->Submit(FTextCommand{std::move(Font), std::move(Text), Position, Options}); }
    TResult<void> FillRectangle(FIntRect Rectangle, const FDrawStyle& Options = {})
    { return m_pQueue->Submit(FRectangleCommand{Rectangle, Options}); }
private:
    FRenderQueue2D* m_pQueue;
};
}
