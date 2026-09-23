# 2D Physics Worldへの最短線分問い合わせ 検証記録

開始SHA: `bc4cfc8cf5569281bab429c28884073513df4161`。main / origin/main一致、開始時の作業ツリーはクリーン（未追跡・stage済み変更なし）。過去の記録（3Dの[WorldSegmentQuery-2026-09-24.md](WorldSegmentQuery-2026-09-24.md)、Native診断記録）は変更していない。対象は本記録と同じコミットの実装・試験・文書。

## 変更と責務

- `FPhysicsWorld2D::RaycastClosest(Start, End, ExcludedBody = {}) const` と値型 `FWorldSegmentHit2D`（`Dxf/WorldSegmentHit2D.h`）を追加。現在のColliderスロットを直接走査し、既存の `ToWorld_Internal`（Body角度＋Colliderのローカル角度・ローカル中心）と新しい2D交差関数を使う。結果・失敗・Step状態・除外・順序の契約は3D版と同じ。
- Toolboxへ `IntersectSegment(FVector2, FVector2, const FCircle2D&)` と `IntersectSegment(FVector2, FVector2, const FOrientedBox2D&)` を追加（`Toolbox/SegmentIntersection2D.h`、実装は既存の `SegmentIntersection.cpp`）。円は既存の2D円Sweep（半径0・許容距離0）を再利用し、回転矩形は `Contact2D` と同じ局所座標規約（中心差を取ってから逆回転、f64）で区間交差を行う。外接矩形では代用しない。
- 結果型とのinclude循環を避けるため、`FBodyId2D` / `FColliderId2D` を `Dxf/BodyId2D.h` / `Dxf/ColliderId2D.h` へ移動（3Dと同じ配置）。型名・フィールド・順序・`RigidBody2D.h` からの従来の利用は保持。
- Physicsの積分・接触・CCD・休止、3D問い合わせ、`SegmentIntersection.h`（3D版）の内容、Snapshot選択、Scene/Scope、モデル・描画、サンプルは変更していない。PhysicsからSupport/Debug/Native/描画への依存は追加していない。
- 通常経路はO(保持Colliderスロット数)、追加領域O(1)。配列確保の不在は実装確認であり、速度・割当回数の計測結果ではない。

[API・2D/3Dの対応表・短い例](../Physics/WorldSegmentQuery.md)。

## 構成と結果（最終コード）

Windows x64、Visual Studio 18 Community / MSVC 19.51、C++20。既存 `Build/FbxContinuation`（Visual Studio 18 2026 Generator、Native・ModelViewer・RenderDebug・NativeSmoke・実デバイス試験ON、`ThirdParty/DxLib-3.25a-source`を自動選択）を再生成して使用。SDKの再構築は行っていない。最終コード（Docsを除く差分と新規ファイル）の指紋は `Build/WorldSegment2D-RootLogs/code-fingerprint.txt`。

| 工程 | 実行結果 | 終了コード |
|---|---|---|
| root再生成・ビルド | Debug / Release成功。新規ファイル由来の警告なし | 各0 |
| root CTest（`-j 1 --no-tests=error --output-on-failure`、構成間直列） | Debug 25/25、Release 25/25。実デバイス4群（NativeSandbox / Native / NativeModel / NativePhysicsDebug）を含む | 各0 |
| PhysicsContinuation内部（root・単独とも） | 各構成168/168。うち新規2D 9/9、既存3D問い合わせ7/7 | 各0 |
| NativeModelDeviceSmoke | Debug・Releaseとも `REAL_SDK_MODEL_SMOKE_PASSED failures=0`（全群内の各1回。単独の反復は行っていない） | 各0 |
| 単独Debug検証 `python Tools/ValidateDebug.py --logs Build/WorldSegment2D-StandaloneLogs` | Debug 21/21、Release 21/21、構成・ビルド・登録・実行の8工程PASS。実DxLib SDKは不使用 | 0 |
| No-STL | 315ファイル、違反0 | 0 |
| Python `unittest discover -s Tools/Tests` | 24/24 | 0 |
| 配布 `python Tools/ValidatePackage.py --logs Build/WorldSegment2D-PackageLogs`（x64開発者環境） | Native OFF・Debug、8段階PASS。`physics-only-run`に2D追加 | 0 |
| `git diff --check` | 問題なし。UTF-8・CRLF（BOMなしのまま）を確認 | 0 |

CTest群はroot 25、単独21のまま。新規9ケースは既存PhysicsContinuationへ追加した（`CMake/PhysicsTests.cmake` の正規登録。単独入口に本体cppを手列挙していない）。CTest群と内部ケースを足し合わせていない。新しい実行ファイル・通常ソリューションのプロジェクト・Viewerは追加していない。

配布は移動したインストールを使い、`dxf::physics` だけにリンクした `PhysicsOnly` が、従来の3D（生成→登録→問い合わせ→自己除外→削除→失効）に続き、2Dの生成→円・矩形の登録→問い合わせ（割合0.25・交点(-1,0)）→自己除外で次の候補（割合0.45）→削除→失効を実行した。段階数は8のまま。SDK未導入PCでの検証ではない。

## 新規回帰の範囲（`Tests/Physics/WorldQuery2DTests.cpp`）

数学の基準値は解析解で、3D版との一致や往復を根拠にしていない。許容誤差は角度0の計算で1e-12（f64の二次式・区間計算）、f32の角度（π/2・π/4・π/6）を含む場合は `cos(f32(π/2))≈-4.4e-8` 等による局所座標の誤差を見込んで1e-6。

