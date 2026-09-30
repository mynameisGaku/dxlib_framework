# J5〜J7：距離Jointのゲーム利用拡張

## 今回の範囲とコード検証点

2D／3Dの型付きBody参照・距離Joint Component、吊り下げ／4体の鎖／Kinematic運搬、解除／再接続／再生成、実Application・実描画、再配置Consumer、境界別測定、故障注入、14変異を実装した。最終全群・単独入口・配布4構成を直列で確認した。

- 開始main：`2d9b34d848d854406e321a39d150b8d51ada2e66`。
- 開始origin/main：`4a211448af60b3508db697321c194c559339ba06`。開始clean、未push6コミット。
- 実行コードのHEAD：`c819a5c1bfcfcc82297287208b33cc7258aaec7f`。
- Source／Tests／Examples／Tools／CMake／Externalと正規root buildファイル、831ファイルの生バイト指紋：`e9fadf92212dc2ea8b68bac0c7d5116d4bd824cbaf2591fcfb7506306a870813`。Docsはこの指紋に含めない。
- Manifest：`Build/JointGameplayCompletion-20261001/FinalSourceManifest.txt`。構成・SDKは各CMakeCacheとregistration JSON、binaryは構成別`*-binary-hashes.json`。
- MSVC 14.51.36231、Visual Studio 18 2026 x64、同じPCの既存`ThirdParty/DxLib-3.25a-source`（3.25a、model extension 3）。取得・SDKライセンス同意を新しく行っていない。
- mainのみ、生成物・SDK・生ログ・画像・退避物はcommitしない。空Starter・Sandbox、ufbx、モデル、Scope待機、Scene寿命、数値計算を維持。

## 実装と責務

`FPhysicsBodyReference2D/3D`はRigidBody／KinematicMoverの型付きハンドルか完全なBody IDを保持する。`DDistanceJoint2D/3DComponent`はJoint一つの要求・IDを保持し、Worldが実体を所有する。Bodyの生成・破棄を代行しない。全Body登録後のPrePhysicsで接続、成功PostPhysicsで物理姿勢の値を観察する。表示は補間Anchorを既存Rendererへ渡す。

同じ有効な設定の再要求はIDとWarm Startを維持する。最後の要求を採用し、EndpointLost後は明示的な再接続が必要。検証／確保が失敗した再接続は旧Jointを保持する。登録時に空きslot領域を準備することで、noexceptのJoint解放中の確保を除いた。仕様・両次元の利用例は[距離Joint](../Physics/Joints.md)。

実SceneのComponent回帰は26件追加。実ApplicationはJoint関連12件追加し、38件。PhysicsはWarm Startの隣接拘束への漏れと接続Body対のContactを各次元で補い、既存34群に4件追加した。実DxLibは既存系列の後に同じSampleからJoint線・両端物体・状態文字を一括読戻しで判定する。

## 故障注入と初回／定常の区別

| 対象 | 2D | 3D | 合否 |
|---|---:|---:|---|
| Scene登録・Component構築 | countdown 0〜6の7地点 | 同じ7地点 | 半登録なし、同じSceneで回復、scope終了後の未解放確保差0 |
| 初回接続の配送・Pre/Post予約・World登録 | 0〜4の5地点 | 同じ5地点 | ID／観察を成功扱いにせず、回復ID index=0 |
| 再接続 | 0〜3の4地点 | 同じ4地点 | 旧ID生存、回復後index=32の新ID、旧ID失効 |
| Joint解放 | 33個の登録後に次の確保を失敗設定 | 同じ | 全解放で注入されず、slot32は別世代で再利用 |
| 慣らし後のComponent境界 | 追加確保0 | 追加確保0 | 解決・Pre/Post予約・ID／観察読取のみ。Owner配送・World.Stepを除く |

