#pragma once
#include "Dxf/Result.h"
#include <string>
namespace Dxf
{
struct FWindowSettings
{
	std::string Title = "dxlib_framework";
	int Width = 1280;
	int Height = 720;
	bool bWindowed = true;
	bool bVSync = true;
};
class IPlatform
{
public:
	virtual ~IPlatform() = default;
	/** On failure the implementation must undo its own partial initialization. */
	virtual TResult<void> Initialize(const FWindowSettings& Settings) = 0;
	virtual void Shutdown() noexcept = 0;
	/** Success(false) requests normal termination. */
	virtual TResult<bool> PumpEvents() = 0;
};
}
