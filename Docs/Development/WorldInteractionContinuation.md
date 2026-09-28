# World相互作用の継続点（2026-09-27）

- 状態：接触・Trigger・動く床（R0〜R7）を実装し、最終回帰はコード `b6d1573` で実施済み。詳細は [検証記録](WorldInteraction-2026-09-27.md)、機能の状態は [Physicsの進捗表](../Physics/Progress.md)（I〜L）、使い方は [World相互作用](../Physics/WorldInteraction.md)。
- 最後の実行（直列）：root CTest Debug／Release 33/33（終了0）、ValidateDebug 27/27×2、配布4構成成功（Nativeは実機起動）、No-STL違反0、Python 56件（実生成物ありでskip 0）、公開ヘッダー53単位、`dxf_character_benchmark interaction` 通常版・計測版とも126条件。ログは `Build/InteractionTakeover-3d9b21fc/final`（コミット対象外）。
- 未解決の最初の項目：イベントを有効にした固定更新の配送の予約に1件／Stepの確保が残る（Prepare）。Solidの物理更新の確保は既存Solver由来。
- `NativeModelDeviceSmoke` の `ProcessMessage=-1` は今回再発していないが原因未特定。UIの3Dパネル4系列の2割当／フレームも別の未解決項目。
- 次に行う場合の候補：配送の予約の事前確保、人による実機操作（F5・物理入力）、複数DPIの実モニター、SDK未導入PCでの配布の起動。

## 追記（2026-09-28: Dynamic Character Interaction とS8）

- Dynamicの双方向押し合い・カプセル・Solver BroadPhaseの検証と最終回帰は
  [Dynamic相互作用の検証記録](DynamicCharacterInteraction-2026-09-28.md)にまとめている。過去の記録を書き換えていない。
- 今回の最終コードでの結果: root CTest Debug／Release 27/27（28群389/389件、0失敗）、
  ValidateDebug 8/8、No-STL 652ファイル0違反、Python 60件（IDE実生成物ありで4件）、
  配布4構成（Native OFF Debug／Release 11/11、Native ON Debug／Release 14/14）、
  ベンチマーク3系列の終了コード0。
- IDE表示は`UISample`と`GameplaySample`のGUI入口を専用ディレクトリへ移し、本体のcppを表示専用で載せた。
  本体の二重コンパイルはなく、`Tools/Tests/test_ide_filters.py`が生成物を集合検査する。
- 配送予約は慣らし後の定常0確保/Step（`interaction`の126行すべてで0）を確認した。
