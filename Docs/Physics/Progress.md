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

## 実装済み（段階B: 接触情報と最小の衝突応答）

- `Source/Toolbox/Public/Toolbox/Contact2D.h`（FOrientedBox2D、FContactPoint2D）
- `Source/Toolbox/Public/Toolbox/Contact3D.h`（FContactPoint3D）
- `Source/Toolbox/Private/Contact2D.cpp`、`Contact3D.cpp`（最近傍接触、法線B→A、符号付き分離距離、特徴ID）
- ワールドへコライダー取り付け（円／回転矩形、球／OBB、摩擦・反発つき）と世代管理ID
- Sequential Impulseソルバー（法線非負累積、反発しきい値と衝突前速度の保存、
  2D接線・3D接平面の合成上限摩擦、世代・特徴・法線検査つき前回Impulse再利用、
  許容幅と上限つき位置補正）
- 2D箱同士はSATと辺切り取りの最大二点多様体。3D箱同士は生成しない（段階Eへ）
- 混合則は摩擦が相乗平均、反発が最大値（入れ替え対称を試験）
- テスト `Tests/Physics/ContactTests.cpp`（11ケース）、`Tests/Physics/SolverTests.cpp`（18ケース）

検証済みの振る舞い: 床静止、摩擦なし滑走、摩擦による転がり遷移、斜面滑走、
反発頂点、質量比の弾性衝突と運動量保存、初期貫通の安全解消、移動床の運搬、
混合則の対称性、薄縁支持、箱の面着地、回転Kinematic箱の押し出し、コライダー寿命。

## 検証結果（自環境、Windows/MSVC Debug、段階B完了時）

- CTest 4/4（PhysicsContinuation、NoStl、Framework、NativeContract）
- dxf_tests.exe 180/180、physicsは10/10・16/16・11/11・20/20・11/11・18/18
- `python Tools/CheckNoStl.py` は163ファイル、違反0
- CTest登録数と実行ファイル内ケース数は別々に数える

TDD記録は `Docs/Tdd/Physics/B1-*.log`、`B2-*.log`。B2では摩擦で停止しない
真のRedを診断し、転がりへの遷移が理論通り（速度0.6、角速度-1.2）であることを
確認してテスト側を転がり判定へ修正した。箱の面着地の失敗は箱同士接触の
分離符号反転と非A基準面の選択誤りで、実装を修正した。

TDD記録は `Docs/Tdd/Physics/A1-*.log`。A1-redは新規API不在のコンパイル失敗、
green2は private 入れ子型への自由関数アクセス（前例に合わせてFImplメンバー化）、
green4は単一刻み陽解法に対するテスト許容の理論不整合（テスト側を60分割へ修正）。
バッチ2はバッチ1のGreenが先行実装したため解析解ベースの回帰として記録し、
段階B以降は厳密なRed先行へ戻す。

## 実装済み（段階C: Scene／GameObject接続）

- `FFixedTickContext`（入力・時刻・未配達複写・補間割合・物理ワールド・遷移管理）
- `OnFixedTick` と `FixedTick_Internal` を DLifecycleObject、ILifecycleGroup、
  TManagedCollection、TManagedUpdaterへ追加。既存の OnTick 契約は維持
- `DPhysicsScene2D`／`DPhysicsScene3D`（Gameplay層、OnTick は final で固定更新を駆動）
- `DRigidBody2DComponent`／`DRigidBody3DComponent`（遅延生成、外力予約、補間描画、Teleport）
- `DCollider2DComponent`／`DCollider3DComponent`（兄弟剛体への遅延接続）
- ワールドへ `SetBodyTransform`、`ClearContactCache`、`IsColliderAlive` を追加
- テスト `Tests/FixedTickTests.cpp`（5ケース）、`Tests/PhysicsSceneTests.cpp`（9ケース）

確定した動作: 固定更新の呼び出し元はシーンだけ。配布→物理更新の順序。
オブジェクトの生成・破棄の確定は描画フレーム境界、物理の力・姿勢変更は
固定更新境界で適用する。Dynamicは物理が正、Kinematicはゲーム速度指示が正。
描画は前回・今回の物理姿勢と補間割合から求め、書き戻さない。
エッジは最初の更新と未配達複写だけで有効（最大8件保持）。可変Snapshotは不変。
一時停止中は固定更新を進めず、終了要求後は残り更新を打ち切る。

## 検証結果（自環境、Windows/MSVC Debug、段階C完了時）

- CTest 4/4、dxf_tests.exe 194/194
- physicsは10/10・16/16・11/11・20/20・11/11・18/18
- `python Tools/CheckNoStl.py` は169ファイル、違反0

TDD記録は `Docs/Tdd/Physics/C1-*.log`、`C2-*.log`。C1後に増分ビルドで
SegFaultが出たが、クリーンビルドで解消した。vtable配置を変える
Runtime層ヘッダー変更の後は `-Clean` で再検証する。

## 次に着手する失敗テスト（段階D）

- 高速円／球と薄い壁のすり抜け防止、両者移動の衝突、一固定更新中の複数接触
- 静止接触からの離脱、端・角をかすめる非衝突、反復上限と残り時間の診断

## 残課題

- 接触イベント（Begin／Stay／End）とTrigger、Sleep／Island、CCD接続は未実装
- 3D箱同士の接触多様体、Joint、Mesh動的対応は未実装
- Linux sanitizer、Release、実DxLib SDKでの検証は未実行
- 性能測定は未実施
