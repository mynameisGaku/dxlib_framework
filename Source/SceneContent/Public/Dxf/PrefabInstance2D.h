// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_PREFABINSTANCE2D_H
#define DXF_CONTENT_PREFABINSTANCE2D_H
#include "Dxf/GameObject.h"
#include "Dxf/ContentView2D.h"
#include "Dxf/RenderContext.h"
#include "Dxf/PreparedPrefab2D.h"
#include "Dxf/PrefabSpawnOptions2D.h"
#include "Dxf/PrefabInstanceState.h"
#include "Dxf/PhysicsBodyReference2D.h"
#include "Dxf/PrePhysicsStep.h"
#include "Dxf/PostPhysicsStep.h"
#include "Dxf/ContactListenerComponent2D.h"
#include "Dxf/DistanceJointComponent2D.h"
#include "Dxf/RevoluteJointComponent2D.h"
#include "Dxf/FixedJointComponent2D.h"
#include "Dxf/PrismaticJointComponent2D.h"
namespace Dxf
{
namespace ContentPrivate
{
class FPrefabRuntime2D;
}
/**
 * 既存Collectionに所有されるPrefab root。BodyとJointは既存Componentへ委譲する。
 */
class DPrefabInstance2D : public DGameObject, private IPrePhysicsStep, private IPostPhysicsStep
{
public:
	/**
	 * @param Prepared 所有側で準備した定義と資源。
	 * @param Placement 生成時だけ適用する配置変換。
	 */
	explicit DPrefabInstance2D(FPreparedPrefab2D Prepared, FPrefabSpawnOptions2D Placement = {});
	/**
	 * 子は既存の終了境界で先に終了する。即時deleteを要求しない。
	 */
	~DPrefabInstance2D() override;
	/**
	 * 受付・初期化・成功観察・外部破棄の状態を読み取る。
	 */
	EPrefabInstanceState GetState() const noexcept;
	/**
	 * 保持中の不変定義を返す。
	 */
	const FPrefabDefinition2D& GetDefinition() const noexcept;
	/**
	 * 型付き公開先を取得する。初期化前・失効・不存在・種類違いは例外。
	 * 未登録Bodyのhandleは登録待ちでありReadyを意味しない。
	 * @param Name Prefabが宣言した公開名。
	 */
	TObjectHandle<DRigidBody2DComponent> GetRigidBody(Toolbox::FStringView Name) const;
	/**
	 * 型付き公開先を取得する。初期化前・失効・不存在・種類違いは例外。
	 * 未登録Bodyのhandleは登録待ちでありReadyを意味しない。
	 * @param Name Prefabが宣言した公開名。
	 */
	TObjectHandle<DKinematicMover2DComponent> GetKinematicMover(Toolbox::FStringView Name) const;
	/**
	 * 型付き公開先を取得する。初期化前・失効・不存在・種類違いは例外。
	 * 未登録Bodyのhandleは登録待ちでありReadyを意味しない。
	 * @param Name Prefabが宣言した公開名。
	 */
	TObjectHandle<DDistanceJoint2DComponent> GetDistanceJoint(Toolbox::FStringView Name) const;
	/**
	 * 型付き公開先を取得する。初期化前・失効・不存在・種類違いは例外。
	 * 未登録Bodyのhandleは登録待ちでありReadyを意味しない。
	 * @param Name Prefabが宣言した公開名。
	 */
	TObjectHandle<DRevoluteJoint2DComponent> GetRevoluteJoint(Toolbox::FStringView Name) const;
	/**
	 * 型付き公開先を取得する。初期化前・失効・不存在・種類違いは例外。
	 * 未登録Bodyのhandleは登録待ちでありReadyを意味しない。
	 * @param Name Prefabが宣言した公開名。
	 */
	TObjectHandle<DFixedJoint2DComponent> GetFixedJoint(Toolbox::FStringView Name) const;
	/**
	 * 型付き公開先を取得する。初期化前・失効・不存在・種類違いは例外。
	 * 未登録Bodyのhandleは登録待ちでありReadyを意味しない。
	 * @param Name Prefabが宣言した公開名。
	 */
	TObjectHandle<DPrismaticJoint2DComponent> GetPrismaticJoint(Toolbox::FStringView Name) const;
	/**
	 * 型付き公開先を取得する。初期化前・失効・不存在・種類違いは例外。
	 * 未登録Bodyのhandleは登録待ちでありReadyを意味しない。
	 * @param Name Prefabが宣言した公開名。
	 */
	TObjectHandle<DContactListener2DComponent> GetContactListener(Toolbox::FStringView Name) const;
	/**
	 * @param Name 公開Sound cue。不在・型違い・失効は例外。
	 */
	const FSound& GetSound(Toolbox::FStringView Name) const;

	/**
	 * @param Name 公開Texture。資源を複製・再読込せず、不在・種類違い・失効は例外。
	 */
	const FTexture& GetTexture(Toolbox::FStringView Name) const;
	/**
	 * @param Name 公開Model。資源を複製・再読込せず、不在・種類違い・失効は例外。
	 */
	const FModel& GetModel(Toolbox::FStringView Name) const;
	/**
	 * @param Name 公開Font。資源を複製・再読込せず、不在・種類違い・失効は例外。
	 */
	const FFont& GetFont(Toolbox::FStringView Name) const;

	/**
	 * 補間Poseを既存描画APIへ渡す。物理・生成・モデル再生時刻は進めない。
	 * @param Render 現在の描画窓口。3Dでは呼出し側が指定したViewを使う。
	 * @param View 2Dのmと画素の対応、クリップ。
	 */
	TResult<void> DrawContent(FRenderContext& Render, const FContentView2D& View = {}) const;

protected:
	/**
	 * 個体別モデルを一回だけ進める。描画から呼ばない。
	 * @param Context 通常の可変更新。Pause中は再生を進めない。
	 */
	void OnTick(const FTickContext& Context) override;

	/**
	 * 親の初期化トランザクションで子を追加する。
	 * @param Context 所有側の資源サービス。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
	/**
	 * 登録後の観察を予約する。World.Stepは呼ばない。
	 * @param Context 既存固定更新の一回。
	 */
	void OnFixedTick(const FFixedTickContext& Context) override;
	/**
	 * 非所有予約は既存境界までに消費される。子の所有を奪わない。
	 */
	void OnDeinitialize() noexcept override;

private:
	void OnPrePhysicsStep_Internal(const FFixedTickContext& Context) override;
	void OnPostPhysicsStep_Internal(const FFixedTickContext& Context) override;
	bool IsPostPhysicsStepAlive_Internal() const noexcept override;
	/**
	 * 構成の非所有handle集合と不変定義。
	 */
	Toolbox::TUniquePtr<ContentPrivate::FPrefabRuntime2D> m_pRuntime;
};

} // namespace Dxf
#endif
