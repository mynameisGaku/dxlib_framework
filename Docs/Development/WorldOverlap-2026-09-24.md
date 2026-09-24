# 2D／3Dの範囲問い合わせ（OverlapAll） 検証記録

開始SHA: `ea73b413648191c60afbf821697ed28ff70e51c0`。main / origin/main一致、開始時の作業ツリーはクリーン（未追跡・stage済み変更なし）。過去の記録（対象フィルター・2D／3D線分問い合わせ・Native診断）は変更していない。対象は本記録と同じコミットの実装・試験・文書。

## 設計の確定

- 公開API: `FPhysicsWorld2D::OverlapAll(const FCircle2D& Area, TOptional<FBodyId2D> ExcludedBody = {}, const FWorldQueryFilter& Filter = {}) const` と3Dの`FSphere`版。戻り値は値所有の`TVector<FColliderId2D/3D>`、生存する対象Colliderのスロット昇順、Collider単位、交点・割合・法線なし。指令書の第一候補どおりで、名前・引数順・既定値の変更はない。
- 3Dの形状判定: 既存の公開`Intersects(const FCollisionShape&, const FCollisionShape&, Tolerance)`は`Tolerance <= 0`を例外とし、球専用経路の前に`Bounds`を計算するため、World経路で許容距離0を明示できない。既存関数の契約は変えず、球専用の距離計算（`SphereObbDistanceSquared_Internal`。球同士の距離は同じ式を`SphereSphereDistanceSquared_Internal`へ抽出）を直接使う`Toolbox::IntersectsSphere(球, 球／OBB, Tolerance)`を追加した（既定値なし、0可）。名前を分けたのは、`FSphere`を直接渡す既存の3引数呼出しが新しい多重定義へ吸い込まれ、許容距離0の可否や境界箱の検査の有無が黙って変わるのを避けるため。
- 2Dの形状判定: 円同士は既存`Toolbox::Intersects(円, 円, 0)`を再利用（既存は0を受け付ける）。円と回転矩形は、`Contact2D`と同じ反時計回りの角度・逆回転規約で、中心差を最初からf64で求める最近点距離の真偽判定`Toolbox::Intersects(const FCircle2D&, const FOrientedBox2D&, f32 Tolerance)`を`Contact2D.h/.cpp`へ追加（既定値なし）。`FindContact`・Solverの式は変更していない。3D専用の`SegmentIntersection.h`へ2D型は入れていない。
- World: 既存の`RequireQueryState_Internal`・`Resolve_Internal`・`ToWorld_Internal`とColliderスロットの直接走査を再利用。順序は、入力検査→状態ガード→除外IDの検査→スロット昇順（非生存・カテゴリ不一致・自己Bodyを除外）→所有Bodyの解決と形状変換→許容距離0の判定→一致したIDだけをローカル配列へ追加。例外時はローカル配列が破棄され、部分結果は外へ出ない。新しいWorld登録・Map・Snapshot・Jobは作らない。
- 既存の`FWorldQueryFilter`・`QueryCategory`は変更せず、コメントを線分と範囲の両方に通用する表現へ更新した。Description・Hit型・Snapshotのスキーマ・RaycastClosest・カテゴリ操作・接触応答は変更していない。

## 構成と結果（最終コード）

Windows x64、Visual Studio 18 Community / MSVC 19.51、C++20。`Build/FbxContinuation`（Visual Studio 18 2026、x64、`ThirdParty/DxLib-3.25a-source`を自動選択）を、TESTS・NATIVE・STARTER・EXAMPLE・MODEL_VIEWER・RENDER_DEBUG・NATIVE_SMOKE・RUN_DEVICE_TESTSを明示ONにして再生成（終了0）。SDKの再構築は行っていない。

| 工程 | 実行結果 | 終了コード |
|---|---|---|
| rootビルド | Debug / Release成功。新規コード由来の警告なし | 各0 |
| root CTest（`-j 1 --no-tests=error --output-on-failure`、Debug→Releaseの直列） | Debug 26/26、Release 26/26。新規群`PhysicsOverlapFault`と実デバイス4群を含む | 各0 |
| PhysicsContinuation内部（root・単独とも） | 各構成208/208。うち新規の範囲問い合わせ20、フィルター20、2D線分9、3D線分7 | 各0 |
| PhysicsOverlapFault（root・単独とも） | 各構成14/14 | 各0 |
| NativeModelDeviceSmoke | Debug・Releaseとも `REAL_SDK_MODEL_SMOKE_PASSED failures=0`（全群内の各1回、単独の反復なし） | 各0 |
| 単独 `python Tools/ValidateDebug.py --logs Build/WorldOverlap-StandaloneLogs` | Debug 22/22、Release 22/22、8工程PASS。`PhysicsOverlapFault`を必須群へ追加。実DxLib SDK不使用 | 0 |
| 公開ヘッダーの単独コンパイル（cl /W4） | `RigidBody2D.h`、`RigidBody3D.h`、`WorldQueryFilter.h`、`Contact2D.h`、`CollisionShapes.h`、2D＋3Dの同時include | 各0 |
| No-STL | 319ファイル、違反0 | 0 |
| Python `unittest discover -s Tools/Tests` | 24/24 | 0 |
| 配布 `python Tools/ValidatePackage.py --logs Build/WorldOverlap-PackageLogs`（x64開発者環境） | Native OFF・Debug、8段階PASS（2回目。1回目は下記の失敗） | 0 |
| `git diff --check` | 問題なし。変更ファイルはUTF-8・CRLF（BOMなしのまま） | 0 |

