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
|FingerprintMigrationDebug01|明示ゲーム移行のDebug、同5群|5/5、保存36・要求10、全終了0|7a0eeb88d94129195c8a2d09a6ef3567f2fe3d0e24312bf4ef3ec1a3a6a90538|
|FingerprintMigrationStorageDebug02|移行・上限ヘッダー・逆完了試験|build終了0、4/5群成功。要求群10/11でB開始待ちが失敗。試験がレーン数をWorker数と誤認|d62abaf830f7c4f98c7f0cb393f6521d90d5e3833d091eda7a9921ec68747871|
|FingerprintMigrationStorageDebug03|試験のWorker条件を修正|5/5、保存39・故障3・要求11。全終了0|fa1ad148729316a88082dd47e21dc8bb7c8e43a1099cb08e8a2cfc095802b194|
|FingerprintMigrationStorageDebug04|Workerの実完了数照合・改行統一後|5/5、保存39・故障3・要求11、Content47・Content故障5。全終了0|f15d54f8fc902d831afd7e45ee2354d3885598e9dd6a34788fb499bf47655305|
|FingerprintMigrationStorageRelease01|同じ差分のRelease、同5群|5/5、No-STL・差分を含め全終了0|同上|
|PackageMigrationOffRelease01|移行API追加後の再配置Release|12工程、全終了0。保存コアの別プロセスreadが移行を検証し、元slotはゲームバージョン1のまま保持|同上|

CoreDebug03の確保故障探索はencodeの最初の非注入countdown=377、decode=224、Stage=679。注入した失敗と非注入正常終了を分け、未解放件数の差0を確認した。全Lの故障境界を検証した件数ではない。

追加した公開入口はSaveDocument、SaveCodec、SaveStore、SaveTransaction、GameSaveRequest、GameSaveTicket。保存コアはToolboxだけへ依存する。非同期要求はRuntimeを借用する。Scene／Prefabチェックポイントadapter、入力プロファイル、Sample UI、2D／3Dの別Applicationプロセス再開、Native画素、L-M01〜20、配布4構成、測定、最終pushは未完了。

各試行のcommand、cwd、UTC、exit、manifest、登録JSON、JUnit、LastTest、バイナリ指紋は固有ログに保持する。Portable全群以降の結果は実行後に追記する。

PortableDebug01の中断はharness分類。最初の保存コア用の全体90秒制限を全群へ流用した誤りである。CTest自体の群ごとのtimeoutは変更せず、全群を収める実行器の時間枠を変更した。中断したCTestの子プロセスも終了したことを確認してから、公開失敗の明示再試行を追加したコードのPortableDebug02を新しい記録先で開始した。古いLastTestを全群の成功記録として採用しない。

No-STLは899ファイル・違反0。保存コアの別プロセス検証では、専用Root `C:/dxfc/l30a407-off-debug/run-4bq7r042/専用 SaveRoot 日本語` と `C:/dxfc/l30a407-off-release/run-9xx9kboc/専用 SaveRoot 日本語` を使用した。各write/readは新プロセス、別CWDで、同じexeの指紋を前後照合した。Debugは`7194a95e1c7740ddf4ab168d60a41e7fd4faa60a6a41b8332e6805fd2980dccc`、Releaseは`d3861a32c47f05eb90ad207229d6e9c3004c013d302727c84d4efe2704f88303`。ゲームのApplication／Worldのチェックポイント再開を確認した試験ではない。

配布の工程は既存10工程を保持し、保存コアwriteとreadの2工程を追加した。Native用ConsumerのCRT設定へSaveCoreOnlyも登録した後はソース指紋が変わる。上記集合は変更前の指紋に限定し、L10最終採用集合とは表示しない。実行環境設定の最初の直接cmd呼出しは引用処理で終了1となり、検証器を開始していない。固有batchから既存vcvarsを呼ぶ経路で実行した。これはharness分類で、SDK取得・再構築を行っていない。

mainの`c1b71db3339a7525cd24a729a79ae2de92263b31`へ保存コア／CPU要求をcommitした。続く指紋差分はSHA-256の既知入力と独立Python hashlibの8境界値、2D／3Dの再配置・ディスク変更後の旧値保持、入れ子DAGの重複除外を検証した。原本の定義は変更せず、今回専用のコピーだけを変更した。Sceneの全依存と独立LoadPrefabの依存を保持し、資源ファイルの内容や任意World保存を証明した機能ではない。

指紋差分は`843b7996f108f8fe78877635ae50546c28accdb9`へcommit済み。ゲーム移行は今回用fixtureのcoins→scoreを検証し、将来のゲームバージョン、別Content、移行後の不正JSON・容量超過を拒否する。容量超過面の短いヘッダー検査と公開トランザクションの入口上限検証を追加した。これは製品境界の補強で、全I/O故障地点の試験完了ではない。

逆完了試験の初回失敗は試験期待条件の分類。JobSystemの実行レーン数には所有スレッドが含まれ、2指定ではWorkerは1個だった。3レーンで二つのWorkerを借用し、Aを停止中にBのPrepare完了数1を確認してからAを解放する。Dispatcherの順序を変更せず、AのSupersededとBの採用を照合した。失敗試行は成功試行へ合算しない。

チェックポイントの型付き文書・許可カタログ・純粋な再開計画を追加した。個体の論理キーから生成順を作り直し、明示除去、動的個体、parameter、全4種類の内部接続と個体間接続を両次元の実Worldで確認した。配列位置や旧実行時IDを保存・復活させない。型付きexportの別名は同じ設定だけを許し、矛盾を拒否する。成功World.Stepと全PostPhysics終了後の採取フックを追加したが、ゲーム状態の自動採取・Navigatorとの再開完了接続は未完了である。

