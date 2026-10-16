# L0〜L10：セーブ・ロード・チェックポイント・入力設定

指令識別子：DXF-L-SAVE-4b20f89-20261002-R2。L0〜L10の実装と採用検証をまとめた記録。実装・全群・実機・配布の実行日は2026-10-02、記録の確定日は2026-10-16。Bの検証記録をLの成功件数へ転記しない。前半の各節はその時点の状態を残しており、末尾の「最終採用集合」と区別する。

採用コードは `cad82cce837d95dfe5ee578c57ac69d32965016e`、manifestは `300fb505893b055ef5d303f443827ad6f032df77b13182cadebb65d9d1e62d55`。Portable Debug／Release各42/42、Native各49/49、正規ValidateDebug各42/42、配布OFF各18工程／ON各25工程、実IDE指定Python88/88、公開ヘッダー47構成、No-STL998ファイル・違反0。進行・設定の真の別プロセス再開とNative画素、L-M01〜20、関連する故障・測定を含む。コード検証commitと文書だけの後続commitを区別し、通常push後の最終HEAD／origin/main／remote refは終了報告とGit終了ログで照合する。

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


## 再配置installの外部ゲームConsumer（追加のpilot）

既存ConsumerとNativeAppへ限定したwrite／read入口を追加し、元Source・Examplesのcppを外部targetへ追加せず公開されたgame_save／input_profilesだけを使った。信頼する配布JSONから三個の初期Prefabを作り、四種類の内部接続と四種類の個体間接続を確認。ゲーム処理が取得物を除去し、score42・解錠・north・動的cargo・片側解除・左右の異なる速度指示を保存した後、score99・二個目cargoへ変更する。新起動は保存時の論理進行へ戻り、保存したKeyboard K／Mouse Right／Pad4 Button16を使用し、Jump Actionが実BodyへのImpulseへ届く。個体別parameterも独立した期待値で確認する。

|試行|終了|source manifest|同一exe SHA-256|別起動|
|---|---|---|---|---|
|ExternalCheckpointDebug04|全工程0|`7f6036968ac8d9b2804059d872457806947232b12e49b7601cbc656bb346e2df`|`908e14aeb4454d578e8ea3a9a650de59f94ea47d61e47ba40577f13f2b595de9`|2D write/read＋3D write/readの4プロセス|
|ExternalNativeCheckpointDebug04|全工程0|`a57a544d258b6ea27f994dccb2a1588c0e87d8fb311da6a57e36f2c8e983ec3d`|`a320e081bdbaf03c886c2935c57016e90d54b07e2fb9ed0de151012d9361874b`|2D write/read＋3D write/readの4プロセス|

CPU側は実Navigator／Scene／Worldと資源backendの試験実装、Native側は実Application／AssetService／DxLib。Nativeは保存前／保存時／その後の変更／Continue後の4状態を、全画面／左右／1001×501で24PNGへ保存し、青い物体領域の存在／消失と別領域のTexture／Modelを一括GPU読戻しで照合した。2D／3DのContinue後の奇数寸法画像も目視した。日本語・空白の再配置先、別CWD、新規SaveRootであり、人の入力・聴感ではない。前後source manifest（原本Assetsを含む）は同一。四配布全構成の最終採用はまだ未実施。

途中失敗：CPU01はConsumerの必要include欠落、02／03はFStringViewの診断出力型を既存APIへ合わせていないcompile失敗。ビルド非0後のexeは実行しなかった。Native01／02は画面座標型の曖昧さ、PNG引数／UTF-8 ABIの取り違えによるcompile失敗。Native03はCount5入口が固定ウィンドウ設定で、全画面と分割の画素は一致しても奇数寸法のbackbufferにならなかった。既存可変ウィンドウ設定をCount5だけへ適用し、画素閾値を保持した04で成功。成功数へ初回失敗を合算しない。

## Workerでの再開計画と保存UIの追加検証（途中結果）

256個体の中間測定で、再開のcodec／参照検査／純粋なゲーム変換を所有側Pollが処理していたため、GameSaveRequestのCPU準備フックへ移した。不変のカタログと要求別のCPU値だけをWorkerが保持し、Pollは資源採用・候補Scene・通常Navigator要求を担当する。実スレッド識別の回帰と、旧adapter破棄後に旧Workerが戻る対照を両次元で追加した。旧Sceneと設定を保持する契約、必須Readyの完了判定は維持した。

|試行|結果|前後一致したsource manifest|
|---|---|---|
|UserSaveLinkRepair03|全工程終了0|`0b789831dc35442ebb3aaf14455adf66fcd5bfd649a80629774c369f449e7ea5`|
|WorkerPrepareDebug02|全工程終了0、対象3/3群、43秒|`8cb4d4fc82ce9766606897ec790311680936a77ca0e16baf8494f138dfc9d8d9`|
|WorkerFaultRelease01|全工程終了0、対象8/8群、156秒|`08953547a0621897456f4e331c6921329f1e256b9f6f5921eeaae0867f6d17d6`|
|NativeSaveUiDebug08|全工程終了0、対象2/2群、41秒|`2d0b1e10a303d41746955708f4d7b7e22780ca4a5e9dc2d57ed5c76fcc099520`|
|NativeSaveUiRelease02|全工程終了0、対象2/2群、33秒|`2d0b1e10a303d41746955708f4d7b7e22780ca4a5e9dc2d57ed5c76fcc099520`|
|WorkerStatic02|全工程終了0|`2d0b1e10a303d41746955708f4d7b7e22780ca4a5e9dc2d57ed5c76fcc099520`|