CTest群はroot 25→26、単独21→22（新規`PhysicsOverlapFault`）。範囲問い合わせの20ケースは既存PhysicsContinuationへ正規登録（`CMake/PhysicsTests.cmake`）で追加し、故障注入の実行ファイルも同じ登録関数でrootと単独の両方へ登録した。単独入口へ本体cppを手列挙していない。群数と内部ケースを足し合わせていない。

コード指紋（Docsを除く差分と新規ファイル）: root・単独・ヘッダー確認は`2a57d609…2c47`のコードで実行した。その後、配布Consumerの生成スクリプト`Tools/ValidatePackage.py`だけを修正し（下記）、最終コード`fc55e843…b5e8`で配布・Python・No-STL・差分検査を再実行した。このスクリプトはroot・単独のビルドやCTestに含まれない。

配布は移動したインストールを使い、`dxf::physics`だけにリンクした`PhysicsOnly`が、既存の2D／3D線分・フィルター手順に続けて次を実行した。2D: 円と回転矩形の登録→カテゴリと自己除外付きのOverlapAllで2件の完全なID→障害物込みRaycastClosestで見通せる候補と壁に遮られる候補を区別→小さい範囲で壁を取得→Stepなしのカテゴリ0化と姿勢変更の反映→Body削除・同スロット再生成後に保存IDを読み替えない。3D: 回転OBBの壁で同じ候補抽出と遮蔽の区別→カテゴリ変更後に射線が通る→削除後のID失効。段階数は8のまま。SDK未導入PCでの検証ではない。

## 新規回帰の範囲

`Tests/Physics/WorldOverlapTests.cpp`（20ケース、実FPhysicsWorld2D／3Dと既存Solverにリンク）。期待値は手計算しやすい配置の解析値で、製品の変換・判定関数から作っていない。

- 形状（Toolbox）: 2D円と回転矩形は、辺への接触（G01）、正の隙間約4.1e-6は許容0では含まず1e-5なら含む（G02）、角の外側(1.8,1.8)は距離の二乗1.28で含まない（G06）、30度回転の半幅(2,0.5)と中心(1.5,1)は正しい向きで含み-30度では含まない（G07）、点・境界・外部（G05）、完全包含（G04）、半幅0の辺・点（G10）、f32で差があふれる距離の正常な非交差と2^20付近の0.125の隙間（G11）、不正入力。3D球と球／OBBも、接触・隙間・角(1.6,1.6,1.6)の距離の二乗1.08・X軸回り90度のOBB（XY平面外の軸、G09）・半幅0の面・半径0・巨大座標・不正な軸。
- World（2D／3Dの各10ケース）: 空World・Colliderなし・全非交差（W01）、接触を含み隙間を含まない、重心が範囲外の大きい形状（G03）、Static／Kinematic／Dynamic（W02）、Body回転＋Colliderローカル回転＋ローカル中心の一回だけの適用（G08）、同じBodyの複数Collider・異なるカテゴリ・スロット昇順・疎なスロット・ビット規約・自己除外（W03〜W06）、Stepなしの反映と取得済み配列の保持・再利用・別World・World破棄後の配列（W07・W08・W17）、不正範囲・無効除外ID（マスク0でも）・後続の変換不能形状・カテゴリ／Bodyでの除外・Step引数失敗と問い合わせ失敗の後の再利用・途中失敗Stepの拒否と回復（W09〜W13）、非変更（W14）、休止・接触・Snapshot（W15・W19）、300件の全件取得（W16）。
- ゲーム利用（2D／3D）: 範囲候補→Body単位の重複排除→障害物込みの射線。見通せるA（2 Collider）・壁の奥のB・範囲外のC・手前の拾得物。キャラクターだけの射線は壁越しに当たることを確認。壁のカテゴリ0化で、StepなしにBが見える側へ変わる。視点と同じ位置の候補は0長の射線を投げずに扱い、その形状が他の射線の始点を含んで遮ることも確認。削除・同スロット再生成後に保存IDを読み替えない。

`Tests/Physics/OverlapAllocationFaultTests.cpp`（隔離した実行ファイル。群名`PhysicsOverlapFault`、`AllocationFault.cpp`をこのターゲットだけにリンク、2D／3D各7項目）: 全非交差とマスク0で確保が起きない、最初の確保と拡張時の確保の両方へ注入が実到達し例外になる、失敗時に以前の結果を部分配列へ差し替えない、追跡中の未解放が区間の前後で増えない、注入解除後に同じWorldで全件へ戻る、ID・カテゴリ・再問い合わせ結果が不変。この追跡は該当区間の証拠であり、プログラム全体・D3D資源のリーク不存在の証明ではない。

