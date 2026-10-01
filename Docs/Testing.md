# TDDと回帰テスト

テストは外部ライブラリに依存しない小さなC++テストランナーで実行します。`REQUIRE`は失敗時に例外を投げるため、ReleaseのNDEBUGで消えるassertではありません。テストは表示名ごとに登録・集計します。

## 0.1.0の実装順

| 記録 | Redで先に固定した契約 | Greenの実装 |
|---|---|---|
| 01 | 結果型、RTTI、世代付き所有、時計、入力 | Foundationと入力 |
| 02 | 資源の共有・解放、描画、音声、セッション | Loader／Registry／Queue／Audioなど |
| 02b | Native状態復元に失敗しても続行していた問題 | フレーム中止と状態の無効化 |
| 03 | 遅延生成・破棄、Component、Sceneの準備と切り替え | Lifecycle・Collection・SceneNavigator |
| 04 | Applicationの実行、巻き戻し、終了の再入防止 | ApplicationとAppRunner |
| 05 | DxLib呼び出しの引数・状態・失敗時の解放 | 接続部と明示的なテストダブル |
| 06 | ネイティブハンドルを追加確保なしで所有へ移す | noexceptの解放関数ポインタとContext |
| 07 | 実サンプルの移動・描画・Scene切り替え・音 | SandboxGame |
| 08 | 成功データがFError型の場合の結果型の区別 | variantのインデックスによる分岐 |
| 10 | 少数の呼び出しで起動する入口 | Run&lt;Scene&gt; |
| 11 | Sceneへ渡す描画Contextからの安全な即時操作 | IRenderControlとContextへの委譲 |

09・12・13は既存実装への追加の回帰確認、責務ごとのファイル分割、命名のリファクタリングです。新規機能のRedを作った記録とは区別しています。

最初のRedは、まだないヘッダー・APIのコンパイル失敗から始めたものを含みます。02bは実行時に再現した失敗、08は既存テンプレートが新しい妥当な型でコンパイルできない問題です。**すべての失敗が実行時アサーションだった、という説明はしていません。** 対応する生ログは `Tdd/` にあります。

## 再実行

```sh
cmake --preset portable-debug
cmake --build --preset portable-debug
ctest --preset portable-debug

# 個別ケース名を表示
./Build/portable-debug/dxf_tests
./Build/portable-debug/dxf_native_contract_tests
```

Windowsでは末尾に `.exe` を付けて実行できます。

Linuxでの全検証は `python Tools/Validate.py --with-sanitizers` です。各コマンド、終了コード、コンパイル・テスト出力を実行ごとの新しいディレクトリ（既定は `Build/ValidationLogs/Portable/<UTC時刻>-<ID>`）に記録します（下の「検証ログの出力先」）。メモリ検査はAddressSanitizerとUndefinedBehaviorSanitizer、リーク検査は `ASAN_OPTIONS=detect_leaks=1` を用います。

公開ヘッダーは一つずつ別cppからincludeし、PCHやumbrella headerに依存しない構成でコンパイルします。さらに別のCMakeプロジェクトからadd_subdirectoryで組み込み、テスト・サンプル・SDK依存が勝手に有効にならないことと、リンク・実行を確認します。

## テストダブルの限界

`Tests/Support/FakeBackend.h` は基盤の契約を検査するためのバックエンドです。`Tests/FakeDxLib/DxLib.h` はNative実装がどの関数へ何を渡すかを検査するための、手書きで限定的な代替ヘッダーです。後者は実SDKの型・ABI・自動リンク・デバイス動作の検証を代替しません。

## Git履歴

別配布の `dxlib_framework_0.3.0_history.bundle` には、この作業で実際に作ったRed／Green／Refactorのコミットを含めます。

```sh
git clone dxlib_framework_0.3.0_history.bundle dxlib_framework_history
cd dxlib_framework_history
git log --oneline
```

古いRedコミットは意図的にビルドまたはテストに失敗します。履歴は作成順を示すものであり、各コミットが配布可能なリリースであることを意味しません。

## 0.2.0の継続開発

以下は `Tdd/Completion/` に記録した追加開発です。元の95ケースを先に再実行し、そのソースへ修正を積み上げています。