|試行|対象|結果|manifest|
|---|---|---|---|
|FingerprintCheckpointDebug01|checkpoint追加後のbuild|LNK1136、終了1。Content試験exeの初発原因は未特定。旧exeは実行せず、対象の再リンクを実施|固有Summaryへ保存|
|CheckpointRelinkDebug01|該当2targetの再リンク|終了0。通常の全target buildと後続CTestも成功|固有Summaryへ保存|
|FingerprintCheckpointDebug02／Release01|checkpoint codec限定回帰|各6/6、終了0|47b55516a190a9dc1f095fc58f449e135dad1bc6e71614491858ad46a47f12f4|
|FingerprintCheckpointRestoreDebug01|許可カタログからの再開計画|6/6、終了0|14da236df434bcb0fecfb4cdb49aea4f8b97af484f5365b38aeb3bfd054e7624|
|PortableCheckpointBoundaryDebug01|成功固定更新境界試験のbuild|終了1、試験のAssetService include不足|固有Summaryへ保存|
|PortableCheckpointBoundaryDebug02|同build|終了1、試験のScene破棄APIの取り違え|固有Summaryへ保存|
|PortableCheckpointBoundaryDebug03|修正後のPortable全群|34/34、失敗0・skip0、155.86秒。全終了0|f2302877d6c29a8c56487af0cf3290a13fb56044cfa1ba9ca1cd7d15c697e1da|
|PortableCheckpointFrozenDebug01|上限・別名・外部利用・整形後の全群|34/34、失敗0・skip0、162.77秒。全終了0|94b49673dda3d4807569a5759155761b4c68431d89b2f4999c9d941e980948f0|
|PortableCheckpointFrozenRelease01|同コードのRelease全群|34/34、失敗0・skip0、29.02秒。全終了0|同上|
|CheckpointHeaders01|新規公開ヘッダー単独14・正逆順・保存コア単独|17構成、/utf-8付き、全終了0。リンク確認は外部Consumerに分離|同上|
|PackageCheckpointOffRelease01|再配置Consumer・保存コア別プロセス|12工程の個別終了は全0。ただしSummary記録のos.replaceでWinError 5、全体終了1。配布全成功として採用しない|同上|
|PackageCheckpointOffRelease02|同コード・新しい記録先で1回の限定再試験|12工程と全体終了0|同上|

上記の最新No-STLは925ファイル・違反0。GameCheckpoint群は15ケース。共有Gameplay変更を含むFramework群の成功も全群へ含む。外部CheckpointConsumerはinstallの公開targetだけから新しい計画APIをリンクし、実ファイル指紋を持つ2D／3Dの生成・除去・動的個体・解除を実行した。まだ別Applicationプロセスのゲーム進行再開やLの実画素を確認したものではない。PackageCheckpointOffRelease01の失敗は結果記録の環境／harness境界で、初発原因は未特定。失敗試行を削除せず、限定再試験へ合算しない。


Scene所有のCheckpointCapture2D/3Dを追加した。採取要求は成功World.Stepと全PostPhysics後だけで反映し、保留Joint設定・未初期化個体・失効世代は採取を待つ。最後の正常文書は失敗でも保持する。動的Spawn受付と初期化完了を区別し、明示破棄を除去履歴に残す。同名の別世代へ自動追従しない。公開HasPendingRequestとContentScene.GetDefinitionは読み取りのみ。再開完了のNavigator接続・ゲーム固有進行Controller・入力設定・Sample UIはまだ未完了。

|試行|結果|manifest|
|---|---|---|
|PortableCaptureFrozenDebug01|35/35、失敗0・skip0、201秒|df8d13d6c004a9b94ea9e73b8e2fc57880d489c9556af7d0ff7d8c62f89304b6|
|PortableCaptureFrozenRelease01|35/35、失敗0・skip0、35秒|df8d13d6c004a9b94ea9e73b8e2fc57880d489c9556af7d0ff7d8c62f89304b6|
|CaptureHeaders01|29工程、全終了0|df8d13d6c004a9b94ea9e73b8e2fc57880d489c9556af7d0ff7d8c62f89304b6|
|PackageCaptureOffDebug01|12工程、全終了0|df8d13d6c004a9b94ea9e73b8e2fc57880d489c9556af7d0ff7d8c62f89304b6|
|PackageCaptureOffRelease01|12工程、全終了0|df8d13d6c004a9b94ea9e73b8e2fc57880d489c9556af7d0ff7d8c62f89304b6|

公開ヘッダーは26個の単独includeと正順・逆順・保存コア単独の計29構成でコンパイルした。Native OFFの外部Consumerはinstallの公開targetから、両次元の論理再開計画・実World・成功境界採取を実行した。配布12工程は既存10と保存コア別プロセスwrite/readの2工程であり、ゲームの別Applicationプロセス再開を確認したものではない。No-STL933ファイル・違反0。これはL10の最終採用集合ではない。

CaptureBuildDebug01〜05のbuild失敗は新規製品コードのFStringView変換誤りと、試験の確保注入header隔離・Optional比較・namespace指定の不備。06〜08では境界外採取の検査順を修正し、試験に既存ApplicationのObject確定境界とPause情報を正しく与えた。09の2群は終了0。CaptureFaultDebug01は探索4095までで最初の非注入を到達できず失敗。02は後続の可変更新へ注入が漏れた試験範囲の不備。03では採取処理の終了時に注入を解除し、両次元の全注入失敗で旧正常文書・未解放件数差0・次の正常採取を確認した。最初の非注入countdownは2D4568、3D4750。正常Checkpoint19ケース、独立確保故障3ケース。各失敗のログを保持し、成功集合へ合算していない。


所有側のCheckpointResumeRequest2D/3D、ACheckpointScene2D/3D、ポインタを含まないScene観察Ticketを追加した。明示した純粋ゲームレシピを構成検証の前後で扱い、資源準備後に通常Navigatorへ要求する。全必須Readyと成功物理観察の後だけCompletedを公開する。初期化失敗・候補終了・所有者退役は未完了状態を終端へ確定する。公開前取消しをC++構築フック後にも再照合する。旧Worldの上書きや追加Stepは行わない。現在のCPU計画生成と資源準備は所有側Pollで行い、完全Worker準備ではない。ゲーム固有Controller、Sensor経由進行、入力設定、Sample UI、別Applicationプロセス・実画素再開は未完了である。

|試行|結果|manifest|
|---|---|---|
|PortableResumeFrozenDebug01|35/35、失敗0・skip0、202秒、全終了0|45b02a2c2f0603720b4ba72715b0e7abcdbfa122f0b1c852aa00c9969788d7d7|
|PortableResumeFrozenRelease01|35/35、失敗0・skip0、35秒、全終了0|45b02a2c2f0603720b4ba72715b0e7abcdbfa122f0b1c852aa00c9969788d7d7|
|ResumeHeaders01|35工程・全終了0|45b02a2c2f0603720b4ba72715b0e7abcdbfa122f0b1c852aa00c9969788d7d7|
|PackageResumeOffRelease01|12工程・全終了0|45b02a2c2f0603720b4ba72715b0e7abcdbfa122f0b1c852aa00c9969788d7d7|

