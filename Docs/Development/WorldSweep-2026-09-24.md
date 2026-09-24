# 2D／3Dの円・球スイープ問い合わせ（SweepClosest） 検証記録

開始SHA: `364b0a0f87062742710ba505225109b15fcc3bc3`。main / origin/main一致、開始時の作業ツリーはクリーン。過去の記録（範囲問い合わせ・対象フィルター・2D／3D線分問い合わせ・Native診断）は変更していない。対象は本記録の直前のコミットの実装・試験・文書。

## 設計の確定

- 公開API: `FPhysicsWorld2D::SweepClosest(const FCircle2D& StartShape, FVector2 EndCenter, TOptional<FBodyId2D> ExcludedBody = {}, const FWorldQueryFilter& Filter = {}) const`と3Dの`FSphere`／`FVector3`版。指令書の名前・引数順・既定値のまま。結果は新規の`FWorldSweepHit2D`／`FWorldSweepHit3D`（`Collider`、f64の`Fraction`、問い合わせ形状の中心`CenterAtHit`、`bInitialContact`）。既存の`FWorldSegmentHit`の`Position`（接触表面の点）とは意味が違うため、型を共有していない。法線は返さない。
- 再利用の監査: 既存の`Toolbox::Sweep`（ContinuousCollision）は変位を受け取り、代表法線とCCD用の猶予を含むため、World問い合わせの契約（終点の中心、許容距離0、法線なし）へ直接は使えない。その内部の`PointBallEntry_Internal`（相対位置の点が半径の球へ入る最初の時刻。外積による安定した判別式、開始時の包含、離れる向きの扱いを含む）はそのまま再利用し、公開の入口`Toolbox::SweepToCenter`（`ShapeSweep2D.h`／`ShapeSweep3D.h`、結果は`ShapeSweepHit.h`の`FShapeSweepHit`）を追加した。既存`Sweep`の契約・式は変更していない。
- 箱（回転矩形・OBB）: 面・辺・頂点の各特徴（軸ごとに下限・上限・自由の3状態、全自由の内部状態を除く）について、自由な実軸のGram行列で射影した残差を`PointBallEntry_Internal`へ渡し、特徴の有限範囲に入る区間と[0,1]の交わりの中の最初の時刻を選ぶ。OBBの軸は正規化・直交化しない（`IntersectsSphere`と同じ平行六面体）。2Dの回転矩形は`Contact2D`と同じ反時計回りの角度規約の軸`{{c,s},{-s,c}}`で3Dと同じ計算へ流す。開始時の接触は既存の`IntersectsSphere(球, OBB, 0)`／`Intersects(円, 回転矩形, 0)`で判定し、`Fraction=0`・`bInitialContact=true`。
- World: 入力検査（負の半径・非有限・f32で表現できない移動量）→ 半径0かつ移動ありは既存`RaycastClosest`へ委譲（`Position`→`CenterAtHit`、割合0を初期接触）→ 状態ガード（`RequireQueryState_Internal`）→ 除外IDの検査 → スロット昇順の走査（非生存・カテゴリ不一致・自己Bodyを変換前に除外）→ `Resolve_Internal`・`ToWorld_Internal`で現在の姿勢へ変換 → `SweepToCenter` → 時刻が[0,1]の有限値であることを検査 → 厳密な最小（同じ割合は先のスロット）。割合0でも走査を打ち切らない。`CenterAtHit`はf64の線形補間をf32へ丸め、有限でなければ例外。新しいWorld登録・Map・配列確保・Snapshot・Jobは作らない。
- 既存の`FWorldQueryFilter`・`QueryCategory`・`RaycastClosest`・`OverlapAll`・Description・Snapshotのスキーマ・Solver・CCDは変更していない。

## 構成と結果（最終コード）

Windows x64、Visual Studio 18 Community / MSVC 19.51、C++20。`Build/FbxContinuation`（Visual Studio 18 2026、x64、`ThirdParty/DxLib-3.25a-source`を自動選択）を、TESTS・NATIVE・STARTER・EXAMPLE・MODEL_VIEWER・RENDER_DEBUG・NATIVE_SMOKE・RUN_DEVICE_TESTSを明示ONにして再生成（終了0）。SDKの再構築は行っていない。

