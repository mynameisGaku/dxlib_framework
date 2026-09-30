# J4-C〜F：距離拘束のStep確定・Island並列実行・CCD境界

作業日：2026-10-01（利用者の作業日付）。過去の検証本文は変更していない。

## 開始状態と範囲

- 開始main：`87c263da58396776739daa7a5f6561ac4b0a6eef`。
- fetchで確認したorigin/main：`4a211448af60b3508db697321c194c559339ba06`。
- 作業開始時の未コミット・未追跡・stage済み変更はなし。既存のローカル5コミットを保持した。
- J4-C〜Fだけを実施する。新しい公開API、設定、Manager、GUI実行ファイルは追加しない。
- mainへローカルcommitする。push、J5のGameplay利用例、J6の配布・Benchmark・全14変異、J7の全体完了作業は行わない。
- `87c263d`の3D Anchor回転修復を保持する。`Rotate_Internal(Q, Local)`を使い、逆慣性用の`TransformDiagonal_Internal`へ戻していない。

## 実装と所有境界

Privateの`FDistanceJointSolveState2D/3D`に、開始時の生存・世代、蓄積Impulse、保存方向とその有効性を保持する。
Worldが配列容量を再利用するが、各Stepの入力検証後、Bodyを変更する前に全slotの内容を作り直す。
一回のStep内では全SubStepが同じ作業値を使う。速度求解、Warm Start、位置補正の保存方向はこの作業値だけを更新する。
最後の索引更新とイベント発行まで成功してから、所有スレッドがslot昇順に生存・世代を照合して確定する。
失敗したStepの作業値を次のStepへ持ち越さない。

直列とJob経路が同じ`SolveConstraints_Internal`を使う既存構成を保持した。
各速度反復の中でContact→DistanceJointの安定順を使い、Jointだけを最後にまとめて反復しない。
Workerが書くのは所属IslandのDynamic BodyとJoint作業slotだけ。
共有Static/Kinematicへのゼロ加算も既存の実効逆質量・逆慣性のガードで省かれることをソースで確認した。
起床、Contact Warm Start、起床の再伝播、Joint Warm Start、位置補正、休止更新は所有スレッドへ残す。

**Joint記録の確定を取り消す保証と、Bodyの巻き戻しは別である。**
Bodyの位置・速度・力やContact記録を元へ戻す契約は追加していない。
getterは登録設定と現在のBody姿勢から距離を計算し、作業値を公開しない。公開APIの同時呼出し安全性も追加していない。

## CCDの初発不一致と修復

新しい実World試験では、同じJoint島と離れたCCD球を置き、壁の配置だけを変えて0・1・3回の衝突を作る。
変更前は2D・3DともJoint島のbit一致検査が失敗した（`ccd-before2.log`）。
`SolveNow_Internal`がJointを含むIslandを作り、TOIごとにJointも求解することを確認した。
ContactだけのIslandを作る`SolveContactNow_Internal`へ変更すると2Dは成功し、3Dだけ不一致が残った（`ccd-contact.log`）。

残った3D差は、速度が変わらないBodyの位置・QuaternionをTOI区間ごとに積分・丸めていたことに対応した。
同一SubStep内で速度が変わらない間は、同じ開始姿勢から到達時刻まで既存の積分関数を適用する。
接触で速度または休止状態が変わったBodyだけ、その時点の実姿勢を新しい基準にする。
Quaternionの積分式、Gyroscopic処理、位置補正の定数・反復数は変更していない。
最終版では2D・3Dとも0・1・3回の結果と次の正常Stepの結果がbit一致した。
試験は未処理時間0を要求しており、World全体の保守停止時にも無関係なBodyを全時間進めるという主張ではない。

Jointの速度求解は各SubStepの離散処理で一度（設定された速度反復数）だけ実行する。
TOIの即時処理はContactだけを求解し、JointのWarm Start・作業値を変更しない。
Joint位置補正はCCD後の最終姿勢で行う。TOI途中の連続的な長さ保証、Anchor Sweep、Joint TOIは未対応のまま。

## 新規回帰と失敗注入

