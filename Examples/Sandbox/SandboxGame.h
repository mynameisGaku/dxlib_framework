#pragma once
#include "Dxf/GameScene.h"
#include "Dxf/GameInstance.h"
#include "Dxf/Texture.h"
#include "Dxf/Font.h"
#include "Dxf/Sound.h"
#include "Dxf/MathTypes.h"
namespace Dxf::Sandbox
{
/**
 * ゲーム固有の状態を保持する。描画器や入力システムは所有しない。
 */
class DSandboxGameInstance final : public DGameInstance
{
public:
	/**
	 * シーンを訪れた回数を加算する。
	 */
	void RecordSceneVisit() noexcept
	{
		++m_SceneVisits;
	}
	/**
	 * 累積のシーン訪問回数を取得する。
	 */
	Toolbox::int32 GetSceneVisits() const noexcept
	{
		return m_SceneVisits;
	}

private:
	/**
	 * 累積のシーン訪問回数。
	 */
	Toolbox::int32 m_SceneVisits = 0;
};
/**
 * 入力に応じて移動するサンプルプレイヤー。
 */
class DPlayer final : public DGameObject
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Texture 描画するテクスチャ。
	 */
	explicit DPlayer(FTexture Texture);
	/**
	 * 描画位置を取得する。
	 */
	const FVector2& GetPosition() const noexcept
	{
		return m_Position;
	}

protected:
	/**
	 * 派生型固有の初期化を行う。
	 */
	TResult<void> OnInitialize(const FInitContext&) override;
	/**
	 * 現在のフレーム情報で状態を更新する。
	 * @param Context 処理に必要な実行環境。
	 */
	void OnTick(const FTickContext& Context) override;

private:
	/**
	 * 描画するテクスチャ。
	 */
	FTexture m_Texture;
	/**
	 * 描画位置。
	 */
	FVector2 m_Position{320, 240};
};
/**
 * 入力・描画・音声・シーン遷移を示すサンプルシーン。
 */
class DSandboxScene final : public DGameScene
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param AssetRoot アセットの基準ディレクトリ。
	 * @param bAlternate 代替シーンの配色を使用するか。
	 */
	explicit DSandboxScene(Toolbox::FString AssetRoot, bool bAlternate = false);
	/**
	 * プレイヤーのハンドルを取得する。
	 */
	TObjectHandle<DPlayer> GetPlayer() const noexcept
	{
		return m_Player;
	}

protected:
	/**
	 * 派生型固有の初期化を行う。
	 * @param Context 処理に必要な実行環境。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
	/**
	 * シーンが有効になったときの処理を行う。
	 * @param Context 処理に必要な実行環境。
	 */
	void OnEnter(const FSceneActivationContext& Context) noexcept override;
	/**
	 * 現在のフレーム情報で状態を更新する。
	 * @param Context 処理に必要な実行環境。
	 */
	void OnTick(const FTickContext& Context) override;
	/**
	 * 現在の状態を描画する。
	 * @param Render 現在の描画コンテキスト。
	 */
	void OnDraw(FRenderContext& Render) const override;

private:
	/**
	 * アセットの基準ディレクトリ。
	 */
	Toolbox::FString m_AssetRoot;
	/**
	 * 文字描画に使うフォント。
	 */
	FFont m_Font;
	/**
	 * 再生する音声。
	 */
	FSound m_Sound;
	/**
	 * プレイヤーのハンドル。
	 */
	TObjectHandle<DPlayer> m_Player;
	/**
	 * シーンを訪れた回数。
	 */
	Toolbox::int32 m_VisitCount = 0;
	/**
	 * 代替シーンの配色を使うか。
	 */
	bool m_bAlternate = false;
};
} // namespace Dxf::Sandbox
