#pragma once
#include "Dxf/AssetBackend.h"
namespace Dxf
{
class FDxLibFontBackend final : public IFontBackend
{
public:
    TResult<int> CreateFont(const FFontOptions& Options) override;
    void DeleteFont(int Handle) noexcept override;
};
}