ResumeBuildDebug01とResumeTargetDebug01は限定2群が全終了0で、初期化失敗による旧Scene保持と両次元の単独Prismatic有限目標駆動を確認した。最新GameCheckpointは26ケース、確保故障3ケース、Framework417ケース。公開ヘッダーは32個の単独includeと正逆順・保存コア単独の35構成を/utf-8付きで検査。外部ConsumerはContent Rootと専用SaveRootを別引数で渡し、両次元の実読込定義と保存から通常Navigator・初回成功固定更新を通した。同じプロセスの再開であり、別Applicationプロセス試験とは数えない。既存12配布工程を保持した。最終L10集合ではない。

任意のInputProfilesを追加した。許可Action・予約入力・競合・全件構成を検証してから既存InputMapへ一括採用し、不正入力や確保失敗では旧MapとAction履歴を残す。Keyboard／Mouse／4Padの保存名は明示した文字列対応を使う。再割当の開始入力と適用時の押し続けを解放まで抑止し、フォーカス喪失・Pad切断・Modal終了にも同じ規則を使える。従来Bind経路の挙動は保持する。設定文書は進行スロットから独立し、入力・音量・UI倍率を全検証して値として返す。既存Sampleの音声・UI倍率への実適用と再割当画面は未完了である。

|試行|結果|manifest|
|---|---|---|
|ProfileBuildDebug01|build終了1。故障試験のOut宣言に必要なToolbox/Platform.hが不足。旧exeは実行していない|固有Summaryを保持|
|ProfileBuildDebug02／ProfileTargetDebug01|修正後の2群が終了0。通常12ケース・確保故障3ケースまで追加|固有Summaryを保持|
|PortableProfileFrozenDebug01／Release01|各37/37、失敗0・skip0。Debug199秒、Release35秒、全終了0|8b2ceada55f5c8b30237dc82b77e918cf95ed7ded3d5d73551bd5d9af27ef496|
|ProfileHeaders01|下位層だけのincludeで単独11・正逆順2、/utf-8付き13工程、全終了0|同上|
|PackageProfileOffRelease01|既存12と設定保存・読込の別プロセス2を合わせ14工程、全終了0|同上|
|ProfileCommentDebug01／ProfileLinkDebug01|コメント整理とBinding比較の同名cpp分離後、2群が終了0|固有Summaryを保持|
|PortableProfileLinkFrozenDebug01|37/37、失敗0・skip0、200秒、全終了0|c9d696be080e3338793009d16de664859330be030c33f2f0418c8dbd4f6d3876|
|PackageProfileLinkOffRelease01|比較処理のリンク登録を含む14工程、全終了0、前後指紋同一|同上|
|ParamCommentsChecks01|引数ごとの改行、説明件数・BOM・改行形式・コメント以外の生バイト同一を検査。Python74件中67成功・7条件付きskip、No-STL965ファイル違反0、diff検査終了0|3e2b2043c225f3651eb1d7a8a4141d8ef7c16367729c6ecef1628f1ce4b5fb9f|
|PortableParamCommentsFrozenRelease01|引数コメント修正後の全群37/37、失敗0・skip0、36秒、全終了0|同上|

入力故障は一括適用・encode・decodeを独立に探索した。最初の非注入countdownは5・64・51。注入された失敗ごとに旧有効割当・未解放件数差0・次の正常要求での回復を確認した。Action更新・解放抑止・再割当Pollの定常32Snapshotは追加確保0で、WorldやNative全体の0確保を意味しない。正規Python入口の最初の呼出しは固定Python01へ記録する旧実行器に引数を渡したためLogsInUseErrorで終了した。過去ログを上書きせず、新しいProfilePython01の実行器で73件中66成功・7skipを確認した。これはharnessの呼出し誤りである。

外部InputProfileOnlyは保存コアとSupportだけからリンクし、SceneContent／Physics／UIを依存に含めない。最新の別プロセス試験は`C:/dxfc/l30a407-profile-link-release/run-gfeku_z3/入力設定 配布 exe/InputProfileOnly.exe`、専用Root`C:/dxfc/l30a407-profile-link-release/run-gfeku_z3/入力設定 SaveRoot 日本語`、無関係CWDを使用した。同一exeのSHA-256は`9863a120f74e3ddf21aa1d3cbb7692526ace4e2e33064df8e87559224110bfa6`、write／readの各終了0。読込後のK・MouseRight・Pad4のButton16を実InputMapへ渡し、旧Spaceが操作しないことを確認した。音量0.4・UI倍率1.75は値の永続化までで、聴感・実UI hit testやゲームApplication再開の証拠には数えない。最新SaveCoreOnlyの別プロセスexe指紋は`9ea04f11569063b1556ef6caa8f58fa44bfc7bca3282bb8bc44dcd514278072c`。いずれもL10最終採用集合ではない。

利用者の指摘に従い、現在Source・Examples・Tests・外部Consumerの引数コメントを1引数1行へ整理した。25ファイル・54行の分割で、説明・処理・宣言を変更しない。CodingStandardへ規則を追加し、Toolsの静的検査で再発を検出する。過去の検証記録は書き換えていない。

New GameからチェックポイントSceneを作る際の手作業を減らすため、両次元の`FCheckpointRestore::MakeInitial`を追加した。既存Captureの初期文書生成を共有し、許可Prefab・生成順・全Joint公開先・個体間接続と独立したゲーム値を記録する。容量と構造を検証してから純粋な再開検証へ通し、不正なゲーム値ではレシピを呼ばない。World・資源・Sceneの生成はこの入口で行わない。

|試行|結果|manifest|
|---|---|---|
|InitialCheckpointDebug01|正常群1/1、内部28/28、No-STL・引数コメント・diff検査終了0|c041eee38702c1caf080d6bdf4caf46b9f94390310d79d1687bf2cb9f0af1165|
|InitialCheckpointRelease01|buildと登録は終了0。tests開始前のSummary.json更新でWinError 5、全体終了1。CTest未実行|固有失敗記録を保持|
|InitialCheckpointRelease02|同コードを新しいログ先で1回だけ限定再試験。2/2、正常28ケース・故障3ケース、全終了0|f7c6720b40f9a4175686143e29f0c81c63df6e2ffa21877f3890659325994d7c|
|PackageInitialCheckpointOffRelease01|14工程・全終了0、公開入口を両次元の再配置Consumerから実リンク・実行|同上|