| 記録 | Red／確認内容 | 修正・追加 |
|---|---|---|
| 01 | 7件の実行時失敗 | デストラクタの再入、終了要求、例外境界、Scene準備中の終了 |
| 02 | 8件の実行時失敗と1件の既存成功回帰 | UTF-8、NUL、所有元、描画エラー、null登録、準備中の自己破棄 |
| 03 | 未実装APIによるコンパイル失敗 | 複数デバイスのInputMap、型付き検索、SpriteComponent |
| 04 | install先が存在しない統合検査の失敗 | 層別CMakeターゲット、再配置できるfind_package |
| 05 | 2件の実行時失敗 | 文字描画・ウィンドウ名のネイティブ文字列検証 |
| 06 | 未実装Smokeヘッダーによるコンパイル失敗 | 自動終了する共通Smokeコードと実SDK用ターゲット |
| 07 | 2件の実行時失敗 | コンストラクタからの終了、キャッシュによる設定検証回避 |
| 08 | 追加の成功回帰検査 | 2万操作・資源1,000周・全Unicodeコードポイント値 |
| 09 | Python配布モジュール未実装の読込失敗 | 再現可能なZIP・SHA-256・除外規則の6テスト |

08をRed→Greenの不具合修正とは扱っていません。Windows用スクリプトとCI設定には静的検査のみを行い、実機成功として数えていません。全コード行や全条件をテストしたという意味でもありません。

0.2.0時点の通過件数はC++113＋14件、Python6件です。C++はGCC Debug／ReleaseとClang ASan／UBSanの3構成で実行しました。公開ヘッダー65個とSandboxヘッダー1個を独立した翻訳単位で検査しています。

## 0.3.0の継続開発

0.2.0（48コミット時点）のソースとテストを復元し、既存127件が通ることを確認してから着手しました。以下の記録は`Tdd/Continuation/`です。

| 記録 | 修正前の実測 | 修正後 |
|---|---|---|
| 01 | 新規8件が失敗、113 / 121通過 | 描画失敗の保持・例外境界。121 / 121通過 |
| 02 | 新規11件が失敗、121 / 132通過 | Scene差し替えの再入防止・子階層の停止。132 / 132通過 |
| 03 | 新規6件のうち5件が失敗報告、最後のケースで終了コード139のクラッシュ | 資源終了・音声開始と状態取得の再入。138 / 138通過 |
| 04 | Pythonの既存6件通過、新規3件失敗 | 古い成功Summaryの除去・タイムアウト出力の保存。9 / 9通過 |

03のRedではプロセスが途中で落ちたため、実行ファイル末尾の総件数は出ていません。クラッシュを単なるassert失敗や「全ケース実行済み」として数えていません。Greenでは最後まで走らせています。

01では、旧テストの「Nativeコールバックが例外を投げても状態復元できればEndFrame成功」という期待値も、失敗フレームを提示しない新しい契約へ変更しました。これは意図した動作変更であり、互換性を保った修正とは説明していません。対応する変更は同じGitコミットで確認できます。

今回追加したC++25件はいずれも修正前に実行失敗を再現した回帰テストです。14件の代替SDK契約テストは件数を増やしていませんが、全構成で再実行します。Windows／実SDK検査とは区別してください。

Pythonの検証失敗テストでは、わざと子プロセス失敗を注入するため、成功したテストの中に`debug-configure: FAIL`という出力が含まれます。ユニットテスト全体のOKと、実際の全検証Summaryのstatusを別々に確認してください。


## 単独Debug検証入口（現行の正規ターゲット）

`Tools/DebugValidation` はrootを `add_subdirectory` で取り込み、正規のToolbox / Support / Physics / Runtime / Gameplay / Debugと試験登録を利用します。本体cppを別のcoreへ手列挙しません。Native・実デバイス・Viewer・Starter/Sandbox実行ファイル・installはこの入口ではOFFです。サンプルのSceneソースを使うCPU試験と、手書きNative境界を使う翻訳試験は実行します。