Releaseの確保故障はCPU準備2D countdown0〜15365／3D0〜16095、所有側採用2D0〜3876／3D0〜4062で実注入した。最初の未注入正常試行はそれぞれ15366／16096／3877／4063。各失敗後の旧Scene・正常保存generation1の保持、明示再要求で通常1Stepの必須Ready、終了後保有数一致を検査した。同期Prepareで探索した同じ処理を、別の実Worker回帰が使用する。新しいWorker内人工例外APIは作っていない。Debug探索と全群累積は別途採用する。

Native保存UIは保存プロセスAと新しい読込プロセスBを両次元で実行し、保存した音量.35／UI倍率1.5／Jump Kを確認する。倍率1.5のメニューで表示したButtonの背景色を一括GPU読戻しし、実クリックとSlider操作を通常寸法と1001×501で検査した。各構成は4プロセス・28PNG。物理はPause中に進めず、設定だけを操作し、元設定は保存面を書き換えず戻す。原本Assets不変・同一exeを実行器で照合した。固定入力であり、人手操作・聴感の成功ではない。

途中失敗は分けて保持した。NativeSaveUiDebug02はSlider解放後に所有側が設定を適用する前の試験期待誤りで、通常更新を待つ修正を行った。Debug04／06の診断倍率1は整数用ToStringの小数切捨てであり、設定値は独立assertで1.5だった。診断を既存FJsonWriter::Numberで出力するDebug08／Release02では1.5を記録し、実行器はこの倍率・画像・マーカー不足を拒否する。画素条件は緩めていない。WorkerStatic01は実行器のcmd引用誤りによる環境取得失敗、02で修正した。

通常Pythonは88件中80成功・8skip。公開ヘッダー47構成は単独・正逆順・2D／3D併用・保存コア単独・Profile単独でcompile終了0、/utf-8指定。IDE指定のPythonは別に実行する。利用者のLIB／ヘッダーエラーは正規通常／Development再生成とDebug／ReleaseのGameplaySample実リンクで確認した。生成Solution／vcxprojは追跡しない。最終配布4構成・全群・測定・pushの完了を意味しない。

WorkerFaultDebug01は8/8群・失敗0・skip0・全終了0、864秒。前後manifest `2d0b1e10a303d41746955708f4d7b7e22780ca4a5e9dc2d57ed5c76fcc099520` が一致した。CPU準備2D277.58秒／3D304秒、所有側2D92秒／3D98秒。各群の個別上限600秒は変更していない。

|Worker化後の再確認|結果|正常復元manifest|
|---|---|---|
|L-M07-Worker01|build0→期待Red→生バイト復元→実再コンパイル→Green|`8d39c5299b1a279e9dae408c2dbd9fed98e0a6151b1e429e6ad1ea55f422ffd3`|
|L-M08-Worker01|build0→期待Red→生バイト復元→実再コンパイル→Green|`8d39c5299b1a279e9dae408c2dbd9fed98e0a6151b1e429e6ad1ea55f422ffd3`|
|L-M09-Worker01|build0→期待Red→生バイト復元→実再コンパイル→Green|`8d39c5299b1a279e9dae408c2dbd9fed98e0a6151b1e429e6ad1ea55f422ffd3`|
|L-M10-Worker01|build0→期待Red→生バイト復元→実再コンパイル→Green|`8d39c5299b1a279e9dae408c2dbd9fed98e0a6151b1e429e6ad1ea55f422ffd3`|
|L-M11-Worker01|build0→期待Red→生バイト復元→実再コンパイル→Green|`8d39c5299b1a279e9dae408c2dbd9fed98e0a6151b1e429e6ad1ea55f422ffd3`|
|L-M12-Worker01|build0→期待Red→生バイト復元→実再コンパイル→Green|`8d39c5299b1a279e9dae408c2dbd9fed98e0a6151b1e429e6ad1ea55f422ffd3`|
|L-M19-Worker01|build0→期待Red→生バイト復元→実再コンパイル→Green|`8d39c5299b1a279e9dae408c2dbd9fed98e0a6151b1e429e6ad1ea55f422ffd3`|

M07は新しい準備途中の取消し照合も含めて故障化し、旧CPU値が後続の結果を上書きする非等価な変異を確認した。M09は所有側の資源／候補準備前に旧Sceneを破棄する変異へ合わせた。どちらもWorkerへSceneを渡す製品変更ではない。3Dの奇数寸法保存UIの実PNGも確認した。一時変異なし。

## 凍結したコードの測定と配布

検証コードは `a0caf2200560871830d36ee53cdd91d344128758`、生バイトmanifestは `8d39c5299b1a279e9dae408c2dbd9fed98e0a6151b1e429e6ad1ea55f422ffd3`。この節は成功した測定・配布の記録で、まだ実行中の累積全群やpushを完了として扱わない。

