# 現在のPhysics Worldへの最短線分問い合わせ

`FPhysicsWorld2D::RaycastClosest`（`Dxf/RigidBody2D.h`）と`FPhysicsWorld3D::RaycastClosest`（`Dxf/RigidBody3D.h`）は、現在登録されている形状のうち有限線分が最初に当たるColliderを返します。リンク先は `dxf::physics` だけです。カメラ、Debug、Native、GPU読戻しは不要です。名前は「Raycast」ですが、対象はStart～Endの有限線分です。無限Rayや厚みのある移動判定ではありません。

「近くに何があるか」から候補を集める場合は、[範囲問い合わせ（OverlapAll）](WorldOverlapQuery.md)を使います。範囲との重なりと射線の遮蔽は別の問い合わせで、組み合わせて使います。半径のある円／球を移動させる場合は[スイープ問い合わせ（SweepClosest）](WorldSweepQuery.md)を使います。

## 2Dと3Dの対応

| 項目 | 2D | 3D |
|---|---|---|
| 呼出し | `RaycastClosest(FVector2 Start, FVector2 End, TOptional<FBodyId2D> ExcludedBody = {}) const` | `RaycastClosest(FVector3 Start, FVector3 End, TOptional<FBodyId3D> ExcludedBody = {}) const` |
| 結果 | `TOptional<FWorldSegmentHit2D>`（`Dxf/WorldSegmentHit2D.h`） | `TOptional<FWorldSegmentHit3D>`（`Dxf/WorldSegmentHit3D.h`） |
| 対象形状 | 円・回転矩形（Body角度＋Colliderのローカル角度） | 球・OBB |
| 座標 | 2D物理ワールド。メートル・Y上向き・角度はラジアン。ピクセルではない | 3D物理ワールド。メートル |
| 対象の絞り込み | `RaycastClosest(Start, End, ExcludedBody, const FWorldQueryFilter& Filter) const`、`Set/GetColliderQueryCategory(FColliderId2D, ...)` | 同左（`FColliderId3D`） |
| 形状交差 | `Toolbox::IntersectSegment(FVector2, FVector2, FCircle2D / FOrientedBox2D)`（`Toolbox/SegmentIntersection2D.h`） | `Toolbox::IntersectSegment(FVector3, FVector3, FSphere / FOBB)`（`Toolbox/SegmentIntersection.h`） |

結果の型・失敗・Step状態・除外・順序・対象の絞り込みの契約は2Dと3Dで共通です（下記）。法線と全件一覧はどちらにもありません。候補の絞り込みには、Worldが自動で保つ索引（[World問い合わせの索引](QueryAcceleration.md)）を使います。

## ゲームから使う（2D）

```cpp
#include "Dxf/RigidBody2D.h"

// 敵の位置からプレイヤーへの射線。座標は2D物理ワールド（メートル・Y上向き）。
const Toolbox::FVector2 Eye = World.GetPosition(EnemyBody);
const Toolbox::FVector2 Target = World.GetPosition(PlayerBody);
// 自分のBodyの全Colliderを除外する。
const auto Hit = World.RaycastClosest(Eye, Target, EnemyBody);
if (Hit && Hit->Collider.Body == PlayerBody)
{
    // 最初に当たったのがプレイヤーなら見えている。Hit->Positionは問い合わせ時点の交点。
}
```

画面のピクセル座標（Y下向き）から使う場合は、ゲーム側でPhysicsの座標へ変換してから渡します。このAPIは画面座標を受け取りません。

## ゲームから使う（3D）

```cpp
// Start / Endはワールド座標。SelfBodyはこのWorldの生存Body ID。
const auto Hit = World.RaycastClosest(Start, End);
const auto OtherHit = World.RaycastClosest(Start, End, SelfBody);
if (OtherHit)
{
    const Dxf::FColliderId3D Collider = OtherHit->Collider;
    const Toolbox::FVector3 Position = OtherHit->Position;
    // ColliderとPositionをゲーム固有の処理へ渡す。
}
```

画面座標を使う場合だけ `dxf::support` の座標変換を組み合わせます。

```cpp
#include "Dxf/ViewCoordinates.h"
#include "Dxf/RigidBody3D.h"

const auto Segment = Dxf::MakeViewPickSegment(View, Width, Height, MousePosition);
if (Segment && Segment.Value())
{
    const auto Hit = World.RaycastClosest(Segment.Value()->Start, Segment.Value()->End);
    // 選択結果をゲームへ反映する。描画状態の変更・復元は不要。
}
// SegmentのFailureは変換エラー、成功した空値はビュー範囲外。
```