```powershell
cmake -S Tools/DebugValidation -B Build/DebugValidation-local -A x64
cmake --build Build/DebugValidation-local --config Debug --parallel 4
ctest --test-dir Build/DebugValidation-local -C Debug -N
ctest --test-dir Build/DebugValidation-local -C Debug -j 1 --no-tests=error --output-on-failure
cmake --build Build/DebugValidation-local --config Release --parallel 4
ctest --test-dir Build/DebugValidation-local -C Release -j 1 --no-tests=error --output-on-failure
```

各工程の終了コードを確認し、生成・ビルド失敗後は試験へ進みません。単一構成Generatorでは構成ごとに別ディレクトリを使ってください。

再発確認は `python Tools/ValidateDebug.py` です（ログは `Build/DebugValidationLogs/<UTC時刻>-<ID>`。CTestは `-V` で成功した試験の出力も残し、JUnitはrunのディレクトリへ出します）。毎回新しい作業ディレクトリを作り、Debug/Releaseの全ビルド、必要群の登録、Native等のOFF、JUnitの実行結果を照合します。欠落・重複・スキップ・失敗を成功にしません。Windowsでは既定Visual Studio/x64、その他は既定Generatorを使います。Python単体試験は検証器の制御だけを確認し、C++コンパイラーやSDKを要求しません。

旧13群（DebugTools / DebugPhysicsCapture / RenderContinuation / JobFault 5群 / RenderViews / NativeViewsTranslation / Transparency 3群）を保持し、現行では正規のportable登録を含む22群です（範囲問い合わせの確保故障注入 `PhysicsOverlapFault` を含む）。DebugPhysicsCaptureはSnapshot選択と実RenderDebugの11ケース、PhysicsContinuationはWorld問い合わせ（3D 7ケース、2Dの線分交差とWorld問い合わせ9ケース、2D／3Dの対象フィルター20ケース、2D／3Dの範囲問い合わせ20ケース、2D／3Dのスイープ問い合わせ20ケース、スイープの接触法線9ケース、円・球の移動候補（1回の滑り）15ケース、2D／3Dのキャラクター移動（接触・初期重なり・反復滑り・接地・段差・ジャンプ）16ケース、World問い合わせの索引（木の構造7ケース、同じWorldで索引と総当たりの結果・例外を比べる一致6ケース、自動同期・失敗・世代・順序・切り替え条件・診断の契約20ケース））を含みます。範囲問い合わせの結果確保の故障注入と、スイープ問い合わせ（接触法線の有無を含む）・移動候補・キャラクター移動（接触の取得・MoveAndSlide・StepCharacter）の通常経路で確保しないことの確認、索引の登録途中の確保失敗で索引に何も残らないことと、登録・移動の直後の問い合わせで確保しないことの確認（6件）は隔離した `PhysicsOverlapFault` 群です。キャラクター移動Component（2D／3D）の14ケースは `Framework`（dxf_tests）群です。`debug_tests` / `debug_physics_tests` の実行ファイル名は正規の `dxf_debug_tools_tests` / `dxf_debug_physics_tests` に統一しました。通常は名前を直接実行せずCTestを使います。

警告はrootの設定を利用します。旧入口で `/WX` だったContinuation / JobFault / RenderViews / NativeViewsTranslationの4ターゲットでは厳格条件を保持します。MSVCのC4324のみ、既存FVector3/FQuaternionの `alignas(16)` による意図したパディングとしてPRIVATEで除外し、他の警告をエラーにします。型・ABIは変えません。非MSVCでは同じ4ターゲットに加え、旧coreを構成した正規層と旧Debug/Transparency試験にも `-Werror` を保持します。

`DXF_DEBUG_ASAN=ON` はaddress/undefined、`DXF_DEBUG_TSAN=ON` はthreadの計測を、実装ライブラリも含む全対象へ付けます。同時指定は拒否します。この入口の計測は非WindowsのGCC/Clangを対象とし、コンパイラー/ランタイムのリンク可否を構成時に確認します。Windows等の対象外環境では明示的に生成失敗とし、指定を無視しません。ONを受理できても計測実行の成功とは別です。

これは同じPCで実DxLib SDKを使用しない検証です。SDK未導入PC・実DxLib描画・rootの実デバイス試験とは別の結果として扱ってください。[修復の実行記録](Development/DebugValidationRepair-2026-09-24.md)に修正前の失敗、今回の結果と未解決事項を記録しています。


