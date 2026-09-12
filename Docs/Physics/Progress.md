// SPDX-License-Identifier: NOASSERTION
# Physics 進捗記録

## 最新状態

- 作業ブランチ: `physics/continuation-89ec1f0`
- 起点: `41669d9a0595d23e6e45732dc58be4f9c2cc07f0`（originと一致、乖離なし）
- mainへの変更・merge・pushなし

## 確定した規約

- 単位はm、kg、s、rad。2DはX右・Y上、角度は反時計回りが正。3Dは右手系、四元数は右手則。
- 画面ピクセルやDxLib座標への変換は将来のアダプター側で行い、Physics層は持たない。
- 積分は半陰的Euler。速度→位置の順に更新する。自由落下位置は連続解と `g*h*t/2` ずれる。
- 減衰は `1/(1+d*h)` 係数。減衰後に重力と外力を加算する。
- 状態保存はf32、中間計算（差分、慣性、ジャイロ項、姿勢微分）はf64。
- 法線はB→Aへ統一する（段階B以降）。
- 物理姿勢は重心基準。Kinematicの運動量は追跡せずゼロを返す。
- 無効入力は状態変更前にFException。期限切れIDの取得系は例外、破棄系はfalse。

## 実装済み（段階A: Bodyと自由運動）

- `Source/Physics/Public/Dxf/BodyType.h`（EBodyType）
- `Source/Physics/Public/Dxf/RigidBody2D.h`（FBodyId2D、FBodyDescription2D、FPhysicsWorld2D）
- `Source/Physics/Public/Dxf/RigidBody3D.h`（FBodyId3D、FBodyDescription3D、FPhysicsWorld3D）
- `Source/Physics/Private/PhysicsWorld2D.cpp`、`PhysicsWorld3D.cpp`
- `Source/Toolbox/Public/Toolbox/Vector2.h`へ最小の2D演算（加減算、内積、外積）を追加
- CMakeへ `dxf::physics` を追加し、install／exportへ反映。物理テストは `dxf::physics` 経由で解消
- テスト `Tests/Physics/RigidBodyTests.cpp`（20ケース）

検証済みの振る舞い: 静止、等速、重力の質量独立、自由落下の一次収束、
力・トルクの質量依存と更新後消去、分割数不変、Impulseの一回作用、
重心外Impulseの回転、Static／Kinematic、異方慣性の角運動量保存と姿勢正規化、
減衰、GravityScale、無効入力とID世代管理。

## 検証結果（自環境、Windows/MSVC Debug）

- CTest 4/4（PhysicsContinuation、NoStl、Framework、NativeContract）
- dxf_tests.exe 180/180、physicsは10/10・16/16・11/11・20/20
- `python Tools/CheckNoStl.py` は157ファイル、違反0
- CTest登録数と実行ファイル内ケース数は別々に数える

TDD記録は `Docs/Tdd/Physics/A1-*.log`。A1-redは新規API不在のコンパイル失敗、
green2は private 入れ子型への自由関数アクセス（前例に合わせてFImplメンバー化）、
green4は単一刻み陽解法に対するテスト許容の理論不整合（テスト側を60分割へ修正）。
バッチ2はバッチ1のGreenが先行実装したため解析解ベースの回帰として記録し、
段階B以降は厳密なRed先行へ戻す。

## 次に着手する失敗テスト（段階B）

- 2D円と静的箱、3D球と静的箱の接触点・単位法線・分離距離／貫通深度・Feature ID
- 法線B→A、深度の正負統一、A/B交換での対応
- Sequential Impulseの最小応答（床静止、滑走、反発、質量差、初期貫通、移動床）

## 残課題

- 接触多様体、摩擦、Sleep、CCD接続、Scene連携、Joint、Mesh動的対応は未実装
- Linux sanitizer、Release、実DxLib SDKでの検証は未実行
- 性能測定は未実施
