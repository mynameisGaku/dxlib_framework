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

Linuxでの全検証は `python Tools/Validate.py --with-sanitizers` です。各コマンド、終了コード、コンパイル・テスト出力を `Docs/Validation` に記録します。メモリ検査はAddressSanitizerとUndefinedBehaviorSanitizer、リーク検査は `ASAN_OPTIONS=detect_leaks=1` を用います。

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

再発確認は `python Tools/ValidateDebug.py --logs Build/DebugValidationLogs` です。毎回新しい作業ディレクトリを作り、Debug/Releaseの全ビルド、必要群の登録、Native等のOFF、JUnitの実行結果を照合します。欠落・重複・スキップ・失敗を成功にしません。Windowsでは既定Visual Studio/x64、その他は既定Generatorを使います。Python単体試験は検証器の制御だけを確認し、C++コンパイラーやSDKを要求しません。

旧13群（DebugTools / DebugPhysicsCapture / RenderContinuation / JobFault 5群 / RenderViews / NativeViewsTranslation / Transparency 3群）を保持し、現行では正規のportable登録を含む21群です。DebugPhysicsCaptureはSnapshot選択と実RenderDebugの11ケース、PhysicsContinuationはWorld問い合わせ（3D 7ケース、2Dの線分交差とWorld問い合わせ9ケース）を含みます。`debug_tests` / `debug_physics_tests` の実行ファイル名は正規の `dxf_debug_tools_tests` / `dxf_debug_physics_tests` に統一しました。通常は名前を直接実行せずCTestを使います。

警告はrootの設定を利用します。旧入口で `/WX` だったContinuation / JobFault / RenderViews / NativeViewsTranslationの4ターゲットでは厳格条件を保持します。MSVCのC4324のみ、既存FVector3/FQuaternionの `alignas(16)` による意図したパディングとしてPRIVATEで除外し、他の警告をエラーにします。型・ABIは変えません。非MSVCでは同じ4ターゲットに加え、旧coreを構成した正規層と旧Debug/Transparency試験にも `-Werror` を保持します。

`DXF_DEBUG_ASAN=ON` はaddress/undefined、`DXF_DEBUG_TSAN=ON` はthreadの計測を、実装ライブラリも含む全対象へ付けます。同時指定は拒否します。この入口の計測は非WindowsのGCC/Clangを対象とし、コンパイラー/ランタイムのリンク可否を構成時に確認します。Windows等の対象外環境では明示的に生成失敗とし、指定を無視しません。ONを受理できても計測実行の成功とは別です。

これは同じPCで実DxLib SDKを使用しない検証です。SDK未導入PC・実DxLib描画・rootの実デバイス試験とは別の結果として扱ってください。[修復の実行記録](Development/DebugValidationRepair-2026-09-24.md)に修正前の失敗、今回の結果と未解決事項を記録しています。


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