Release01の記録失敗は環境／harness分類で、以前と同じWindowsのファイル置換エラー。初発原因は未特定であり、自動承認による操作拒否ではない。失敗試行を成功へ合算しない。外部Consumerの初期個体表の手書きを公開入口へ置き換え、既存の除去・動的生成・保存・Navigator再開を保持した。ここも同じプロセスのチェックポイント再開で、別Applicationプロセスのゲーム進行実証は残る。

Sample専用の`FSaveCourseProgress`／`FSaveCourseProgressCodec`を追加した。取得済みキーの集合、厳密な得点・生成カウンター・進行revision、checkpoint、名前付きの扉の解錠・開扉をCPU値で扱う。ゲームルールを保存コアへ追加せず、既存のJSON文法・Writer・整数文字列変換を利用した。手書きfixtureの項目順と扉順を逆にし、2^53を超える値とuint64最大値、個体取り違え、未知項目、重複、将来バージョン、旧ゲーム値保持を確認した。Sensor・再開レシピ・UIのゲーム実利用へはまだ接続していない。

|試行|結果|manifest|
|---|---|---|
|SaveCourseProgressDebug01|既存コースを含むInteractionSample52/52、21.32秒、全終了0|2548a798c1f977e2314af9e1658ba31c13049d0245ee6bf91df8da77703b3f8b|
|SaveCourseProgressRelease01|同群52/52、2.43秒、No-STL970ファイル違反0、全終了0|ee0b56dc74d2a15b4dbf9d19069a29e2da910702c48ec0a9979986daaee39f55|
|PortableSaveCourseProgressFrozenRelease01|コメント訂正と.hpp検査追加後の全群37/37、失敗0・skip0、35秒、全終了0、前後manifest同一|482a61ef01e7fd005db67ce33698ab5ea7497e4e52f2b04fc66fbe2b37c47a8e|

両試行の違いは役割コメントの追加で、試行ごとに前後manifest同一を確認した。その後、取得済みキーの説明コメント1箇所を訂正し、引数コメントの再発検査へ.hppも含めた。正常コードの分岐・数値・公開宣言は変更していない。これらはL7の純粋な進行codecの検証であり、両次元の別Application再開や実画素の成功へ転用しない。

Save用の2D／3Dコースと許可カタログ、純粋なゲーム再開レシピを追加した。従来Assets/Contentのコースは保持した。`FSaveCourseRecipes::NewGame`がデータ由来の初期開閉も含む進行を作り、`Restore`が取得物の除去と安全地点、片側だけの開扉を候補へ反映する。開扉時はleafと固定されたcargoをレール方向へ移し、元のBody-local Frameを保持する。実Applicationでは初回成功StepからTranslation=2m、元のLimit[0,2]、固定cargoの相対位置、別扉の閉扉、未取得鍵、動的cargoを両次元で独立に確認した。隠れたStep・初期化中の音声は使わない。

|試行|結果|manifest|
|---|---|---|
|SaveCourseRecipeDebug01〜03|build終了1。2Dの同名座標型、Stateへ存在しないLimits、ベクトルの存在しないmemberを修正。旧exeは実行しなかった|各固有失敗記録を保持|
|SaveCourseRecipeDebug04|build終了0、InteractionSample52/54。両次元のMakeInitialが埋込Prefabの空指紋を検出、CTest終了8|各固有失敗記録を保持|
|SaveCourseRecipeDebug05|Sourceの実読込指紋を埋込Prefabへ保持する修正後54/54、全終了0|d60f236bf409f4d5ea2c41935d8ea01bbda87e78359c37d82626df7529d51e79|
|PortableSaveCourseRecipeDebug01|不正な完全parameter集合と旧値保持を追加後、全群37/37、失敗0・skip0、200.63秒、No-STL973違反0、前後指紋同一|d15bc15adbcd7a84ca65323bb25686bb8a21e88f024c251b4d08063fe6f95d5d|
|PortableSaveCourseRecipeRelease01|全群36/37。追加したデータコピー試験の単一ディレクトリ作成を再帰作成と誤認、InteractionSample55/56、CTest終了8|固有失敗記録を保持|
|SaveCourseRecipeRelease02|試験が所有する親ディレクトリを順に作成する修正後56/56、全終了0|固有記録の指紋を参照|
|PortableSaveCourseRecipeFrozenRelease01|NewGameの初期進行整合も追加した全群37/37、失敗0・skip0、35.40秒、No-STL973違反0、前後指紋同一|f7666e99e22e41f74618ad2c9e5cb6b6b6ef8aa953a978c8301f07c7f067c81b|
|SaveCourseRecipeFrozenDebug01|同じ最終製品コードでInteractionSample56/56、引数コメント・No-STL・diff検査とも終了0|同上|
|RelatedContentMutationRoot02 B-M16|既存のProjectRoot→CWD誤用を再検出。build0→Red1→生バイト復元→再compile0→Green0|通常コードは同上へ復元|

埋込指紋の欠落は製品不具合、最初の3buildは実装／試験のAPI参照誤り、Release01のディレクトリ失敗は試験harnessの誤りとして区別する。失敗群へ単独再試験の成功を足していない。既存変異runnerの最初の呼出しは識別子を文字列で渡したため引数解析終了2で、Source変異・build・検出は未実行。正しい整数指定のRoot02だけを検出結果とした。

データ変更試験は専用の新規コピーRootでopenの既定値だけを変更し、同じ試験exeのコードから初期開扉が一度だけ反映されることを確認した。読込後はそのコピーだけを別名へ移し、レシピがディスクを再読込しなくても生成できること、原本扉定義のSHA-256が不変であることを確認した。実Applicationの新規プロセスA/BやNative実画素の成功ではない。Scene要求の埋込Prefab指紋は要求全体の依存集合を含み、単独Prefab要求と区別する。

B-M16の対象Source復元SHA-256は`087f04a6b528699336404ed5892c43acc09fd28d4ea29f466ba3ec50dc9b6d40`。バックアップ・変更時刻・各buildとexe指紋は`Build/GameSave-20261002-30a407/RelatedContentMutationRoot02`へ保存した。一時変異は残っていない。L-M01〜20、Sensorによる進行更新、Sample保存UI、別プロセスのゲームApplication再開、Native、測定・配布4構成は未完了で、ここはL10最終採用集合ではない。