| 工程 | 実行結果 | 終了コード |
|---|---|---|
| rootビルド | Debug / Release成功。追加・変更した行に警告なし（既存箇所のC4324等は変更前から。`new-code-warnings.log`） | 各0 |
| root CTest（`-j 1 --no-tests=error --output-on-failure`、Debug→Releaseの直列） | Debug 26/26、Release 26/26（実デバイス群を含む） | 各0 |
| PhysicsContinuation内部（root・単独とも） | 各構成228/228。うち新規のスイープ問い合わせ20 | 各0 |
| PhysicsOverlapFault（root・単独とも） | 各構成16/16（スイープの確保なし確認2を含む） | 各0 |
| NativeModelDeviceSmoke | Debug・Releaseとも `REAL_SDK_MODEL_SMOKE_PASSED failures=0`（全群内の各1回、単独の反復なし） | 各0 |
| 単独 `python Tools/ValidateDebug.py --logs Build/WorldSweep-StandaloneLogs` | Debug 22/22、Release 22/22、8工程PASS。実DxLib SDK不使用 | 0 |
| 公開ヘッダーの単独コンパイル（cl /W4 /WX /permissive-、プロジェクトと同じく/wd4324） | `RigidBody2D.h`、`RigidBody3D.h`、`WorldSweepHit2D.h`、`WorldSweepHit3D.h`、`ShapeSweep2D.h`、`ShapeSweep3D.h`、`ShapeSweepHit.h`、2D＋3D（両順序）、`Dxf/ViewCoordinates.h`＋World（3D両順序・2D）、ShapeSweep2D＋3Dの13通り | 0 |
| No-STL | 325ファイル、違反0 | 0 |
| Python `unittest discover -s Tools/Tests` | 24/24 | 0 |
| 配布 `python Tools/ValidatePackage.py --logs Build/WorldSweep-PackageLogs`（x64開発者環境） | Native OFF・Debug、8段階PASS（1回目で成功） | 0 |
| `git diff --check` | 問題なし。変更ファイルはUTF-8・CRLF（BOMなしのまま） | 0 |

CTest群の数はroot 26・単独22のまま（スイープは既存のPhysicsContinuationと隔離済みのPhysicsOverlapFaultへ正規登録で追加。新しい群はない）。群数と内部ケースを足し合わせていない。

コード指紋（Docsを除く差分と新規ファイル）: 上記のroot・単独・ヘッダー・No-STL・Python・配布・差分検査はすべて同じ`5f4a6db5…81b2`のコードで実行し、実行後の再計算も一致した（`fingerprint-before.txt`／`fingerprint-after.txt`）。

配布は移動したインストールを使い、`dxf::physics`だけにリンクした`PhysicsOnly`が、既存の線分・フィルター・範囲の手順に続けて次を実行した（関数を分けて変数名の衝突を避けた）。2D: カテゴリ付きの障害物だけを対象に、中心線の射線は外れるが円のスイープは壁に当たり中心x≈4.2を返す→フィルターと除外なしでは自己Bodyへの初期接触（割合0）→壁のカテゴリ0化でStepなしに通り、戻すと再び当たる→壁に接した開始位置では初期接触→Body削除後は保存IDが失効し、スイープも当たらない。3D: 回転したOBBの壁で同じ流れ（射線は外れ、球は0<割合<1で当たる→カテゴリ0化→初期接触→削除後の失効）。段階数は8のまま。SDK未導入PCでの検証ではない。

## 新規回帰の範囲

`Tests/Physics/WorldSweepTests.cpp`（20ケース、既存PhysicsContinuationへ`CMake/PhysicsTests.cmake`で正規登録）。期待値は手計算できる配置の解析値で、製品の関数から作っていない。

- 形状（Toolbox、2ケース）: 円／球同士は、正面の接触（中心(3.5,0)・割合0.35、G01）、終点を変位と取り違えると変わる配置（G02）、途中の接線接触と2^-10離した非交差（G03）、終点ちょうど（割合1、G04）、開始時の重なり・境界接触は離れる向きでも割合0（G05）、開始＝終点（G06）、半径0・ゼロ移動の点の重なり（G07）、2^20の共通オフセットと長い移動・小さい半径（G18）。箱は、2Dの角・3Dの頂点・辺・面（膨張した外接箱なら割合0.5になる配置、G08〜G12）、半幅(2,0.5)の+30度と-30度で異なる最初の中心（G13）、格納したf32の軸長誤差とせん断の軸を正規化しないこと（G15）、半幅0の辺・点・面（G16）、両端が離れた途中の厚さ0の板の通過（割合は格納した`0.1f`を使う(5-r)/10、G17）。
- World（2D／3Dの各9ケースと、3Dの非平面変換・2Dの回転矩形Worldの各1ケース）: 空World・Colliderなし・最短・結果メンバー・同じ割合のスロット順・複数の初期接触（W01・W02）、半径0の移動と`RaycastClosest`の一致（G07）、カテゴリ・自己除外・完全なID・Stepなしの反映（W03〜W06、W14）、入力検証（空World・マスク0でも）・候補外の変換不能形状・割合0の後の後続の計算失敗・Step状態・失敗後の再利用（W07〜W10）、問い合わせの有無で数値経過が一致し対象の速度が結果に影響しない（W11・W12。対照Worldにも同じ速度の設定と解除）、休止中のBodyも対象で起こさない、Debug表示の上限を超える300件の後ろのスロット（W16）、Body姿勢（X軸回り90度）＋ローカル中心＋非等方の半幅（G14）、2DのBody角度＋Colliderローカル角度（G13のWorld経路）。
- ゲーム利用（W15、2D／3D）: 中心線の射線は外れるが半径0.5の円／球は下面y=0.4の壁の角に当たり、返るのは中心(4.2,0)（接触表面ではない）。初期接触なら動かさない方針の例、距離単位の余白Skinの割合への換算、止まった中心からの再問い合わせは同じ壁で割合1e-6未満、壁のカテゴリ0化でStepなしに終点まで通る（壁の登録は残る）。

