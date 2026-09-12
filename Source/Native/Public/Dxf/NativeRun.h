#pragma once
#include "Toolbox/UniquePtr.h"
#include "Dxf/NativeBackends.h"
#include "Dxf/AppRunner.h"
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * 標準構成で起動する窓口。独自のGameInstanceにはFApplicationを直接使用する。
 * @param Settings 初期化に使用する設定。
 * @param Args 生成先へ転送する引数。
 */
template <typename TScene, typename... TArgs> TResult<void> Run(FApplicationSettings Settings, TArgs&&... Args)
{
	static_assert(Toolbox::IsBaseOf<DScene, TScene>);
	// 各ネイティブ機能の実装。
	FDxLibBackends Backends;
	// サービスを結合した実行用のアプリケーション。
	FApplication Application(Backends.GetServices(), Toolbox::Move(Settings));
	// アプリケーションの実行器。
	FAppRunner Runner;
	return Runner.Run(Application, Toolbox::MakeUnique<TScene>(Toolbox::Forward<TArgs>(Args)...));
}
} // namespace Dxf