|試行|UTC開始／終了|成功工程／失敗／skip|内容|
|---|---|---|---|
|FinalPackageOffDebug01|2026-10-02T12:35:11.172589+00:00／2026-10-02T12:35:32.430197+00:00|18／0／0|CPU再開4別起動、保存コア2別起動、Profile2別起動、旧Support／Physics／UI／UI runtimeを実行|
|FinalPackageOffRelease01|2026-10-02T12:35:34.106715+00:00／2026-10-02T12:36:01.421196+00:00|18／0／0|CPU再開4別起動、保存コア2別起動、Profile2別起動、旧Support／Physics／UI／UI runtimeを実行|
|FinalPackageOnDebug01|2026-10-02T12:36:03.039379+00:00／2026-10-02T12:36:47.794448+00:00|25／0／0|CPU再開4別起動、保存コア2別起動、Profile2別起動、Native再開4別起動・24画像、旧Native／UIも実行|
|FinalPackageOnRelease01|2026-10-02T12:36:49.468357+00:00／2026-10-02T12:37:38.471747+00:00|25／0／0|CPU再開4別起動、保存コア2別起動、Profile2別起動、Native再開4別起動・24画像、旧Native／UIも実行|

旧配布10工程を保持した。入力設定のwrite/readで2工程、保存コアで2工程、2D／3Dのゲームwrite/readで4工程を追加し、OFFは18工程。ONは旧Native ApplicationとUIの3工程に、Nativeゲームwrite/readの4工程を加え、25工程。install／exportからコピーした公開targetを使い、元Source／Examples cppを外部targetへ追加していない。日本語・空白の再配置先、別CWD、新規SaveRoot、原本Assetsとexeの前後指紋を確認した。SDKは既存のDxLib-3.25a-sourceで、新規取得・再構築は行っていない。

|構成|CPUゲーム同一exe SHA-256|Nativeゲーム同一exe SHA-256|SaveRootの試行ディレクトリ|
|---|---|---|---|
|FinalPackageOffDebug01|`0b9d762f28e586627578df4f61838f6e7838837eecdade55e533291a99d46c75`|対象なし|`Build/GameSave-20261002-30a407/FinalPackageWork/run-uegizllh` 以下、継続ゲーム SaveRoot／実描画ゲーム SaveRootの専用保存2D・3D|
|FinalPackageOffRelease01|`c20dc4b61117cdc55b07ef485f34ffa740d7c8fe7144ea390dd5af930a1fbd2e`|対象なし|`Build/GameSave-20261002-30a407/FinalPackageWork/run-cgu32l0r` 以下、継続ゲーム SaveRoot／実描画ゲーム SaveRootの専用保存2D・3D|
|FinalPackageOnDebug01|`6c5717ee969b634ae3803d9e8ff1b2405a1cce6638b563e41288b1e0dec6c30f`|`d26279db1e84175abdac9381e11fbc94b8be8473c63f189a3125c364941f1134`|`Build/GameSave-20261002-30a407/FinalPackageWork/run-ics42dao` 以下、継続ゲーム SaveRoot／実描画ゲーム SaveRootの専用保存2D・3D|
|FinalPackageOnRelease01|`1969c5b5807ca097060934ff3ef9a7ede1eaa57f0b98bf20a63fd9153b9a0c76`|`a6ff711a086e290e8a09cf0fc34ff4ad9f8e7dd7c690cfb1cdec0c8d5453faf4`|`Build/GameSave-20261002-30a407/FinalPackageWork/run-zm2hx1rt` 以下、継続ゲーム SaveRoot／実描画ゲーム SaveRootの専用保存2D・3D|

外部writeは初期三個体から取得物を除去し、score42・north・解錠・cargo X=8・四接続と片側解除・左右独立parameterを保存した。その後score99・追加cargo X=-4へ変更する。新しいreadはscore42・取得済み除去・cargo一個へ戻り、変更後の個体は出現しない。左右のDriveと内部／個体間の全4種類の接続先を独立した期待値で照合し、保存したKのJumpが実Bodyへ届く。保存したMouse Right／Pad slot3 Button16もActionを変える。Nativeはbefore／saved／changed／continuedを全画面／左右／1001×501で描き、物体の有無・数・位置とTexture／Modelの領域を一括読戻しする。固定入力であり、人の入力・聴感ではない。3Dの再開後奇数寸法PNGも確認した。

### Release費用

SaveBenchmarkRelease06のUTCは2026-10-02T12:09:03.743779+00:00〜2026-10-02T12:35:08.852372+00:00、同一exe SHA-256は `7adfb035ce74d28b10a4ee1c1d80166789440ed1d25b74be86972d78ee154e22`。AMD Ryzen 7 9800X3D（8 core／16 logical）一台、各条件5つの新しいWorld／SaveRoot。予備6条件と本測定6条件を分けて保存した。下表は本測定のμs／呼出しまたはμs／Stepで、値は中央値（最小〜最大）。確保はToolboxの回数であり、OS／CRT／Nativeの全確保を表す値ではない。GPU時間は測っていない。

製品ソースへ時間probeを挿入していない。資源境界はCPUの試験実装で、資源無しの同期World。各個体は2 Body／4 Jointだが、保存された仕掛けentryはPrismatic export一個。四種類の保存状態を計測した表とは説明しない。Stageはcodecとslot排他・実write／flush／close／読戻しの合計で、純粋disk時間ではない。Worker待機／配送は残った待ち時間とCommit配送で、純粋Worker CPU時間ではない。所有側受付、資源採用・候補初期化、通常の最初のReadyを別に測った。Readyは両次元とも1通常Step。

16個体：Body32／Joint64／保存entry16、文書2D 6536bytes／3D 6696bytes。

