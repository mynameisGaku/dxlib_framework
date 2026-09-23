# 現在のPhysics Worldへの最短線分問い合わせ

`Dxf/RigidBody3D.h` の `FPhysicsWorld3D::RaycastClosest(Start, End, ExcludedBody = {}) const` は、現在登録されている球・OBBのうち有限線分が最初に当たるColliderを返します。リンク先は `dxf::physics` だけです。カメラ、Debug、Native、GPU読戻しは不要です。

## ゲームから使う

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

返り値は `Toolbox::TOptional<FWorldSegmentHit3D>`。正常な非交差は空です。不正入力・計算不能・利用できないWorld状態は既存Physics APIと同じ `Toolbox::FException` で通知するため、呼び出し境界で処理してください。

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

## 結果と失敗の契約

- `Collider` はBodyのWorld識別子・スロット・世代と、Colliderのスロット・世代を含む値です。削除・再登録後も同じ物体とは限りません。保存後に利用する場合は `World.IsColliderAlive(Hit->Collider)` で確認します。生存していても、保存した交点は問い合わせ時の位置です。
- `Fraction` は始点0～終点1の `f64` の割合です。距離・時間・描画深度ではありません。`Position` は線分上のワールド交点です。接線・両端を含み、始点が内部なら0。完全に同じ割合の候補はColliderスロットの小さい順です。
- 除外は生存Body一つまでで、そのBodyに属する全Colliderを除きます。省略した空Optionalと、明示指定した `FBodyId3D{}` は異なります。指定した無効・削除済み・旧世代・別WorldのIDは例外です。
- 空WorldでもNaN/Inf、ゼロ長、f32で表現できない始終点の変位を拒否します。各対象の変換後形状・ローカル線分・結果が既存交差計算で表現できない場合も例外です。割合0の候補を見つけても後続対象の計算失敗を隠しません。除外対象の形状は計算しません。

## 現在の状態と読み取り専用性

Static / Kinematic / Dynamic / 休止中のBodyを区別せず、全生存Colliderを調べます。Bodyの現在位置・姿勢とColliderのローカル中心・箱の軸から、接触処理と共通の変換を利用します。Attach / Detach / Destroy / Create / SetBodyTransformは追加Stepなしで反映されます。

問い合わせはStep、起床、採取、力の消去、履歴保存を行いません。Step中、または引数検査後に途中失敗したStepの後は拒否し、次の正常Step完了で回復します。最初のStep前は使用できます。Stepの引数検査だけで失敗した場合は問い合わせを禁止しません。中断したStepを巻き戻すAPIではありません。

`const` は同時実行の安全性を保証しません。呼び出し側でStepや登録変更等と直列化してください。

Colliderスロットを直接走査するため、削除済みも含めた保持スロット数nに対しO(n)、追加領域O(1)です。通常経路で候補配列やSnapshotを確保しません。Debug表示の件数上限はありません。空間索引や高速化の性能保証は今回の範囲外です。

## 保存Snapshotとの違い

[Snapshot選択](../Rendering/PhysicsSnapshotPicking.md)は採取時の表示・履歴を調べます。本APIは現在のWorldを調べます。RenderDebugの履歴選択は引き続き保存Snapshotを使用し、現在の値へ置き換えません。

今回の対象は3D球/OBB・最短一件・自己Body一つの除外までです。2D、全件一覧、カテゴリ、モデル三角形、法線、形状の二重登録は追加していません。

検証と実行範囲は[World問い合わせの検証記録](../Development/WorldSegmentQuery-2026-09-24.md)を参照してください。