## 検証ログの出力先

`Tools/Validate.py`・`ValidateDebug.py`・`ValidatePackage.py`は、実行ごとに自分のログのディレクトリを持ちます（共通処理は`Tools/ValidationSupport.py`）。

- `--logs`を省くと、既定の親の下に`<UTC時刻>-<ID>`の新しいディレクトリを作ります。同じ名前を再使用しません。
- `--logs`を指定する場合、存在しないか空のディレクトリだけを受け付けます。ファイルがあるディレクトリは、子プロセスを一つも起動する前に拒否し、中身を変更しません（削除・初期化もしません）。別のディレクトリを指定し直してください。
- `Summary.json`は排他的に作成し、同じディレクトリを同時に使う二つ目の実行は失敗します。状態は`running`から`passed`／`failed`／`interrupted`になり、run ID・開始／終了時刻・作業ディレクトリ・HEAD・`git status`・実バイトのソース指紋（`source_sha256`、一覧は`SourceManifest.txt`）・各工程のコマンド・作業ディレクトリ・終了コード（`TIMEOUT`・`NOT_STARTED`を含む）・ログのパスを持ちます。
- 同じ工程名のログは上書きしません（再試験は別の工程名で記録します）。
- 実機の試験が出す`DXF_CHECK <名前>=verified|not_exercised|failed`の行は、成功した工程でも`Summary.json`の`checks`へ記録します。終了コード0だけで、実行しなかった確認（前面でないためのOSの捕捉など）を確認済みにしません。
- 実バイトの指紋だけを取るには`python Tools/SourceFingerprint.py <新しいファイル>`を使います（Gitのindexではなく作業ツリーのバイトを対象にし、HEADと`git status`は別の行に書きます）。

`Docs/Validation`の既存のファイルは、当時の実行の記録として残しています（新しい実行はそこへ書きません）。

## キャラクター移動のサンプルを固定入力で確認する（NativeGameplayDeviceSmoke）

`DXF_RUN_DEVICE_TESTS=ON` のrootでは、`NativeGameplayDeviceSmoke` が `Examples/GameplaySample` のSceneを実Application・実DxLibで、入力だけを固定して操作します（出力先 `gameplay-smoke-<構成>`）。2D／3Dの両方で同じ手順（`SmokeAcceptance`）を実行します: 歩行・低い段差・30度の坂・60度の急坂の手前での停止、停止と接地、リセット、ジャンプと着地、低い天井で頭を打って着地、初期重なりからの復帰、壁と床の角（3Dは二つの壁の稜線）での停止、一時停止と再開（アニメーション時間を含む）、途中での歩行キャラクターの生成と破棄（Colliderの数が増減する）、毎フレーム地形と重ならないこと、プレイヤーの描画位置の画素。さらに `SmokeTraces` で、同じ固定入力（生成・歩行・ジャンプ・反転）を1画面・2画面・総当たりの参照経路（`SetQueryIndexEnabled_Internal(false)`）で、それぞれ新しいApplicationで実行し、フレームごとの位置・速度・固定更新の数・ジャンプと着地・歩行キャラクターの位置・アニメーション時間がビット単位で一致することを確かめます。シーンの切替・再入場・2画面の表示の保存・終了と、終了後の新しいApplicationでの再起動も確認します。上限は120秒です（Debugで約13秒）。他のデバイス試験と同じデバイスを使うため、並列に実行しないでください（`-j 1`）。

