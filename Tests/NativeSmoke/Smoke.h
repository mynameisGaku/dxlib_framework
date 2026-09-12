#pragma once
#include "Dxf/Result.h"
#include "Toolbox/Platform.h"
namespace Dxf::Testing
{
/**
 * 実機スモーク検証で確認できた機能の記録。
 */
struct FNativeSmokeReport
{
	/**
	 * 完了したスモーク検証のフレーム数。
	 */
	Toolbox::int32 Frames = 0;
	/**
	 * 日本語を含むパスでの読み込みを確認したか。
	 */
	bool bJapanesePathTested = false;
	/**
	 * 実機の音声再生を確認したか。
	 */
	bool bAudioTested = false;
	/**
	 * 終了時に資源参照が失効したか。
	 */
	bool bResourcesInvalidated = false;
};
/**
 * Shared harness; only a binary linked to the real SDK constitutes a device test.
 */
TResult<FNativeSmokeReport> RunNativeSmoke(const Toolbox::FPath& AssetsDirectory, bool bTestAudio);
} // namespace Dxf::Testing
