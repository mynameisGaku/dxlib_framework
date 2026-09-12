#include "SandboxGame.h"
#include "Dxf/GameObjectComponent.h"
#include "Dxf/AssetService.h"
#include "Dxf/AudioPlayer.h"
#include "Dxf/SceneNavigator.h"
#include "Dxf/RenderContext.h"
#include "Toolbox/Algorithm.h"
#include "Toolbox/Utility.h"
namespace Dxf::Sandbox
{
namespace
{
/**
 * 処理の失敗を例外として呼び出し元へ伝える。
 * @param Result 処理結果。
 */
void RequireSuccess_Internal(TResult<void> Result)
{
	/**
	 * FApplicationが利用者フックの例外を捕捉し、順序どおり後始末する。
	 */
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
}
/**
 * 所有者の位置を参照してスプライトを描画する。
 */
class DSpriteComponent final : public DGameObjectComponent
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Texture 描画するテクスチャ。
	 * @param Position 描画位置。
	 */
	DSpriteComponent(FTexture Texture, const FVector2& Position)
	    : m_Texture(Toolbox::Move(Texture)), m_pPosition(&Position)
	{
	}

protected:
	/**
	 * 現在の状態を描画する。
	 * @param Render 現在の描画コンテキスト。
	 */
	void OnDraw(FRenderContext& Render) const override
	{
		/**
		 * 処理に適用する設定。
		 */
		FSpriteDrawOptions Options;
		Options.Pivot = {32, 32};
		Options.Layer = 10;
		RequireSuccess_Internal(Render.Draw(m_Texture, *m_pPosition, Options));
	}

private:
	/**
	 * 描画するテクスチャ。
	 */
	FTexture m_Texture;
	/**
	 * 描画位置。
	 */
	const FVector2* m_pPosition;
};
} // namespace
/**
 * 必要な依存関係を受け取り、初期状態を構築する。
 */
DPlayer::DPlayer(FTexture Texture) : m_Texture(Toolbox::Move(Texture))
{
}
/**
 * 派生型固有の初期化を行う。
 */
TResult<void> DPlayer::OnInitialize(const FInitContext&)
{
	/**
	 * スプライトのコンポーネント。
	 */
	auto Sprite = AddComponent<DSpriteComponent>(m_Texture, m_Position);
	return Sprite ? TResult<void>{} : TResult<void>::Failure(Sprite.Error());
}
/**
 * 現在のフレーム情報で状態を更新する。
 * @param Context 処理に必要な実行環境。
 */
void DPlayer::OnTick(const FTickContext& Context)
{
	/**
	 * 左右の移動方向。
	 */
	Toolbox::f32 X = static_cast<Toolbox::f32>(Context.Input.IsDown(EKey::D)) -
	                 static_cast<Toolbox::f32>(Context.Input.IsDown(EKey::A));
	/**
	 * 上下の移動方向。
	 */
	Toolbox::f32 Y = static_cast<Toolbox::f32>(Context.Input.IsDown(EKey::S)) -
	                 static_cast<Toolbox::f32>(Context.Input.IsDown(EKey::W));
	/**
	 * ベクトルまたは文字列の長さ。
	 */
	const Toolbox::f32 Length = Toolbox::Sqrt(X * X + Y * Y);
	if (Length > 0)
	{
		X /= Length;
		Y /= Length;
	}
	/**
	 * このフレームで移動する距離。
	 */
	const Toolbox::f32 Distance = static_cast<Toolbox::f32>(Context.Time.DeltaSeconds) * 280.0f;
	m_Position.X = Toolbox::Clamp(m_Position.X + X * Distance, 32.0f, 1248.0f);
	m_Position.Y = Toolbox::Clamp(m_Position.Y + Y * Distance, 180.0f, 688.0f);
}
/**
 * 必要な依存関係を受け取り、初期状態を構築する。
 * @param AssetRoot アセットの基準ディレクトリ。
 * @param bAlternate 代替シーンの配色を使用するか。
 */
DSandboxScene::DSandboxScene(Toolbox::FString AssetRoot, bool bAlternate)
    : m_AssetRoot(Toolbox::Move(AssetRoot)), m_bAlternate(bAlternate)
{
}
/**
 * 派生型固有の初期化を行う。
 * @param Context 処理に必要な実行環境。
 */