返り値の正常な非交差は空Optionalです。不正入力・計算不能・利用できないWorld状態は既存Physics APIと同じ `Toolbox::FException` で通知するため、呼び出し境界で処理してください。

## 問い合わせ対象の絞り込み（2D／3D共通）

Colliderごとの**問い合わせカテゴリ**（`FColliderDescription2D/3D::QueryCategory`、`uint32`のビット集合、既定1）と、
呼出しごとの**マスク**（`Dxf/WorldQueryFilter.h` の `FWorldQueryFilter::IncludeCategories`、既定は全ビット）で、線分問い合わせの候補を絞ります。
`(QueryCategory & IncludeCategories) != 0` のColliderだけが候補です。カテゴリの意味（壁・キャラクターなど）はゲーム側で決め、フレームワークは固定の分類を持ちません。

```cpp
// ゲーム側のカテゴリ定義。
constexpr Toolbox::uint32 ObstacleCategory = 1u << 0;
constexpr Toolbox::uint32 CharacterCategory = 1u << 1;
constexpr Toolbox::uint32 PickupCategory = 1u << 2;

Dxf::FColliderDescription2D Wall;
Wall.Shape = Toolbox::FOrientedBox2D{{0, 0}, {0.5f, 2}, 0};
Wall.QueryCategory = ObstacleCategory;
const auto WallId = World.AttachCollider(WallBody, Wall);

// 障害物とキャラクターを対象にし、自分のBodyは除外する。拾得物は射線を遮らない。
Dxf::FWorldQueryFilter Sight;
Sight.IncludeCategories = ObstacleCategory | CharacterCategory;
const auto Hit = World.RaycastClosest(Eye, Target, SelfBody, Sight);
// 最短のColliderがプレイヤーかはゲーム側で判断する。手前に壁があれば壁が返る。
const bool bCanSee = Hit && Hit->Collider.Body == PlayerBody;

// 自己Bodyを除外しない場合は空Optionalを渡す。
const auto Any = World.RaycastClosest(Eye, Target, {}, Sight);

// 壁を一時的に射線の対象から外し、後で戻す。どちらも追加Stepなしで次の問い合わせへ反映する。
World.SetColliderQueryCategory(WallId, 0u);
World.SetColliderQueryCategory(WallId, ObstacleCategory);
```

3Dも同じ呼出しです（`FColliderDescription3D`、`FVector3`）。プレイヤーのカテゴリだけを指定すると壁の奥のプレイヤーも返るため、「壁越しには見えない」判定にはなりません。見えるかどうかは、遮るものも含めたマスクで最短を調べてください。

- **問い合わせだけに効く**: 接触応答・重力・休止・CCD・Snapshot採取・描画は変わりません。カテゴリ0のColliderも衝突し、Snapshotに含まれます。既存の双方向の衝突フィルター（`Toolbox::FCollisionFilter`）とは別の、片方向の判定です。
- **カテゴリ0**: そのColliderをこのWorldの線分問い合わせから外す明示指定です。従来の2／3引数の入口（全ビットのマスクと同じ）でも対象になりません。これまでのコードはQueryCategoryを設定していないため、既定の1で従来どおり対象です。
- **マスク0**: 正常な「対象なし」で、結果は空です。ただし線分・World状態・除外IDの検証は省略しません（不正なら例外）。最上位ビット`0x80000000u`を含む全32ビットを使えます。
- **最短候補の選定前に絞る**: 対象外のColliderは形状の変換・交差計算をしません（変換できない形状があっても対象外なら失敗しません）。対象のColliderの計算失敗は、先に割合0の候補があっても例外です。同じ割合の候補は、対象の中でColliderスロットの小さい順です。
- **自己Body除外との併用**: 除外したBodyのColliderは、カテゴリが一致していても対象外です。
- **変更**: `SetColliderQueryCategory` はカテゴリだけを変え、ID・世代・形状・姿勢・速度・力・接触キャッシュ・休止・StepIndexを変えません。Getterは保持値を返します。どちらも別World・無効・削除済み・旧世代のIDとStep中・途中失敗後を例外で拒否し、失敗時は値を変えません。最初のStep前から使えます。Colliderスロットの再利用時は新しいDescriptionの値を使い、以前のカテゴリは残りません。
- **保存Snapshotとの違い**: Snapshotにはカテゴリを保存しません。問い合わせ設定を復元するデータではなく、Debugの履歴選択は従来どおり保存時の全Colliderを調べます。
- **再ビルド**: Descriptionへのフィールド追加と新しい公開型のため、フレームワークと利用側を再ビルドしてください。旧バイナリとのABI互換は保証しません。