最初の非注入試行も記録する。Scene登録0〜7、初回0〜5、再接続0〜4を各次元で実行している。予約前の所有階層配送の失敗と、Component到達後の観察値失効を区別した。無関係Bodyは生存する。Worker本体での人工例外は未実施で、既存Job投入拒否の確認と区別する。

## 最終ソースに対する14変異

限定入口はPhysicsの`--joint-all`（既存通常起動は全34群を維持）、M11はInteractionSample、M12はJointComponentFault。各変異は保存生バイトへfinallyで復元し、時刻更新・実再コンパイル・同じ試験のGreenを確認した。ビルド失敗は検出に数えない。全14のbuild=0、Red=1、復元build=0、Green=0、Source hash復元一致。

| ID | 変異内容 | 検出する回帰（代表） | build／Red／Green |
|---|---|---|---|
| M01 | 3D Local Anchorを回転しない | rotated local anchor follows the body | 0／1／0 |
| M02 | 両次元のJoint Lambda符号反転 | static distance joint resists gravity | 0／1／0 |
| M03 | Warm Start／速度求解の角Impulseを捨てる | off-center anchor spins counter clockwise | 0／1／0 |
| M04 | A/BのLocal Anchorを入替 | off-center anchorの非対称応答 | 0／1／0 |
| M05 | slot再利用で旧AccumulatedImpulseを維持 | reused warm impulse does not reach adjacent constraint | 0／1／0 |
| M06 | Body破棄で接続Jointを残す | distance joint dies with its body | 0／1／0 |
| M07 | Joint辺のDynamicフラグを外す | Joint-only求解、島・起床回帰 | 0／1／0 |
| M08 | 共有StaticもDynamic辺として結合 | static joint sleep／shared static islands | 0／1／0 |
| M09 | 島の起床と起床の再伝播を省く | explicit wake up propagates in the island | 0／1／0 |
| M10 | Job経路だけJoint求解を省く | J4 bit match 0/1/2/4/8 | 0／1／0 |
| M11 | 3D描画のたびに追加World.Step | joint one two views and odd resize bit match | 0／1／0 |
| M12 | World登録を公開した後に遅延確保で例外 | initial recovery left a ghost joint | 0／1／0 |
| M13 | 接続Body対のContact候補を抑止 | connected bodies still generate solid contact | 0／1／0 |
| M14 | Collider DetachでBody Jointも破棄 | distance joint outlives collider detach | 0／1／0 |

対象Source SHA-256は両Worldとも全Physics変異で共通：

- `PhysicsWorld2D.cpp`：`c7a71aa7da1dd2888a8282c2136f4ae355e216c990b8d2de380d0043737b9b14`
- `PhysicsWorld3D.cpp`：`dd5413f846f15674eb0e794965347b388c820ed170c7f91ace91336c0e788c29`
- M11 `JointCourseOverlay.cpp`：`72276c43c50cb370adaf16446363b5adb31d920671b1d6ba8bfc756d7184fc88`

各変異後のhash、各段階の所要秒、最初の失敗、復元buildは`Build/JointGameplayCompletion-20261001/MutationsFinal/M01.json`〜`M14.json`と同名ログに残す。M05初回は変異が生存し、既存試験が非ゼロImpulseを作らず十分な反復で履歴差を隠していた。隣の拘束と一反復を使う回帰を追加してから最終系列を実行した。M07最初の補助スクリプトは探索箇所の誤記で変異前に停止し、復元済み。コンパイル／挙動検出成功には数えない。

## CPU費用（実測範囲）

pilot（慣らし30・測定12・5回）は終了0、1.855秒。本測定の最終CSVは`final-joint-release.csv`、108条件・360集計行。通常16/64/256接続、鎖8/32/128、両次元、同期と4レーン。各条件は新規Worldを5回作り、各回の初回、慣らし30、連続120Stepを分離する。全測定とbuildの最終コマンドは約7.40秒、終了0。

下表は慣らし後World.Stepのms／Step中央値（括弧内は最小〜最大）。全体の描画、UI、所有階層配送を含まない。休止数はCSVで併記し、常に全Bodyが活動した負荷とは呼ばない。