TResult<void> DSandboxScene::OnInitialize(const FInitContext& Context)
{
	/**
	 * 描画するテクスチャ。
	 */
	auto Texture = Context.Assets.LoadTexture(m_AssetRoot + "/player.bmp");
	if (!Texture)
	{
		return TResult<void>::Failure(Texture.Error());
	}
	/**
	 * 文字描画に使うフォント。
	 */
	auto Font = Context.Assets.LoadFont();
	if (!Font)
	{
		return TResult<void>::Failure(Font.Error());
	}
	m_Font = Toolbox::Move(Font).Value();
	/**
	 * 再生する音声。
	 */
	auto Sound = Context.Assets.LoadSound(m_AssetRoot + "/confirm.wav");
	if (!Sound)
	{
		return TResult<void>::Failure(Sound.Error());
	}
	m_Sound = Toolbox::Move(Sound).Value();
	/**
	 * プレイヤーのハンドル。
	 */
	auto Player = Spawn<DPlayer>(Toolbox::Move(Texture).Value());
	if (!Player)
	{
		return TResult<void>::Failure(Player.Error());
	}
	m_Player = Player.Value();
	return {};
}
/**
 * シーンが有効になったときの処理を行う。
 * @param Context 処理に必要な実行環境。
 */
void DSandboxScene::OnEnter(const FSceneActivationContext& Context) noexcept
{
	/**
	 * シーン間で共有するゲーム状態を取得して有効性を確認する。
	 */
	if (auto* Game = Context.Game ? Context.Game->TryCast<DSandboxGameInstance>() : nullptr)
	{
		Game->RecordSceneVisit();
		m_VisitCount = Game->GetSceneVisits();
	}
}
/**
 * 現在のフレーム情報で状態を更新する。
 * @param Context 処理に必要な実行環境。
 */
void DSandboxScene::OnTick(const FTickContext& Context)
{
	if (Context.Input.WasPressed(EKey::Escape) && Context.Scenes)
	{
		Context.Scenes->RequestQuit();
		return;
	}
	if (Context.Input.WasPressed(EKey::P))
	{
		GetClock().SetPaused(!GetClock().IsPaused());
	}
	if (Context.Input.WasPressed(EKey::Enter) && Context.Scenes)
	{
		RequireSuccess_Internal(Context.Scenes->RequestChange<DSandboxScene>(m_AssetRoot, !m_bAlternate));
	}
	if (Context.Input.WasPressed(EKey::Space) && Context.Audio)
	{
		/**
		 * 処理に適用する設定。
		 */
		FPlaybackOptions Options;
		Options.Volume = 0.35f;
		Options.Scope = Context.AudioScope;
		/**
		 * 開始した音声の再生ハンドル。
		 */
		auto Voice = Context.Audio->Play(m_Sound, Options);
		if (!Voice)
		{
			throw Toolbox::FException(Voice.Error().Message);
		}
	}
}
/**
 * 現在の状態を描画する。
 * @param Render 現在の描画コンテキスト。
 */
void DSandboxScene::OnDraw(FRenderContext& Render) const
{
	/**
	 * 背景の描画設定または設定結果。
	 */
	FDrawStyle Background;
	Background.Color = m_bAlternate ? FColor{30, 30, 30, 255} : FColor{12, 12, 12, 255};
	Background.Layer = -10;
	RequireSuccess_Internal(Render.FillRectangle({0, 160, 1280, 720}, Background));
	/**
	 * 描画する文字列。
	 */
	FDrawStyle Text;
	Text.Layer = 100;
	RequireSuccess_Internal(Render.DrawText(m_Font, "dxlib_framework / Sandbox", {24, 20}, Text));
	RequireSuccess_Internal(
	    Render.DrawText(m_Font, "WASD: 移動  SPACE: 効果音  P: ポーズ  ENTER: シーン切替  ESC: 終了", {24, 56}, Text));
	/**
	 * 画面へ表示する状態の文字列。
	 */
	const Toolbox::FString Status =
	    "シーン訪問回数: " + Toolbox::ToString(m_VisitCount) + (GetClock().IsPaused() ? "  [PAUSED]" : "");
	RequireSuccess_Internal(Render.DrawText(m_Font, Status, {24, 92}, Text));
}
} // namespace Dxf::Sandbox