新規の14定義と3つのC++ファイルを既存のCRLFへ揃え、変異復元後に再検証した。PortableSaveCourseRecipeCrLfRelease01は全群37/37・失敗0・skip0、35.73秒、全終了0。SaveCourseRecipeCrLfDebug01はInteractionSample56/56、引数コメント・No-STL・diff検査も終了0。両試行の前後manifestは`26d36c21b2ec76b96040a973f58f4892d6385e6cd6fbe67847b63172dd1a688f`で一致した。改行前の結果と区別し、この実装単位の確認値とする。L10の累積採用回帰は今後の実装が揃った後に行う。

両次元のSample Scene／Controllerを追加した。本体Sensorから鍵取得・得点・解錠を更新し、片側だけを有限Motorで開き、動的荷物を履歴付きで生成する。全PostPhysics後の採取と新Sceneでの取得済み抑止を実Applicationで確認した。KeyboardのK、Mouse Right、Pad4の16番へJumpを変更し、実BodyのImpulseで結果を照合した。0固定更新の押下保持、複数固定更新の単発配送、UI終了の押し続け、Pad切断・フォーカス喪失、既存Playbackと次のPlayの音量適用を含む。接触を起こす配置は試験が明示的にTeleportするもので、製品Controllerは力・Impulseと専任Joint要求だけを使用する。

|試行|結果|manifest|
|---|---|---|
|SaveCourseControllerDebug01|build1。テンプレートヘッダーの必要な型includeを補正。旧exe未実行|固有失敗記録を保持|
|SaveCourseControllerDebug02|build1。試験のOptional比較を、存在するID値の比較へ訂正。旧exe未実行|固有失敗記録を保持|
|SaveCourseControllerDebug03|build0、56/58。UI消費後の偽の解放で解除待ちが終了する製品不具合を両次元で検出、CTest8|固有失敗記録を保持|
|SaveCourseControllerDebug04|入力更新後に元Snapshotで抑止する修正後58/58、全終了0|固有記録を参照|
|SaveCourseControllerDebug05|マウス・Pad・フォーカス・2Viewの代表を追加後58/58、コメント・No-STL・diffも終了0|3c3c5fe03c8075deabeeb3d4271c6e3f176c4598a481ebbf975862c931e04830|
|PortableSaveCourseControllerRelease01|CRLFを揃えた全群37/37、失敗0・skip0、35.60秒、No-STL981違反0、全終了0、前後同一|55101a8b1b80f4c7f862fbcdd866e2585ca600157efaf75abc7837a729ad1dc2|
|SaveCourseControllerFrozenDebug01|同じCRLFコードで58/58、コメント・No-STL・diffも終了0、前後manifest同一|同上|

進行を保持するGameInstanceと保存UI、実ファイルへ保存して終了するゲームプロセスA/B、UI倍率、Native画素、Lの変異、測定・配布4構成はこの試行時点ではまだ実装・検証中である。同一プロセスの新Scene生成を別プロセスの成功へ転用しない。

Scene切替を越えて保存と設定を所有する`ASaveCourseGameInstance`を追加した。成功境界の手動採取、Pause時の最後の正常値、checkpoint revision autosave、独立設定スロット、通常Navigatorと必須Readyまでの完了観察を通す。同期・既存Dispatcherの両経路を実Applicationで確認した。受付Frameで同期ロードを即採用すると取消し境界を失う不具合を修正し、取消し・Aの後にBを受付した場合の旧要求抑止も両次元で検証した。

|試行|結果|
|---|---|
|SaveCourseSessionDebug01|build1。試験のパス・時刻API参照を訂正。旧exeは未実行|
|SaveCourseSessionDebug02|build0、58/60、CTest8。RequestChange受付と次Frameの採用を同時と扱った試験期待を訂正|
|SaveCourseSessionDebug03|60/60、引数コメント・No-STL・diffとも終了0|
|SaveCourseSessionDebug04|build0、58/60、CTest8。同期受付Frameで候補を採用するため取消しが遅れる製品不具合を検出|
|SaveCourseSessionDebug05|受付Frameの候補採用を分離後60/60、28.66秒、全終了0|
|ProcessPilotBuildDebug01／03／04／05|別プロセス試験の文字列出力・進行field・個体一覧API参照誤りでbuild1。旧exeは実行しなかった|
|ProcessPilotBuildDebug02／06|それぞれ訂正後build0|
|ProcessPilotDebug01|2D-write終了1。未知個体getterを失効handleとして扱った試験期待を訂正、readと3Dは未実行|
|ProcessPilotDebug02|2D／3Dのwrite／read各終了0。旧ファイル・原本Assetsは未変更|
|PortableSaveCourseSessionFrozenRelease01|整形・CRLF後の全群38/38、失敗0・skip0、37.04秒、全終了0|
|SaveCourseSessionFrozenDebug01|同じ最終製品コードでInteractionSample60/60、引数コメント・No-STL・diffとも終了0|
|PortableSaveCourseSessionFrozenDebug01|同じCRLFコードの全群38/38、失敗0・skip0、全終了0。別プロセス4起動も終了0|

FrozenRelease01と限定FrozenDebug01の前後manifestは`241f4a6a5a1db837ef409bb9674489708ee31a50d27a8d43c30108587be08677`で同一。登録が37から38へ増えたのは`SaveCourseProcesses`を追加したためで、既存群は削除していない。失敗試行は分類・原ログを保持し、成功へ合算しない。L10の最終累積集合ではない。

別プロセス試験はGUIを増やさないCPU検証target`dxf_save_course_process`を使う。同じexeのプロセスAでNew Game、実Sensor取得、GameActionによるcargo生成、有限Motorで片側開扉、checkpoint保存、Jump=K・音量0.35・UI倍率1.5保存を行って終了0。プロセスBは新しいApplication・JobSystem・WorldでContinueし、得点10、north、動的cargo1、doorAの初回Translation2、doorBの閉扉、取得済み除去、全4種類の内部Joint、二重得点／音声なしを独立期待値で照合した。旧Spaceが動かずKの押下で実Bodyが動くことも確認した。UI倍率は文書と現在値の検証で、画面の実適用は残る。