1. 円: 中心(0,0)半径1を(-2,0)→(2,0)で割合0.25、縦方向0.4、接線0.5、終点境界1、手前・接線外・遠ざかる線分の非交差、内部・境界始点0、半径0（点）、不正形状・NaN。
2. 回転矩形: 軸平行0.3/0.4、辺に沿う接触、角だけを通る0.5、内部始点、90度回転0.4、45度のひし形(5-√2)/10、回転方向を検出する非対称な30度（(4+√3)/10、逆回転なら(5-√3)/10）、外接矩形なら当たる位置の非交差、半幅0の辺・点、中心が遠い場合、不正形状・NaN角度。
3. 実World: 空World、ColliderなしBody、Static/Kinematic/Dynamic、最短と交点(2,0)、Body角度＋Colliderのローカル中心、Body45度＋Collider45度（二重回転なら割合が変わる）、非対称なBody角度30度、回転Bodyのローカル中心の円、1Body複数Collider。
4. Stepなしの姿勢変更・着脱・Body削除とスロット再利用・Collider単独の再利用、疎なスロット、同距離のスロット昇順、自己Body全Colliderの除外、保存IDの失効、旧世代・`FBodyId2D{}`・別World（同じスロット番号）の拒否、省略と空Optionalの一致。
5. Debug表示上限を超える301番目のCollider。
6. 空Worldのゼロ長・NaN・Inf・変位の桁あふれ、先行ヒット割合0の後に続く変換不能形状の失敗、除外Bodyの形状は計算しないこと。
7. 3D回帰と同じ故障注入（停止済みJobSystem）でStepを途中失敗させ、問い合わせ拒否→正常Step後の回復。Step引数の拒否だけでは禁止しない。
8. 問い合わせあり／なしの実Worldへ同じ力・トルクを与え、20Stepの位置・角度・速度・角速度・休止・Step数が一致。蓄積力が消えていないこと（速度が正）も確認。
9. 接地して600Step後に休止したBodyを100回選択しても非起床・位置不変・Step数不変。

Step実行中に別スレッドから問い合わせる試験は行っていない（契約上、外側で直列化する。データ競合をガードの試験に使わない）。

## 試験先行・変異試験

- 試験と登録を先に追加し、実装前にビルド: `dxf_physics_tests` がAPI不在のコンパイルエラー90件で失敗（終了1、`Build/WorldSegment2D-RootLogs/tdd-red-compile.log`）。挙動のRed（実装済みAPIでの失敗）は経ていない。
- 実装後: 168/168（Debug）。その後、全回転試験が線分に対し対称で回転方向の誤りを検出できないことに気付き、非対称な30度の試験を追加した。
- 変異試験（新規コードへ一つずつ故意の欠陥を入れ、ビルド・実行し、元に戻す）: 10件すべて検出（`Build/WorldSegment2D-RootLogs/mutation.log`）。
  矩形が角度を無視／回転方向の反転／円の許容距離0.01／同距離で後のスロットを選ぶ／除外を無視／割合0で走査打切り／Step状態ガードなし／除外IDを検証しない／交点を始点にする／ゼロ長を受理。

## 途中の失敗

- 最初のroot全体ビルドはDebug・Releaseとも失敗（終了1、`Build/WorldSegment2D-RootLogs/attempt1-*-build-failed.log`）。2D交差を既存の `SegmentIntersection.h` へ追加し2D形状のヘッダーをincludeしたため、`using namespace Toolbox; using namespace Dxf;` を使う既存の `Tests/ViewCoordinatesTests.cpp` で `FVector2` が曖昧になった。3D版の利用側へ2Dの型を持ち込まないよう、専用の `SegmentIntersection2D.h` へ分離し、3D版ヘッダーを元の内容へ戻した。Physics単独ターゲットのビルドだけでは検出できなかった。
- 新規試験の定数式 `Max * 2` がC4056警告を出したため、実行時の乗算へ変更。上記の最終結果はこの修正後の再ビルドで得たもの。

## モデル実描画の偶発終了

今回のroot全群（Debug・Release各1回）ではNativeModelDeviceSmokeは成功し、早期終了は発生しなかった。これは限定回数の結果であり、以前の偶発終了の原因は引き続き特定していない。「修正した」「再発しない」とは扱わない。bc4cfc8で追加した診断・厳格な判定はそのまま保持した。

## 再実行とログ

```powershell
cmake -S . -B Build/FbxContinuation
cmake --build Build/FbxContinuation --config Debug --parallel 8
ctest --test-dir Build/FbxContinuation -C Debug -j 1 --no-tests=error --output-on-failure
cmake --build Build/FbxContinuation --config Release --parallel 8
ctest --test-dir Build/FbxContinuation -C Release -j 1 --no-tests=error --output-on-failure
python Tools/ValidateDebug.py --logs Build/WorldSegment2D-StandaloneLogs
python Tools/CheckNoStl.py
python -m unittest discover -s Tools/Tests -v
# Visual Studioのx64開発者環境で実行する。
python Tools/ValidatePackage.py --logs Build/WorldSegment2D-PackageLogs
```

root・Python・No-STLのログは `Build/WorldSegment2D-RootLogs/`（`*-build-final.log`、`*-ctest.log`、`*-LastTest.log`、`*-ctest-list.log`）、単独は `Build/WorldSegment2D-StandaloneLogs/`（Summary.json、`physics-cases-*.log`）、配布は `Build/WorldSegment2D-PackageLogs/`。生成物・ログ・SDKはコミットに含めない。

## 未実施・対象外

SDK未導入PC、開発ツール未導入PC、実Direct3D9、物理入力、音声の聴感、全D3D/COM資源のリーク列挙、非WindowsのSanitizer、速度・割当の専用測定は未実施。配布はNative無効・Debugであり、Native有効配布やRelease配布、別PCでの成功へ読み替えない。全Hit一覧、カテゴリ／マスク、接触イベント、法線、Sweepの公開World API、空間索引、2D画面選択には進めていない。