## 結果と失敗の契約（2D／3D共通）

- `Collider` はBodyのWorld識別子・スロット・世代と、Colliderのスロット・世代を含む値です。削除・再登録後も同じ物体とは限りません。保存後に利用する場合は `World.IsColliderAlive(Hit->Collider)` で確認します。生存していても、保存した交点は問い合わせ時の位置です。結果に生ポインタや内部配列の参照は含みません。
- `Fraction` は始点0～終点1の `f64` の割合です。距離・時間・描画深度ではありません。`Position` は線分上のワールド交点で、倍精度の凸結合から作ります。接線・両端を含み、始点が内部または境界上なら0。完全に同じ割合の候補はColliderスロットの小さい順です。許容幅で異なる割合を同順位にまとめません。
- 除外は生存Body一つまでで、そのBodyに属する全Colliderを除きます。省略した空Optionalと、明示指定した `FBodyId2D{}` / `FBodyId3D{}` は異なります。指定した無効・削除済み・旧世代・別WorldのIDは例外です。
- 空WorldでもNaN/Inf、ゼロ長、f32で表現できない始終点の変位を拒否します。各対象の変換後形状・ローカル線分・結果が既存交差計算で表現できない場合も例外です。割合0の候補を見つけても後続対象の計算失敗を隠しません。除外対象の形状は計算しません。
- 形状の有効条件は登録時と同じです。2Dでは半径0の円（点）と半幅0の軸を持つ矩形（辺・点）も登録でき、問い合わせでは接触として扱います。回転矩形を外接矩形で代用しません。

## 現在の状態と読み取り専用性

Static / Kinematic / Dynamic / 休止中のBodyを区別せず、全生存Colliderを調べます。Bodyの現在位置・姿勢とColliderのローカル中心・角度（3Dは箱の軸）から、接触処理と共通の変換を利用します。Attach / Detach / Destroy / Create / SetBodyTransformは追加Stepなしで反映されます。StepIndexを使ったキャッシュはありません。

問い合わせはStep、起床、採取、力の消去、履歴保存、接触キャッシュの更新を行いません。Step中、または引数検査後に途中失敗したStepの後は拒否し、次の正常Step完了で回復します。最初のStep前は使用できます。Stepの引数検査だけで失敗した場合は問い合わせを禁止しません。中断したStepを巻き戻すAPIではありません。

`const` は同時実行の安全性を保証しません。呼び出し側でStepや登録変更等と直列化してください。

索引の木を線分の範囲でたどり、範囲に重なるColliderだけを判定します（木の訪問はおおむね対数。長い線分・密集では候補が増え、索引を使えない条件では保持スロット数nに対しO(n)の総当たり）。最短の結果・同じ割合の順序・失敗は総当たりと同じです。追加領域O(1)で、通常経路で候補配列やSnapshotを確保しません。Debug表示の件数上限はありません。測定は[検証記録](../Development/QueryScale-2026-09-25.md#性能測定)を参照してください（性能の保証ではありません）。

## 保存Snapshotとの違い

[Snapshot選択](../Rendering/PhysicsSnapshotPicking.md)は採取時の表示・履歴を調べます。本APIは現在のWorldを調べます。RenderDebugの履歴選択は引き続き保存Snapshotを使用し、現在の値へ置き換えません。

対象は最短一件・自己Body一つの除外・問い合わせカテゴリによる絞り込みまでです。衝突応答用のカテゴリ、全件一覧、任意コールバックの絞り込み、モデル三角形、法線、Sweepの公開World API、2D画面選択、形状の二重登録は追加していません。

検証と実行範囲は[3Dの検証記録](../Development/WorldSegmentQuery-2026-09-24.md)、[2Dの検証記録](../Development/WorldSegmentQuery2D-2026-09-24.md)、[対象フィルターの検証記録](../Development/WorldQueryFilter-2026-09-24.md)を参照してください。
