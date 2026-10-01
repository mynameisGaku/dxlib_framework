// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_CONTENTSCENE3D_H
#define DXF_CONTENT_CONTENTSCENE3D_H
#include "Dxf/PhysicsScene3D.h"
#include "Dxf/PreparedScene3D.h"
#include "Dxf/PrefabInstance3D.h"
namespace Dxf
{
/**
 * 準備済み初期構成を既存PhysicsSceneへ生成する。独自の更新や解放順を作らない。
 */
class DContentScene3D : public DPhysicsScene3D
{
public:
	/**
	 * @param Prepared Scene要求の前に所有側で完成した必須資源。
	 * @param Settings 明示した場合だけ定義の固定更新設定を上書きする。View数で更新数を変えない。
	 */
	explicit DContentScene3D(FPreparedScene3D Prepared, Toolbox::TOptional<Toolbox::FFixedStepSettings> Settings = {});
	/**
	 * @param Id Sceneが宣言したインスタンスID。不在・未初期化・破棄済みは例外。
	 */
	TObjectHandle<DPrefabInstance3D> GetPrefab(Toolbox::FStringView Id) const;

	/**
	 * @param First 全画面または左View。
	 * @param Second 任意の右View。設定失敗なら旧表示条件を保持する。
	 */
	void SetViews(const FRenderView3D& First, const Toolbox::TOptional<FRenderView3D>& Second = {});

	/**
	 * 同じSceneへ準備済みPrefabを配置する。返却は受付でありReadyとは別。
	 * @param Id Scene内のASCII識別子。生存中の同名は拒否する。
	 * @param Prepared 所有側で準備済みの不変定義と資源。
	 * @param Placement 初期配置だけの剛体変換。
	 */
	TResult<TObjectHandle<DPrefabInstance3D>> SpawnPrefab(Toolbox::FString Id, FPreparedPrefab3D Prepared, FPrefabSpawnOptions3D Placement = {});

	/**
	 * @param Id Sceneに宣言した接続名。未初期化・不在・種類違い・失効は例外。
	 */
	TObjectHandle<DDistanceJoint3DComponent> GetDistanceConnection(Toolbox::FStringView Id) const;
	/**
	 * @param Id Sceneに宣言した接続名。未初期化・不在・種類違い・失効は例外。
	 */
	TObjectHandle<DRevoluteJoint3DComponent> GetRevoluteConnection(Toolbox::FStringView Id) const;
	/**
	 * @param Id Sceneに宣言した接続名。未初期化・不在・種類違い・失効は例外。
	 */
	TObjectHandle<DFixedJoint3DComponent> GetFixedConnection(Toolbox::FStringView Id) const;
	/**
	 * @param Id Sceneに宣言した接続名。未初期化・不在・種類違い・失効は例外。
	 */
	TObjectHandle<DPrismaticJoint3DComponent> GetPrismaticConnection(Toolbox::FStringView Id) const;

protected:
	/**
	 * Viewごとに表示だけを受付する。ObjectのTickやWorld.Stepは呼ばない。
	 * @param Render 現在の描画窓口。
	 */
	void OnDraw(FRenderContext& Render) const override;

	/**
	 * 親の初期化トランザクションで全PrefabをSpawnする。
	 * @param Context 所有側の既存初期化環境。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;

private:
	/**
	 * Native資源の解放はSceneの所有側で行う。
	 */
	FPreparedScene3D m_Prepared;
	/**
	 * 専任Joint Componentを持つCollection所有Objectへの非所有参照。
	 */
	TObjectHandle<DGameObject> m_Connections;
	/**
	 * 所有側で変更できる表示条件。初期化後も不変定義を書き換えない。
	 */
	Toolbox::TArray<FRenderView3D, 2> m_Views;
	/**
	 * 描画だけのView数。
	 */
	Toolbox::uint32 m_ViewCount = 1;

	/**
	 * Collectionが所有する個体の世代付き非所有参照。
	 */
	Toolbox::TVector<TObjectHandle<DPrefabInstance3D>> m_Instances;
	/**
	 * 動的配置も含む明示解決用ID。TickとDrawで文字列検索しない。
	 */
	Toolbox::TVector<Toolbox::FString> m_Ids;
};
} // namespace Dxf
#endif