|工程|2D μs|3D μs|2D確保|3D確保|
|---|---:|---:|---:|---:|
|trusted-read-parse|379.800 (362.900〜474.100)|382.700 (357.100〜501.600)|3752 (3752〜3752)|3928 (3928〜3928)|
|initial-value-plan|53557.200 (53111.700〜54986.200)|58773.000 (57549.300〜59998.300)|152063 (152063〜152063)|160543 (160543〜160543)|
|owner-assets-prepare|108.400 (102.300〜119.100)|134.800 (117.000〜139.400)|2081 (2081〜2081)|2081 (2081〜2081)|
|scene-initialize|79.200 (62.800〜157.000)|86.600 (80.800〜144.600)|361 (361〜362)|361 (361〜362)|
|first-fixed-ready|18667.400 (18330.800〜18975.700)|20352.100 (19973.600〜20834.600)|48792 (48792〜48792)|51704 (51704〜51704)|
|warm-fixed-total|416.140 (415.760〜419.237)|727.813 (726.877〜730.820)|395 (395〜395)|395 (395〜395)|
|steady-fixed-total|415.618 (415.270〜417.481)|726.499 (725.166〜731.036)|395 (395〜395)|395 (395〜395)|
|safe-capture-only|18147.600 (17956.000〜18846.000)|19687.900 (19424.400〜20503.900)|48338 (48338〜48338)|51250 (51250〜51250)|
|checkpoint-encode|18085.500 (17743.700〜18246.100)|19711.000 (19150.800〜20025.400)|48248 (48248〜48248)|51160 (51160〜51160)|
|face-encode-checksum|5584.900 (5486.400〜5864.800)|6038.700 (5945.000〜6153.600)|14587 (14587〜14587)|15387 (15387〜15387)|
|stage-write-flush-readback|7783.200 (7586.700〜8799.000)|8150.400 (7741.600〜8387.200)|14655 (14655〜14655)|15455 (15455〜15455)|
|owner-publish-confirm|437.300 (394.400〜518.300)|441.300 (425.800〜447.700)|4 (4〜4)|4 (4〜4)|
|load-verify-decode|11483.500 (11070.000〜11613.100)|12329.300 (12067.800〜12382.700)|30948 (30948〜30948)|32676 (32676〜32676)|
|restore-plan|26487.500 (26107.700〜26910.500)|28867.500 (28224.800〜29149.000)|74876 (74876〜74876)|79116 (79116〜79116)|
|resume-owner-assets-initialize|18253.200 (18069.300〜18650.600)|19933.900 (19677.700〜20085.800)|50449 (50449〜50449)|53361 (53361〜53361)|
|resume-first-fixed-ready|18384.900 (18206.400〜18557.900)|20359.300 (20239.600〜20698.200)|48792 (48792〜48792)|51704 (51704〜51704)|
|profile-apply|1.400 (1.400〜4.100)|1.300 (1.200〜1.700)|3 (3〜3)|3 (3〜3)|
|profile-update-idle|0.003 (0.003〜0.003)|0.003 (0.003〜0.004)|0 (0〜0)|0 (0〜0)|
|save-poll-idle|0.034 (0.033〜0.034)|0.033 (0.033〜0.034)|0 (0〜0)|0 (0〜0)|
|resume-poll-idle|0.022 (0.022〜0.023)|0.022 (0.022〜0.022)|0 (0〜0)|0 (0〜0)|
|async-submit-worker-wait-deliver|7697.100 (7541.300〜8290.100)|8264.400 (7997.700〜8436.800)|14676 (14676〜14676)|15476 (15476〜15476)|
|async-owner-publish|496.500 (380.300〜516.100)|451.800 (388.200〜469.600)|4 (4〜4)|4 (4〜4)|
|game-payload-migrate-validate|90.000 (88.800〜90.500)|89.900 (89.300〜91.800)|1132 (1132〜1132)|1132 (1132〜1132)|
|resume-worker-wait-deliver|72347.900 (71986.500〜73545.800)|79004.300 (77799.400〜79875.700)|202678 (202678〜202678)|214086 (214086〜214087)|
|resume-owner-submit|80.100 (72.300〜89.200)|81.300 (68.800〜110.100)|1231 (1231〜1231)|1231 (1230〜1231)|
|scene-shutdown|70.400 (54.500〜78.600)|85.400 (74.500〜95.200)|0 (0〜0)|0 (0〜0)|

64個体：Body128／Joint256／保存entry64、文書2D 25775bytes／3D 26415bytes。