`DistanceJointParallelTests.cpp`は24ケース、`DistanceJointContinuousTests.cpp`は4ケース。
既存の32群493ケースを削除せず、34群521ケースとした。通常起動は全群、`--joint-j4`だけが限定28ケース。

新規構築した各Worldについて、`nullptr`、`FJobSystem(1)`、2・4・8レーンを比較する。
各フレームの位置、速度、角速度、AngleまたはQuaternion全成分、休止、Joint距離・誤差・生存を成分ごとにbit比較する。
構造体の余白やWorld識別子は比較しない。`f32`の成分も有限値と符号付きゼロを保持する`f64`へ変換して比較し、epsilonは使わない。

対象は16独立Static接続、共有Static、静止・移動Kinematic、Dynamic対のoff-center Anchor、実接触との混在、16 Dynamicの一本の鎖、逆登録順、二分割、全島休止から一部の起床。
一本の鎖は一つのIslandであり、島内部の並列高速化を示す試験ではない。
Job経路の`SolverIslandCount`にはJointだけの島も入り、16島・二分割なら32、鎖の一分割なら1。
同期経路は0、実行レーンの診断は借用した1・2・4・8を要求する。
二分割の作業値引き継ぎは、蓄積力を使わない同入力を半分の刻みで二回正常確定した対照ともbit一致させた。
共有Staticは8レーンで固定3回の反復を全て検査した。これはTSanによる競合検査の代わりではない。

停止済みの既存Job Systemを借用し、積分・BroadPhase・NarrowPhaseのJob化を無効にして、二つのJoint島の求解投入だけを拒否する。
期待する`island solver failed`を確認する。成功したX方向の記録から、失敗したY方向の記録へ誤って確定しないことを、Anchorを重ねた後の公開操作から検出する。
失敗後のSnapshot拒否、正常Stepでの回復、同slotの別世代Jointが古い保存方向を使わないことも確認する。

確保失敗は既存の`PhysicsOverlapFault`へ追加した。
非ゼロの速度拘束を正常に確定した履歴を対照Worldにも作り、確保位置を0から順に一地点ずつ失敗させる。
失敗後のBody姿勢・速度を公開APIで対照と同じ入力へ戻し、Joint記録を初期化せずに次の正常Stepを比較する。
2D・3Dそれぞれcountdown 0〜41の42地点を実際に注入し、全てbit一致で回復した。17〜41の25地点は島求解完了後の失敗を含む。
42では注入地点に達しない正常終了となり、探索を終了する。成功するまで同じ失敗を再試行する運用ではない。
別ケースでJoint登録を33件へ増やし、Step作業配列の拡張に失敗してもBodyが未変更で、注入解除後に進めることを確認した。
private cacheを公開する検証APIは追加していない。保存値の保持は公開操作による回復と故障化で検出しており、全privateフィールドの直接採取ではない。

## 限定5変異の試行結果

各変異は2D・3Dへ一種類ずつ適用した。元の生バイトを保存し、変異中のbuildが成功してから実行した。
各試行の後に元の生バイトへ復元して更新時刻を進め、Source SHA-256の一致、再build、限定28ケースと確保故障群のGreenを確認した。
background build・試験を重ねていない。

| 最終版の試行 | 変更点 | 変異の終了コード | 復元後の限定／確保群 |
|---|---|---:|---|
| J4-W1 | Solverから登録Impulseにも直接書く | 1：各次元25地点の回復不一致 | 0／0 |
| J4-W2 | Warm Start直後に登録値を早期確定する | 1：各次元の拒否後の保存方向が変化 | 0／0 |
| J4-PAR1 | Job経路のDistanceJoint求解を省く | 1：同期とのbit不一致 | 0／0 |
| J4-PAR2 | Job完了後に所有スレッドでもJointを再求解する | 1：同期とのbit不一致 | 0／0 |
| J4-CCD1 | TOIのIslandへJointを再び含める | 1：0・1・3衝突のbit不一致 | 0／0 |

上記は期待したRedであり、通常の全群成功へ合算しない。全14変異はJ6として未実施。
再現する場合は表の変更を一件ずつ行い、通常版を生バイトで退避・復元する。限定群は`--joint-j4`、W1は確保故障群を実行する。

復元済み最終Source SHA-256：

