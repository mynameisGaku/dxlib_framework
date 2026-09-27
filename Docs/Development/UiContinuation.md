# UI継続点（2026-09-27）

- 状態：ウィンドウ統合（P0〜P7）を実施し、HEADは文書更新の前が`f24ec65`。詳細は[P0〜P7の検証記録](UiWindowIntegration-2026-09-27.md)、前回は[W0〜W7の検証記録](UiWindowsCompletion-2026-09-26.md)、機能の状態は[UI進捗](../UI/Progress.md)。
- 最後に実行した確認（`f24ec65`、直列）：root CTest Debug／Release 32/32（終了0）、`Tools/ValidateDebug.py` 26/26×2（終了0）、配布4構成（終了0、`--run-device`でNativeApp・NativeUiApp成功）、No-STL 570ファイル違反0、Python 47件（実生成物ありでskip 0）、変更公開ヘッダー20/20、変異11件すべて検出。ログは`Build/UiIntegration-Logs/final`（コミット対象外）。
- 未解決のエラーはなし。`NativeModelDeviceSmoke`の`ProcessMessage=-1`は今回再発していないが原因未特定。
- 前回の`Build/UiCompletion-Logs/final2`のValidateDebugの生ログは今回の実行で上書きした（経緯は同フォルダーの`VALIDATE_LOGS_OVERWRITTEN.txt`）。
- W5の改善前の基準（`161f6af`）はリポジトリ外の`C:\Users\g0190\ui-w5-baseline`に保存。現行ツリーへ戻さない。
- 次に行う場合の候補：人によるF5・ウィンドウ操作・物理入力、複数のDPIの実モニター、他のアプリによる捕捉の奪取、3Dパネルを描く系列の2回/フレームの割当元、Native実行の区間の文字の命令ごとの割当元、SDK未導入PCでの配布の起動。
- `68e145d`のコミットメッセージの「奥行641/639」は「幅641/639」の誤記（履歴は書き換えない）。
