# 現在のPhysics Worldへの範囲問い合わせ（OverlapAll）

`FPhysicsWorld2D::OverlapAll`（`Dxf/RigidBody2D.h`、範囲は円）と`FPhysicsWorld3D::OverlapAll`（`Dxf/RigidBody3D.h`、範囲は球）は、範囲と重なる（接触を含む）現在の全ColliderのIDを返します。リンク先は `dxf::physics` だけです。Input・Camera・Support・Debug・Nativeは不要です。

「近くに何があるか」から始める処理（近くの拾得物、爆発範囲に触れる対象、周囲のキャラクター候補）に使います。調べる方向と終点が決まっている処理は[線分問い合わせ（RaycastClosest）](WorldSegmentQuery.md)を使います。半径のある円／球を移動させる途中の最初の接触は[スイープ問い合わせ（SweepClosest）](WorldSweepQuery.md)を使います。

## 呼出し

```cpp
#include "Dxf/RigidBody2D.h"

// 2D物理ワールド座標（メートル、Y上向き）。ピクセルではない。
const Toolbox::FCircle2D Area{Eye, 8.0f};
Dxf::FWorldQueryFilter Characters;
Characters.IncludeCategories = CharacterCategory;

const auto Candidates = World.OverlapAll(Area, SelfBody, Characters); // 自己Bodyを除く
const auto Pickups = World.OverlapAll(Area, {}, PickupFilter);        // 自己除外なし
const auto All = World.OverlapAll(Area);                               // 既定カテゴリ条件で全候補
```

3Dは `Toolbox::FSphere` を渡します（`World.OverlapAll(Toolbox::FSphere{Eye, 8.0f}, SelfBody, Characters)`）。

| 項目 | 2D | 3D |
|---|---|---|
| 宣言 | `TVector<FColliderId2D> OverlapAll(const FCircle2D& Area, TOptional<FBodyId2D> ExcludedBody = {}, const FWorldQueryFilter& Filter = {}) const` | `TVector<FColliderId3D> OverlapAll(const FSphere& Area, TOptional<FBodyId3D> ExcludedBody = {}, const FWorldQueryFilter& Filter = {}) const` |
| 調べる形状 | 円・回転矩形 | 球・OBB |
| 形状判定 | 円同士: `Toolbox::Intersects(円, 円, 0)`、円と回転矩形: `Toolbox::Intersects(円, 回転矩形, 0)`（`Contact2D.h`） | `Toolbox::IntersectsSphere(球, 球／OBB, 0)`（`CollisionShapes.h`。`Intersects`の球専用経路と同じ距離計算） |

## 範囲との重なりと、射線の遮蔽は別

- 範囲の中に重心があるかではなく、範囲と形状が接しているかを調べます。重心が範囲外の大きいColliderも、表面が範囲に入れば対象です。逆に、重なったBodyの重心が範囲内やCollider内部にあるとは限りません。
- 範囲に入っていることと、壁に遮られていないことは別です。OverlapAllは遮蔽・視野角・透明度を判断しません。
- 始点・終点がないため、割合（Fraction）や「手前」の順位、代表交点、法線、侵入量は返しません。
- 半径0は点の問い合わせとして有効です（RaycastClosestのゼロ長拒否とは異なります）。

## 結果・順序・所有

- 一件はColliderです。同じBodyに重なるColliderが複数あれば、それぞれのIDを返します。Body単位の重複排除はゲーム側で行います。
- IDはBodyのWorld・スロット・世代と、Colliderのスロット・世代を含む完全な値です。順序は生存する対象Colliderの**スロット昇順**（距離順・Body順ではありません）。同じIDは一回だけです。
- 非交差・一致なしは正常な空配列です。結果は値所有の `Toolbox::TVector` で、World内部の配列を参照しません。World破棄後も読めますが、IDの生存や現在の重なりは保証しません。後で使う場合は `IsColliderAlive` / `IsAlive` で確認します。
- 全件を返します。件数の上限・切り詰め（Debug表示の上限等）はありません。
- 許容距離は0です。利用者が指定していない幅で範囲を膨らませません（接触は含み、正の隙間は含みません）。

## フィルター・自己除外（線分問い合わせと共通）