```text
PhysicsWorld2D.cpp 6655bc283a7e9e3ad4eba1399c4d153ff43a75f170c3e8df949993d0d6a43f46
PhysicsWorld3D.cpp 82dc436ec452ec9efe8916d7b2b70a7ca2416325aa7f7df74f0fbf11eec9f01c
```

## 最終検証

Buildは今回専用の`Build/J4-20261001/Root`。Visual Studio 18 2026、x64、MSVC 14.51.36231、C++20。
Native・実デバイス・各サンプルの製品ターゲットは無効。正規のportableテスト登録27群を使う。
警告設定は既存の既定値を保持した。C4324の構造体余白、既存PlatformのC4459等は残っており、警告ゼロの結果ではない。

| 工程 | Debug | Release |
|---|---|---|
| 生成 | 終了0（共通マルチ構成） | 同左 |
| 最終build | 終了0 | 終了0 |
| root CTest直列・必要群0件を拒否 | 27/27、終了0、135.73秒 | 27/27、終了0、27.59秒 |
| Physics内部 | 34群521/521件 | 34群521/521件 |
| J2／J3／3D Frame RCA | 40／34／13件を保持 | 同左 |
| Framework／DebugPhysicsCapture | 364/364・12/12 | 364/364・12/12 |
| 作業領域拡張＋確保位置0〜41 | 両次元とも成功、失敗0 | 両次元とも成功、失敗0 |
| 公開Joint関連4本＋PhysicsExecutionの単独include | 5本成功・終了0 | 5本成功・終了0 |

Debug全群の途中版は試行1が27/27・136.89秒、試行2が27/27・136.53秒で終了0。
試行2以降、確保失敗試験の正常履歴に非ゼロImpulseを明示的に作る検査を加えたため、以下の最終試行を採用する。
途中版の成功を最終版の成功へ読み替えない。

| 全群試行 | 対象 | 結果 | 経過秒 | 終了コード |
|---|---|---|---:|---:|
| Debug 1 | 途中版・最終の非ゼロ正常履歴検査を加える前 | 27/27 | 136.89 | 0 |
| Debug 2 | 途中版・最終の非ゼロ正常履歴検査を加える前 | 27/27 | 136.53 | 0 |
| Debug 3 | 最終ソース | 27/27 | 135.73 | 0 |
| Release 1 | Debug 3と同じ最終ソース | 27/27 | 27.59 | 0 |

最終版の限定実行もDebug・Releaseとも24/24＋4/4、終了0。
bit一致は今回の同一MSVC／x64の同入力比較であり、異なるコンパイラー・OS間の保証ではない。

| CCD接続ケース（両構成同じ値） | SubSteps 1の最終誤差 | SubSteps 4の最終誤差 | 衝突回数 |
|---|---:|---:|---|
| 2D | 0.036069911803484178 | 0.051285779256970532 | 各1 |
| 3D | 0.036069911803484178 | 0.051285779256970532 | 各1 |

各ケースはJoint生存、有限値、壁の手前を確認した。四分割で誤差は約0.0152増えたが、この配置での大幅悪化を拒否する検査を満たす。
分割数に対する単調改善の契約へ読み替えない。CCD無効の対照は両次元とも誤差2.272135937402334、球のX=1.82111454で、CCDがない場合との違いを確認した。

No-STLは666ファイル・違反0。Pythonは60件登録、53件実行成功、7件スキップ、終了0。
スキップは生成済みVS/UI構成を指定する環境変数が未設定のIDE検査である。実行済み60件とは報告しない。
`git diff --check`は終了0。変更テキストはUTF-8／CRLF、既存BOMを保持し、無関係な一括整形はしていない。
clang-formatは新規ファイルと変更範囲へ限定し、新規の引数等は利用者の一行規約を優先した。

## 再実行手順と記録先

