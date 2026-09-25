// SPDX-License-Identifier: NOASSERTION
# Physics 進捗記録

## ゲームプレイ基盤の進捗表

恒久の表です。機能を追加・変更したら、この表の行を更新してください（下の「最新状態」以降は段階A〜Eの当時の記録で、更新しません）。
「済」は実装・試験・使い方の3点がそろったものだけです。証拠の試験名は `Tests/Physics/CharacterMovementTests.cpp`（PhysicsContinuation群）、
`Tests/CharacterMovementComponentTests.cpp`（Framework群）、`Tests/GameplaySampleSmoke/Main.cpp`（NativeGameplayDeviceSmoke群）、
`Tools/ValidatePackage.py`（配布の利用者）です。

| 機能 | 2D | 3D | 使い方 | 証拠 | 残り |
|---|---|---|---|---|---|
| 形状の接触（符号付き距離と法線） | 済 | 済 | `FindShapeContact`（Toolbox）、`QueryContacts`（World、最大32件と総数） | shape contacts analytic、world contacts and initial-contact sweep | 同心・同距離の面は法線なし（仕様） |
| 初期接触を無視するスイープ | 済 | 済 | `SweepClosestIgnoringInitialContacts` | world contacts and initial-contact sweep | なし（SweepClosestの契約は不変） |
| A. 初期重なりの解消 | 済 | 済 | `ResolveCharacterOverlap`、StepCharacter・Componentでは自動 | character overlap recovery、recovery determinism validation、実Appの床へのめり込みからの復帰（2D） | 解消不能（Ambiguous・TooDeep・Blocked・上限）は移動せず理由を返すだけ |
| B. 反復滑り（平面・角・稜線・上限） | 済 | 済 | `MoveAndSlide` | character move and slide、3D character crease and three planes、実Appの二つの壁の角での停止（3D） | 接触の保持は8件まで（超えるとContactLimit） |
| C. 接地・坂・吸い付き | 済 | 済 | `ProbeCharacterGround`、StepCharacter | character ground、slopes steps cliff、実Appの30度の坂を上り60度の急坂の手前で停止（2D） | 動く床の上の接地・追従は未対応 |
| D. 段差上り・重力・ジャンプ・着地 | 済 | 済 | `StepCharacter`（上→前→下） | walk jump ceiling、slopes steps cliff、実Appの段差上り（2D／3D）とジャンプ・着地（2D） | 低い天井の下の段差は上らない（仕様） |
| E. 移動Component（固定更新・入力・補間・寿命・Body一つ・剛体併用の拒否・登録順） | 済 | 済 | `DCharacterMovement2DComponent` / `3DComponent` | Framework群の14ケース（2D／3D各7） | 剛体との押し合い、動く床、カプセル形状は未対応 |
| F. 操作できるサンプルと実Appの固定入力確認 | 済 | 済 | 開発用ソリューションの `GameplaySample`（[Tab]で2D／3D切替） | NativeGameplayDeviceSmoke（リセット・一時停止・再開・再入場・終了、画素照合、1画面／2画面で固定更新の回数が同じ） | 実機の目視操作は手動（自動確認は固定入力のみ） |
| G. 配布の利用者 | 済 | 済 | `dxf::physics`だけ／`dxf::framework` | ValidatePackage（PhysicsOnlyでMoveAndSlide・ProbeCharacterGround・StepCharacter、ConsumerでComponentの生成・更新・破棄） | Native ON・Releaseの配布検証は未実施 |
| 性能測定 | 測定済 | 測定済 | `dxf_character_benchmark`（CTest外） | [検証記録](../Development/Gameplay-2026-09-25.md#性能測定) | BroadPhaseがなく問い合わせはCollider数に比例。3Dの1問い合わせあたりの費用が2Dより大きい |

今後の大きな単位（未着手）: BroadPhase（問い合わせの候補絞り込み）、動く床への追従、キャラクターと剛体の押し合い、カプセル形状、
接触イベント（Begin／Stay／End）とTrigger、Island管理、Joint、Mesh Collider、経路探索、アニメーションとの接続。

使い方は[キャラクター移動](CharacterMovement.md)、検証は[ゲームプレイ基盤の検証記録](../Development/Gameplay-2026-09-25.md)を参照してください。


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

## 実装済み（段階D: CCDを物理更新へ接続）

- 剛体へ bUseContinuous を追加し、ワールドへ反復設定・診断・対応照会を追加
- 分割内の手順は速度積分→離散接触→TOI前進と解決→位置補正。力の積分は繰り返さない
- 候補抽出は移動区間を覆う境界で行い、Sweepへ速度×残り秒数を渡す
- 時刻ゼロの接触は中心速度と法線で離反と接近を区別する。離反は離散解決に任せ、
  接近はその場で解いて走査し直す。繰り返しは保守停止して未処理時間を診断へ出す
- 対応は円／球同士と軸平行箱との組だけ。箱同士と回転箱は対象外で離散処理へ回す
- 候補選別はCCD要求（Dynamicの連続指定）と相対運動を分けて判定する。
  対象自身が静止し、Kinematicや連続無効Dynamicだけが動く組も走査する
- テスト Tests/Physics/ContinuousTests.cpp（17ケース）

検証済みの振る舞い: 薄壁への高速衝突と対照のすり抜け、両者移動の弾性衝突、
一更新中の複数接触、かすめ軌道の非衝突、離脱、初期貫通の安全、反復上限の
保守停止と診断、回転壁の代替計数、静止接触の不変、
静止CCD円／球へのKinematic壁の横切り（登録順反転つき）、同速並走の非接触。

## 検証結果（自環境、Windows/MSVC Debug、段階D完了時）

- CTest 4/4、dxf_tests.exe 194/194
- physicsは10/10・16/16・11/11・20/20・11/11・18/18・12/12
- python Tools/CheckNoStl.py は170ファイル、違反0

TDD記録は Docs/Tdd/Physics/D-*.log。複数接触の失敗はテストの初期位置が
壁の外側だったためで、内側へ修正した。走査2回目の見逃しは離反中の時刻ゼロ
接触が真のTOIを塞いだためで、接近・離反の区別を実装して直した。

## 実装済み（段階E: 安定化と休止の初期回帰）

- `Tests/Physics/StabilityTests.cpp`（7ケース）
- 固定長ID配列 `Toolbox::TArray` とSleep ON／OFF共用の五段積みヘルパー
- 貫通・浮き・水平ずれ・傾き・残留速度・非有限の指標と初期上限
- 2DはSleep時に全段休止を要求、3Dは島休止伝播が将来課題のため指標のみで判定
- 緩斜面の静止摩擦と両側壁同時接触の回帰を追加
- 2D／3D箱同士接触で基準面がB側の場合に法線が反転する不具合を修正
- 以前は五段積みが上下入替まで貫通し、2段でも再現した

## 検証結果（自環境、Windows/MSVC Debug、段階E初期回帰時）

- CTest 4/4、dxf_tests.exe 194/194
- physicsは10/10・16/16・11/11・20/20・11/11・18/18・12/12・7/7
- `python Tools/CheckNoStl.py` は171ファイル、違反0

## 追補（レビュー指摘の修正）

- 2D箱同士接触の側方クリップ幅が法線側半辺長になっていた不具合を修正し、
  接線側半辺長を使うようにした。正方形では隠れる欠落で、横長床の端や
  縦長壁の上下端、回転床のずれ位置で接触が消えていた
- SolverTestsへ9ケースを追加（横長床の中央・ずれ・登録順反転・端、
  縦長壁の上下端の押出し、寸法入替柱、回転床のずれ、分離の無接触）
- CCD候補選別を「CCD要求」と「相対運動」に分離し、静止CCD対象へ
  Kinematic壁が横切る組も走査するようにした（2D／3D）
- ContinuousTestsへ5ケースを追加（静止円／球への壁横切りと登録順反転、
  同速並走の非接触）
- 壁挟み試験を両壁へ0.02ずつ食い込む初期配置にし、左右同時接触を前提に
  できるようにした。3D版も追加し、安定性は8ケースになった
- TDD記録は Docs/Tdd/Physics/F1-red.log、F1-green.log

## 検証結果（自環境、Windows/MSVC Debug、追補時）

- CTest 4/4、dxf_tests.exe 194/194、dxf_native_contract_tests 15/15
- physicsは10/10・16/16・11/11・20/20・11/11・27/27・17/17・8/8
- `python Tools/CheckNoStl.py` は171ファイル、違反0

## 追補2（接触・休止の契約修正）

- 同一剛体のCollider組を多様体生成とCCD走査から除外した（2D／3D）。
  除外前は重なる自己接触が休止判定の接触扱いになり、単独漂流体が眠って止まった
- 接触キャッシュを今の多様体に属する組だけに保つ再構築方式にした（2D／3D）。
  分離中の組の記録は捨てられ、再接触の初回は新接触として解く。
  特徴点の出入りでは捨てない。単一接触の再接近は収束で一致するため、
  新旧比較の回帰2件は変更前後とも一致するガードとして残す
- 既知接触でも前回確定の接触点相対運動が休止限界を超えれば起こす
  ようにした（2D／3D）。Kinematicは力積分を受けないため現在速度で見て
  同刻み起床し、Dynamicは今刻みの重力・外力分を除くため前回確定値で見る。
  休止中は止まっているものとして扱う。静止物との接触では起こさない
- 休止無効化と重力変更で凍結を残さないようにした（2D／3D）。
  無効化への遷移と重力値の変化で全登録を起こす。同値の再設定では起こさない
- SolverTestsへ16ケース、ContinuousTestsへ4ケースを追加。
  自己接触の回転・混合・分離・誤休止ガード、再接触比較、台運動・回転・
  積伝播の起床、休止無効化・重力変更の起床と同値維持
- TDD記録は Docs/Tdd/Physics/G1-red.log、G1-green.log

## 検証結果（自環境、Windows/MSVC Debug、追補2時）

- CTest 4/4、dxf_tests.exe 194/194、dxf_native_contract_tests 15/15
- physicsは10/10・16/16・11/11・20/20・11/11・49/49・21/21・8/8
- `python Tools/CheckNoStl.py` は171ファイル、違反0

## 次に着手する失敗テスト（段階Eの残り）

- 1m箱10段・60秒以上の長時間品質ゲート（2D／3D、Sleep ON/OFF、実貫通と定常誤差の分離）
- 回転CCDの明示的拒否と候補抽出の責務分離
- ContactとTriggerの区別、Begin／Stay／Endの通知方針

## 残課題

- 接触イベント（Begin／Stay／End）とTriggerの実装は未着手
- 完全なIsland管理（接触島単位の一括起床・休止判定）は未実装。
  現行は既知接触の相対運動による一段ずつの起床伝播まで
- 線形CCDは対応形状の組に接続済み。回転を伴う箱・重心外Colliderの線形扱いは未対応で、
  現行は離散フォールバックへ回す。明示的拒否とopt-in診断は将来課題
- Joint、Mesh動的対応は未実装
- Linux sanitizer、実DxLib SDKでの検証は未実行
- BroadPhase（総当たり比較つき）と性能測定は未実施
