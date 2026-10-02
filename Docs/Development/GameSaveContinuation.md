# L作業の継続点

依頼はDXF-L-SAVE-4b20f89-20261002-R2のL0〜L10。B0〜B10の再確認へ戻らない。mainだけを使い、変更を破棄しない。

現在HEAD／origin/main：`4b20f89a451233bc8c328df98f48a259808a3285`。保存コア・非同期要求・試験・CMake／install／IDE／配布コピー登録を未commitで追加中。利用者の既存編集は開始時に無かった。新規試験Rootは今回のBuild内だけで、Assets原本や過去ログは変更していない。

L0：部分調査済み。既存Toolbox JSONを再利用。L1：保存封筒、Writer、制限、厳密整数、互換性検査は実装。型付きcheckpointと移行は未完了。L2：Root、slot、二面、CRC、排他、取消し、回復を実装。全I/O fault double・全競合試験は未完了。L3：TaskDispatcherを借用する保存／読込の値要求を実装。ゲーム採取／Scene Ready接続は未完了。L4〜L8：未実施。L9：コア単独Consumerの追加中。L10：部分回帰のみ、最終push未実施。

ログ：`Build/GameSave-20261002-30a407`。初回Debug28ケース成功、故障／要求を追加したCoreDebug03で3群成功。生ログと試行別結果はGameSaveCompletion-2026-10-02.mdに集約。

PortableDebug01は実行器の全体90秒制限で中断。子プロセス終了を確認した。公開結果不明の明示RetryPublicationを追加したPortableDebug02とPortableRelease01は各33/33成功。Native OFFの配布はDebug／Release各12工程成功。通常Pythonは64成功・7skip。これらのソース指紋はe88609c54bc5d4edc2f12731c4f5c1bdb10c0bef3a46c24086bb66a832d66414。

Native用SaveCoreOnlyのCRT設定を追加したPackageOnDebug01／PackageOnRelease01も各15工程成功。既存描画／UIと保存コアの別プロセスを確認した範囲で、2D／3Dのゲーム再開を確認した結果ではない。指紋c39286f3a98ac6a2c23852423d97146445fc775ad9187bf75dc25c10b236b69f。現在、実行中の検証プロセスはない。

次の一手：保存コア／要求の安全なまとまりをmainへcommitする。Lの最終pushではない。その後、実際に準備に使用した定義バイトの指紋を追加する。

続く実装：準備時に実際に読み込んだ定義バイトの指紋をSceneContent値へ保持→型付きcheckpoint文書／許可カタログ／純粋移行／進行参加者→安全境界採取→Contentから新Sceneを作る再開adapterと全4Joint個体間接続→入力profile一括適用とrelease-to-arm→既存Sample/UI→別プロセス／実画素／L変異／故障／測定／配布4構成→最終全回帰／文書／通常push。

一時変異なし。復元待ちbackupなし。承認拒否された操作なし。Build内の以前の拒否対象へ触れていない。
