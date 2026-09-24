# 円・球の移動候補（ComputeSlideMove、1回の滑り） 検証記録

開始SHA: `4be52302f51d19f5f11aad85c2ff1211ad91d479`。main / origin/main一致、開始時の作業ツリーはクリーン。過去の記録（F5起動設定・接触法線・スイープ問い合わせなど）は変更していない。対象は本記録の直前のコミットの実装・試験・文書。

## 設計

- 公開API: 指令書の第一候補どおり、状態を持たない自由関数`Dxf::ComputeSlideMove`（`Dxf/WorldSlideMove2D.h`：`FPhysicsWorld2D`と`FCircle2D`、`Dxf/WorldSlideMove3D.h`：`FPhysicsWorld3D`と`FSphere`）。引数は`World`、`StartShape`、`DesiredEndCenter`（終点の中心）、`BackoffDistance`（f64、経路上の後退距離、既定値なし）、`ExcludedBody`、`Filter`。結果は値型`FWorldSlideResult2D/3D`（`EndCenter`、`Stop`、`FirstHit`、`SlideHit`）と停止理由`EWorldSlideStop`（`NoMovement`・`ReachedDesiredEnd`・`SlideCompleted`・`Blocked`・`InitialContact`・`MissingNormal`・`PrecisionLimit`）。「滑り経路を走り終えた」と「希望終点へ到達した」は別の理由。WorldやColliderの内部配列を指すポインターは持たない。
- 配置: 実装は`Source/Physics/Private/Dxf/WorldSlideMove.cpp`を正規の`dxf_physics`へ登録（`CMakeLists.txt`のソース一覧）。2D／3D共通の手順はこのcpp内の無名名前空間の小さいテンプレートで共有し、公開テンプレート基盤・コールバック・Manager・所有クラスは作っていない。PhysicsからGameplay／Runtime／Debug／Renderer／Nativeへの依存は追加していない。
- 手順: 最初の移動をSweepClosest→非交差なら希望終点、初期接触なら開始中心→接触割合tから`safe = max(0, t - BackoffDistance / L)`の候補をf64で求めてf32へ丸め、開始中心から丸めた候補までを同じ条件で再スイープ（接触すれば`PrecisionLimit`で開始中心、候補が開始中心そのものなら再検査しない）→`Normal`が空なら`MissingNormal`→残り`R = Q - A`から`N * (min(dot(R, N), 0) / dot(N, N))`を除く→進む成分がなければ`Blocked`、f32で同じ中心になれば`PrecisionLimit`→滑り経路を同じ半径・除外ID・FilterでSweepClosest→非交差なら`SlideCompleted`、初期接触なら`Blocked`でA、それ以外は同じ後退・再検査で`Blocked`（再検査で接触すれば`PrecisionLimit`でA）。SweepClosestは最大4回。2回目の補正、最初の対象の除外、IDの一時除外、法線方向への未検査の押し出しはしない。
- 数値: 差・長さ（最大成分で割ってから平方和）・割合・補間・滑りはf64。公開座標へ戻すときに有限かつf32の範囲かを検査し、外れれば`Toolbox::FException`（途中の中心は返さない）。法線は`dot(N, N)`で割る（f32の単位法線の丸めを考慮）。問い合わせ半径は有限の正の値に限り、後退距離も有限の正の値に限る（どちらも例外）。
- 既存の契約（SweepClosestの結果・初期接触・法線の欠落条件・順序・カテゴリ・自己除外・後続エラー、CCD用Sweep、法線の閾値、Solver、Snapshot）は変更していない。

## 構成と結果（最終コード）

Windows x64、Visual Studio 18 Community / MSVC 19.51、C++20。rootは実SDK・実デバイス試験有効の既存`Build/FbxContinuation`（Visual Studio 18 2026、x64、PATHの`C:/Program Files/CMake/bin/cmake.exe`で既存と同じGenerator）を、TESTS・NATIVE・STARTER・EXAMPLE・MODEL_VIEWER・RENDER_DEBUG・NATIVE_SMOKE・RUN_DEVICE_TESTSを明示ONにして再構成（終了0）。登録一覧に実デバイス群と`PhysicsContinuation`・`PhysicsOverlapFault`があることを確認した（`root-*-ctest-list.log`）。

