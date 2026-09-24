# 現在のPhysics Worldへの円・球スイープ（SweepClosest）

`FPhysicsWorld2D::SweepClosest`（`Dxf/RigidBody2D.h`、円）と`FPhysicsWorld3D::SweepClosest`（`Dxf/RigidBody3D.h`、球）は、半径一定の円／球を現在の中心から指定した終点の中心まで直線移動させたとき、最初に接触するColliderを返します。リンク先は `dxf::physics` だけです。

調べる対象は問い合わせ時点の姿勢で固定したWorldです。相手のBodyの速度で未来位置を予測しません（動く物同士の衝突時刻や、World.StepのCCD・衝突応答とは別の機能です）。

## 三種類の問い合わせの違い

| API | 答える問い |
|---|---|
| [RaycastClosest](WorldSegmentQuery.md) | 厚みのない線分が最初に当たるColliderは何か |
| [OverlapAll](WorldOverlapQuery.md) | 今の円／球に重なる全Colliderは何か |
| SweepClosest | 半径のある円／球を終点まで動かす途中で、最初に何へ接触するか |

中心線のRaycastは、中心線が外れて半径の部分だけが接触する場合を検出しません。移動前後のOverlapAllは、途中にある薄い障害物を検出しません。

## 呼出し

```cpp
// 2D: StartShape.Centerが始点、EndCenterが終点の中心（変位・速度ではない）。物理ワールド座標（メートル、Y上向き）。
Toolbox::TOptional<Dxf::FWorldSweepHit2D> SweepClosest(const Toolbox::FCircle2D& StartShape, Toolbox::FVector2 EndCenter,
                                                      Toolbox::TOptional<Dxf::FBodyId2D> ExcludedBody = {},
                                                      const Dxf::FWorldQueryFilter& Filter = {}) const;
// 3D
Toolbox::TOptional<Dxf::FWorldSweepHit3D> SweepClosest(const Toolbox::FSphere& StartShape, Toolbox::FVector3 EndCenter,
                                                      Toolbox::TOptional<Dxf::FBodyId3D> ExcludedBody = {},
                                                      const Dxf::FWorldQueryFilter& Filter = {}) const;
```

移動中心は `C(t) = (1 - t) * StartShape.Center + t * EndCenter`（0 ≤ t ≤ 1）です。半径の変更、回転、曲線移動、箱・カプセルの移動は扱いません。

## 結果（`Dxf/WorldSweepHit2D.h` / `Dxf/WorldSweepHit3D.h`）

| メンバー | 意味 |
|---|---|
| `Collider` | BodyのWorld・スロット・世代を含む完全なCollider ID |
| `Fraction` | 移動区間全体を1とした最初の接触割合（0～1）。秒数・距離ではない |
| `CenterAtHit` | その割合での問い合わせ円／球の**中心**。接触表面の点ではない（RaycastのPositionとは意味が違う） |
| `bInitialContact` | 開始状態で接触または重なりがあったか（`Fraction`は0）。厳密な貫通だけを示すものではない |

返すのは最大1件で、非交差は空Optionalです。法線・接触点・侵入量・押し戻しベクトルは返しません。`CenterAtHit`はf64の計算結果をf32へ丸めた値です。その位置へ置けば常に隙間がある、または常にちょうど接触する、という保証はありません。

## 境界の扱い

| 入力・状態 | 結果 |
|---|---|
| 開始時に重なる・境界だけ接する | `Fraction=0`、`bInitialContact=true`（離れる向きの移動でも） |
| 開始時は外で、途中または終点で接する | 最初の割合（終点なら1）、`bInitialContact=false` |
| 開始＝終点、半径は正 | 静止した円／球の重なりを調べる。重なれば0、なければ空 |
| 半径0、移動あり | 同じ条件の`RaycastClosest`と同じID・割合・位置（割合0は初期接触） |
| 半径0、移動なし | 点の重なり（RaycastClosestのゼロ長拒否とは別の契約） |
| 負の半径・NaN／Inf・f32で表現できない移動 | 空Worldやマスク0でも`Toolbox::FException` |
| 半幅0の箱・半径0の対象 | 面・辺・点として扱う（厚みを足さない） |

接触を含み、許容距離は0です（利用者が指定していない余白で膨らませません）。同じ割合の候補はColliderスロットの小さい順で、許容幅で異なる割合をまとめません。最短が0でも、後続の対象形状の計算失敗は隠しません。

## 対象・状態・失敗（線分・範囲問い合わせと共通）