FrozenRelease01の再配置exe SHA-256は`3608c0cb4eda64412fff40b8ffb441409392e3ba3c6d720a3498e92d9c727e9c`。証拠は`Build/GameSave-20261002-30a407/Portable/SaveCourseProcesses/Release/20261002T081250Z-4091afde/ProcessEvidence.json`、各プロセスのcommand／CWD／時刻／exitは同ディレクトリへ保存した。`再配置 ゲーム`、`保存 専用 2D`／`3D`、`無関係 作業場所`を使用し、exe前後と原本Assetsのhash不変を確認した。描画と資源の境界は試験実装で、実GPU・人手操作・独立install Consumerの成功とは区別する。

保存コースの既存UIを接続した。手動3系列・autosaveの状態を初回／終端／明示更新で読み、通常DrawからI/Oを行わない。New／Continue／Load／Retry／公開前Cancel、上書きModal、独立設定保存、公開結果未確定の明示再確認、扉目標、動的荷物の生成／破棄を要求へ渡す。Keyboard／Mouse／Padの採取と競合拒否、音量と倍率、画面のスクロールを既存部品で操作する。

候補Sceneを構築した後に設定を変更すると、採用Sceneが古いBindingを持つ競合を試験で検出した。GameInstanceはSceneの世代付きTicketと設定変更番号を照合し、そのSceneの入力更新前に現在の完全な設定を適用する。ロードした進行は操作設定を巻き戻さない。設定倍率をドラッグ中に直ちに変えるとクリック座標が動くため、倍率の適用はSliderの解放後へ送る。

|UIの試行|結果と分類|
|---|---|
|SaveCourseUiDebug02／03|60/62。試験用Fontが文字計測を提供せず、UI文字が描画されないharness不備|
|SaveCourseUiDebug04／05|62/62。実クリック・保存Modal・設定・新Sceneの押し続け抑止|
|SaveCourseUiDebug06|60/62。倍率変更と試験クリック位置の検査。原ログを保持|
|SaveCourseUiDebug07|build1。protectedな操作を参照した誤りを公開IsPressedへ訂正。旧exe未実行|
|SaveCourseUiDebug08|62/62。音量、競合するBinding候補、Mouse／Padを追加|
|NativeUiBuildDebug01|GameplaySample Debugの構成・build終了0。ウィンドウ起動／Native画素は未実施|
|SaveCourseUiSettingsRaceDebug01|build1。試験Backendの入力参照を訂正。旧exe未実行|
|SaveCourseUiSettingsRaceDebug02|60/62。候補構築後の設定変更が新Sceneへ届かない製品不具合を両次元で検出|
|SaveCourseUiSettingsRaceDebug03|62/62。世代と設定変更番号の照合後Green|
|SaveCourseUiFrozenDebug01／02、SaveCourseUiScrollDebug01|60/62。領域上端と一致する文字へ不要な余白を要求し、Wheelを往復させるharness不備。終盤の座標記録を保持|
|PortableSaveCourseUiFrozenRelease01|全38/38・失敗0・skip0、38.18秒、全終了0|
|PortableSaveCourseUiFrozenDebug01|同じコードで全38/38・失敗0・skip0、221.03秒、全終了0|
|SaveCourseUiEntryDebug01|58/62。追加Scrollによる従来方向操作の妨害、試験描画BackendのLine未対応、Continue確定境界の期待誤り|
|SaveCourseUiEntryDebug02|58/62。Line対応後も、祖先Scrollの方向操作消費と受付Frameの期待誤りが残る|
|SaveCourseUiEntryDebug03|62/62、34.59秒、コメント／No-STL／diff終了0。Continueは受付と次の失敗確定を分けて確認|
|PortableSaveCourseUiEntryFrozenRelease01|build1。新しい独立Scroll試験のFocus getter名を訂正。旧exe未実行|

最初のUiFrozen両構成の前後manifestは`496a59cb56cb0340edf1ce23dc17c095a37681bfd4a6adff19d41a64b690dd9a`。EntryDebug03は`acda8de36817f450db8b85d4397934a3c04e274ea443f63c3f86c50b69981a04`。どちらも後続編集前の限定採用で、L10の最終集合ではない。EntryDebug03では旧相互作用Sceneの実メニューから空のContinueをクリックし、旧Scene保持と失敗の文字表示後、New Gameをクリックして両次元のReadyへ進む。Drawに記録した位置を使うCPU Backend試験であり、人手・Native画素の証明とは区別する。

UI入口・Scroll独立回帰を追加した後の採用はPortableSaveCourseUiEntryFrozenRelease02とPortableSaveCourseUiEntryFrozenDebug01。両構成全38/38・失敗0・skip0、build／No-STL／diff終了0、No-STL989ファイル・違反0。前後manifest `b44e4345af55ebad03fdb705315a21f1b2b3745ef3210bcda0aa049ca0da0d4d` は同一。NativeUiEntryBuildDebug01も同じコードで通常GameplaySample Debugの構成・build終了0。これはGUIのビルド確認で、まだNative保存・再開画素の成功ではない。新しいScroll試験は子Buttonへの方向移動とWheelスクロールを独立して照合し、フォーカス可能な従来Scrollの操作も既存全群で保持した。

## Nativeの保存・別プロセス再開（L7／L8部分）

既存NativeGameplaySmokeに限定保存／読込入口を追加し、通常Gameplayコースの回帰を保持した。各次元でプロセスAが初期状態から鍵一件・得点10・doorA開扉・動的荷物一個・north checkpointを保存し、その後は鍵二件・得点20・荷物二個・doorA閉扉／doorB開扉へ変える。別プロセスBはmanual1から保存時の進行へ戻り、取得物を復活させず、初回正常StepでdoorAのTranslation=2を確認する。別系列の設定K／音量0.35／UI倍率1.5を読み、Space無効とKの実Body跳躍を比較した。

表示は扉二個、鍵二個、動的荷物、2D画像／3Dモデルの独立領域を一回のGPU読戻しで確認する。保存前／保存時／変更後／Continue後×一画面／二画面／1001×501の24画像を保存する。元Assetsとexeの指紋を前後で照合した。固定Snapshotによる実Application・World・DxLib試験で、人手操作・聴感・SDKなしPC・install Consumerの証拠とは区別する。