| 条件 | 次元 | 同期 | 4レーン |
|---|---|---:|---:|
| 独立Static・256 | 2D | 0.0850 (0.0786〜0.0886) | 0.1019 (0.1013〜0.1026) |
| 独立Static・256 | 3D | 0.1304 (0.1295〜0.1325) | 0.1273 (0.1229〜0.1312) |
| 共有Kinematic・256 | 2D | 0.1079 (0.1075〜0.1088) | 0.1240 (0.1216〜0.1244) |
| 共有Kinematic・256 | 3D | 0.2304 (0.2295〜0.2326) | 0.2047 (0.1998〜0.2075) |
| 鎖・128 | 2D | 0.0294 (0.0293〜0.0313) | 0.0576 (0.0552〜0.0592) |
| 鎖・128 | 3D | 0.0567 (0.0564〜0.0581) | 0.0833 (0.0828〜0.0852) |
| Component維持・256 | 2D | 0.0738 (0.0732〜0.0751) | 0.1002 (0.0992〜0.1030) |
| Component維持・256 | 3D | 0.1299 (0.1281〜0.1440) | 0.1258 (0.1249〜0.1263) |
| Component切替・256 | 2D | 0.0465 (0.0463〜0.0476) | 0.0633 (0.0623〜0.0636) |
| Component切替・256 | 3D | 0.0961 (0.0950〜0.0979) | 0.0869 (0.0861〜0.0936) |

Componentの解決・予約、Pre、Postは明示Body ID参照を使い、維持系列の定常追加確保は全条件0。World.Stepは独立Static256／Component維持256で同期523・4レーン625確保／Stepが残る。鎖128は144／172確保。これはSolver・島の既存作業確保を含み、ゲーム全体0確保とは報告しない。小さい独立島でも4レーンが速くなる保証はなく、鎖は一島のまま。最大島のBody数は配置から導いた列であり、一般Worldの新しい診断APIではない。

## 途中の試行（最終結果へ合算しない）

以下は開発途中の記録であり、当時の未実施表記を最終状態と読み替えない。

# 開発途中の継続メモ

## 開始状態

- 開始main：`2d9b34d848d854406e321a39d150b8d51ada2e66`。
- origin/main：`4a211448af60b3508db697321c194c559339ba06`。
- 未push履歴は6コミット。作業開始時の作業ツリーはclean。
- 指令書：`dxlib_framework_Claude_2d9b34d_J5-J7_GameplayJoint_Expansion_Directive.md`。
- 過去のJ4のportable結果と今回の実行結果は分ける。
- 今回はJ5〜J7を継続して実施し、最終検証後に通常pushする。

## 現在の実装と検証

J5-Aの2D／3D Body参照、接続状態、設定、成功Stepの観察値、距離Joint Componentを実装中。
既存PrePhysics／PostPhysics予約を使い、参照を解決して新Jointの生成が成功してから旧Jointを破棄する。
同設定の接続要求は現在IDを維持する。EndpointLostからの自動再接続はしない。
Gameplayの通常実装を同名cppへ分けるため、正規`dxf::gameplay`を静的ライブラリへ変更した。
Physicsの逆依存は追加していない。既存Gameplayのテンプレートと公開ターゲット名は維持する。

初回製品buildではContextの既存予約先名と2D回転の型変換を訂正した。製品build第3回はDebug終了0。
初回CPU回帰では376/382件成功・終了1。後から追加した対象の所有境界をfixtureが通していなかった6件を検出した。
これは製品の自動初期化を変更する根拠ではない。CPU fixtureをApplicationと同じFreeze／Commit境界へ修正して再実行中。

## 記録先と次工程

今回のログは`Build/JointGameplayCompletion-20261001/`。生成物・生ログ・作業スクリプトはcommitしない。
変異試験はまだ開始しておらず、製品への変異は適用していない。
J5-B/C、J6、J7は未実施。完了したとは扱わない。