|工程|2D μs|3D μs|2D確保|3D確保|
|---|---:|---:|---:|---:|
|trusted-read-parse|380.500 (354.600〜439.500)|415.500 (382.300〜468.300)|3752 (3752〜3752)|3928 (3928〜3928)|
|initial-value-plan|690848.600 (687394.200〜692121.900)|752537.900 (749946.300〜988320.200)|600279 (600279〜600279)|634199 (634199〜634199)|
|owner-assets-prepare|629.800 (612.500〜656.000)|741.300 (678.600〜793.300)|12857 (12857〜12857)|12857 (12857〜12857)|
|scene-initialize|305.000 (227.800〜357.800)|341.800 (306.200〜448.700)|1421 (1421〜1422)|1421 (1421〜1422)|
|first-fixed-ready|236475.100 (235924.500〜238312.600)|260524.500 (258669.100〜263085.900)|188208 (188208〜188208)|199856 (199856〜199856)|
|warm-fixed-total|1681.003 (1675.240〜1693.630)|2939.340 (2911.970〜2998.740)|1551 (1551〜1551)|1551 (1551〜1551)|
|steady-fixed-total|1688.602 (1672.402〜1721.871)|2933.228 (2910.511〜2956.172)|1551 (1551〜1551)|1551 (1551〜1551)|
|safe-capture-only|235016.100 (233976.600〜255530.100)|256520.200 (255516.200〜270608.500)|186572 (186572〜186572)|198220 (198220〜198220)|
|checkpoint-encode|235042.700 (233479.800〜245517.000)|256016.400 (254750.200〜257690.100)|186242 (186242〜186242)|197890 (197890〜197890)|
|face-encode-checksum|72114.400 (71955.800〜72419.800)|78676.600 (78150.800〜78901.800)|56551 (56551〜56551)|59751 (59751〜59751)|
|stage-write-flush-readback|78622.100 (78485.100〜80360.600)|86346.400 (85285.100〜88933.200)|56619 (56619〜56619)|59819 (59819〜59819)|
|owner-publish-confirm|617.400 (391.900〜760.600)|579.300 (482.900〜753.100)|4 (4〜4)|4 (4〜4)|
|load-verify-decode|145099.400 (144052.200〜146546.400)|158742.200 (156982.100〜188714.300)|119730 (119730〜119730)|126642 (126642〜126642)|
|restore-plan|346075.400 (345238.000〜347159.800)|380177.000 (376538.300〜413570.000)|298862 (298862〜298862)|315822 (315822〜315822)|
|resume-owner-assets-initialize|246824.800 (235782.000〜276647.200)|257865.500 (257099.200〜274841.800)|199511 (199511〜199511)|211159 (211159〜211159)|
|resume-first-fixed-ready|237858.700 (237378.500〜282323.200)|260768.300 (259509.900〜287586.800)|188208 (188208〜188208)|199856 (199856〜199856)|
|profile-apply|1.800 (1.600〜4.300)|1.700 (1.400〜1.900)|3 (3〜3)|3 (3〜3)|
|profile-update-idle|0.003 (0.003〜0.003)|0.003 (0.003〜0.003)|0 (0〜0)|0 (0〜0)|
|save-poll-idle|0.033 (0.033〜0.033)|0.033 (0.033〜0.033)|0 (0〜0)|0 (0〜0)|
|resume-poll-idle|0.022 (0.022〜0.022)|0.022 (0.022〜0.023)|0 (0〜0)|0 (0〜0)|
|async-submit-worker-wait-deliver|83233.400 (79470.400〜98184.400)|85740.900 (85420.900〜87148.700)|56640 (56640〜56640)|59840 (59840〜59840)|
|async-owner-publish|675.100 (522.600〜841.100)|708.900 (524.500〜787.500)|4 (4〜4)|4 (4〜4)|
|game-payload-migrate-validate|146.600 (92.000〜185.300)|126.300 (90.600〜181.600)|1132 (1132〜1132)|1132 (1132〜1132)|
|resume-worker-wait-deliver|951585.700 (941444.000〜1003278.800)|1050115.300 (1023627.800〜1087658.000)|802618 (802618〜802618)|848250 (848250〜848250)|
|resume-owner-submit|317.700 (256.900〜370.100)|384.700 (254.700〜441.200)|4399 (4399〜4399)|4399 (4399〜4399)|
|scene-shutdown|364.700 (264.500〜535.700)|350.000 (296.200〜431.300)|0 (0〜0)|0 (0〜0)|

256個体：Body512／Joint1024／保存entry256、文書2D 103085bytes／3D 105645bytes。