|試行|結果・分類|
|---|---|
|NativeSavePilotDebug01|2D-writeの保存後の奇数幅分割で荷物の投影位置が画面外。製品表示の縮尺をコース幅に合わせて修正。閾値を緩めず、失敗画像を保持|
|NativeSavePilotDebug02|2D-write／read成功、3D-writeのbefore-split右側でdoorA画素0。ViewCount=1なのに未使用の二番目のカメラを使う製品不具合。二画面で明示された一カメラを共用へ修正|
|NativeSavePilotDebug03|四プロセス全終了0、24画像。前後manifest 799683fb9eb7b3bab0fb0101650602d9b7ef17d12f760f80e1ccb487a1e17297。試験登録・実行器の後続変更前の限定結果|
|NativeSaveFrozenRelease01／Debug01|NativeGameplaySmokeのbuild、コメント、No-STL、diff全終了0。991ファイル・違反0|
|NativeSaveFrozenReleaseDevice01|従来NativeGameplayDeviceSmokeと新NativeSaveCourseProcesses、2/2・失敗0・skip0、32.23秒。実行器の欠落画像／exe改変／子プロセス失敗／原Assets改変拒否の5試験も成功|
|NativeSaveFrozenDebugDevice01|同2/2・失敗0・skip0。JUnit、詳細ログ、登録一覧を保存|
|PortableSaveNativeFrozenRelease01|build1。新しい試験がFColorの未定義==を使った誤り。成分別比較へ訂正、古いexe未実行|
|PortableSaveNativeFrozenRelease02|全38/38・失敗0・skip0、38.21秒。build／No-STL／diff全終了0|

Native両構成の登録後manifestはd9cca20103f7a6b0a7885652677cf0c0a63a6cf8ff0403225b0f3ce59f4f9aa0。後続の変更はPortableのカメラ試験の色比較だけで、製品・Native実行入口のバイトは変えていない。これらは途中の限定採用で、L10最終全群とは数えない。

Debugの同一exe SHA-256 `8a91b4404d2f04fd8bda8cd6366d9cbfb5218b0b7f83abedf0232c5c7cd4ca7e`。SaveRoot親と画像は `Build/GameSave-20261002-30a407/Native/NativeSaveCourseProcesses/Debug/20261002T092549Z-f0064dd1`、別CWDはその中の `無関係 作業場所`。実行した四つのcommand・開始終了UTC・exit・前後manifestをSummaryへ保存し、原本Assetsは変更なし。

Releaseの同一exe SHA-256 `cb6e6b7261e82336b6c94c33763eedd7f3aea7bd78840fdd1b1afc1386f27b49`。SaveRoot親と画像は `Build/GameSave-20261002-30a407/Native/NativeSaveCourseProcesses/Release/20261002T092449Z-6d5c0d89`、別CWDはその中の `無関係 作業場所`。実行した四つのcommand・開始終了UTC・exit・前後manifestをSummaryへ保存し、原本Assetsは変更なし。

Portableのカメラ成分検査の訂正後の同一manifest `0529e71f36b4a73e2c2f78c902a6d49c91acbe489844e82dc05fc65a20672db4` では PortableSaveNativeFrozenRelease02:38/38、38秒、PortableSaveNativeFrozenDebug01:38/38、215秒。両構成build／No-STL／diff全終了0、skip0。新しい保存の実描画追加に対して、旧全Portable群を保持した。最終累積・故障・変異・測定・最終配布4構成は引き続き未完了。

## 一時書込・公開故障とL-M01〜06（L8部分）

保存前の一時書込と公開結果確認をprivateの小さいtemplateへ分離し、製品は追加確保なしのstack adapterから既存Toolbox OS操作を呼ぶ。公開APIやglobalな故障フックは追加しない。Toolboxの短いwriteは実際に書けた長さだけ進め、0バイト・指定長超過・OS失敗を拒否する。OS処理と元の二面選択・排他・取消しの契約は保持した。

局所I/O試験では実際に新規ファイルを作り、writeの一バイト残留、flush、close、読戻しI/O、不在、短い値、改変値を一地点ずつ注入する。全てでclose後に所有tempだけを回収し、正常なmanual.aの全生バイトと世代1を保持、次の通常保存で世代2へ回復する。CREATE_NEW拒否では、既存tempの生バイトを削除・変更しない。

公開は通常、名前変更前の失敗、名前変更済みだがOS失敗、読戻しI/O、実配列確保、値不一致、不在を別々に確認した。OS失敗でも予定バイトが公開先にあれば成功と確定し、確認できない場合はPublicationUnknownで所有を保持する。同じ要求の明示再確認と次の通常保存で世代3へ回復し、正常な元面は全条件で不変。実ファイルの共有違反とCancel拒否は既存SaveData／GameSaveRequestのWindows試験も保持する。実電源断・全FSの耐久性証明ではない。

|試行|結果|
|---|---|
|SaveIoFaultDebug01|build1。保存要求target名を誤った実行器を現行CMakeのdxf_game_save_testsへ訂正。旧exe未実行|
|SaveIoFaultDebug02|一時書込と短いwrite、3/3群成功|
|SaveIoFaultDebug03／Release01|公開故障追加後、3/3群成功|
|SaveIoFaultRelease02|checksum／長さの独立fixture追加後、3/3群成功|
|SaveIoFrozenDebug01／Release01|整形後同一コード3/3・失敗0・skip0。保存41/41、故障8/8、要求11/11。コメント／No-STL995対象・違反0／diff終了0|

L-M02の初回は検査を省く製品変異が生き残った。従来の破損例はJSON構文も壊していたため、checksumを外しても構文検査で拒否できた。正常JSONのpayload一文字だけを変更して元checksumを保つfixtureと、正常payload／checksumでヘッダー長さだけを変更するfixtureを追加し、同じ変異が両方をRedにすることを確認した。最初の未検出を成功へ転記しない。

|変異|ログ下の試行|検出と復元|
|---|---|---|
|L-M01|Mutations/L-M01-Core01|最新正常面を先に空にする。世代交互更新の保護試験Red|
|L-M02|Mutations/L-M02-Initial01|未検出、正常バイト復元と再build／Green|
|L-M02|Mutations/L-M02-Strengthened01|長さ／checksumの省略を独立した二例でRed、正常復元Green|
|L-M03|Mutations/L-M03-Core01|世代をf64へ一度変換し、9007199254740993のexact round-tripがRed|
|L-M04|Mutations/L-M04-Core01|未公開tempの最高世代を採用し、temp無視試験Red|
|L-M05|Mutations/L-M05-Core01|将来formatを受理して未来バージョン拒否試験Red|
|L-M06|Mutations/L-M06-Core01|gameId／Content revision不一致を受理して互換性試験Red|