サンプルは開発用ソリューション（`GenerateProjectFiles.bat -Development`）の `GameplaySample` です。キャラクター移動の負荷測定 `dxf_character_benchmark` はCTestに登録していません（[キャラクター移動](Physics/CharacterMovement.md#性能測定)）。系列は `legacy`（従来の18条件、地形がDynamic）・`legacy-static`（同じ条件で地形をStaticにした比較の主系列）・`heavy`（1024／64）・`scaling`（移動・局所・動く物体・密集・生成と破棄・大きな床）・`costs`（問い合わせの種類ごとの費用）・`kernels`（詳細判定の単体費用）で、`--reference` で総当たりの参照経路を測ります。

## 接触・Trigger・動く床のサンプルを確認する（InteractionSample）

[相互作用サンプル](Physics/WorldInteraction.md#既存gameplaysampleで試す)は、通常のGameplaySample、CPU試験、既存のNativeGameplaySmokeで同じ `dxf_gameplay_sample` を使います。確認する内容と実行環境を分けて記録してください。

| 入口 | 確認する内容 | 描画の扱い |
| --- | --- | --- |
| `InteractionSample`（`dxf_interaction_sample_tests`） | 実Applicationと2D／3D Sceneで、箱の接触、取得物の一度だけの破棄、圧力板のBody単位の占有、チェックポイント復帰、View数・ポーズ・設定Modal・描画先寸法とDPIの変更を確認する | Backendは代替。描画命令や分割領域の境界を確認し、実画素の証拠にはしない |
| `NativeGameplayDeviceSmoke`（`NativeGameplaySmoke`） | 従来のキャラクター試験に加え、2D／3Dそれぞれで同じ時刻・固定入力を1／2Viewへ与え、数値・ゲーム状態とContact／Triggerイベントの内容・順序を比較する。移動床、扉、支持先の破棄、ポーズ、設定からの復帰、Scene切替・再入場・Application再起動も含む | 実DxLibの描画先からプレイヤーと床の画素を読み戻して判定し、PNGを保存する |

上の「単独Debug検証入口」で生成したビルドから、CPU群だけを選ぶ例です。Releaseは構成名を置き換えます。全群の確認には `python Tools/ValidateDebug.py` を使います。

```powershell
cmake --build Build/DebugValidation-local --config Debug --target dxf_interaction_sample_tests
ctest --test-dir Build/DebugValidation-local -C Debug -j 1 -R '^InteractionSample$' --no-tests=error -V
```

Nativeは[WindowsのSDK設定](WindowsValidation.md)を済ませ、rootの `DXF_BUILD_NATIVE=ON`・`DXF_BUILD_NATIVE_SMOKE=ON`・`DXF_RUN_DEVICE_TESTS=ON` で生成したビルドを使います。次の `Build/NativeInteraction-local` はそのビルド先へ置き換えてください。他の実デバイス試験と同時に起動しません。

```powershell
cmake --build Build/NativeInteraction-local --config Debug --target NativeGameplaySmoke
ctest --test-dir Build/NativeInteraction-local -C Debug -j 1 -R '^NativeGameplayDeviceSmoke$' --no-tests=error -V
```

固定入力の確認は数値とゲーム効果の検査です。仕掛けの開始位置へ移るための公開Teleportを含むので、全コースをキー操作だけで連続完走した検査ではありません。`INTERACTION_VIEW_INVARIANCE` の比較では、別ApplicationのWorld識別子だけを対応付け、Body／Colliderの世代やイベント順を比較に残します。

実画素の判定は `INTERACTION_PIXEL` の出力と `interaction2d_single.png`／`interaction2d_split.png`／`interaction3d_single.png`／`interaction3d_split.png` を確認します。設定画面の `interaction2d_settings.png`／`interaction3d_settings.png` は画像保存であり、設定UIの画素合否とは区別します。CPU試験の成功、画素の判定、保存画像の目視確認を一つの結果にまとめないでください。

各コマンドの終了コードと出力は[実行ごとのログ](#検証ログの出力先)へ保存します。`ValidateDebug.py` の `--logs` は新規または空のディレクトリだけを受け付け、省略すれば固有のrunを作ります。個別のCTestには `--output-log <run内の未使用ファイル>` と `--output-junit <run内の未使用XML>` を付けられますが、CTest自体には同じ出力名の上書き防止はありません。再試験は別runへ記録し、最初の失敗を残します。

NativeのCTest画像出力先はビルド内の `gameplay-smoke-<構成>` で固定です。各実行のログと画像を再実行前にrunへ保存してください。実行ファイルを直接起動する場合は `NativeGameplaySmoke <ソースルート> <新しい画像出力先>` の二引数で出力先を分けられます。`--logs` は検証スクリプトの引数であり、この実行ファイルやCTestの引数ではありません。

相互作用のCPU費用は、CTestとは別に既存の `dxf_character_benchmark interaction` をReleaseで実行します。上記のVisual Studio用Debug検証入口を使う例です。

```powershell
cmake --build Build/DebugValidation-local --config Release --target dxf_character_benchmark
.\Build\DebugValidation-local\framework\Release\dxf_character_benchmark.exe interaction --pilot
.\Build\DebugValidation-local\framework\Release\dxf_character_benchmark.exe interaction
```

`--pilot` は短い測定、通常実行は同じ系列の本測定です。`interaction` は `--reference` を受け付けません。各実行のCSVと終了コードを別のログへ保存し、[系列・列の定義と測定できない内訳](../Tools/CharacterBenchmark/Interaction.md)に従って比較します。これらの値に実描画やUIの費用は含みません。

## 再配置したパッケージを使う（ValidatePackage）

`python Tools/ValidatePackage.py` は、ビルド・install・インストール先の移動・`find_package` による外部の利用（`Tools/PackageConsumer`）を、ネットワークなしで確認します。既定はNative OFF・Debugで、SDKは不要です（ログは `Build/ValidationLogs/Package/<構成>/<UTC時刻>-<ID>`）。

| 引数 | 内容 |
|---|---|
| `--config Debug` / `Release` | 構成。ログの親は `Build/ValidationLogs/Package/<構成>-Native` または `-Portable` |
| `--native --sdk-root <SDK>` | `dxf::native` もビルドし、外部のApplication（2D／3Dのキャラクター移動を実DxLibで動かす）を作る。SDKは公式VCパッケージか `Tools/DxLibFbx` のビルド（`DxLibFbx.json`）を明示する。取得・ビルドはしない |
| `--run-device` | 実行ファイルだけを別のディレクトリへ置いて起動する（窓とデバイスを使う。他のデバイス試験と重ねない） |
| `--work` / `--logs` | 作業・ログの出力先。`--logs` は存在しないか空のディレクトリだけを受け付ける（下の「検証ログの出力先」） |

確認: 移動後のCMakeファイルに元のソースツリー・元のBuildの絶対パスが残らないこと、`dxf::physics` の依存にNative・Debug・Support・Runtime・Gameplayが含まれないこと、Physicsだけ・Supportだけ・全体の利用者の実行。SDKがない環境では `--native` を実行できず、その構成は未確認として扱います（成功とはみなしません）。

## Visual StudioからNativeModelSmokeを起動する（F5）

`GenerateProjectFiles.bat -Development` で生成したルートの `dxlib_framework-development` ソリューションを開き、`NativeModelSmoke` を起動対象に選んでF5を押すと、CMakeが生成した二引数（ソースルートと、構成別の出力先 `Build/VisualStudio-development/model-smoke-vs-Debug` または `-Release`）が付き、作業ディレクトリは同じ構成の実行ファイルのディレクトリになります。手で引数を入力する必要はありません。既定の起動対象は変更していません。CMakeLists.txtを変更した後や設定が見当たらない場合は、同じコマンドで再生成してください。

この設定はVisual Studio Generatorだけに適用され、デバイス試験のCTest登録（`DXF_RUN_DEVICE_TESTS`、通常の `-Development` ではOFF）とは独立しています。CTestの `NativeModelDeviceSmoke` は、デバイス試験を有効にしたBuildで `model-smoke` へ出力します。出力先が別でも同じデバイスを使うため、F5・CTest・Debug・Releaseの実行は重ねないでください。exeを直接起動する場合は、従来どおり `NativeModelSmoke <ProjectRoot> <output directory>` の二引数が必要で、引数がなければ使い方を表示して終了コード2で終わります（出力先の親ディレクトリは存在している必要があります）。

## モデル実描画の終了・再起動の診断

`NativeModelDeviceSmoke` はPicking、低レベル描画、複数の新規Application、再度Pickingと描画を同じプロセスで実行します。順序・画素判定・既存の120秒上限は維持します。

`ModelLifecycle` は構成、Application通し番号、期待値、フレーム番号、Stepのtrue/false/errorと最初のエラー、初期化・OnDraw・故障注入実行・読戻し・終了への到達を出力します。継続を期待するStepが成功falseなら直ちに試験を失敗させ、その後のScene要求へ進みません。注入試験は、実際の資源失効と、その描画フックが返す特定のUserExceptionを要求します。

Runtimeの `ApplicationLifecycle` は継続不可を決めた分岐とScene/資源/Platform終了順を記録し、Nativeの `NativeLifecycle` は同じ一回のDxLib_Init / DxLib_Endの戻り値、ProcessMessageが継続不可を返した際の実戻り値・初期化状態を記録します。RuntimeにSDK依存は追加していません。ProcessMessageの成功フレームを無制限に記録したり、採取のためにStepやSDKを余分に呼んだりしません。既存DXF_LOGのInfo水準を使用し、検証時はログをOffにしないでください。

単独調査は構成ごとに直列に実行し、最初の有用な失敗で反復を止めます。上限はDebug/Release各5プロセスとし、全群検証内のモデル試験も回数に含めて管理すると上限を超えません。

```powershell
ctest --test-dir Build/FbxContinuation -C Debug -j 1 -R '^NativeModelDeviceSmoke$' --no-tests=error --output-on-failure
# 毎回、別の名前で保存する。次のCTestはLastTest.logを上書きする。
Copy-Item Build/FbxContinuation/Testing/Temporary/LastTest.log Build/model-attempt-1-full.log
Get-FileHash Build/FbxContinuation/Debug/NativeModelSmoke.exe -Algorithm SHA256
```

Releaseは構成名と実行ファイルのパスを変更します。成功時の詳細はLastTest.logにも残ります。`Build/FbxContinuation/Log.txt` があれば各単独試験の直後に別名保存してください。SDKのLog.txtは再初期化時に更新されるため、それだけで全セッションやOS通知の起源を説明できるとは限りません。全群終了後のLog.txtは後続の別デバイス試験のものになり得ます。

CPU回帰は終了契約と試験判定の検証であり、実DxLibの偶発終了を再現した証拠ではありません。[今回の診断と検証記録](Development/NativeModelLifecycle-2026-09-24.md)を参照してください。

## Jointのゲーム利用を検証する

`Framework`は実SceneのBody参照・初期化順・寿命・失敗観察、`InteractionSample`は実Applicationと固定入力のPause／Modal・0／複数固定更新・1／2表示・1001×501リサイズ・Scene再入場／終了、`JointComponentFault`は独立した実行ファイルで登録・初回接続・再接続の確保失敗を検証します。`PhysicsContinuation`は接触と拘束の併存、世代入替後のWarm Startも含みます。

NativeGameplayDeviceSmokeは通常の既存シナリオを保ち、同じSampleのJoint線・両端物体・状態文字を一括GPU読戻しで確認します。1／2表示のBody／Joint全成分はWorld識別子だけを対応付けて比較します。診断用の`--joint-only`／`--interaction-only`は部分実行であり、全体成功へ数えません。

配布はNative OFF／ON×Debug／Releaseを別runで実行します。Windowsの外部Consumerは日本語を含むリンク入力を扱うためVisual Studioの複数構成ビルドを使い、明示した`--config`をbuild・exeの双方へ適用します。配布本体のNinjaビルドとは区別します。日本語と空白を含むインストール／外部ソース／配置先で、無関係な作業ディレクトリからNativeAppを実行します。既存SDKを使用した同一PCでの確認であり、SDK未導入PCではありません。

測定はCTest外の`dxf_character_benchmark joint --pilot`、続いて`joint`。変異は保存した対象生バイトへ復元し、再buildと同じ限定回帰を実行します。最終全群・配布・公開ヘッダー・IDE実生成物の結果は[試行別記録](Development/JointGameplayCompletion-2026-10-01.md)にまとめます。

## 回転・固定・直動の仕掛けを検証する

新3種類は2D/3Dで共通slotと成功Step確定を使用します。PhysicsContinuationの`--mechanisms`は63件の限定実World/解析、Frameworkは6Component・型付きBody参照と目標制御、JointComponentFaultは全種類の予約・初回接続・再接続・終了の確保失敗を含みます。限定実行を登録済み全群の成功へ数えません。

NativeGameplayDeviceSmokeの標準系列は旧試験を保持し、実GameplaySampleの全種類・両次元を通常/左右/1001×501で一括読戻しして確認します。`--mechanism-only`は診断用の部分実行です。NativeUiAppの配布Consumerは既存UIを保持して新装置を操作し、要求だけでなく有限Torqueで逆転した物理観察も確認します。自動入力と人の操作を区別します。

```powershell
python Tools/ValidateMechanismMutations.py --build <Portable-VS-Build> --logs <新規ログ先>
python Tools/ValidateDistanceJointMutations.py --build <Portable-VS-Build> --logs <別の新規ログ先>
# 個別指定の例（Releaseの対象だけbuildし、正常版の生バイトへ戻して再compileする）。
python Tools/ValidateMechanismMutations.py --build <Portable-VS-Build> --logs <新規ログ先> 1 8 12
```

変異は他のbuild・配布・測定と同時実行しません。各対象の生バイトbackup、開始/終了時刻、command/cwd、source/exe指紋、更新時刻、build/Red/復元build/Greenを残します。build失敗は検出に数えず、復元後のGreenでもその変異試行の失敗は維持します。検証器自体のbuild失敗時復元を一時コピーでPython回帰します。

`dxf_character_benchmark mechanism --pilot`と`mechanism`は5新規World・両次元・同期/4レーン（代表8レーン）、独立/共有支点/鎖/Contact混在、活動Motor/Limit停止/Sleep、Component同値維持/Drive変更/接続切替を測ります。初回/慣らし/定常のms/Stepと中央値/最小/最大、確保、Body/種類/構成した基本拘束行数、実Contact/島/活動/休止数を保存します。最大島は配置から導いた値で、一般Worldの実測ではありません。一本鎖の内部並列化やWorld無確保を保証しません。

配布4構成では新PhysicsOnly、6Component/目標helperのFramework、NativeApp/NativeUiAppを再配置して実行します。旧support/ui/ui_runtimeのConsumerを維持し、コピー集合とConsumer CMakeの両方へ登録します。SDKの取得・再構築は行いません。[今回の全試行・未実施](Development/MechanismJointsCompletion-2026-10-01.md)を参照してください。

## Scene／Prefabを検証する

SceneContentはJSON／schema／参照／配置／公開先、実Worldの両次元Prefab、Task要求の終端と採用順、実Applicationの資源・遷移・表示・音の契約を確認します。SceneContentAllocationFaultは独立した確保故障基盤を使い、読解・親初期化・固定更新・再読み込み・所有側部分資源準備の注入と回復、終了での非注入を確認します。InteractionSampleは既存ゲーム経路を保持し、実Sampleの定義コース・UI・個体操作を追加します。

NativeGameplayDeviceSmokeは既存標準系列の後に同じデータ定義の2D画像・3Dモデル・四種類Jointの構成を通常／左右／1001×501で確認します。一括読戻しと対象領域の画素判定を使い、HUDだけで合格しません。`--content-only`は限定診断であり全群成功へ数えません。

`Tools/ValidateContentData.py`は新しい検証コピーの定義だけを変更し、同じexe・別CWD・日本語パスでA/BのWorld値と実画素の差、壊れたCと再試行を記録します。`Tools/ValidateContentMutations.py`はB-M01〜18のbuild／Red／復元build／Greenと生バイト指紋を記録します。変異は全群・配布・測定と重ねません。初回未検出やハーネス失敗も残します。

`dxf_character_benchmark content --pilot`と`content`は両次元・1/32/256個体・Jointなし／四種類、定義共有／個別、試験用Textureの共有／個別を比較し、5新規環境で読取・検証・準備・受付・初期化・Ready・定常・状態読取・World比較・終了をμsと確保数で保存します。CPU境界のダブルを使う準備費用と実Nativeの取込費用を混同せず、Content状態読取の0確保をWorld全体の0確保と呼びません。

ValidationSupportのmanifestにはAssetsも含み、実行中に製品／試験／ツール／定義のバイトが変わったrunは採用しません。生成物・ログは新しい保存先へ置きます。配布Consumerは任意targetをリンクして定義Assetsを配置し、既存PhysicsOnly／Support／UIの入口を保持します。[形式](Content/SceneDefinitions.md)・[Prefab](Content/Prefabs.md)・[準備と失敗境界](Content/Loading.md)を参照してください。
