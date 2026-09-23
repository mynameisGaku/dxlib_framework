# 採取済みPhysics SnapshotのCollider選択

`Dxf/PhysicsDebugPicking3D.h`の`PickPhysicsDebugSnapshot3D`は、表示用Snapshotと有限線分だけを受け取るDebug層の問い合わせ。`dxf::debug_tools`にリンクする。World、Native、GPUへ問い合わせず、入力値を編集しない。

```cpp
const auto Ray = Dxf::MakeViewPickSegment(View, Width, Height, MousePosition);
if (Ray && Ray.Value())
{
    const auto Pick = Dxf::PickPhysicsDebugSnapshot3D(DisplayedSnapshot, *Ray.Value());
    if (Pick && Pick.Value())
    {
        const Dxf::FColliderId3D Id = Pick.Value()->Collider;
        // Id.Body.World / Index / GenerationとId.Index / Generationを合わせて識別する。
    }
    // !Pickは入力・計算エラー、Pickが成功して値が空なら非交差。
}
```

## 入力・結果・制限

- 入力Shapeは既にワールド座標。ローカル中心やBody姿勢を二重適用しない。既存の球/OBB `IntersectSegment`を利用し、最小の割合を選ぶ。同距離はItemsで先の要素を優先する。
- `FPhysicsDebugPick3D`はCollider ID、採取Step、始点0～終点1のFraction、ワールド交点Positionを値で保持。生ポインタを保持しない。始点内部は0、端点・接触を含む。近遠面の外は線分外として非交差。
- World=0・空Items・BodyCount=0の未採取値は通常の非交差。不正線分は空Snapshotでも失敗する。
- 既存`IsValidPhysicsDebugSnapshot3D`の値域・形状・World一致・世代非0・件数検査を使い、さらにColliderスロット重複、同じBodyスロットの異なる世代、BodyCount不足を拒否する。Indexは疎なスロット番号なのでItemsの添字範囲へ制限しない。
- 不正な後続要素や計算不能を、手前の候補が見つかったことを理由に無視しない。例外はInvalidArgumentのTResult失敗へ変換する。返すFractionは有限な[0,1]、Positionは有限な既存f32表現。
- 最大256 Collider、既存Body上限4096。ID整合性検査O(n²)（最大32640組）、形状交差O(n)、追加記憶O(1)。Snapshot複写・並べ替え・BVH・常設索引なし。性能計測による保証ではない。
- World破棄後の保存Snapshotにも利用できる。IDは採取当時の情報であり、Live Worldでの生存・編集可能性を保証しない。Step番号や履歴AgeだけでもSnapshotを一意に識別できない。

## RenderDebugの操作

通常の更新/Step → 既存Recorder → 表示履歴の確定 → ビュー準備 → 同じSnapshotで選択 → 同じビュー/形状を描画、の順。クリックに伴う追加Step・再採取・形状登録はない。

左クリックで手前を選び、非交差・Viewport外では解除。選択形状を黄色にし、重心に印と`Collider COM`を置く。画面下にLIVE/HISTORY、採取Step、World、Body/ColliderのIndex:Generation、採取時の重心・速度・休止状態を表示する。Shapeの内容を上書きして強調しない。

重心はCollider中心/交点と異なる場合がある。投影の成功とbInsideViewを確認し、印・ラベルは観察用2Dとして遮蔽物越しにも表示する。クリック時の交点を以後の最新交点として表示しない。

- Liveは完全な世代付きIDが表示Snapshotに残る間だけ選択を保持。表示値は新しいSnapshotから読む。Body/Collider世代・Worldが異なるものへ引き継がない。
- Z/Xの履歴切替、Live/履歴切替、F8、EnterのWorld再生成、Scene終了で解除。同じ履歴表示ではその保存値を選び、Live Worldを再照会しない。
- F8 OFFは最新観察値と選択を消し、保存履歴を保持。Z/Xやクリックで観察を再開しない。F8 ONも古い選択を復活させない。
- 可視の説明パネル`[0,1280)×[0,208)`、F9の2D観察領域`[950,1270)×[520,710)`、選択中の詳細欄`[0,950)×[650,720)`は3Dクリックへ渡さない。これらの上では既存選択を変更しない。Tabで説明を閉じれば隠れていた形状を選べる。
- 右ドラッグ・移動・停止P・手送りN・F8/F9等の既存操作を維持する。表示先は既存の1280×720。新しい分割表示モードは追加しない。

サンプルに追加した読取入口（GetDisplaySnapshot/GetDisplayView/GetPickedCollider/GetSimulationSeconds/GetHistoryCount）は同じ実Sceneを試験するためにも利用する。フレームワークへInspector/ManagerやWorld編集APIは追加していない。

## 範囲と検証

これは保存Colliderへの観察用交差問い合わせ。Live World全体のRaycast、モデル三角形・透明画素・全描画物の可視性を判定するAPIではない。2D版選択や物理への書戻しは含まない。

[検証記録](../Development/PhysicsSnapshotPicking-2026-09-23.md) / [Debug利用手順](DebugTools.md) / [座標変換](ViewCoordinates.md) / [Physics Snapshot](../Physics/Snapshots.md)
