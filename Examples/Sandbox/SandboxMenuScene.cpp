// SPDX-License-Identifier: NOASSERTION
#include "SandboxMenuScene.h"
#include "SandboxGame.h"
#include "Dxf/AssetService.h"
#include "Dxf/SceneNavigator.h"
#include "Dxf/RenderContext.h"
namespace Dxf::Sandbox
{
namespace
{
// 更新・描画フックでの失敗をApplicationへ伝え、標準の終了処理に任せる。
void RequireSuccess(TResult<void> Result)
{
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
}
} // namespace
// Scene間では結果値だけを渡す。
ASandboxMenuScene::ASandboxMenuScene(bool bResult, Toolbox::f64 Seconds) : m_bResult(bResult), m_Seconds(Seconds)
{
}
// フォントのキャッシュと解放順序はAssetServiceへ任せる。
TResult<void> ASandboxMenuScene::OnInitialize(const FInitContext& Context)
{
	// 表示用フォントの取得結果。
	auto Font = Context.Assets.LoadFont();
	if (!Font)
	{
		return TResult<void>::Failure(Font.Error());
	}
	m_Font = Toolbox::Move(Font).Value();
	return {};
}
// Scene切替はNavigatorへの要求だけで行う。
void ASandboxMenuScene::OnTick(const FTickContext& Context)
{
	if (!Context.Scenes)
	{
		return;
	}
	// 遅延したScene準備の結果は、公開窓口から取得する。
	const auto& Error = Context.Scenes->GetLastTransitionError();
	m_Error = Error ? Error->Message : Toolbox::FString();
	if (Context.Input.WasPressed(EKey::Escape))
	{
		Context.Scenes->RequestQuit();
	}
	else if (Context.Input.WasPressed(EKey::Enter))
	{
		RequireSuccess(Context.Scenes->RequestChange<DSandboxScene>("Assets", false, true));
	}
	else if (Context.Input.WasPressed(EKey::Space))
	{
		if (m_bResult)
		{
			RequireSuccess(Context.Scenes->RequestChange<ASandboxMenuScene>());
		}
		else
		{
			RequireSuccess(Context.Scenes->RequestChange<DSandboxScene>());
		}
	}
}
// タイトルと結果は同じ案内表示を共有し、ゲーム用オブジェクトは生成しない。
void ASandboxMenuScene::OnDraw(FRenderContext& Render) const
{
	RequireSuccess(Render.Get2D().DrawText(m_Font, m_bResult ? "GOAL!" : "右端のゴールを目指そう", {80, 100}));
	RequireSuccess(Render.Get2D().DrawText(m_Font,
	                                       m_bResult ? "ENTER: リトライ   SPACE: タイトル   ESC: 終了"
	                                                 : "ENTER: スタート   SPACE: 機能サンプル   ESC: 終了",
	                                       {80, 160}));
	if (!m_Error.IsEmpty())
	{
		RequireSuccess(Render.Get2D().DrawText(m_Font, "開始失敗（ENTERで再試行）: " + m_Error, {80, 280}));
	}
	if (m_bResult)
	{
		// 整数のミリ秒で結果を示す。
		const Toolbox::FString Score =
		    "クリア時間: " + Toolbox::ToString(static_cast<Toolbox::int64>(m_Seconds * 1000)) + " ms";
		RequireSuccess(Render.Get2D().DrawText(m_Font, Score, {80, 220}));
	}
}
} // namespace Dxf::Sandbox