各検出はbuild0→期待する実挙動Red→生バイト復元→更新時刻前進→対象cppの実再コンパイル→正常41/41を確認した。コンパイル失敗を検出件数へ数えず、一件ずつ行った。各Result.jsonに元ファイルhash、退避パス、期待したケース、exe hash、復元manifestを保持する。保存Rootは登録済みの今回専用試験ディレクトリで、Assetsや実ユーザー保存へ変異を実行していない。L-M07〜20と上位の追加故障は未完了。

整形後の採用manifestは `6d1127f07ed53794f01d1093942d974a279ca1564c3a54b71bacc01a887d2d11`。六つのCore変異の復元manifestは `239d3e367083a4476d176a634679f31dc41965556a863aaff3d6c31a50147781`、差はその後のFileWriter／試験lambdaの宣言分離整形であり、二面選択・codecは変更なし。最終累積をこの限定3群で代用しない。


## 上位の故障境界と変異の追加検証

再開準備は旧Sceneを変更しないPoll区間へ確保故障を限定し、各失敗後に同じ保存からの明示再要求を正常に完了させた。共有ログが初回に作るMutexを開始前に初期化し、候補・診断・資源参照の終了後保有数を試行ごとに一致確認した。試験自身の8192という探索上限はpilotの実数より小さかったため、有限32768の上限に修正した。既存の採取故障群のtimeoutは60のまま。新しい再開準備群はpilotの2D16877／3D17690地点を基に次元別の独立実行へ分け、各600秒とした。

|試行|構成|登録した対象の実行／成功／失敗／skip|秒|source manifest|
|---|---|---|---|---|
|UpperFaultRelease03|Release|5／5／0／0|131.02|`ba2fe4a840f2b97bab7d1865e6e68fe898022d3fefd145e26dab06daecedc48f`|
|UpperFaultDebug05|Debug|5／5／0／0|753.41|`ba2fe4a840f2b97bab7d1865e6e68fe898022d3fefd145e26dab06daecedc48f`|

内部ケースはCheckpoint29、旧採取故障3、新再開故障2D1／3D1、InteractionSample62。登録CTest数と合算しない。2D countdown0〜16876、3D0〜17689で実注入、最初の未注入は各16877／17690。write／readやBody rollbackを同じ注入区間へ含めていない。

設定PublicationUnknownの間は旧正常面の生バイトを保持し、現在設定を使い続けてもDirtyを残す。明示再確認でgeneration2を確定後にDirtyを解除した。未確定要求はスロット排他を保持するため、その間の通常Store.Loadを試験で呼ばず保護面を読み取って比較した。ゼロ固定更新の1／2Viewで実Body位置・成功Step・採取Step・進行revision・生成数が変わらないことも追加した。

|検出ID|試行|復元|
|---|---|---|
|L-M07|L-M07-Upper01|対象生バイトとmanifest一致、更新時刻前進、実再コンパイル、Green|
|L-M08|L-M08-Upper01|対象生バイトとmanifest一致、更新時刻前進、実再コンパイル、Green|
|L-M09|L-M09-Upper01|対象生バイトとmanifest一致、更新時刻前進、実再コンパイル、Green|
|L-M10|L-M10-Upper02|対象生バイトとmanifest一致、更新時刻前進、実再コンパイル、Green|
|L-M11|L-M11-Upper01|対象生バイトとmanifest一致、更新時刻前進、実再コンパイル、Green|
|L-M12|L-M12-Upper01|対象生バイトとmanifest一致、更新時刻前進、実再コンパイル、Green|
|L-M13|L-M13-Upper02|対象生バイトとmanifest一致、更新時刻前進、実再コンパイル、Green|
|L-M14|L-M14-Upper01|対象生バイトとmanifest一致、更新時刻前進、実再コンパイル、Green|
|L-M15|L-M15-Upper01|対象生バイトとmanifest一致、更新時刻前進、実再コンパイル、Green|
|L-M16|L-M16-Upper01|対象生バイトとmanifest一致、更新時刻前進、実再コンパイル、Green|
|L-M17|L-M17-Upper01|対象生バイトとmanifest一致、更新時刻前進、実再コンパイル、Green|
|L-M18|L-M18-Upper01|対象生バイトとmanifest一致、更新時刻前進、実再コンパイル、Green|
|L-M19|L-M19-Upper01|対象生バイトとmanifest一致、更新時刻前進、実再コンパイル、Green|
|L-M20|L-M20-Upper02|対象生バイトとmanifest一致、更新時刻前進、実再コンパイル、Green|

M07は最新番号比較だけの削除がm_Current所有により等価となるため、旧CPU結果を保持して後続結果の採用後へ上書きする非等価な変異を使用し、遅延Aを検出した。M18は退役した採取参加者が要求を受付ける変異であり、解放済みポインタを意図的に参照する試験ではない。M20は別プロセス実行器が新規にコピーしたAssetsだけへ故障化した保存を書き、原本Assetsの指紋不変を別途確認する。

途中失敗：UpperFaultDebug01は試験が存在しない再試行名を呼んだcompile失敗。Debug02は探索上限と公開前Stageより早いロック、Debug03は未確定要求の排他中にStore.Loadを呼んだ試験期待誤り（新再開故障は未実行）、Debug04はpilotの上限不足。Release01／02は初回共有ログMutexを候補の残留へ誤計数した試験境界。M10-Upper01はLNK1168でbaselinebuild失敗、変異未実行・復元Green。限定したUpper02で実build0→Red→復元Greenを実行した。元の失敗試行を成功数へ足さない。

ログと対象別hashは `Build/GameSave-20261002-30a407/UpperFault*` と `Mutations/*/Result.json`。今回のまとまりは累積最終全群・外部installゲーム再開・四配布・測定・pushの完了を意味しない。

M13-Upper01は実Body比較のRedを検出したが、診断の標準エラーがケース名の途中へ混ざり実行器の文字一致が失敗した。正確に復元して62/62へ戻した後、ログの混在を処理したUpper02で再確認した。M20-Upper01は誤保存先に実在するautosave.aをmanual1.aと取り違えた実行器の検査失敗。原本Assetsは不変で復元Green、Upper02で新しいコピー内のautosave.aの存在と専用Root読込の失敗を照合した。これらの初回失敗を検出成功件数へ加算しない。