次工程はJ5-Aの実Scene回帰を通し、追加の寿命・非主軸Anchor回帰と故障注入を整えてから、既存GameplaySampleへ仕掛けを追加する。

## 2026-10-01 継続追記（J5実装、J6検証中）

J5-AのCPU実Scene回帰は390/390（追加26件）、実ApplicationのInteractionSampleは34/34（追加8件）、Debugで終了0。
J5-Bは吊り下げ・4連鎖・Kinematic運搬・J/K/L/N/B操作を2D/3Dへ追加した。空StarterとSandboxは変更していない。
J5-Cの通常NativeGameplayDeviceSmokeは初回終了8、14.50秒。Joint検査に到達する前の既存昇降床判定で失敗した。
Jointだけを初期化しない比較実行でも同じ失敗になった。RecoveryはTooDeep、対象Bodyは昇降床の箱。
箱のGamePositionは初期ゲーム設定値であり、診断はWorld.GetPositionへ訂正した。キャラクターを箱のない床左側へ置くと比較実行が終了0、6.725秒。
床の運搬・Y変化の既存判定は保持し、試験の配置のみ修正した。通常Debug実行の後続は終了0、22.211秒と21.884秒。
後者は同じGPU読戻しから線・両端物体・状態文字も判定する。Joint単独成功は通常全体成功へ足していない。

Native/Portableの両Debug/Release製品・試験ビルドはすべて終了0（13.424/23.763/39.923/45.059秒）。これは全CTestの実行結果ではない。
Native無効Debugの再配置配布先でPhysicsOnlyのJointとGameplay Componentの解除・世代入替・終了が成功した。最終版の4構成確認は未実施。

確保注入の試行3/4はcountdown=4でnoexcept DestroyJointのJointFree.PushBack確保に到達し異常停止したため、所有する試験プロセスを中断した。
現行Worldの登録時に空きslot領域を先に用意する最小修正を2D/3Dへ入れた。
修正後の試行5は各次元countdown=0〜3の4確保を実注入して旧接続を維持、回復後に新世代へ接続、countdown=4は非注入で終了0。
予約前の所有階層配送失敗と、Component固定更新到達後の観察値失効は分けている。
Componentのみの参照解決・Pre/Post予約・読み取りは慣らし後に追加確保0。Owner配送全体の確保0とは呼ばない。

性能測定器のjoint系列を追加し、Release pilotは終了0、1.855秒（慣らし30・測定12・5繰り返し）。本測定は未実施。
変異14項目、最終全群・単独入口・4構成配布・IDE・公開ヘッダー・文書整合は次工程として残る。

### その後の試行

- Native Releaseの通常Gameplay試験（Joint軌跡全成分比較を追加した版）：終了0、21.71秒。部分選択ではなく標準系列。
- 配布Native preflight 1：Ninjaのリンク応答で日本語パスが崩れ、NativeUiAppがLNK1104。preflight 2：NMakeでも同じ失敗。配布済みライブラリの不足とは扱わず、Consumerを正規VS複数構成へ変更。preflight 3は従来Consumer・両Joint・NativeApp画素・NativeUiApp両スタイルを含め終了0。最終4構成は別run。
- 最終回帰の補助runner初回はNative登録名を`NativeTransparencyDeviceSmoke`と誤記し、Native Debug build成功後、試験開始前に停止。現物の`NativeDeviceSmoke`と他5実デバイス名へ照合を訂正し、Portableの実行済み結果を保持して再開。製品ソースは変更していない。
- IDEの直接PowerShell起動：実行ポリシーで停止。リポジトリ標準GenerateProjectFiles.batは通常／Developmentとも終了0（4.875／5.797秒）。実生成物を指定したPythonは65/65、skip0、終了0。
- 公開ヘッダー：9単独＋2順序＋RigidBody／Mover併用の12構成、MSVC `/std:c++20 /utf-8 /W4 /permissive- /EHsc /GR /Zs`で全終了0。リンク・実行は外部Consumerで別確認。