| 工程 | 実行結果 | 終了コード |
|---|---|---|
| rootビルド Debug／Release | 成功。追加・変更した行に警告なし（`new-code-warnings.log`。既存箇所のC4324等は変更前から） | 各0 |
| root CTest Debug→Release（`-j 1 --no-tests=error --output-on-failure`、直列） | Debug 26/26、Release 26/26。`NativeModelDeviceSmoke`は両構成で`REAL_SDK_MODEL_SMOKE_PASSED failures=0` | 各0 |
| PhysicsContinuation内部（root・単独とも） | 各構成252/252。うち新規の移動候補15 | 各0 |
| PhysicsOverlapFault（root・単独とも） | 各構成20/20（移動候補の確保なし確認2を含む） | 各0 |
| 単独 `python Tools/ValidateDebug.py --logs Build/WorldSlide-StandaloneLogs`（Native無効） | Debug 22/22、Release 22/22、8工程PASS | 0 |
| 公開ヘッダーの単独コンパイル（cl /W4 /WX /permissive-、既存と同じ/wd4324だけ） | `WorldSlideStop.h`・`WorldSlideResult2D/3D.h`・`WorldSlideMove2D/3D.h`の単独、2D＋3D（両順序）、`RigidBody2D/3D.h`・`WorldSweepHit2D/3D.h`・`Dxf/ViewCoordinates.h`との組合せ、計15通り | 0 |
| No-STL | 335ファイル、違反0 | 0 |
| Python `unittest discover -s Tools/Tests` | 31件中30件成功、1件skip（`DXF_CHECK_VS_BUILD`未指定時の実生成物確認） | 0 |
| NativeModelSmokeのF5設定の保持（`DXF_CHECK_VS_BUILD=Build/FbxContinuation`で実生成物確認を別実行） | 成功（`f5-settings-preserved.log`） | 0 |
| 配布 `python Tools/ValidatePackage.py --logs Build/WorldSlide-PackageLogs`（x64開発者環境、Native無効・Debug） | 8段階PASS | 0 |
| `git diff --check` | 問題なし。変更・追加したテキストはPythonでバイト検査し、UTF-8・CRLF・BOMなし | 0 |

CTest群の数はroot 26・単独22のまま（新規ケースは既存のPhysicsContinuationと隔離済みのPhysicsOverlapFaultへ正規登録で追加）。群数と内部ケースを足し合わせていない。

コード指紋（`git diff -- . ':!Docs'`とDocs以外の未追跡ファイルの内容のSHA-256。対象は製品コード・試験・CMake・検証ツール）: 上表の実行の前後とも`c167906e2c71ec2f731bf9816900274601e292940a0302f556db4d9b2113b533`で一致（`fingerprint-before.txt`／`fingerprint-after.txt`）。最終実行の後に変更したのは`Docs/`の文書（本記録を含む）だけで、指紋の対象外。

配布は移動したインストールを使い、`dxf::physics`だけにリンクした`PhysicsOnly`が、既存の手順に続けて2D／3Dで次を実行した: 正面の壁で4.49に`Blocked`、斜めに当たって1回滑り途中の板でy=2.49に`Blocked`（SlideHitは板）、板をDetachすると`SlideCompleted`でy=4、壁に接した開始位置では開始中心のまま`InitialContact`。段階数は8のまま。SDK未導入PCでの検証ではない。

## 新規回帰の範囲

`Tests/Physics/WorldSlideTests.cpp`（15ケース、既存PhysicsContinuationへ`CMake/PhysicsTests.cmake`で正規登録。実FPhysicsWorld2D／3Dへリンク）。期待値は手計算の解析値で、製品の関数から作っていない。許容差は、座標10前後のf32の半ulp（約4.8e-7）と、f32の単位法線（各成分2^-25）と残り移動（約6）の積を合わせて1e-6。

- 基本（2D／3D）: 空Worldで希望終点、移動なし（`NoMovement`）、移動なしで重なり（`InitialContact`）、マスク0。
- 壁（2D／3D、指令書の解析配置）: 左面x=5の壁へ正面から（中心4.49で`Blocked`）。(0,0)→(10,4)、半径0.5、後退0.01で、割合0.45、x=4.5-10·0.01/√116、滑り先y=4（`SlideCompleted`）。滑り経路の途中に厚さ0の板（下面y=3）を加えると、中心y=2.5で接して0.01戻したy=2.49で`Blocked`（SlideHitは板、割合は滑り経路に対する値）。滑り先(x,4)は空いている（OverlapAllで空を確認）が、途中の板を飛び越さない。
- 境界（2D／3D）: 開始時の重なり、境界接触で離れる向き（どちらも開始中心で`InitialContact`、法線なし）、終点でちょうど接触（割合1、4.49で`Blocked`）、後退距離1が接触までの0.5より長い（割合0へ切り詰め、開始中心そのもの）、接線（法線(0,-1)、残りは全部接線方向で滑るが、4.99から同じ円に再び接して`Blocked`。SlideHitは最初と同じCollider）。
- 法線: 2Dの角(5,0.5)（半径1、法線(-√3/2,-1/2)、滑り=(R/4,-√3R/4)）、3Dの同じ位置のZ方向の辺、3DのXY面外（(0,0,0)→(10,0,4)でZ=4まで滑る）、法線が空（半径1e-6で±1e7。接触の手前で`MissingNormal`、希望終点へ進まない）。
- 数値（2D／3D）: 半径0・負・NaN・Inf、後退距離0・負・NaN・Inf、非有限の終点、f32で表現できない移動（マスク0でも例外）。2^20付近（f32の間隔0.125）で0.001戻した候補が接触中心へ丸められ、再検査で接触して開始中心で`PrecisionLimit`、0.2戻すと2^20+4.25で再検査を通る。1e6付近で1e-6radだけ傾いた壁は、滑りが(0,-5.75e-6)程度でf32では同じ中心になり`PrecisionLimit`（再試行しない）。極小移動。滑り終点のyが-3.55e38になる配置では、最初の移動（SweepClosest単体でも法線付きで成功）の後、滑り終点の表現不能で例外になり、呼出し側の以前の結果は変わらない。
- 登録（2D／3D）: 複数Colliderを持つ自己Bodyの除外（除外しなければ初期接触）、滑り経路にだけある自己Bodyの別Colliderと対象外カテゴリのColliderを、滑り経路の問い合わせでも同じ除外・Filterで無視する、同じ割合の二つの壁は先のスロット、カテゴリ0・マスク0・Body移動・Attach・Detach・DestroyのStepなしの反映、削除済み・別WorldのBodyの除外IDはマスク0でも例外。
- World状態と非変更（2D／3D）: 最初のStep前から使える、Stepの引数検査だけの失敗では使える、途中で失敗したStepの後は移動0・マスク0でも例外、正常Stepで回復。問い合わせあり／なしの2つのWorldで同じ外力と20回のStepを与え、位置・姿勢・速度・角速度・休止が各Stepで一致し、StepIndexも同じ。休止中のBodyへの問い合わせを50回行っても起こさない。