```powershell
cmake -S . -B Build/J4-20261001/Root -G "Visual Studio 18 2026" -A x64 -DDXF_BUILD_NATIVE=OFF -DDXF_BUILD_NATIVE_SMOKE=OFF -DDXF_BUILD_EXAMPLE=OFF -DDXF_BUILD_STARTER=OFF -DDXF_BUILD_GAMEPLAY_SAMPLE=OFF -DDXF_BUILD_UI_SAMPLE=OFF -DDXF_BUILD_MODEL_VIEWER=OFF -DDXF_BUILD_RENDER_DEBUG=OFF -DDXF_BUILD_TESTS=ON
cmake --build Build/J4-20261001/Root --config Debug --parallel 4
ctest --test-dir Build/J4-20261001/Root -C Debug -j 1 --no-tests=error --output-on-failure
cmake --build Build/J4-20261001/Root --config Release --parallel 4
ctest --test-dir Build/J4-20261001/Root -C Release -j 1 --no-tests=error --output-on-failure
Build/J4-20261001/Root/Debug/dxf_physics_tests.exe --joint-j4
Build/J4-20261001/Root/Release/dxf_physics_tests.exe --joint-j4
python Tools/CheckNoStl.py
python -m unittest discover -s Tools/Tests -v
git diff --check
```

各生成・buildの終了0を確認してから次の工程へ進める。Generatorの異なる既存Buildを混用しない。
各全群実行直後に`Testing/Temporary/LastTest.log`を別名保存する。
今回の生ログ、変異JSON、実行ファイルSHA-256一覧、単独includeの作業コピーは`Build/J4-20261001/`に保存し、commitしない。
最終記録の対象は、同じソース差分を同梱するmainのローカルcommitである。

最終ログは`final3-debug-build.log`、`final3-debug-ctest.log`、`final3-debug-tests.log`、`final2-release-build.log`、`final-release-ctest.log`、`final-release-tests.log`。
限定試験は`final-j4-debug.log`と`final-j4-release.log`、Pythonは`final2-python.log`、単独includeは`headers.log`。
変異の各`J4-*.json`にはbuild・Red・復元build・Greenの終了コードと経過秒、復元前後のSource SHAを保存している。
開発中のinclude指定や試験のMaxIterations=64（公開設定の上限32を超過）の誤りは、ビルド／試験作成上の不備として訂正した。
これらを製品のCCD不一致や、最終版の検証失敗とまとめていない。

## 限界と終了

- 今回の発見は再現性のあるCPU／実WorldのCCD境界と確定漏れ検出であり、過去の実デバイス終了の原因特定ではない。
- Job停止による求解投入拒否と確保失敗を実施した。実WorldのWorker本体へ別の人工例外入口は追加していない。
- Native実描画、SDK未導入PC、別OS、TSan、全D3D／COM資源のリーク検査は今回未実施。
- J5のComponent・GameplaySample、J6のPackage・Benchmark・全14変異、J7の全体文書・pushは未着手。
- 公開API・World問い合わせ・Snapshot選択・Physics数値定数・既存ufbx／描画機能・空のStarter・Sandboxを変更していない。
- 最終検証が完了した時点でJ4を区切り、追加機能へ進まず停止する。

## 最終実行ファイルSHA-256

- Debug/dxf_physics_tests.exe : abd2a7971338833958bcc3ab75048818cc1ddde1de9dc178aa4d0e512c726e21
- Release/dxf_physics_tests.exe : aa21a23944c05c1b917b0e0654e69d1d84d0dae7667987c0a53739a72ca78550
- Debug/dxf_tests.exe : dad04e40457ccf1fc14bed8ba235d9b58eca8a5e24021b244c234dc8543a702a
- Release/dxf_tests.exe : 06608303e51aa61f10faa84a10576c594cd6dbd5f8f2127a333dc6c15fe61df4
- Debug/dxf_debug_physics_tests.exe : c8ad26e4f634aaa520869b5c585822703ce20322ae543530505b6ca5149e2f7b
- Release/dxf_debug_physics_tests.exe : 99018f3d873b65bf4a5ea0b9545f16c45e39dcdc292d232ffab0ac7ae48831df
- Debug/dxf_physics_tests_overlap_fault.exe : 154c5af5c6537c9812b68d69b3e68e8da8338316b8a5d7b2f1a07b96a45ded92
- Release/dxf_physics_tests_overlap_fault.exe : 4f31d0cdf6115eb5bc488ad1ab79534af96c703cadb5a7df5542137433b0817b
