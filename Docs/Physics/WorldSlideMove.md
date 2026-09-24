# 円・球の移動候補と1回の滑り（ComputeSlideMove）

`Dxf::ComputeSlideMove`（`Dxf/WorldSlideMove2D.h`：円、`Dxf/WorldSlideMove3D.h`：球）は、[SweepClosest](WorldSweepQuery.md)を組み合わせて、希望位置までの移動候補を計算します。最初の接触で手前へ戻し、有効な法線があれば方向を**1回だけ**補正し、その滑り経路で接触したら止まります。リンク先は `dxf::physics` だけです。

読み取り専用です。Bodyの位置・速度・力・休止状態、カテゴリ、Step数は変わりません。返した中心を採用するか、どう移動へ反映するかはゲーム側で決めます。

初期接触からの離脱・押し出し、接地・ジャンプ・段差・坂の制御、複数の面を使う反復、動く床への追従は行いません。完成したキャラクターコントローラーや、どんな状況でも動き続けられる移動機能ではありません。

## 呼出し

```cpp
#include "Dxf/WorldSlideMove2D.h"

// CurrentCenterはゲームが管理する円の中心。ここではBodyを直接移動しない。
Toolbox::FCircle2D Probe;
Probe.Center = CurrentCenter;
Probe.Radius = 0.5f;

const auto Move = Dxf::ComputeSlideMove(World, Probe, DesiredCenter, 0.01, SelfBody, Obstacles);
CurrentCenter = Move.EndCenter;
// Move.Stopを確認して、その後の入力・演出などはゲーム側で決める。
```

```cpp
Dxf::FWorldSlideResult2D ComputeSlideMove(const Dxf::FPhysicsWorld2D& World, const Toolbox::FCircle2D& StartShape,
                                          Toolbox::FVector2 DesiredEndCenter, Toolbox::f64 BackoffDistance,
                                          Toolbox::TOptional<Dxf::FBodyId2D> ExcludedBody = {},
                                          const Dxf::FWorldQueryFilter& Filter = {});
// 3Dは FPhysicsWorld3D・FSphere・FVector3・FBodyId3D・FWorldSlideResult3D。
```

- `DesiredEndCenter`は希望する終点の**中心**です。変位・速度・Bodyの重心位置ではありません。
- `BackoffDistance`は、接触位置から**移動経路に沿って**戻す距離です（物理Worldと同じ距離単位、有限の正の値が必須、既定値なし）。例の0.01は例の設定です。経路上の後退距離なので、対象表面との全方向の最小すき間は保証しません（斜めや接線方向では、経路上の距離と表面までの最短距離は違います）。
- 問い合わせの円／球は、有限の正の半径に限ります。半径0（点）の滑りは提供しません（SweepClosest自体の半径0の契約は変わりません）。
- `ExcludedBody`と`Filter`は、すべての内部の問い合わせで同じものを使います（途中の問い合わせだけ除外やカテゴリを変えません）。

Colliderにローカル中心のオフセットがある場合、円／球の中心とBodyの重心は同じではありません。`EndCenter`をそのまま`SetBodyTransform`の重心へ渡さないでください。Dynamic Bodyの積分やStepへ自動で重ねる機能でもありません。

## 結果（`Dxf/WorldSlideResult2D.h` / `3D.h`）

| メンバー | 意味 |
|---|---|
| `EndCenter` | 選んだ最終中心（円／球の中心。接触表面の点ではない）。開始時の接触で止めた場合は開始中心 |
| `Stop` | `EWorldSlideStop`（`Dxf/WorldSlideStop.h`）。下表 |
| `FirstHit` | 開始中心→希望終点の最初の移動の`FWorldSweepHit2D/3D`（SweepClosestの結果をそのまま保持。ID・Fraction・Normalを改変しない）。非交差なら空 |
| `SlideHit` | 補正した滑り経路のヒット。`Fraction`は滑り経路に対する割合で、元の移動全体の割合ではない。滑り経路を問い合わせなかった場合と非交差の場合は空 |

| `Stop` | 状況 | `EndCenter` |
|---|---|---|
| `NoMovement` | 開始＝希望終点で、開始時の接触もない | 開始中心 |
| `ReachedDesiredEnd` | 最初の移動が非交差 | 希望終点 |
| `SlideCompleted` | 最初の接触で1回補正し、滑り経路を非交差で走り終えた（元の希望終点とは限らない） | 滑り経路の終点 |
| `Blocked` | 補正後に進む成分がない（残りがすべて内向き）、または滑り経路で接触した | 接触の手前 |
| `InitialContact` | 最初の移動の開始時に接触・重なり（離れる向き・移動なしでも） | 開始中心（重なっていてもそのまま） |
| `MissingNormal` | 最初の接触に有効な法線がない | 接触の手前（希望終点へは進めない） |
| `PrecisionLimit` | f32へ丸めた候補の再検査で接触した、または補正後の終点が同じf32の中心になる | 直前に採用した中心 |

通常の停止は例外ではありません。入力（半径・後退距離・非有限値・f32で表現できない移動や中心）、World状態（Step中・途中で失敗したStepの後）、無効な除外ID、対象形状の計算失敗は、既存Physicsと同じ`Toolbox::FException`です。例外時は途中の中心を返さず、`Previous = ComputeSlideMove(...)`の`Previous`は以前の値のままです。

## 手順

1. 開始中心Pから希望終点Qへ`SweepClosest`。移動0・マスク0でも、Worldの状態と除外IDの検査を通ります。非交差ならQ、初期接触なら開始中心で終わります。
2. 接触割合tに対して、区間の長さLで`safe = max(0, t - BackoffDistance / L)`、候補`(1 - safe) * P + safe * Q`をf64で求め、f32へ丸めます。丸めた候補は、Pからその候補まで同じ条件で再スイープし、接触すれば採用せず（`PrecisionLimit`）Pで止まります。候補がPそのもの（safe=0）なら、最初のスイープで非接触を確認した同じ値なので再検査しません。
3. 最初のヒットの`Normal`が空なら、採用した中心で`MissingNormal`。
4. 採用した中心Aから、残り`R = Q - A`の法線方向の内向き成分だけを除きます: `slide = R - N * (min(dot(R, N), 0) / dot(N, N))`。反射ではなく、残りの長さへ正規化して増やしません。接線・外向きの成分はそのままです。
5. Aから`A + slide`（f32へ丸めた点）まで、同じ半径・除外ID・Filterで`SweepClosest`。最初に当たった対象も除外しません。非交差なら`SlideCompleted`、初期接触ならAで`Blocked`、それ以外は2と同じ後退と再検査で止まります。2回目の補正はしません。

`SweepClosest`は最大4回（最初の移動・その再検査・滑り経路・その再検査）で、World全体のSnapshot、配列の確保、Job、無制限のループは使いません。通常経路で配列を確保しないことを隔離した故障注入試験で確認しています。速度は測定していません。

## 限定仕様

- 丸い対象に沿って何度も向きを変える動き、二つの面の交線に沿う移動、初期接触からの離脱はできません。
- 接線方向の接触では、補正後の滑り経路で同じ対象に再び接して止まることがあります（ヒットを消して通過させません）。
- 途中に薄い障害物がある経路は、最終位置が空いていても滑り経路で止まります（最終位置の重なりだけで代用しません）。
- 同じWorldの変更・Step・破棄とは、**この関数全体の間**、呼出し側で直列化してください。`const`は並行実行の安全性を保証しません。

検証と実行範囲は[移動候補の検証記録](../Development/WorldSlide-2026-09-25.md)を参照してください。