|工程|2D μs|3D μs|2D確保|3D確保|
|---|---:|---:|---:|---:|
|trusted-read-parse|426.800 (388.100〜445.900)|468.000 (438.000〜1216.000)|3752 (3752〜3752)|3928 (3928〜3928)|
|initial-value-plan|10548031.100 (10511646.900〜10556612.300)|11491535.500 (11477571.400〜11689261.100)|2669215 (2669215〜2669215)|2804895 (2804895〜2804895)|
|owner-assets-prepare|5332.300 (5286.700〜5468.700)|5529.600 (5437.700〜5588.700)|125081 (125081〜125081)|125081 (125081〜125081)|
|scene-initialize|1656.900 (1556.500〜1785.900)|1869.300 (1822.000〜2053.700)|5649 (5649〜5650)|5649 (5649〜5650)|
|first-fixed-ready|3615319.600 (3574994.900〜3811449.700)|3956415.600 (3900972.600〜3995480.400)|745656 (745656〜745656)|792248 (792248〜792248)|
|warm-fixed-total|6807.710 (6796.007〜8473.453)|11754.487 (11748.883〜11763.583)|6163 (6163〜6163)|6163 (6163〜6163)|
|steady-fixed-total|6799.717 (6786.321〜7155.530)|11742.749 (11720.482〜12008.872)|6163 (6163〜6163)|6163 (6163〜6163)|
|safe-capture-only|3564403.500 (3555555.300〜3606894.800)|3951339.300 (3919258.100〜3956009.900)|739382 (739382〜739382)|785974 (785974〜785974)|
|checkpoint-encode|3627086.000 (3615784.500〜3638411.700)|3952070.400 (3940465.700〜3955714.200)|738092 (738092〜738092)|784684 (784684〜784684)|
|face-encode-checksum|1093943.000 (1091600.000〜1103084.000)|1193036.900 (1191204.700〜1236945.800)|224371 (224371〜224371)|237171 (237171〜237171)|
|stage-write-flush-readback|1142831.800 (1100804.400〜1145683.500)|1196641.500 (1194576.000〜1256652.300)|224439 (224439〜224439)|237239 (237239〜237239)|
|owner-publish-confirm|542.200 (411.800〜709.200)|689.900 (523.700〜749.100)|4 (4〜4)|4 (4〜4)|
|load-verify-decode|2196902.000 (2183839.700〜2382284.100)|2422703.500 (2379904.200〜2434604.400)|474768 (474768〜474768)|502416 (502416〜502416)|
|restore-plan|5278594.300 (5271623.500〜5301960.700)|5745718.300 (5730703.500〜5761955.300)|1332848 (1332848〜1332848)|1400688 (1400688〜1400688)|
|resume-owner-assets-initialize|3657402.100 (3590777.300〜3875914.300)|3967665.100 (3945271.700〜4151364.300)|864741 (864741〜864741)|911333 (911333〜911333)|
|resume-first-fixed-ready|3633411.500 (3588506.300〜3741345.300)|3975043.900 (3921792.700〜4130204.400)|745656 (745656〜745656)|792248 (792248〜792248)|
|profile-apply|2.000 (1.900〜7.200)|1.800 (1.700〜2.000)|3 (3〜3)|3 (3〜3)|
|profile-update-idle|0.003 (0.003〜0.004)|0.003 (0.003〜0.003)|0 (0〜0)|0 (0〜0)|
|save-poll-idle|0.033 (0.033〜0.034)|0.034 (0.033〜0.064)|0 (0〜0)|0 (0〜0)|
|resume-poll-idle|0.022 (0.022〜0.023)|0.022 (0.022〜0.023)|0 (0〜0)|0 (0〜0)|
|async-submit-worker-wait-deliver|1151632.300 (1107868.000〜1268012.800)|1206661.000 (1196621.800〜1264274.300)|224460 (224460〜224460)|237260 (237260〜237260)|
|async-owner-publish|674.500 (592.500〜959.000)|717.600 (533.600〜954.800)|4 (4〜4)|4 (4〜4)|
|game-payload-migrate-validate|107.500 (104.700〜108.400)|106.300 (101.800〜145.700)|1132 (1132〜1132)|1132 (1132〜1132)|
|resume-worker-wait-deliver|14586197.200 (14453600.700〜14762173.300)|15775162.200 (15731893.700〜15988120.600)|3478308 (3478308〜3478308)|3660836 (3660836〜3660838)|
|resume-owner-submit|1408.400 (1270.200〜1475.400)|1461.200 (1344.200〜1554.900)|17081 (17081〜17081)|17081 (17079〜17081)|
|scene-shutdown|1935.600 (1909.400〜2004.800)|2170.800 (2091.500〜2901.800)|0 (0〜0)|0 (0〜0)|

要求なしPollと通常の無入力Action更新の定常追加確保は0。Worldの定常更新は16／64／256で395／1551／6163回であり、全体0と呼ばない。大きい条件の採取・codec・純粋な値計画には秒単位の費用があり、所有側資源準備の最大は2D 3,875,914.3μs／3D 4,151,364.3μs。同期初期値計画と純粋な同期Buildを比較測定する工程も所有側でブロックする。通常非同期APIの所有側受付も値コピー等があり、完全非ブロックとは主張しない。上限近傍のWorld測定は256の予備実時間から追加していない。時間は合否閾値にせず、この費用を次の棚卸し候補として残す。

### 実OSの追加確認

SaveOsProbeDebug01は公開installのsave_dataだけをリンクした追加の外部試験。専用Rootの旧面だけをread-onlyにし、実ERROR_ACCESS_DENIED=5を確認した。最新正常面と旧面の全バイトを保持し、属性を戻した同じトランザクションの再試行でgeneration3を確定した。Python親が所有するOS slot排他を、別C++プロセスのWriterがBusyで拒否し、保護面不変も確認した。全部の終了0、前後manifest一致。同一probe exeは `d02750de4079929f15708b0d5ec48c9d96d993b5c92a79590e0c934fe8af45ed`。Build内の新規probeソースは製品ソース変更や恒久targetの追加ではなく、今回の実I/O確認として区別する。試験専用の属性をguardで復元し、実ユーザー保存・Assets原本・既存の拒否された削除対象は変更していない。

## 最終採用集合

検証コードcommit `cad82cce837d95dfe5ee578c57ac69d32965016e`、最終の製品・試験・CMake・Tools・Assetsの生バイトmanifest `300fb505893b055ef5d303f443827ad6f032df77b13182cadebb65d9d1e62d55`。以下は新しい試行の実結果で、途中失敗や限定再試験を加算した集合ではない。現在仕様文書は別の後続commitとして扱う。

FinalPortableDebug01は42群中41成功・1失敗・skip0、971.01秒、CTest終了8／実行器終了1。SaveCourseProcessesの新しい2D読込プロセスが「更新10000回」でCPU準備待ちを打ち切った。製品の失敗・timeoutではなく、Worker化後の試験待機条件の不備である。ProcessWaitRepair01でSaveCourseProcess.cppとSaveCourseSessionTests.cppだけを、既存Worker試験と同じ実時間5秒に変更した。物理を進めないTick(0)と全期待値を保持し、子プロセス45秒・CTest120秒や各故障群600秒の制限を変えていない。Debug／Release各2群と各4別プロセスの正常終了を確認してcad82ccへcommitした。初回失敗を全群成功へ合算しない。

