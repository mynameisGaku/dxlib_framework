// SPDX-License-Identifier: NOASSERTION
#ifndef SANDBOX_MENU_SCENE_H
#define SANDBOX_MENU_SCENE_H
#include "Dxf/Scene.h"
#include "Dxf/Font.h"
namespace Dxf::Sandbox
{
/**
 * 小さいゲームのタイトルと結果画面。結果値だけを受け取り、プレイ中の資源を持ち越さない。
 */
class ASandboxMenuScene final : public DScene
{
public:
	/**
	 * 表示する画面を選ぶ。
	 * @param bResult trueなら結果、falseならタイトル。
	 * @param Seconds ゴールまでのゲーム内経過秒数。
	 */
	explicit ASandboxMenuScene(bool bResult = false, Toolbox::f64 Seconds = 0);

protected:
	/**
	 * 表示用フォントを取得する。読込失敗はSceneの開始失敗として返す。
	 * @param Context アセットの窓口。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
	/**
	 * 開始・リトライ・タイトル復帰・終了の入力を処理する。
	 * @param Context 入力とScene切替の窓口。
	 */
	void OnTick(const FTickContext& Context) override;
	/**
	 * 操作案内と結果を描く。
	 * @param Render このフレームの描画窓口。
	 */
	void OnDraw(FRenderContext& Render) const override;

private:
	/**
	 * 結果画面として表示するか。
	 */
	bool m_bResult;
	/**
	 * 前のプレイから受け取った秒数。
	 */
	Toolbox::f64 m_Seconds;
	/**
	 * このSceneが保持する共有フォント。
	 */
	FFont m_Font;
	/**
	 * 前回の開始失敗。元の画面に残って再試行できることを案内する。
	 */
	Toolbox::FString m_Error;
};
} // namespace Dxf::Sandbox
#endif
