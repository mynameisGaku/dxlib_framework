#pragma once
#include "Dxf/RenderBackend.h"
#include <vector>
namespace Dxf
{
class FRenderQueue2D
{
public:
    TResult<void> Submit(FRenderCommand Command);
    TResult<void> Execute_Internal(IRenderBackend& Backend);
    void SetTarget_Internal(int Target) noexcept { m_Target = Target; }
    void SetAccepting_Internal(bool bAccepting) noexcept { m_bAccepting = bAccepting; }
    void Clear_Internal() noexcept { m_Commands.clear(); }
private:
    TResult<void> Validate_Internal(const FRenderCommand& Command) const;
    std::vector<FRenderCommand> m_Commands;
    int m_Target = -1;
    bool m_bAccepting = false;
    bool m_bExecuting = false;
};
}