|全群試行|登録／実行／成功／失敗／skip|UTC開始／終了|CTest秒|exit|
|---|---|---|---:|---:|
|FinalPortableDebug02|42／42／42／0／0|2026-10-02T13:03:48.712964+00:00／2026-10-02T13:19:52.220483+00:00|961|0|
|FinalPortableRelease01|42／42／42／0／0|2026-10-02T13:19:52.535875+00:00／2026-10-02T13:23:21.256574+00:00|183|0|
|FinalNativeDebug01|49／49／49／0／0|2026-10-02T13:23:21.569817+00:00／2026-10-02T13:41:30.762397+00:00|1071|0|
|FinalNativeRelease01|49／49／49／0／0|2026-10-02T13:41:31.076908+00:00／2026-10-02T13:46:05.334761+00:00|229|0|

各試行の全target build、Cache、実登録JSON、JUnit、直後のLastTest、SourceManifest／FinalSourceManifest、登録実行ファイルのSHA-256を固有フォルダーへ保存した。Nativeは既存ソースSDKとdevice ON、旧モデル・UI・Gameplayも実行し、固定名の画面読戻しも実行前後にその試行へコピーした。人のF5や物理操作を確認した結果ではない。

|正規独立構成|登録／実行／成功／失敗／skip|CTest秒|exit|
|---|---|---:|---:|
|Debug|42／42／42／0／0|951|0|
|Release|42／42／42／0／0|171|0|

|静的・生成物の試行|結果|
|---|---|
|FinalIde01|全工程exit0。正規通常／Developmentの再生成、Starter／Sandbox／GameplaySample両構成の実リンク、Solutionとfilterの検査。Visual Studio GUI目視・F5は未実施|
|FinalStatic01|全工程exit0。公開ヘッダー47構成（単独、正逆、両次元、保存コア・Profile単独）、/utf-8、No-STL、差分、実IDE指定Python|
|FinalPythonOrdinary01|全工程exit0。通常Python。IDE条件のskipは成功へ加算しない|
|FinalPythonIdeAll-20261016/20261016T122926Z-abd0bc54|Python 88/88、失敗0・skip0、exit0。実Solution、Visual Studio build、UI file APIの3指定を全て渡した。UTC 2026-10-16T12:29:26.630908+00:00〜2026-10-16T12:29:33.606433+00:00|

FinalStatic01のPythonは88件中85成功・3skip（VS build／UI file APIの指定不足）。上の追加試行で全条件を有効にした88/88を独立実行した。通常実行は88件中80成功・8skip。結果を合算しない。No-STLは998ファイル・違反0、公開ヘッダー47構成。実機を含む全群と配布は2026-10-02に完了しており、中断後の2026-10-16に同じ生バイトmanifest、実行ファイル指紋、ログの成功を確認して文書を確定した。14日後に実機全群を再実行したとは表示しない。

### 最終配布と別プロセス

|試行|工程／失敗／skip|UTC開始／終了|CPU exe SHA-256|Native exe SHA-256|
|---|---|---|---|---|
|FinalPackageOffDebug02|18／0／0|2026-10-02T13:01:21.099053+00:00／2026-10-02T13:01:42.509288+00:00|`6660e584b6dd4be95ed015f362fc3ca4364f67f0bcc89a36ffc3b526ab58d6d1`|対象なし|
|FinalPackageOffRelease02|18／0／0|2026-10-02T13:01:44.128620+00:00／2026-10-02T13:02:11.016647+00:00|`aead067dbafbaf1b00c774e80e86661d1cb6b1dad5d84b01040a4dc8e126f095`|対象なし|
|FinalPackageOnDebug02|25／0／0|2026-10-02T13:02:12.615456+00:00／2026-10-02T13:02:57.942005+00:00|`7e181f295c1f69d7f3e26a13c46127f3e4dc75d4df3fd15f4e8d66e2c8107b47`|`944ab49a3bd0602bac69358511001952b768ce278f73f4853071d5297758d740`|
|FinalPackageOnRelease02|25／0／0|2026-10-02T13:02:59.545051+00:00／2026-10-02T13:03:48.378516+00:00|`bd28f185642cefdabcc4ffbf0cbd337a29978cf96331665659c12bed2c0f7faf`|`362f576f147d09df0957dae664ad5f1126cb74eb42a74afd065678b1573258c0`|

OFF各18工程、ON各25工程の増減理由と独立期待値は上の配布節と同じ。保存・読込は新しい別プロセス、進行42と設定.4／1.75／K・Mouse・Padを確認する。原本Assetsと同一exeの前後指紋、必要Assetsの再配置、元Source／Buildの絶対参照不在、公開install targetだけの使用を確認した。ONは追加4ゲームプロセス・24画像、root NativeのSampleは4ゲームプロセス・28画像（保存UIの倍率と実クリック4画像を含む）。成功文字だけで画素を判断していない。

ReadOnlyAssetsDebug03は新規再配置AssetsのOSアクセス権を読取・実行に限定し、新規ファイル作成と既存confirm.wavへのwrite openが実PermissionErrorになることを確認した。そのまま2D／3Dの保存／読込4別プロセスを通し、別SaveRootだけが更新された。全終了0、定義・Assets・exe不変、同一exe `6660e584b6dd4be95ed015f362fc3ca4364f67f0bcc89a36ffc3b526ab58d6d1`。finallyでこの新規フォルダーのアクセス記述を元の文字列へ復元して一致を確認した。CPUの実Navigator／Scene／World経路で、資源backendは試験実装。初回01はPowerShell Security moduleの読込環境、02はCPU配布にAssetsコピーがない試験準備の誤りで、いずれも権限変更前に停止した。修正はBuild内実行器だけで製品コードを変更していない。

### 測定・変異の影響範囲

