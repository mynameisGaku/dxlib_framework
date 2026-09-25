// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_CHARACTERS_H
#define DXF_GAMEPLAY_SAMPLE_CHARACTERS_H
#include "Dxf/CharacterMovementComponent2D.h"
#include "Dxf/CharacterMovementComponent3D.h"
#include "Dxf/GameObject.h"
namespace Dxf::GameplaySample
{
/**
 * 2Dのプレイヤー。入力を移動要求へ変え、DCharacterMovement2DComponentへ渡すだけ（物理計算はComponentが行う）。
 */
class DPlayer2D final : public DGameObject
{
public:
	/**
	 * 移動Componentを返す。初期化後だけ有効。
	 */
	DCharacterMovement2DComponent& GetCharacter() const;

protected:
	/**
	 * 移動Componentを追加する。
	 * @param Context 初期化の実行環境。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
	/**
	 * A/Dで左右、Spaceでジャンプを要求する。
	 * @param Context フレーム更新の実行環境。
	 */
	void OnTick(const FTickContext& Context) override;

private:
	/**
	 * 移動Component。
	 */
	TObjectHandle<DCharacterMovement2DComponent> m_Character;
};
/**
 * 3Dのプレイヤー。A/DでX、W/SでZ、Spaceでジャンプを要求する。
 */
class DPlayer3D final : public DGameObject
{
public:
	/**
	 * 移動Componentを返す。初期化後だけ有効。
	 */
	DCharacterMovement3DComponent& GetCharacter() const;

protected:
	/**
	 * 移動Componentを追加する。
	 * @param Context 初期化の実行環境。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
	/**
	 * 入力を移動要求へ変える。
	 * @param Context フレーム更新の実行環境。
	 */
	void OnTick(const FTickContext& Context) override;

private:
	/**
	 * 移動Component。
	 */
	TObjectHandle<DCharacterMovement3DComponent> m_Character;
};

/**
 * 2Dの歩行キャラクター。X∈[MinX, MaxX]の間を往復する（入力はこのオブジェクトが決める）。途中で生成・破棄できる。
 */
class DWalker2D final : public DGameObject
{
public:
	/**
	 * 開始位置と往復する範囲を受け取る。
	 * @param Start 開始時の中心。
	 * @param MinX 左端（これより左へ行ったら右へ向きを変える）。
	 * @param MaxX 右端。
	 */
	DWalker2D(Toolbox::FVector2 Start, Toolbox::f32 MinX, Toolbox::f32 MaxX) noexcept;
	/**
	 * 移動Componentを返す。初期化後だけ有効。
	 */
	DCharacterMovement2DComponent& GetCharacter() const;

protected:
	/**
	 * 移動Componentを追加する。
	 * @param Context 初期化の実行環境。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
	/**
	 * 範囲の端で向きを変え、移動要求を渡す。
	 * @param Context フレーム更新の実行環境。
	 */
	void OnTick(const FTickContext& Context) override;

private:
	/**
	 * 移動Component。
	 */
	TObjectHandle<DCharacterMovement2DComponent> m_Character;
	/**
	 * 開始時の中心。
	 */
	Toolbox::FVector2 m_Start;
	/**
	 * 左端。
	 */
	Toolbox::f32 m_MinX;
	/**
	 * 右端。
	 */
	Toolbox::f32 m_MaxX;
	/**
	 * 進む向き（+1または-1）。
	 */
	Toolbox::f32 m_Direction = 1;
};
/**
 * 3Dの歩行キャラクター。開始位置のZのまま、X∈[MinX, MaxX]の間を往復する。
 */
class DWalker3D final : public DGameObject
{
public:
	/**
	 * 開始位置と往復する範囲を受け取る。
	 * @param Start 開始時の中心。
	 * @param MinX 左端。
	 * @param MaxX 右端。
	 */
	DWalker3D(Toolbox::FVector3 Start, Toolbox::f32 MinX, Toolbox::f32 MaxX) noexcept;
	/**
	 * 移動Componentを返す。初期化後だけ有効。
	 */
	DCharacterMovement3DComponent& GetCharacter() const;

protected:
	/**
	 * 移動Componentを追加する。
	 * @param Context 初期化の実行環境。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
	/**
	 * 範囲の端で向きを変え、移動要求を渡す。
	 * @param Context フレーム更新の実行環境。
	 */
	void OnTick(const FTickContext& Context) override;

private:
	/**
	 * 移動Component。
	 */
	TObjectHandle<DCharacterMovement3DComponent> m_Character;
	/**
	 * 開始時の中心。
	 */
	Toolbox::FVector3 m_Start;
	/**
	 * 左端。
	 */
	Toolbox::f32 m_MinX;
	/**
	 * 右端。
	 */
	Toolbox::f32 m_MaxX;
	/**
	 * 進む向き（+1または-1）。
	 */
	Toolbox::f32 m_Direction = 1;
};
} // namespace Dxf::GameplaySample
#endif