`Tests/Physics/OverlapAllocationFaultTests.cpp`（隔離した`PhysicsOverlapFault`）へ、2D／3Dのスイープ通常経路（ヒット・離れる向き・静止・マスク0・半径0）で確保が起きないことの確認を各1項目追加（14→16項目）。区間内の確保の有無の証拠であり、速度や全体の割当回数の測定ではない。

## 試験先行・期待値の誤り・変異試験

- 試験と登録を先に追加して実装前にビルド: `Toolbox/ShapeSweep2D.h`が存在しないコンパイルエラー（C1083、終了1、`Build/WorldSweep-RootLogs/tdd-red-compile.log`）。
- 実装後の最初の実行（`green-debug.log`）: 225/228。失敗した3件はいずれも**試験の期待値の誤り**で、製品の不具合ではなかった。(1) G17の2Dの薄い板で、厚さ`0.1f`のf32値（0.100000001490…）を無視した期待値0.49を1e-12で比較していた（正しくは0.48999999985098835。一時的な確認プログラムで値を確認）。(2)(3) ゲーム利用の2D／3Dで、f32へ丸めた`CenterAtHit`から再問い合わせすると必ず初期接触になると仮定していた。丸めの向きによって、ごくわずかな正の割合で同じ壁に当たる場合がある。期待値を「同じ壁で割合が1e-6未満」へ修正し、この性質を文書に明記した。修正後228/228（`green-debug-2.log`）。製品コードはこの間変更していない。実行前に、ゲーム利用の壁の配置の誤り（y=1.4では接触しない）に気付き修正した。
- 変異試験（新規コードへ一つずつ故意の欠陥を入れ、ビルド・実行し、元へ戻す。`mutate.py`・`mutation.log`）: 実施した10件すべて検出、各復元はSHA-256で一致、復元後のビルド終了0。2Dの半径を0として扱う／箱の面だけ（辺・頂点の丸みなし）／終点を変位として扱う／2Dの回転の符号反転／OBBの軸を正規化／特徴の有限範囲を除去／初期接触を非ヒット扱い（3D OBB）／[0,1]の上限を除去／3Dのカテゴリ判定を最短選択の後へ／2Dで割合0の打切り。

## モデル実描画の偶発終了

今回のroot全群（Debug・Release各1回）ではNativeModelDeviceSmokeは成功し、早期終了は発生しなかった。限定回数の結果であり、以前の偶発終了の原因は引き続き特定していない。bc4cfc8の診断・厳格な判定、画素条件・タイムアウトは変更していない。

## 再実行とログ

```powershell
cmake -S . -B Build/FbxContinuation -DDXF_BUILD_TESTS=ON -DDXF_BUILD_NATIVE=ON -DDXF_BUILD_STARTER=ON -DDXF_BUILD_EXAMPLE=ON -DDXF_BUILD_MODEL_VIEWER=ON -DDXF_BUILD_RENDER_DEBUG=ON -DDXF_BUILD_NATIVE_SMOKE=ON -DDXF_RUN_DEVICE_TESTS=ON
cmake --build Build/FbxContinuation --config Debug --parallel 6
ctest --test-dir Build/FbxContinuation -C Debug -j 1 --no-tests=error --output-on-failure
cmake --build Build/FbxContinuation --config Release --parallel 6
ctest --test-dir Build/FbxContinuation -C Release -j 1 --no-tests=error --output-on-failure
python Tools/ValidateDebug.py --logs Build/WorldSweep-StandaloneLogs
python Tools/CheckNoStl.py
python -m unittest discover -s Tools/Tests -v
# Visual Studioのx64開発者環境で実行する。
python Tools/ValidatePackage.py --logs Build/WorldSweep-PackageLogs
git diff --check
```

rootのログは`Build/WorldSweep-RootLogs/`（configure・build・ctest・`*-LastTest.log`、指紋、Red・変異・ヘッダー確認・No-STL・Python・配布のコンソール出力）、単独は`Build/WorldSweep-StandaloneLogs/`、配布は`Build/WorldSweep-PackageLogs/`。生成物・ログ・SDKはコミットに含めない。

## 未実施・対象外

SDK未導入PC、開発ツール未導入PC、実Direct3D9、物理入力・聴感、全D3D/COM資源のリーク列挙、非WindowsのSanitizer、速度の測定は未実施。配布はNative無効・Debugであり、Native有効配布・Release配布・別PCの成功へ読み替えない。法線・接触点・侵入量、全ヒット一覧、SweepAny、箱・カプセル・凸形状の移動、回転・半径変更、相手の速度を含む予測、押し戻し・滑り・キャラクターコントローラー、BVH、並列化、Viewerへの表示には進めていない。
