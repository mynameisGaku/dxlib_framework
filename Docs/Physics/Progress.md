// SPDX-License-Identifier: NOASSERTION
# Physics 進捗記録

## ゲームプレイ基盤の進捗表

恒久の表です。機能を追加・変更したら、この表の行を更新してください（下の「最新状態」以降は段階A〜Eの当時の記録で、更新しません）。
各欄は「2D／3D」の順です。「済」は、その段階の試験と使い方がそろったものだけです。「—」はその段階に当たらないもの、「未」は未確認です。

- 実装・単体: Toolbox・Physicsの関数を単体で試す（`Tests/Physics/*`、PhysicsContinuation群）
- 実World: `FPhysicsWorld2D/3D` に登録したColliderで試す（同上、索引と総当たりの一致を含む）
- Component: Scene・GameObjectの移動Componentで試す（`Tests/CharacterMovementComponentTests.cpp`、Framework群）
- 実App: 実Application・実DxLibで固定入力により操作する（`Tests/GameplaySampleSmoke`、NativeGameplayDeviceSmoke群）
- 外部利用: 再配置したパッケージを `find_package` で使う（`Tools/PackageConsumer`、`Tools/ValidatePackage.py`）
- 測定: `dxf_character_benchmark`（CTest外）

| 機能 | 実装・単体 | 実World | Component | 実App | 外部利用 | 測定 | 使い方 | 残り |
|---|---|---|---|---|---|---|---|---|
| 形状の接触（符号付き距離と法線） | 済／済 | 済／済 | 済／済 | 済／済 | 済／済 | 済／済（kernels） | `FindShapeContact`（Toolbox）、`QueryContacts`（World、最大32件と総数） | 同心・同距離の面は法線なし（仕様） |
| 初期接触を無視するスイープ | 済／済 | 済／済 | 済／済 | 済／済 | 済／済 | 済／済（costs） | `SweepClosestIgnoringInitialContacts` | なし（SweepClosestの契約は不変） |
| A. 初期重なりの解消 | 済／済 | 済／済 | 済／済 | 済／済（床へのめり込みから復帰） | 済／済 | 済／済 | `ResolveCharacterOverlap`、StepCharacter・Componentでは自動 | 解消不能（Ambiguous・TooDeep・Blocked・上限）は移動せず理由を返すだけ |
| B. 反復滑り（平面・角・稜線・上限） | 済／済 | 済／済 | 済／済 | 済／済（2Dは壁と床の角、3Dは二つの壁の稜線） | 済／済 | 済／済 | `MoveAndSlide` | 接触の保持は8件まで（超えるとContactLimit） |
| C. 接地・坂・吸い付き | 済／済 | 済／済 | 済／済 | 済／済（30度の坂を上り60度の急坂の手前で停止） | 済／済 | 済／済 | `ProbeCharacterGround`、StepCharacter | 動く床はL（Kinematicの床だけ。Dynamicの床は追従しない） |
| D. 段差上り・重力・ジャンプ・着地・天井 | 済／済 | 済／済 | 済／済 | 済／済（段差・ジャンプと着地・低い天井） | 済／済 | 済／済 | `StepCharacter`（上→前→下） | 低い天井の下の段差は上らない（仕様） |
| E. 移動Component（固定更新・入力・補間・寿命・Body一つ・剛体併用の拒否・登録順） | — | — | 済／済（各7ケース） | 済／済（途中の生成と破棄、一時停止と再開） | 済／済（Consumer、Nativeの外部Application） | — | `DCharacterMovement2DComponent` / `3DComponent` | Dynamicとの双方向押し合い・カプセル・足元固定の高さ変更を実装（下記拡張） |
| F. 操作できるサンプル | — | — | — | 済／済（1画面／2画面・索引／総当たりで軌跡がビット単位で一致、再入場・終了後の再起動） | — | — | 開発用ソリューションの `GameplaySample`（[Tab]で2D／3D切替、[N]／[M]で歩行キャラクターの生成／破棄、[V]で2画面） | 実機の目視操作は手動（自動確認は固定入力のみ） |
| G. World問い合わせの索引（自動更新のAABB木） | 済／済（木7ケース） | 済／済（一致6・契約20ケース、故障注入6件、変異12件） | 済／済（Componentは索引経由） | 済／済（索引／総当たりの軌跡の一致） | 済／済（PhysicsOnlyで診断を確認） | 済／済 | 自動（[World問い合わせの索引](QueryAcceleration.md)、任意の診断 `GetQueryDiagnostics`） | Solverの候補生成も索引経由で、総当たりとの一致を検証。詳細は並列実行 |
| I. Solid／Sensorと衝突フィルター（両側の許可） | 済／済 | 済／済（応答・連続衝突・休止・問い合わせ・キャラクター） | 済／済（Sensorを障害物にしない） | 済／済（取得物・圧力板・危険領域） | 済／済（PhysicsOnly） | 済／済（sensor-off／on） | `EColliderResponse`、`FColliderCollisionFilter`、`FWorldQueryFilter::bIncludeSolid／bIncludeSensors` | QueryCategoryとは独立。任意形状の連続Triggerは対象外 |
| J. 接触・TriggerのBegin／Stay／End（成功したStep単位の値のバッチ） | 済／済 | 済／済（総当たりの解析との一致、削除・変更・上限・失敗したStep・確保の故障注入） | — | 済／済（イベント列の一致） | 済／済 | 済／済（sensor-dense ほか） | `SetEventSettings`、`GetEventBatch`（[World相互作用](WorldInteraction.md)） | 1回のStepの間に通過した重なりは通知しない（離散） |
| K. ゲームへの配送（監視・Triggerの領域、物理Stepの後） | — | — | 済／済（固定更新0／複数回、通知中の破棄・購読変更・例外・終了） | 済／済 | 済／済（Gameplay・Native） | 済／済（配送） | `DContactListener2D／3DComponent`、`DTriggerVolume2D／3DComponent` | 予約列は容量を再利用。UI／所有階層配送の確保と区別 |
| L. 動く床（Kinematicの運動・追従・離地速度・挟まれ） | 済／済 | 済／済（`PredictBodyPoint`） | 済／済（横・縦・回転、逆向き歩行、ジャンプ、壁・天井、Teleport、登録順） | 済／済（横・昇降・回転床のコース） | 済／済 | 済／済（moving-floor） | `DKinematicMover2D／3DComponent`、`FCharacterMoveTuning::bFollowMovingGround` | 回転は最大32区間の保守的な経路検査。Dynamicとの押し合いは実装済み。挟み込みは既存の停止・解消不能契約 |
| H. 配布の利用者 | — | — | — | — | 済／済（Native OFF／ON × Debug／Release、Nativeは移動後の実行ファイルを起動） | — | `dxf::physics`だけ／`dxf::framework`／`dxf::native` | SDKのないPCでNative構成は検証できない（未確認として扱う） |
| Distance Joint基盤（J1〜J4） | 済／済 | 済／済 | — | — | 済／済（PhysicsOnly） | 済／済 | [距離Joint](Joints.md)、世代付き所有・Anchor回転・Sleep/Wake・並列・Step確定 | 高速時の連続長保証なし |
| Distance Jointゲーム利用（J5〜J7） | — | 済／済 | 済／済 | 済／済（固定入力・実画素） | 済／済 | 済／済 | Body参照・距離Joint Component・吊り下げ・鎖・動く支点・接続切替 | 追加3種類の現行範囲は下表 |
| Revolute / Hinge | 済／済 | 済／済（自由回転、swing、Frame微分、Limit/Motor） | 済／済 | 済／済（扉・回転腕、固定入力・実画素） | 済／済（Physics/Framework/Native） | 済／済 | `Create/GetRevoluteJoint`、両Component | 主値(-π,π]、反平行Frameは拒否、連続Joint TOIなし |
| Fixed | 済／済 | 済／済（相対Pose、異方性慣性、半回転） | 済／済 | 済／済（荷物・解除、実画素） | 済／済 | 済／済 | `Create/GetFixedJoint`、両Component | 有限反復の誤差。完全剛体の鎖保証なし |
| Prismatic | 済／済 | 済／済（軸自由、動く支点微分、接触停止） | 済／済 | 済／済（スライド・昇降、実画素） | 済／済 | 済／済 | `Create/GetPrismaticJoint`、両Component | Dynamic床へのキャラクター追従は別の未対応 |
| Limit / 有限速度Motor / 目標操作 | 済／済 | 済／済（累積Impulse予算、SubStep、Sleep、失敗履歴） | 済／済（保留要求・到達と停止待ち） | 済／済（既存UI、負荷、Pause/Modal） | 済／済 | 済／済 | `Set/RequestDrive`、`Set/RequestLimits`、`ComputeJointTargetDrive` | Motor努力上限は総Joint反力の上限ではない |
| 性能測定 | — | — | — | — | — | 済／済 | `dxf_character_benchmark`（CTest外） | [問い合わせの大規模化の検証記録](../Development/QueryScale-2026-09-25.md#性能測定)。3Dの詳細判定は2Dより高い |

今後の候補: Mesh Collider、経路探索、任意形状の連続Trigger、アニメーション・描画・アセット・ゲーム構成の拡張。Distance Jointだけを全体の目的にはしません。

R0〜R7 の各構成と試行結果は [World相互作用の検証記録](../Development/WorldInteraction-2026-09-27.md) を参照してください。接触イベント・移動床の利用経路は [World相互作用](WorldInteraction.md) にまとめています。

使い方は[キャラクター移動](CharacterMovement.md)・[World問い合わせの索引](QueryAcceleration.md)、検証は[ゲームプレイ基盤の検証記録](../Development/Gameplay-2026-09-25.md)・[問い合わせの大規模化の検証記録](../Development/QueryScale-2026-09-25.md)を参照してください。


現行の新3種類と最終試行は[仕掛け検証記録](../Development/MechanismJointsCompletion-2026-10-01.md)、Distance時点の過去結果は[当時の記録](../Development/JointGameplayCompletion-2026-10-01.md)で区別します。

## 過去の進行記録

以下は各段階当時の状態・結果・残課題です。古いbranch、未実装、未実行の表記を現在へ読み替えません。2026-10-01の現行状態は上の恒久表とJoint検証記録を起点にしてください。

### 当時の作業状態

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

## 実装単位(M〜P: カプセル・Dynamic押し合い・Solver BroadPhase)

| 段階 | 内容 | 場所 | 試験 |
|---|---|---|---|
| M. カプセルの幾何 | 完了 | `FCapsule`／`FCapsule2D`（`Toolbox/Capsule.h`／`Capsule2D.h`）、`CapsuleContact2D/3D.h`、`CapsuleQuery2D/3D.h` | `CapsuleGeometryTests` |
| N. Worldへの統合 | 完了 | `FPhysicsWorld2D`／`3D`のColliderと問い合わせ（`ColliderId2D/3D.h`の`FCapsule`／`FCapsule2D`） | `CapsuleWorldTests` |
| O. キャラクターのカプセル | 完了 | `CharacterMovement2D/3D`（`ECharacterShape::Capsule`、`HalfHeight`、足元を保つ高さ変更） | `CharacterCapsuleTests` |
| P. Dynamicの押し合い | 完了 | `StepCharacter`の押す／押される要求（`CharacterPushSet2D/3D.h`） | `CharacterPushTests` |
| P'. Solver BroadPhase | 完了 | `SolverPairs.h`、`FPhysicsWorld2D/3D::SetSolverBroadPhaseEnabled_Internal` | `SolverBroadPhaseTests` |

### Solver BroadPhaseの契約

- Solverとイベントの組の候補を、Worldが保つ問い合わせの索引（AABB木）から集める。別の木は作らない。
- 候補は「動く側」（Solverは`Dynamic`、イベントは`Static`以外）のColliderから、葉の境界を
  Marginと丸めの余白だけ広げた範囲で探す。範囲の重なりは対称なので同じ組を二度数えない。
- 索引の候補は`(First, Second)`の昇順に並べ直してから詳細判定する。総当たりと同じ順で同じ組を調べる。
- 索引に入れられないColliderや、座標が大きすぎる場合は総当たりへ戻る（結果は同じ）。
- 並列の経路と連続衝突の候補は切り替えない。

### 検証（このSHA: e4ba140 + 未commitのS5）

- `dxf_physics_tests.exe`（Release）: 27群すべて成功。3回連続で同じ結果（終了コード0）。
- `dxf_physics_tests.exe`（Debug）: 終了コード0。
- `dxf_tests.exe`: 364/364。
- `python Tools/CheckNoStl.py`: 645ファイル、違反0。

## 個別記録(KD): 3Dの箱と箱の接触の調査（2026-09-28に訂正）

- `fafaee9`の記録は「SATで2本目の箱の投影半径が1本目の値になっていた」としたが、`e4ba140`のコードは
  2本目の箱に`B.HalfExtents`を使っており、この原因は事実でなかった。記録を訂正する。
- 記録された最小再現（床＋`Dynamic`の1m箱（x=2）＋壁（x∈[2.5,4.5]、Zの厚さは箱と同じ）、+Xの速度で60回Step）を
  `BoxWallContactTests`として追加した。修正前の判定（辺の外積の長さ>1e-9）でも修正後の判定でも、2D／3Dとも箱は
  x≈2.0で止まり、不具合は再現しなかった。
- そのため、根拠のない挙動の変更（ほぼ平行な辺の組を捨てる閾値の変更）は元へ戻した。軸の半径を返す
  `HalfExtent_Internal`への置き換えは同じ値を返す整理なので残した。回帰試験は残す。

## 現在状態（2026-09-28時点）

恒久表の行M〜P（カプセル幾何／World統合／キャラクターのカプセル／Dynamic押し合い／Solver BroadPhase）は
すべて実装済み。詳細な契約と検証結果は[Dynamic相互作用の検証記録](../Development/DynamicCharacterInteraction-2026-09-28.md)を参照。
下部の過去の個別記録は、当時の記録としてそのまま残す。

| 項目 | 現状 |
|---|---|
| Character→Dynamicの押し | 実装済み。既定は無効。`FCharacterMoveTuning`の`bPushDynamicBodies`で有効化し、接触法線の逆向きに`MaxPushImpulse`以下を与える |
| Dynamic→Characterの押され | 実装済み。`bReceiveDynamicPush`。既存の障害物規則を使い壁・床・天井を貫通させない |
| 挟まれ・圧迫 | 実装済み。解消不能時は`ECharacterRecoveryStatus`と`ECharacterMoveStop`で理由を返す |
| カプセル | 実装済み。2D／3D共通。足元を保つ高さ変更、低い通路、天井下での非拡張を検証 |
| Solver BroadPhase | 実装済み。組の候補を問い合わせの索引から集め、総当たりと同じ順・同じ結果。索引不可時は総当たりへ戻る |
| 3Dの奥行き50m箱の沈み | Solverの不具合ではない。試験条件の慣性不適切（M10用Componentは立方体に変更）。回帰試験を残す |
| DebugのPhysicsContinuation | 既存180秒の制限内で101.33秒。timeoutは延長していない |

## 残課題

- 接触イベント（Begin／Stay／End）とTriggerの実装は未着手
- 完全なIsland管理（接触島単位の一括起床・休止判定）は未実装。
  現行は既知接触の相対運動による一段ずつの起床伝播まで
- 線形CCDは対応形状の組に接続済み。回転を伴う箱・重心外Colliderの線形扱いは未対応で、
  現行は離散フォールバックへ回す。明示的拒否とopt-in診断は将来課題
- Joint、Mesh動的対応は未実装
- Linux sanitizer、実DxLib SDKでの検証は未実行
- BroadPhase（総当たり比較つき）と性能測定は未実施