## 最終検証結果

以下は実行コード`c819a5c1bfcfcc82297287208b33cc7258aaec7f`、831ファイルの同一指紋に対する結果。実デバイスは直列で実行し、build終了0を確認してからCTestへ進めた。Docsだけをこの後に確定する。

| root構成・試行 | build秒／終了 | 登録／成功／skip | CTest秒／終了 |
|---|---:|---:|---:|
| Portable Debug 初回 | 6.969／0 | 28／28／0 | 137.938／0 |
| Portable Release 初回 | 2.531／0 | 28／28／0 | 27.047／0 |
| Native Debug 初回 | 10.813／0 | 34／34／0 | 180.703／0 |
| Native Release 初回 | 15.187／0 | 34／34／0 | 63.750／0 |
| Portable Debug 詳細ログ再保存・第2回 | 1.329／0 | 28／28／0 | 137.078／0 |
| Portable Release 詳細ログ再保存・第2回 | 1.156／0 | 28／28／0 | 26.421／0 |
| Portable Release 変異復元後・第3回 | 2.219／0 | 28／28／0 | 26.891／0 |

初回Portableの合否・JUnit・binary指紋は保持したが、補助runner再開時の列挙で保存済みLastTest.logを空の列挙ログで上書きした。JUnitの成功出力は短縮されており、原ログを復元したとは扱わない。第2回は-V全出力とLastTest.logを直後に別名保存し、両構成で初回と全exe指紋が一致した。変異の実行記録を補った後、Portable Releaseを第3回として全群実行した。各runを合算しない。

PortableはNative・GUIサンプル無効の28群。Nativeは製品・GUIサンプル・実デバイス有効の28 CPU群＋6実デバイス群（Sandbox／Gameplay／Native／Model／PhysicsDebug／Ui）。既存の同一プロセス内終了・新Application・Scene切替系列を保ち、部分選択成功へ置き換えていない。

詳細出力でPhysicsは34内部群525/525件（4件追加）、Frameworkは390/390件（26件追加）、InteractionSampleは38/38件（12件追加）。FrameworkAltCwdの同じ390件を新規件数へ二重計上しない。JointComponentFaultと既存PhysicsOverlapFaultは別プロセスで両次元の注入・回復を維持。

初回の開始／終了UTC・コマンド・指紋・終了コードはfinal-results.json、登録は構成別registration JSON、実行はJUnit。第2回はretained-run2-results.jsonとfinal-Portable-<構成>-retained-run2-*。第3回はrestored-run3-results.jsonとfinal-Portable-Release-restored-run3-*。

### 正規単独入口・配布・その他

| 工程・試行 | 登録／成功／skip、段階 | 秒／終了 |
|---|---|---:|
| ValidateDebug正規単独入口 | Debug 28／28／0、Release 28／28／0、8工程 | 233.937／0 |
| 配布 Portable Debug 最終run | 10/10段階、Native実行なし | 14.500／0 |
| 配布 Portable Release 最終run | 10/10段階、Native実行なし | 18.391／0 |
| 配布 Native Debug 最終run | 13/13段階、Native実行あり | 32.453／0 |
| 配布 Native Release 最終run | 13/13段階、Native実行あり | 34.672／0 |
| No-STL | 690ファイル、違反0 | 0.421／0 |
| Python既定 | 登録65、成功58、skip7 | 5.156／0 |
| Python実IDE生成物指定 | 登録65、成功65、skip0 | 6.047／0 |
| 標準IDE生成 通常／Development | 両方成功、製品／Development分離を維持 | 4.875／0、5.797／0 |
| 公開ヘッダー | 単独9＋順序2＋型付きBody／Mover併用1、12/12 | 全終了0 |
| git diff --check | 文書確定前後で違反0 | 終了0 |

