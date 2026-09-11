#include "SandboxGame.h"
#include "Dxf/GameObjectComponent.h"
#include "Dxf/AssetService.h"
#include "Dxf/AudioPlayer.h"
#include "Dxf/SceneNavigator.h"
#include "Dxf/RenderContext.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace Dxf::Sandbox
{
namespace
{
void RequireSuccess_Internal(TResult<void> Result)
{
    // FApplication catches user-hook exceptions and performs ordered cleanup.
    if (!Result) { throw std::runtime_error(Result.Error().Message); }
}
class DSpriteComponent final : public DGameObjectComponent
{
public:
    DSpriteComponent(FTexture Texture, const FVector2& Position)
        : m_Texture(std::move(Texture)), m_pPosition(&Position)
    {
    }
protected:
    void OnDraw(FRenderContext& Render) const override
    {
        FSpriteDrawOptions Options;
        Options.Pivot = {32, 32};
        Options.Layer = 10;
        RequireSuccess_Internal(Render.Draw(m_Texture, *m_pPosition, Options));
    }
private:
    FTexture m_Texture;
    const FVector2* m_pPosition;
};
}
DPlayer::DPlayer(FTexture Texture) : m_Texture(std::move(Texture))
{
}
TResult<void> DPlayer::OnInitialize(const FInitContext&)
{
    auto Sprite = AddComponent<DSpriteComponent>(m_Texture, m_Position);
    return Sprite ? TResult<void>{} : TResult<void>::Failure(Sprite.Error());
}
void DPlayer::OnTick(const FTickContext& Context)
{
    float X = static_cast<float>(Context.Input.Down(EKey::D)) - static_cast<float>(Context.Input.Down(EKey::A));
    float Y = static_cast<float>(Context.Input.Down(EKey::S)) - static_cast<float>(Context.Input.Down(EKey::W));
    const float Length = std::sqrt(X * X + Y * Y);
    if (Length > 0) { X /= Length; Y /= Length; }
    const float Distance = static_cast<float>(Context.Time.DeltaSeconds) * 280.0f;
    m_Position.X = std::clamp(m_Position.X + X * Distance, 32.0f, 1248.0f);
    m_Position.Y = std::clamp(m_Position.Y + Y * Distance, 180.0f, 688.0f);
}
DSandboxScene::DSandboxScene(std::string AssetRoot, bool bAlternate)
    : m_AssetRoot(std::move(AssetRoot)), m_bAlternate(bAlternate)
{
}
TResult<void> DSandboxScene::OnInitialize(const FInitContext& Context)
{
    auto Texture = Context.Assets.LoadTexture(m_AssetRoot + "/player.bmp");
    if (!Texture) { return TResult<void>::Failure(Texture.Error()); }
    auto Font = Context.Assets.LoadFont();
    if (!Font) { return TResult<void>::Failure(Font.Error()); }
    m_Font = std::move(Font).Value();
    auto Sound = Context.Assets.LoadSound(m_AssetRoot + "/confirm.wav");
    if (!Sound) { return TResult<void>::Failure(Sound.Error()); }
    m_Sound = std::move(Sound).Value();
    auto Player = Spawn<DPlayer>(std::move(Texture).Value());
    if (!Player) { return TResult<void>::Failure(Player.Error()); }
    m_Player = Player.Value();
    return {};
}
void DSandboxScene::OnEnter(const FSceneActivationContext& Context) noexcept
{
    if (auto* Game = Context.Game ? Context.Game->TryCast<DSandboxGameInstance>() : nullptr)
    {
        Game->RecordSceneVisit();
        m_VisitCount = Game->GetSceneVisits();
    }
}
void DSandboxScene::OnTick(const FTickContext& Context)
{
    if (Context.Input.Pressed(EKey::Escape) && Context.Scenes) { Context.Scenes->RequestQuit(); return; }
    if (Context.Input.Pressed(EKey::P)) { GetClock().SetPaused(!GetClock().IsPaused()); }
    if (Context.Input.Pressed(EKey::Enter) && Context.Scenes)
    {
        RequireSuccess_Internal(Context.Scenes->RequestChange<DSandboxScene>(m_AssetRoot, !m_bAlternate));
    }
    if (Context.Input.Pressed(EKey::Space) && Context.Audio)
    {
        FPlaybackOptions Options;
        Options.Volume = 0.35f;
        Options.Scope = Context.AudioScope;
        auto Voice = Context.Audio->Play(m_Sound, Options);
        if (!Voice) { throw std::runtime_error(Voice.Error().Message); }
    }
}
void DSandboxScene::OnDraw(FRenderContext& Render) const
{
    FDrawStyle Background;
    Background.Color = m_bAlternate ? FColor{30, 30, 30, 255} : FColor{12, 12, 12, 255};
    Background.Layer = -10;
    RequireSuccess_Internal(Render.FillRectangle({0, 160, 1280, 720}, Background));
    FDrawStyle Text;
    Text.Layer = 100;
    RequireSuccess_Internal(Render.DrawText(m_Font, "dxlib_framework / Sandbox", {24, 20}, Text));
    RequireSuccess_Internal(Render.DrawText(m_Font, "WASD: 移動  SPACE: 効果音  P: ポーズ  ENTER: シーン切替  ESC: 終了", {24, 56}, Text));
    const std::string Status = "シーン訪問回数: " + std::to_string(m_VisitCount) + (GetClock().IsPaused() ? "  [PAUSED]" : "");
    RequireSuccess_Internal(Render.DrawText(m_Font, Status, {24, 92}, Text));
}
}