SaveBenchmarkRelease06とSaveOsProbeDebug01のmanifestは8d39c5299b1a279e9dae408c2dbd9fed98e0a6151b1e429e6ad1ea55f422ffd3。最終との差を全エントリーで比較し、上述の待機2試験だけが違うことをProcessWaitRepair01へ記録した。製品・公開API・測定target・I/O・Consumer・入力コードの生バイトは同一である。測定はこの同じ製品コードに対する結果として採用し、変更後の試験コードまで同一manifestで測ったとは表示しない。時間probeを製品へ追加していない。全L-M01〜20とWorker変更に関係する7変異の検出／復元は既記録を保持する。待機変更は変異の検出条件や製品分岐を変更しない。関連B-M16を確認し、Physics数式を変更していないため無関係な旧Joint38変異は再実行していない。通常Physics全群は上の採用回帰で再実行した。

### Git・段階・未実施

|段階|到達点|
|---|---|
|L0|初期構成と進行、所有・要求境界を確認|
|L1|厳格codec、互換性、指紋、厳密整数と明示移行|
|L2|専用Root、二面保存、OS排他・旧面回復・公開再確認|
|L3|Scope付きCPU要求、公開前取消し・旧結果抑止・退役|
|L4|成功全Post境界で論理個体と進行を採取|
|L5|許可Contentの新Sceneへ両次元と4種類接続を再構築|
|L6|完全入力プロファイル、再割当、独立音量・UI設定と永続化|
|L7|既存Sample／UIのNew・Continue・Save・Load・Retry|
|L8|故障・L20変異、実World／App／DxLib、真の別プロセス|
|L9|Release測定、独立Consumer、配布4構成|
|L10|同一製品・最終試験manifestの累積回帰。文書commit・通常pushの確認は終了時Git記録で分ける|

検証コードまでのmain commit一覧（最終文書commitは後続）：

```text
c1b71db3339a7525cd24a729a79ae2de92263b31 feat: 最後の正常保存を守る保存コアと要求境界を追加
843b7996f108f8fe78877635ae50546c28accdb9 feat: 定義の準備に使ったバイト列を指紋で識別
2c1ff941d51fc2c37f0d93e6277265699419c740 feat: 保存データの明示移行と将来バージョンの保護を追加
b5770e50c9c05e4fd875dc94d114174cbd4afcee feat: 論理チェックポイントを許可カタログから両次元で再構築する
892469ac168679c5414ee21e8782c86ebada7c58 feat: 成功固定更新で個体履歴と採用済みJoint設定を採取する
f337a64fc7d92a375a7ef22130cd98ddcf4dbdab feat: 論理進行からSceneを再開し成功物理更新後に完了を確定する
37aa078b79bf596f9e90d47d8d2691d19234fbd5 feat: 入力設定を一括適用し解放後の操作と独立した設定保存を追加
f5bf024e4f158989aa591414ddbb8548092e7071 fix: 引数コメントをパラメータごとに改行し再発検査を追加
765d2369b231dcfb0c821da74419e6f4637d8e33 feat: 許可した初期構成から両次元のチェックポイント文書を生成
06b757e51ed1a36eb139f5e45ed432c0ad77fb6a feat: コース固有のチェックポイント進行を型付き値で検証
77b421c77bf9e435474588d9ac9eb7eea29fbe59 feat: データ由来の両次元チェックポイント再開レシピを追加
4601ec832a8e3c995679e269ed825c2fed3e6047 feat: 成功物理更新からゲーム進行を採取する両次元Controllerを追加
734fbfcf9a263aa44b643f6a58b123696c7402c4 feat: Scene切替を越える保存要求と別プロセス再開を接続
ea60e341960bec57dc5ec9c46b99beaabe74f726 feat: 保存メニューと入力再割当を接続し再開時の設定を保持する
aea55d710db8b547d51f297b6040c722adc25798 feat: 実DxLibの別起動で保存進行と入力設定の再開を検証する
9150ad7ca83dacd47d22d8024a9fa1e2742b86ae fix: 保存途中の故障境界を検証し正常なJSONの破損検出を補強する
d240afbe9ef4d43e307adac17ad4bba231435dcc fix: 再開準備と設定公開の失敗境界を両次元で検証する
a0caf2200560871830d36ee53cdd91d344128758 feat: 保存再開のCPU準備をWorkerへ分離し外部利用経路を追加する
cad82cce837d95dfe5ee578c57ac69d32965016e fix: 非同期の再開試験を更新回数で打ち切らず実時間で待つ
```

SDK／開発ツールなしPC、人のF5・物理入力・聴感、複数DPI、TSan、Worker内人工例外、実電源断・任意OS／FS耐久性、全D3D／COMリーク証明は未実施。旧NativeModel早期終了原因、3D UI panel確保、Dynamic床追従、Joint連続拘束は今回の成功で解決済みにしない。保存は論理チェックポイントで、任意Worldのbit再開・Solver snapshot・クラウド・暗号化・任意反射／スクリプトではない。次候補はRoadmapへ記録し、実装を開始しない。

2026-10-16のGit最終化の初回は、改行変換の警告がコマンド出力へ混ざり、六つの文書パスの集合比較を実行器が拒否した。stage／commit／push前に終了1で停止し、ファイル変更や履歴変更は行っていない。文書のUTF-8と意図したLF、ソース指紋を確認したうえで、警告の設定だけをそのGit呼出しに限定して指定し、パス比較へ警告文を含めないようにした。グローバルGit設定や製品ソースを変更していない。失敗ログもFinalGit-20261016内の固有試行へ保持する。