ValidateDebugは正規ターゲットの独立したDebug／Release buildを使用。手列挙coreへ戻していない。ログはStandalone-Final/Summary.jsonとdebug/release-tests.log、作業先Build/DebugValidation/run-y47kzhr_。

配布作業先は次の4つ。各先の再配置 packageをリンクし、外部 Consumerから生成したConsumerBuild/<構成>の実行ファイルを使った。製品はNinja、外部ConsumerはVisual Studio 18 2026 x64。

- Portable Debug：`C:/Users/g0190/OneDrive/Desktop/frame/dxlib_framework/Build/PackageValidation/run-zinrz55q`。
- Portable Release：`C:/Users/g0190/OneDrive/Desktop/frame/dxlib_framework/Build/PackageValidation/run-g93r8txt`。
- Native Debug：`C:/Users/g0190/OneDrive/Desktop/frame/dxlib_framework/Build/PackageValidation/run-eyar_qdh`。
- Native Release：`C:/Users/g0190/OneDrive/Desktop/frame/dxlib_framework/Build/PackageValidation/run-hbuqt1yu`。

PhysicsOnlyは描画／Debug／Gameplay依存なしで両WorldのJointを利用。Consumerは実PhysicsSceneと両Componentの接続・解除・再生成・終了を利用。NativeAppは配布 Application/NativeApp.exeを無関係 cwdから実行し、両最終Native runで2D／3DのEXTERNAL_JOINT_PIXELとNATIVE_CONSUMER_PASSEDを確認した。API成功だけを描画成功とみなしていない。従来SupportOnly／UiOnly／UiRuntime、NativeUiApp両スタイル、exportの元ソース／Build直接参照検査も維持。段階数は現行検証器のOFF=10／ON=13で、過去の8段階を転記していない。

Nativeの画面はJoint線・両端物体・状態文字・1／2表示・1001×501・鎖・動く支点を保存し、画素判定。最終Debugの2D／3D分割画像は実画像も目視した。final-Native-<構成>-gameplay-imagesと各LastTest.logへ保存。OS物理キーでの操作確認とは区別する。

公開ヘッダーのコマンドはHeaderCompile/results.json。C4324は既存の整列型と新しい3D観察型に残る。警告0とは報告しない。既存の限定除外・全体警告方針を拡大していない。IDEは標準batの生成と実project/filter照合までで、人のF5ではない。

### 変異の記録を補った最終系列

先行MutationsFinalは全14を検出したが、実行時刻・exe指紋が不足していた。後続MutationsRecordedへ全14を別runとして実行し、各build／Red／復元build／Greenのコマンド、開始／終了UTC、終了コード、所要秒、Source manifest、Red／Greenの実exe SHA-256、構成（Portable Release、SDKなし）を保存した。製品コードは変えず、生バイト復元と実再コンパイルを確認。両系列は合算しない。

| 変異 | build秒 | Red秒／終了 | 復元build秒 | Green秒／終了 |
|---|---:|---:|---:|---:|
| M01 | 1.704 | 0.328／1 | 2.360 | 0.312／0 |
| M02 | 2.375 | 0.265／1 | 2.391 | 0.312／0 |
| M03 | 2.391 | 0.312／1 | 2.328 | 0.328／0 |
| M04 | 2.375 | 0.328／1 | 2.375 | 0.313／0 |
| M05 | 2.391 | 0.313／1 | 2.359 | 0.312／0 |
| M06 | 2.422 | 0.328／1 | 2.344 | 0.312／0 |
| M07 | 2.375 | 0.187／1 | 2.422 | 0.344／0 |
| M08 | 2.391 | 0.234／1 | 2.359 | 0.313／0 |
| M09 | 2.329 | 0.312／1 | 2.359 | 0.313／0 |
| M10 | 2.359 | 0.188／1 | 2.375 | 0.313／0 |
| M11 | 1.484 | 0.828／1 | 1.516 | 1.547／0 |
| M12 | 2.469 | 0.125／1 | 2.359 | 0.125／0 |
| M13 | 2.391 | 0.281／1 | 2.406 | 0.313／0 |
| M14 | 2.344 | 0.297／1 | 2.360 | 0.297／0 |

