# UI継続点（2026-09-26）

- 起点は添付の`17445ee`。GitHubへのcommit／pushは行っていない。このパッケージは途中のUIを引き継いだ更新コード。
- U1〜U8をゼロから作り直さない。作成済みの主要部品・入力Host・スタイル・検査・UISampleを保持する。
- 現時点のUI CPUケース57件、GCC両構成とClang ASan／UBSanで成功。Native OFF配布はDebug／Releaseで成功。
- RootのLinux全群は24/26。更新前からのAssetRootTestsのWindowsパス期待が2群で失敗。単独ValidateDebugは旧Platform.cppのOut shadow警告エラーで停止。
- Windows実行は未確認。まずこの更新を基準のmainへ差分適用し、`GenerateProjectFiles.bat -Development`のUIサンプルと追加NativeUiSmokeをビルドする。最初のコンパイルエラーから解消し、未検証ソースを「動作済み」と扱わない。
- `NativeUiSmoke`は固定入力と実画素の実行入口を用意済み。ただしこの環境でそのSDKによるリンク・実行は一度もしていない。既存のモデル／Physics／Viewportの実描画も再実行する。
- Native ONのUI外部Consumer、3Dパネルの画素単位透明合成、詳細なClipFit要素ID、広い変異試験等は残る。優先順位は[検証記録](UiResume-2026-09-26.md)と[UI進捗](../UI/Progress.md)を確認する。
- 今回触っていないSolver、問い合わせの索引、キャラクター、空Starter、Sandbox、Assets、過去検証記録を旧配布物で上書きしない。
