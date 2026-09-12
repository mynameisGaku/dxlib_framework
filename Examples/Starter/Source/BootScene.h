// SPDX-License-Identifier: NOASSERTION
#ifndef STARTER_BOOT_SCENE_H
#define STARTER_BOOT_SCENE_H

#include "Dxf/GameScene.h"

namespace Starter
{
/**
 * ゲームを書き始めるための空のシーン。起動時に最初に表示する。
 */
class ABootScene final : public Dxf::DGameScene
{
protected:
	/**
	 * 起動時の準備を書く。失敗した場合はFailureを返す。
	 * @param Context 画像や音声などの読み込み窓口。
	 */
	Dxf::TResult<void> OnInitialize(const Dxf::FInitContext& Context) override;
	/**
	 * 毎フレームのゲーム処理を書く。
	 * @param Context 入力、経過時間、シーン切り替えなどの情報。
	 */
	void OnTick(const Dxf::FTickContext& Context) override;
	/**
	 * 描画したい内容を書く。
	 * @param Context このフレームの描画要求を受け取る窓口。
	 */
	void OnDraw(Dxf::FRenderContext& Context) const override;
	/**
	 * 終了時の後始末を書く。初期化が途中で失敗した場合にも呼ばれる。
	 */
	void OnDeinitialize() noexcept override;
};
} // namespace Starter

#endif
