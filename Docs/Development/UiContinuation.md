# UI継続点（2026-09-27）

- 状態：UIのWindows仕上げ（W0〜W7）を実施し、最終回帰は`eb0dc18`で実行済み。詳細は[検証記録](UiWindowsCompletion-2026-09-26.md)、機能の状態は[UI進捗](../UI/Progress.md)。
- 最後に実行した確認（`eb0dc18`、直列）：root CTest Debug／Release 32/32（終了0）、`Tools/ValidateDebug.py` 26/26×2、配布4構成（`--run-device`でNativeApp・NativeUiApp成功）、No-STL違反0、Python 47件（実生成物の検査を含めskip 0）、W5測定。ログは`Build/UiCompletion-Logs/final2`（コミット対象外）。
- 未解決のエラーはなし。`NativeModelDeviceSmoke`の`ProcessMessage=-1`は今回再発していないが原因未特定。
- W5の改善前の基準（`161f6af`のソースとビルド、測定結果）はリポジトリ外の`C:\Users\g0190\ui-w5-baseline`に保存。現行ツリーへ戻さない。
- 次に行う場合の候補：Visual Studioでの実F5操作・物理入力・聴感の人による確認、SDK未導入PCでの配布の起動、Backendが乗算済みの合成に対応するかを問い合わせる窓口、実字体・実描画を含む性能測定。
- `68e145d`のコミットメッセージの「奥行641/639」は「幅641/639」の誤記（履歴は書き換えない）。
