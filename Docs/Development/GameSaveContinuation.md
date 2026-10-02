# L作業の継続点

対象はDXF-L-SAVE-4b20f89-20261002-R2、L0〜L10。Bへ戻らない。mainのみ。用語はバージョン、各@paramは別行。現在HEAD `734fbfcf9a263aa44b643f6a58b123696c7402c4`、最後にfetchしたorigin/main `4b20f89a451233bc8c328df98f48a259808a3285`。13ローカルcommit、push未実施。開始時に利用者差分なし。

L0調査済み。L1 codec・明示移行、L2二面保存・回復・排他、L3 Scoped要求を実装（全I/O故障未完了）。L4成功全Post後の両次元論理採取、L5信頼するカタログ・新Scene・4種の内部／個体間Joint・必須Ready完了、L6完全Input profileと独立設定、L7実Sensor・扉・動的荷物・手動／autosave／New／Load／Retryを実装。今回、既存メニューのNew／Continue、保存Panel・上書きModal・再割当・Volume／UiScale・スロット表示・公開結果未確定の明示再確認を接続した。候補Scene構築後の設定変更を入力更新前に同期し、非フォーカスScrollは子の方向操作を妨げない。L8部分回帰のみ、L-M01〜20未実施。L9 CPU別プロセス両次元を確認済み、独立installゲームConsumer・測定・Nativeを含む配布4構成は未完了。L10最終全累積・push未実施。

今回の未commit差分は保存UI一式、GameInstance／両Scene／Portal／InteractionUi／通常Windows入口、UI Scrollと独立試験、Session設定競合と実クリック試験、CMake登録と現在Docs。正確な一覧はgit status --short。本UIの論理commitをこれから作る。過去のcommitや利用者の成果は保持する。

最新の成功：PortableSaveCourseUiEntryFrozenRelease02／Debug01全38/38・失敗0・skip0、前後manifest `b44e4345af55ebad03fdb705315a21f1b2b3745ef3210bcda0aa049ca0da0d4d` 同一、No-STL989違反0、build・diff終了0。NativeUiEntryBuildDebug01は同じコードのGameplaySample Debug build0のみ、実デバイス未起動。EntryDebug03はInteractionSample62/62、コメント検査0。ログRoot `Build/GameSave-20261002-30a407`。Summary／Registration／JUnit／LastTest／BinaryHashes／manifestを各固有試行へ保存。失敗試行はCompletion末尾へ分類して保持し、成功へ合算しない。

現在実行中プロセスなし。一時変異・復元待ちなし。関連B-M16 Root02のみ検出・復元済み、Source hash `087f04a6b528699336404ed5892c43acc09fd28d4ea29f466ba3ec50dc9b6d40`。拒否されたBuild複製は未変更。通常pushは全要件完了後。

次の具体的な一手：既存Tests/GameplaySampleSmokeへSaveCourseSmoke.h/cppを追加し、Mainの限定--save-write／--save-read入口から同一NativeGameplaySmoke exeを別プロセスで実行する。FSmokeAppは任意GameInstanceを受け取り、既存一括CapturePointsを再利用する。専用日本語・空白SaveRoot、別CWD、再配置Assets／exeで、2D／3Dの保存前・進行変更後・Continue後のWorld値と扉・取得物・荷物・Texture／Modelの画素を照合する。実デバイスは構成・対象・ウィンドウが開くことを説明してから直列実行。続いてL8故障・変異、L9独立Consumer／4配布／測定、L10累積・Docs・通常pushを進める。