### 実行ファイルの指紋（代表）

初回はfinal-<構成>-binary-hashes.json、第2回／第3回はそれぞれ別名のbinary-hashes.json。再コンパイル後のexeを初回と同じ指紋だと仮定しない。下表のPortable Releaseは第3回、他は初回の検証対象。

| 構成 | exe | SHA-256 |
|---|---|---|
| Portable Debug | dxf_physics_tests.exe | `5615742f4f7dca728e0ed895676a411d7c83c3ff564a029434b19fac4f6bfb83` |
| Portable Debug | dxf_joint_component_fault.exe | `59b491b2aaa8e8c8522382b266ec9356990b0b96473a9589d9b7060cc2900470` |
| Portable Release | dxf_physics_tests.exe | `b81120e9768a481ec8dee43f110da30b5f69652738017eca0467ab8f6a01a768` |
| Portable Release | dxf_joint_component_fault.exe | `926f8cb3e4ce180a344a27a1300204d12296dfad234c71372736d4f2ceb663b7` |
| Native Debug | NativeGameplaySmoke.exe | `f38666843ba1d286785ea1e6d5c3624a7ca8cd38d579b7375e2346cbdd5bf31a` |
| Native Debug | NativeModelSmoke.exe | `bfbc7177a12ed6c8345c1f03da5c59b353101afff0b453945ccb701ac25b7107` |
| Native Release | NativeGameplaySmoke.exe | `b11445d0b86f6b55a2ab73d88ab89de67563c4f5f0d38c17eb233ad0dfdae6da` |
| Native Release | NativeModelSmoke.exe | `32c0f3874f28164f91e8e9ca0b7890e9cd604da9565eeb179fddf548c06203a3` |

### 完了範囲

J5の両次元Component・既存Sample・実Application／実DxLib、J6の外部利用・4構成配布・測定・故障注入・14変異、J7の最終全群・正規単独入口・IDE・ヘッダー・現在文書を完了した。空Starter／Sandboxのソースは変更していない。既存未push6コミットを保持し、今回の4コードコミット（d2773c4、2cb7795、525dba4、c819a5c）と文書をmainへ通常pushする。push後の最終完全SHA／origin一致／cleanは最終報告とignored FinalGitState.jsonで記録する。自己コミットSHAを埋め込むための履歴改変は行わない。

## 再実行手順と未実施

既存VS x64開発者環境で同じNative／Portable optionのBuildを分け、Debug／Releaseのbuild終了0を先に確認し、`ctest --test-dir <Build> -C <構成> -j 1 --no-tests=error --output-on-failure`を実行する。Nativeは既存source SDKを`DXF_DXLIB_CUSTOM_ROOT`へ明示し、`DXF_RUN_DEVICE_TESTS=ON`。通常IDE生成はdevice OFFで別の入口。

`python Tools/ValidateDebug.py --logs <新規>`、`python Tools/ValidatePackage.py --config <Debug|Release> --logs <新規>`。Native配布には`--native --sdk-root ThirdParty/DxLib-3.25a-source --run-device`を追加する。全実デバイス実行は直列にし、LastTest.logと画像を次回実行前に別runへ保存する。`python Tools/CheckNoStl.py`、`python -m unittest discover -s Tools/Tests -v`、`git diff --check`を実行する。

実機の物理キー入力、人のF5、聴感、SDK／開発ツール未導入PC、複数DPI、TSan、Worker内部の人工例外、全D3D／COM資源のリーク検査は未実施。以前のNativeModelのProcessMessage=-1原因とUI 3Dパネルの2確保／frameを、今回の成功で解決済みとはしない。GPUモーフ、影、追加マップ、別Joint種類、Editorへ範囲を広げない。
