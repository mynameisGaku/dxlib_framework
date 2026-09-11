#pragma once
#include "Dxf/Application.h"
#include <chrono>
namespace Dxf
{
class FAppRunner
{
public:
    TResult<void> Run(FApplication& Application, std::unique_ptr<DScene> InitialScene)
    {
        auto Started = Application.Start(std::move(InitialScene));
        if (!Started) { return Started; }
        const auto Origin = std::chrono::steady_clock::now();
        while (Application.IsRunning())
        {
            const double Now = std::chrono::duration<double>(std::chrono::steady_clock::now() - Origin).count();
            auto Step = Application.Step(Now);
            if (!Step) { return TResult<void>::Failure(Step.Error()); }
            if (!Step.Value()) { break; }
        }
        Application.Shutdown();
        return {};
    }
};
}
