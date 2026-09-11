#pragma once
#include "Dxf/GameScene.h"
#include "Dxf/GameInstance.h"
#include "Dxf/Texture.h"
#include "Dxf/Font.h"
#include "Dxf/Sound.h"
#include "Dxf/MathTypes.h"
namespace Dxf::Sandbox
{
/** Game-specific state; no renderer or input system is owned here. */
class DSandboxGameInstance final : public DGameInstance
{
public:
	void RecordSceneVisit() noexcept
	{
		++m_SceneVisits;
	}
	int GetSceneVisits() const noexcept
	{
		return m_SceneVisits;
	}
private:
	int m_SceneVisits = 0;
};
class DPlayer final : public DGameObject
{
public:
	explicit DPlayer(FTexture Texture);
	const FVector2& GetPosition() const noexcept
	{
		return m_Position;
	}
protected:
	TResult<void> OnInitialize(const FInitContext&) override;
	void OnTick(const FTickContext& Context) override;
private:
	FTexture m_Texture;
	FVector2 m_Position{320, 240};
};
class DSandboxScene final : public DGameScene
{
public:
	explicit DSandboxScene(std::string AssetRoot, bool bAlternate = false);
	TObjectHandle<DPlayer> GetPlayer() const noexcept
	{
		return m_Player;
	}
protected:
	TResult<void> OnInitialize(const FInitContext& Context) override;
	void OnEnter(const FSceneActivationContext& Context) noexcept override;
	void OnTick(const FTickContext& Context) override;
	void OnDraw(FRenderContext& Render) const override;
private:
	std::string m_AssetRoot;
	FFont m_Font;
	FSound m_Sound;
	TObjectHandle<DPlayer> m_Player;
	int m_VisitCount = 0;
	bool m_bAlternate = false;
};
}
