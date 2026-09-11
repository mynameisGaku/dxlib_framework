# TDDと回帰テスト

テストは外部ライブラリに依存しない小さなC++テストランナーで実行します。`REQUIRE`は失敗時に例外を投げるため、ReleaseのNDEBUGで消えるassertではありません。テストは表示名ごとに登録・集計します。

## 今回の実装順

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

別配布の `dxlib_framework_history.bundle` には、この作業で実際に作ったRed／Green／Refactorのコミットを含めます。

```sh
git clone dxlib_framework_history.bundle dxlib_framework_history
cd dxlib_framework_history
git log --oneline
```

古いRedコミットは意図的にビルドまたはテストに失敗します。履歴は作成順を示すものであり、各コミットが配布可能なリリースであることを意味しません。
