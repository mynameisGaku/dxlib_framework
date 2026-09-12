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
