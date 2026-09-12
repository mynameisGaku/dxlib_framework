// SPDX-License-Identifier: NOASSERTION
#include "BootScene.h"

namespace Starter
{
Dxf::TResult<void> ABootScene::OnInitialize(const Dxf::FInitContext&)
{
	// 最初に必要な準備をここへ追加する。
	return {};
}

void ABootScene::OnTick(const Dxf::FTickContext&)
{
	// 入力に応じた移動など、ゲームの更新をここへ追加する。
}

void ABootScene::OnDraw(Dxf::FRenderContext&) const
{
	// 画像や図形の描画要求をここへ追加する。
}

void ABootScene::OnDeinitialize() noexcept
{
	// 自分で登録した処理など、終了時に解除するものをここへ追加する。
}
} // namespace Starter