`Tests/Physics/OverlapAllocationFaultTests.cpp`（隔離した`PhysicsOverlapFault`）へ、2D／3Dで「法線付きの最初の接触から補正して問い合わせる・到達・初期接触」の成功経路が確保しないことの確認を各1項目追加（18→20項目）。実Worldの走査で確認し、偽物への置換はしていない。

## 試験先行・期待値の誤り・変異試験

- 先に公開ヘッダーと試験・登録を追加して実装前にビルド: ヘッダーは存在するため、コンパイルは通り、`ComputeSlideMove`の2D／3Dが未解決の外部シンボル（LNK2019、2件）でリンクに失敗（終了1、`tdd-red-compile.log`）。コンパイルエラーではなくリンクのRedとして記録する。
- 挙動Red（`behavior-red.log`）: 常に希望終点へ到達したことにする一時的な実装で、新規15ケースすべてが失敗、既存群は成功。
- 本実装後の最初の実行（`green-debug.log`）: 251/252。失敗は「3Dの休止中のBody」ケースの前提確認（問い合わせ前に休止していること）で、**試験の配置の誤り**だった。3Dの箱の補助関数は壁用に奥行きの半幅を50にしており、動くBodyも1×1×100の箱になって600Stepで休止しなかった。動くBody用に立方体（2Dは正方形）の補助関数を追加して修正し、252/252（`green-debug-2.log`）。製品コードの不具合ではなく、製品コードはこの間変更していない。
- 変異試験（`mutate.py`・`mutation.log`・`mutation-m1b.log`）: 変更前のファイルを作業ツリー外（一時ディレクトリ）へ保存し、一つずつ欠陥を入れてビルド・実行し、戻した。各復元はSHA-256で一致、復元後のビルド終了0。検出した契約: 法線を反転（内向き成分の除去）／法線が空を非交差として希望終点へ（空の法線でも衝突）／後退を割合から固定値として引く（距離の単位）／丸めた候補の再検査を省く（丸め後の検査）／滑り経路で自己除外とFilterを落とす（すべての問い合わせで同じ条件）／滑りを元の残りの長さへ正規化（成分の除去で増やさない）／滑り経路を終点での重なりだけで代用（経路の検査）。最後のものは、最初に用意した「滑り経路の問い合わせを省く（空の結果にする）」変異がビルドできなかったための置き換え（`TOptional`への`= {}`の代入が多重定義の解決であいまい、C2593。`mutation-m1-build.log`）。置き換え後の7件すべてを検出。

## モデル実描画の偶発終了

今回のroot全群（Debug・Release各1回）では`NativeModelDeviceSmoke`は成功し、`ProcessMessage=-1`による早期終了は発生しなかった。限られた回数の結果であり、以前（`WorldSweepNormals-2026-09-24.md`のroot Release 25/26）の偶発終了の起源は引き続き特定していない。画素判定・時間上限・ProcessMessageの終了条件・初期化後の待ち時間は変更していない。

## ログ

`Build/WorldSlide-RootLogs/`（開始SHA、Red・挙動Red・Green、変異、ヘッダー確認、configure・build・ctest・`*-LastTest.log`・登録一覧、No-STL・Python・配布のコンソール出力、指紋、実行スクリプト）、`Build/WorldSlide-StandaloneLogs/`、`Build/WorldSlide-PackageLogs/`。生成物・ログ・SDKはコミットに含めない。

## 未実施・対象外

VS GUIでのF5操作、SDK／開発ツール未導入PC、実Direct3D9、物理入力・聴感、非WindowsのSanitizer、D3D全資源のリーク列挙、性能測定は未実施。配布はNative無効・Debugであり、Native有効・Release配布の検証へ読み替えない。初期重なりの解消・押し出し、接地・ジャンプ・段差・坂の制限、動く床への追従、Dynamic Bodyへの押し返し、多面の反復スライド、箱・カプセルの移動、SweepAny、BVH、並列化には進めていない。
