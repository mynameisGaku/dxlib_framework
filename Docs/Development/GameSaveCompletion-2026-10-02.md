# L0〜L10：セーブ・ロード・チェックポイント・入力設定

指令識別子：DXF-L-SAVE-4b20f89-20261002-R2。これはL作業の途中記録で、完了報告ではない。Bの検証記録をLの成功件数へ転記しない。

開始HEADとfetch後のorigin/mainは`4b20f89a451233bc8c328df98f48a259808a3285`。開始時の作業ツリーはclean。記録先は`Build/GameSave-20261002-30a407`。最初の保存コア編集後に再実行したGit情報は`Baseline/GitAfterFirstEdits.json`であり、編集前のmanifestとは表示しない。

|試行|対象|結果|manifest|
|---|---|---|---|
|CoreDebug01|保存コアDebug、No-STL、差分|CTest 1/1、内部28/28、全終了0|c25cfd313953bc6b8bf294e1b1fd7bd2f30d1fe0929ac0ced3089e7d8c32282a|
|CoreDebug02|保存・確保失敗・要求のbuild|終了1。試験のFConsole呼出し誤り。旧exeを実行していない|該当Summary.jsonへ保存|
|CoreDebug03|保存・確保失敗・要求Debug|CTest 3/3、保存28、故障3、要求8。No-STL、差分を含め終了0|該当Summary.jsonへ保存|
|PortableDebug01|全target Debug build・全CTest|build終了0。実行器の全体90秒制限で最初のPhysics群中にTIMEOUT。製品試験の合否は未確定|該当Summary.jsonへ保存|
|PortableDebug02|全target Debug build・全CTest・No-STL・差分|33/33、失敗0・skip0、153.95秒。保存31・故障3・要求9。全終了0|e88609c54bc5d4edc2f12731c4f5c1bdb10c0bef3a46c24086bb66a832d66414|
|PortableRelease01|同コードのRelease全群・No-STL・差分|33/33、失敗0・skip0、29.39秒。全終了0|同上|
|PackageOffDebug01|install・再配置・従来5Consumer・保存コア2プロセス|12工程、全終了0。上位層不要の保存を検証|同上|
|PackageOffRelease01|同じ配布経路のRelease|12工程、全終了0|同上|
|Python01|通常のTools試験|71件中64成功・7skip、終了0。IDE未指定のskipを成功へ加算しない|同上|
|PackageOnDebug01|既存ソースSDKのNative配布・実device・保存コア|15工程、全終了0。既存Content2D/3DとUIの画素確認。保存からのゲーム再開は未実装|c39286f3a98ac6a2c23852423d97146445fc775ad9187bf75dc25c10b236b69f|
|PackageOnRelease01|同じNative配布経路のRelease|15工程、全終了0。上記と同じ検証範囲|同上|
|FingerprintDebug01|生バイト指紋追加後の保存／要求／Content群|5/5、Content47・保存33・保存故障3・要求9、既存Content故障群成功。No-STL・差分含め終了0|cc35737bddf0d12b732d2ab2675ed954caf2469c801df205b05f1cadb7df71ea|
|FingerprintRelease01|同じ指紋差分のRelease、同5群|5/5、全終了0。全群の最終回帰ではない|同上|

CoreDebug03の確保故障探索はencodeの最初の非注入countdown=377、decode=224、Stage=679。注入した失敗と非注入正常終了を分け、未解放件数の差0を確認した。全Lの故障境界を検証した件数ではない。

追加した公開入口はSaveDocument、SaveCodec、SaveStore、SaveTransaction、GameSaveRequest、GameSaveTicket。保存コアはToolboxだけへ依存する。非同期要求はRuntimeを借用する。Scene／Prefabチェックポイントadapter、入力プロファイル、Sample UI、2D／3Dの別Applicationプロセス再開、Native画素、L-M01〜20、配布4構成、測定、最終pushは未完了。

各試行のcommand、cwd、UTC、exit、manifest、登録JSON、JUnit、LastTest、バイナリ指紋は固有ログに保持する。Portable全群以降の結果は実行後に追記する。

PortableDebug01の中断はharness分類。最初の保存コア用の全体90秒制限を全群へ流用した誤りである。CTest自体の群ごとのtimeoutは変更せず、全群を収める実行器の時間枠を変更した。中断したCTestの子プロセスも終了したことを確認してから、公開失敗の明示再試行を追加したコードのPortableDebug02を新しい記録先で開始した。古いLastTestを全群の成功記録として採用しない。

No-STLは899ファイル・違反0。保存コアの別プロセス検証では、専用Root `C:/dxfc/l30a407-off-debug/run-4bq7r042/専用 SaveRoot 日本語` と `C:/dxfc/l30a407-off-release/run-9xx9kboc/専用 SaveRoot 日本語` を使用した。各write/readは新プロセス、別CWDで、同じexeの指紋を前後照合した。Debugは`7194a95e1c7740ddf4ab168d60a41e7fd4faa60a6a41b8332e6805fd2980dccc`、Releaseは`d3861a32c47f05eb90ad207229d6e9c3004c013d302727c84d4efe2704f88303`。ゲームのApplication／Worldのチェックポイント再開を確認した試験ではない。

配布の工程は既存10工程を保持し、保存コアwriteとreadの2工程を追加した。Native用ConsumerのCRT設定へSaveCoreOnlyも登録した後はソース指紋が変わる。上記集合は変更前の指紋に限定し、L10最終採用集合とは表示しない。実行環境設定の最初の直接cmd呼出しは引用処理で終了1となり、検証器を開始していない。固有batchから既存vcvarsを呼ぶ経路で実行した。これはharness分類で、SDK取得・再構築を行っていない。

mainの`c1b71db3339a7525cd24a729a79ae2de92263b31`へ保存コア／CPU要求をcommitした。続く指紋差分はSHA-256の既知入力と独立Python hashlibの8境界値、2D／3Dの再配置・ディスク変更後の旧値保持、入れ子DAGの重複除外を検証した。原本の定義は変更せず、今回専用のコピーだけを変更した。Sceneの全依存と独立LoadPrefabの依存を保持し、資源ファイルの内容や任意World保存を証明した機能ではない。