## 試験先行・挙動Red・変異試験

- 試験と登録を先に追加して実装前にビルド: `IntersectsSphere`不在等のコンパイルエラーで失敗（終了1、`Build/WorldOverlap-RootLogs/tdd-red-compile.log`。MSVCは最初の不在識別子の18件で停止）。
- 実装後の最初の実行: PhysicsContinuationで6ケース失敗（2D／3D各3）、故障注入は14/14成功（`green-debug.log`）。いずれも試験の期待値の誤りで、製品の不具合ではなかった。(1) 範囲(4,r1)が大きい形状だけでなくx=2の円にもちょうど接していた。(2) f32のπ/2の丸めに依存する、接触ちょうどの配置だった。(3) 視点と同じ位置の候補の形状が他の射線の始点を含むため、割合0で遮る。配置と期待値を修正し、(3)は遮る振る舞い自体を試験・文書へ明記した。修正後208/208（`green-debug-2.log`）。
- 変異試験（新規コードへ一つずつ故意の欠陥を入れ、ビルド・実行し、元へ戻す）: 実施した8件すべて検出（`mutation.log`）。重心だけの判定（2D）、回転の符号反転（2DのToolbox）、許容距離を既定の1e-5へ（3D）、カテゴリ判定を形状計算の後へ（3D）、最初の一致で打切り（2D）、Bodyなしのスロット番号だけのID（3D）、状態ガードの除去（2D）、途中の確保失敗でそこまでの配列を返す（3D。故障注入試験が検出）。

## 途中の失敗

- 配布の1回目（`Build/WorldOverlap-PackageLogs-attempt1-failed/`、`package-console-attempt1-failed.log`）は`consumer-build`で失敗（終了1）。追加したConsumerコードの変数名`Near3D`が、既存のフィルター手順の同名変数と重複した（C2374）。範囲問い合わせ部分の変数名だけを変更し、配布・Python・No-STL・差分検査を再実行して成功した。製品コード・root・単独の結果には影響しない。
- 変更した`CollisionShapes.h`は、作業ツリーの改行が変更前からLFだったため、CRLFへそろえた（Gitに保存される内容は変わらない）。

## モデル実描画の偶発終了

今回のroot全群（Debug・Release各1回）ではNativeModelDeviceSmokeは成功し、早期終了は発生しなかった。限定回数の結果であり、以前の偶発終了の原因は引き続き特定していない。bc4cfc8の診断・厳格な判定は保持した。

## 再実行とログ

```powershell
cmake -S . -B Build/FbxContinuation -DDXF_BUILD_TESTS=ON -DDXF_BUILD_NATIVE=ON -DDXF_BUILD_STARTER=ON -DDXF_BUILD_EXAMPLE=ON -DDXF_BUILD_MODEL_VIEWER=ON -DDXF_BUILD_RENDER_DEBUG=ON -DDXF_BUILD_NATIVE_SMOKE=ON -DDXF_RUN_DEVICE_TESTS=ON
cmake --build Build/FbxContinuation --config Debug --parallel 6
ctest --test-dir Build/FbxContinuation -C Debug -j 1 --no-tests=error --output-on-failure
cmake --build Build/FbxContinuation --config Release --parallel 6
ctest --test-dir Build/FbxContinuation -C Release -j 1 --no-tests=error --output-on-failure
python Tools/ValidateDebug.py --logs Build/WorldOverlap-StandaloneLogs
python Tools/CheckNoStl.py
python -m unittest discover -s Tools/Tests -v
# Visual Studioのx64開発者環境で実行する。
python Tools/ValidatePackage.py --logs Build/WorldOverlap-PackageLogs
git diff --check
```

rootのログは`Build/WorldOverlap-RootLogs/`（`configure.log`、`*-build.log`、`*-ctest.log`、`ctest-*.log`、`*-LastTest.log`、`*-ctest-list.log`、指紋、Red・変異・No-STL・Python・配布のコンソール出力）、単独は`Build/WorldOverlap-StandaloneLogs/`（Summary.json、`physics-cases-*.log`、`overlap-fault-*.log`）、配布は`Build/WorldOverlap-PackageLogs/`。生成物・ログ・SDKはコミットに含めない。

## 未実施・対象外

SDK未導入PC、開発ツール未導入PC、実Direct3D9、物理入力・聴感、全D3D/COM資源のリーク列挙、非WindowsのSanitizer、速度・一般の割当回数の測定は未実施。配布はNative無効・Debugであり、Native有効配布・Release配布・別PCの成功へ読み替えない。範囲の箱・カプセル化、OverlapAny、出力バッファ・再利用版、列挙コールバック、Sweep、法線、接触イベント、BVH、並列化・非同期、ゲームのAI・ダメージ処理には進めていない。