問い合わせカテゴリとマスクの規約は[線分問い合わせの絞り込み](WorldSegmentQuery.md#問い合わせ対象の絞り込み2d3d共通)と同じです（`(QueryCategory & IncludeCategories) != 0`、カテゴリ0は対象外、マスク0は正常な空、既定は全ビット）。
自己Bodyに属するColliderはカテゴリが一致しても除外します。対象外（カテゴリ不一致・自己Body）の形状は変換・計算しません。

## 失敗と状態

- 範囲の不正（非有限・負の半径）、明示した除外IDの無効・別World・削除済み・旧世代、Step中・途中失敗後は `Toolbox::FException` です。マスク0や空Worldでもこれらの検査は省略しません。
- 対象の形状が計算できない（Body位置とローカル中心から作るワールド形状が表現不能など）場合、先に一致した候補があっても問い合わせ全体が失敗し、途中までの配列は返しません。
- 結果配列の確保に失敗した場合も例外で、部分結果は返しません。`Previous = World.OverlapAll(...)` が失敗しても `Previous` は以前の値のままです（その値が現在も正しいという意味ではありません）。
- 最初のStep前から使えます。Create／Attach／Detach／Destroy／SetBodyTransform／SetColliderQueryCategoryは追加Stepなしで次の問い合わせへ反映します。Stepの引数検査だけの失敗や、OverlapAll自体の入力・計算・確保の失敗では、Worldを使用不能にしません。
- 問い合わせでStep・Snapshot採取・起床・力の消去・カテゴリ変更・接触キャッシュ更新を行いません。`const` は並行実行の安全性を保証しないため、同じWorldのStep・変更・破棄とは呼出し側で直列化します。

## 計算量

Worldが自動で保つ索引（[World問い合わせの索引](QueryAcceleration.md)）で範囲の近くのColliderだけを判定します。木の訪問はおおむね対数で、判定の数は範囲に重なる候補の数に比例します（常にO(log n)ではありません。密集・大きな範囲では増え、索引を使えない条件では削除済みを含む保持スロット数nに対しO(n)の総当たりになります）。結果・順序・失敗は総当たりと同じです。追加領域は結果件数kに対しO(k)です（一致がなければ確保しません）。値を返すため、一致がある呼出しでは配列の確保が起きます。測定は[検証記録](../Development/QueryScale-2026-09-25.md#性能測定)を参照してください。

## ゲームでの使い方（候補抽出→射線判定）

```cpp
// 一体を一Bodyで表す前提。射線の目標点はBodyの重心とするゲーム側の方針。
Toolbox::TVector<Dxf::FBodyId2D> FindVisibleCharacters(const Dxf::FPhysicsWorld2D& World, Dxf::FBodyId2D Self,
                                                       Toolbox::FVector2 Eye, Toolbox::f32 Radius)
{
    Dxf::FWorldQueryFilter Characters;
    Characters.IncludeCategories = CharacterCategory;
    // 遮蔽の判定には障害物も含める。キャラクターだけでは壁越しに当たってしまう。
    Dxf::FWorldQueryFilter Sight;
    Sight.IncludeCategories = ObstacleCategory | CharacterCategory;

    const auto Candidates = World.OverlapAll(Toolbox::FCircle2D{Eye, Radius}, Self, Characters);
    Toolbox::TVector<Dxf::FBodyId2D> Visible;
    Toolbox::TVector<Dxf::FBodyId2D> Checked;
    for (const auto& Collider : Candidates)
    {
        // 同じBodyの複数Colliderは一回にまとめる（小さい例なので線形の確認。候補数kに対しO(k²)）。
        bool bDuplicate = false;
        for (const auto& Body : Checked)
        {
            bDuplicate = bDuplicate || Body == Collider.Body;
        }
        if (bDuplicate)
        {
            continue;
        }
        Checked.PushBack(Collider.Body);
        const Toolbox::FVector2 Target = World.GetPosition(Collider.Body);
        if (Target == Eye)
        {
            // 同じ点への0長の射線は投げない（RaycastClosestは0長を拒否する）。見えるものとするのはゲーム側の方針。
            Visible.PushBack(Collider.Body);
            continue;
        }
        const auto Hit = World.RaycastClosest(Eye, Target, Self, Sight);
        if (Hit && Hit->Collider.Body == Collider.Body)
        {
            Visible.PushBack(Collider.Body);
        }
    }
    return Visible;
}
```

この例は重心への一本の線分を調べる方針です。全身の一部が見えるか、視野角、透明な物体の通過は判定しません。視点と同じ位置に別の形状があると、その形状は射線の始点を含むため割合0で最初に当たり、他の射線を遮ります。Body IDは必ずしもゲームのキャラクターIDではありません。
壁の `QueryCategory` を0にすると、Stepなしで射線の結果が変わります（壁の物理接触とSnapshotはそのままです）。同じ関数を3DではFSphereと`FVector3`で書けます。

## 保存Snapshotとの違い

Snapshotは採取時の観察データで、カテゴリを保存しません。本APIは現在のWorldを調べます。Debugの履歴選択は保存Snapshotを使い、OverlapAllへ置き換えません。

## 範囲外

範囲の形状は円／球だけです（箱・カプセルなどは未対応）。OverlapAny、出力バッファ・再利用版、列挙中のコールバック、Sweep、法線、接触イベント、空間索引（BVH等）、並列化は追加していません。

検証と実行範囲は[範囲問い合わせの検証記録](../Development/WorldOverlap-2026-09-24.md)を参照してください。
