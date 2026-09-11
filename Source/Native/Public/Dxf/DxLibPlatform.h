#pragma once
#include "Dxf/Platform.h"
namespace Dxf
{
class FDxLibPlatform final : public IPlatform
{
public:
    FDxLibPlatform() = default;
    ~FDxLibPlatform() override;
    FDxLibPlatform(const FDxLibPlatform&) = delete;
    FDxLibPlatform& operator=(const FDxLibPlatform&) = delete;
    TResult<void> Initialize(const FWindowSettings& Settings) override;
    void Shutdown() noexcept override;
    TResult<bool> PumpEvents() override;
private:
    bool m_bInitialized = false;
};
}