- 問い合わせカテゴリとマスク（`(QueryCategory & IncludeCategories) != 0`、カテゴリ0は対象外、マスク0は正常な空、既定は全ビット）と自己Body除外は[線分問い合わせ](WorldSegmentQuery.md#問い合わせ対象の絞り込み2d3d共通)と同じ規則です。対象外の形状は変換・計算しません。入力する円／球をWorldへ登録する必要はなく、自己Bodyの形状から半径を自動補正することもありません。
- Static／Kinematic／Dynamic／休止中のすべてを対象にします。Create／Attach／Detach／Destroy／SetBodyTransform／SetColliderQueryCategoryは追加Stepなしで反映されます。
- 最初のStep前から使えます。Step中と、途中で失敗したStepの後は例外で拒否し、次の正常Stepで回復します。Stepの引数検査だけの失敗や、SweepClosest自身の入力・計算の失敗ではWorldを使用不能にしません。
- 問い合わせでStep・Snapshot採取・起床・力の消去・速度更新・カテゴリ変更を行いません。初期重なりを見つけても押し出しません。`const` は並行実行の安全性を保証しないため、同じWorldの変更・Step・破棄とは呼出し側で直列化します。
- 失敗時は暫定の結果を返しません。`Previous = World.SweepClosest(...)` が失敗しても `Previous` は以前の値のままです（それが今も正しいという意味ではありません）。結果はWorldを保持しない値で、後で使う場合は `IsColliderAlive` で確認します。
- 走査は削除済みを含む保持Colliderスロット数nに対しO(n)、追加領域O(1)です。通常経路で配列を確保しません（ヒット・非交差・静止・マスク0・半径0で確保が起きないことを隔離した試験で確認）。速度は測定していません。

## 形状の計算

- 円同士・球同士: 相対位置の点が半径の和の円／球へ入る最初の時刻を、既存の安定した二次式（外積による判別式、f64）で求めます。
- 回転矩形・OBB: 箱の面・辺・頂点（2Dは4辺・4頂点、3Dは6面・12辺・8頂点）ごとに、その有限範囲への距離が半径以下になる最初の時刻を求め、最小を選びます。半径分だけ膨らませた外接矩形では代用しません（角・辺の丸みを正しく扱います）。OBBの軸は正規化・直交化せず、丸めを含む実際の軸を使います（`IntersectsSphere`と同じ平行六面体）。開始時の接触は`IntersectsSphere`／`Intersects(円, 回転矩形, 0)`で判定します。
- Toolboxの入口は`Toolbox::SweepToCenter`（`Toolbox/ShapeSweep2D.h`、`Toolbox/ShapeSweep3D.h`）です。既存の`Toolbox::Sweep`（変位を受け取り、代表法線を返すCCD用）とは別の関数です。

## ゲームでの使い方

```cpp
#include "Dxf/RigidBody2D.h"

// 2D: 円の移動候補を調べる。
Toolbox::FCircle2D Probe;
Probe.Center = CurrentCenter;
Probe.Radius = 0.5f;
Dxf::FWorldQueryFilter Obstacles;
Obstacles.IncludeCategories = ObstacleCategory;

const auto Hit = World.SweepClosest(Probe, DesiredCenter, SelfBody, Obstacles);
Toolbox::FVector2 ChosenCenter = DesiredCenter;
if (Hit)
{
    if (Hit->bInitialContact)
    {
        // 既に接触している。押し出しの解を返すAPIではないので、この例では移動を見送る。
        ChosenCenter = CurrentCenter;
    }
    else
    {
        // これは円の中心。接触表面の位置ではない。
        ChosenCenter = Hit->CenterAtHit;
    }
}
// ChosenCenterをどう使うかはゲーム側の方針。WorldのBody移動や速度の変更は自動では行わない。
```

接触した中心からさらに同じ向きへ動かすと、すぐに同じ対象で止まります（丸めによって初期接触になる場合も、わずかな割合になる場合もあります）。繰り返し移動させる制御には、ゲーム側で余白や初期接触時の方針が必要です。距離単位の余白Skinを使う場合は、区間長L>0に対して `max(0, Fraction - Skin / L)` を割合として使います（`Fraction`から固定値を引くと、移動距離に比例した別の余白になります）。これは完成した衝突付き移動・滑り・脱出処理ではありません。

3Dでは同じ呼出しを`Toolbox::FSphere`と`FVector3`で行えます。例えば、球で近似したカメラ位置候補（プレイヤーの頭から希望位置まで）を調べられますが、視錐台全体の衝突や可視性を保証するものではありません。

範囲の候補抽出（OverlapAll）→障害物込みの射線（RaycastClosest）による見通しの判定は、[範囲問い合わせ](WorldOverlapQuery.md#ゲームでの使い方候補抽出射線判定)の例のままです。見通しの中心線を無条件に球のスイープへ置き換えません。

## 範囲外

法線・接触点・侵入量、全ヒット一覧、SweepAny、移動する箱・カプセル・凸形状・モデル三角形、回転・半径変更、相手の速度を含む予測、押し戻し・滑り、列挙コールバック、空間索引（BVH等）、並列化は追加していません。

検証と実行範囲は[スイープ問い合わせの検証記録](../Development/WorldSweep-2026-09-24.md)を参照してください。
