// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_INPUT_ROUTER_H
#define DXF_INPUT_ROUTER_H
#include "Dxf/Contexts.h"
namespace Dxf
{
/**
 * Sceneの更新の前に、そのフレームの入力を一度だけ受け取り、Scene・子のGameObject・Component・固定更新へ渡す入力を返す汎用の窓口。
 * UIの仲介（UIが使った操作をゲームへ二重に届けない）などに使う。Runtimeは具体的なUIを知らない。
 * 返す入力は、次のRouteInputの呼出しまで有効であること。
 */
class IInputRouter
{
public:
	virtual ~IInputRouter() = default;
	/**
	 * そのフレームの入力を処理し、Sceneへ渡す入力を返す。Sceneのポーズ中も呼ぶ（実時間はContext.Time.UnscaledDeltaSeconds）。
	 * @param Context Sceneの更新の情報（Inputは入力の事実）。
	 */
	virtual const FInputSnapshot& RouteInput(const FTickContext& Context) = 0;
};
} // namespace Dxf
#endif
