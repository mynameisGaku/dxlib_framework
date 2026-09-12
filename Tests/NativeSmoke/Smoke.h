#pragma once
#include "Dxf/Result.h"
#include <filesystem>
namespace Dxf::Testing
{
struct FNativeSmokeReport
{
	int Frames = 0;
	bool bJapanesePathTested = false;
	bool bAudioTested = false;
	bool bResourcesInvalidated = false;
};
/** Shared harness; only a binary linked to the real SDK constitutes a device test. */
TResult<FNativeSmokeReport> RunNativeSmoke(const std::filesystem::path& AssetsDirectory, bool bTestAudio);
}
