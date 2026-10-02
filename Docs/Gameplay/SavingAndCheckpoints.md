# 保存コアとチェックポイント再開

New Gameの初期文書は`FCheckpointRestore::MakeInitial`へ信頼するカタログとゲームの初期進行Objectを渡して作る。2D／3Dのoverloadは同じ意味で、初期個体・公開Joint・個体間接続の論理状態を生成順に記録する。共有定義は変更せず、WorldやNative資源も生成しない。進行Objectのゲーム固有の意味は呼出し側のcodecで検証し、構造・容量・Contentとの対応はこの入口で再検証する。外部CheckpointConsumerがこの入口から通常の準備・Navigator・必須Readyまでをcompile／runする。

GameplaySampleの`FSaveCourseProgress`／`FSaveCourseProgressCodec`は、保存コアへゲームのルールを入れずに、鍵・得点・チェックポイント・doorA／doorBの解錠と開扉・生成カウンター・進行revisionを検証する。score・spawnCounter・revisionは厳密な十進文字列とし、配列順を変えても扉の論理キーは取り違えない。未解錠の開扉、不明なキー、重複取得を拒否する。実Sensor通知、純粋な再開レシピ、既存Sampleの保存UIへ接続している。

L0〜L10の実装を進めている。任意リンクの`dxf::save_data`は保存コア、`dxf::game_save`はCPU保存／読込要求と型付きチェックポイント・再開計画、通常Navigatorへの要求と必須Readyの完了管理を提供する。`dxf::input_profiles`は完全割当と独立した設定文書を扱う。GameplaySampleのContinue・設定画面と、別Applicationプロセスの両次元再開を接続した。Native画素、全I/O故障、変異、測定・最終配布は引き続き検証中である。初期構成を読み込むBの成果と、今回の論理進行保存を別の機能として扱う。

## 保存する値

`FSaveDocument`は形式バージョン、文書種類、ゲーム識別子、ゲームpayloadバージョン、Content revision、Scene／checkpoint論理キー、次元、厳密な64bit世代、ゲームcodecのJSON Objectを持つ。保存コアはScene、Physics、Nativeを知らない。payloadの個体・仕掛け・進行の意味と移行を検証する責務は上位codecへ残す。任意C++構造体のメモリや実行時IDを保存する入口ではない。

進行と設定には異なる文書種類とslot系列を使う。同じ系列に別種類を書き込む要求は拒否する。設定の封筒はdimension=0、Content revision／Scene／checkpointは空である。進行はdimension=2または3、許可カタログで解決するASCIIの論理キーを要求する。

UTF-8、復号後の重複キー、非有限数値、末尾データ、未知field、未知の形式バージョンを既存Toolbox JSON readerで拒否する。既定上限は8MiB、深度64、文字列64KiB、値128K、論理キー512bytes。世代は符号・先頭ゼロなしの十進文字列であり、f64を経由しない。Writerは復号済み辞書キーをUTF-8バイト順に並べ、配列順と数値の元の検証済み字句を保持する。したがって1と1.0の字句まで同一へ統一する正準化ではない。

`RequireCompatible`はゲーム／Content／Scene／次元／payloadバージョンを厳格に照合する。revisionは呼出し側の明示値であり、保存コアが資源のバイト一致を自動採取したという保証はない。SceneContentのファイル準備結果には、実際に読んだ定義バイトの`ContentFingerprint`がある。再開adapterは読込済みScene／Prefabの指紋を許可カタログと厳格に照合する。アセットbundle revisionはゲームの明示した識別値であり、全アセットの実ファイルhashを保存コアが採取する保証ではない。

## 明示的なゲームデータ移行

`FSaveCodec::Migrate`は、形式・ゲーム・Content・Scene・次元を先に照合してから、指定された`FSavePayloadMigration`のFromVersion／ToVersionと純粋なConvertを使う。変更する値はゲームpayloadとゲームバージョンだけで、世代や識別は維持する。変換後もJSON、封筒、有限上限を再検査する。payloadのゲーム固有型・範囲はConvert側のcodecが検証する。

未知の将来ゲームバージョン、変換がない古いバージョン、異なるゲーム／Contentは拒否する。元保存を自動的に上書きしない。移行値を保存する場合は、利用者が別の新規slotを明示する。`RequestLoad`の任意Migrationも同じ処理をWorkerで行い、失敗した結果は最後の正常な採用値を置き換えない。

`Tests/Data/SaveData/legacy_progress_v1.json`は今回のゲームschema移行試験用fixtureで、過去に配布したフレームワークとの互換性を示す資料ではない。coinsからscoreへの変換、未知バージョン、別Content、変換先の重複key／容量超過、元バイト保持を検証する。公開targetだけを使う移行例は`Tools/PackageConsumer/SaveCore.cpp`にも登録した。

## 最小の同期利用

公開targetだけを使う実行例は`Tools/PackageConsumer/SaveCore.cpp`にある。ゲーム独自のJSON payloadで保存し、別プロセスが論理値と厳密整数を照合する。このConsumer自体はWorldを生成しない。

```cpp
Dxf::FSaveStore Store(Dxf::FSaveStore::DefaultRoot("my_game"));
Dxf::FSaveDocument Document;
Document.GameId = "my_game";
Document.ContentRevision = "trusted_bundle_v1";
Document.SceneKey = "main";
Document.CheckpointKey = "safe";
Document.Payload = "{\"score\":42}";
const auto Generation = Store.Save("manual", Document);
const auto Loaded = Store.Load("manual");
```

`DefaultRoot`はWindowsのLocalAppDataからアプリ識別子を区切った場所を返す。試験とConsumerは完全修飾した専用Rootを明示する。CWD、Assets、.dxfpathsへ切り替えない。初期実装ではWindowsのdrive完全修飾を受け付け、UNC／device pathは未対応として拒否する。字句検査はsymlink／reparse pointを含むOS隔離ではない。

slotはパスではなく短いASCII識別子で、区切り、..、NUL、Windows予約名を拒否する。Windowsのslotはファイル名と同じ大文字小文字の扱いへ揃える。他のゲーム論理キーの大小文字は変更しない。`ListSlots`は二つの確定面から最大128系列を列挙し、temp／lockを含めない。一覧の後のLoadも排他と完全性を再確認する。

## 二つの確定面と失敗

面の表現は`DXFSAVE\n1\n<generation>\n<payload bytes>\n<CRC32>\n<document>`で、ヘッダーの各整数は十進ASCII。CRC-32/ISO-HDLCは実際に書くdocumentバイトへ計算する破損検出であり、署名ではない。面の世代とdocument内の世代も照合する。

Storeは非待機のslot排他をOSから取得し、最新の正常面を変更しない。反対面の専用tempをCREATE_NEWで作り、全量write、flush、明示close、全バイト読戻し検証を行う。`Stage`が返す`FSaveTransaction`はまだ公開していない。`Publish`が反対面だけへ公開し、実ファイルの完全一致を確認して世代を返す。

読込は最高の正常世代を使う。片面不正なら`RecoveredPrevious`と元の理由を返す。両面不正、同世代の異なるpayload、明確な将来バージョンは通常成功にしない。既存Store／別プロセスのWriterと競合するとBusyで拒否し、永続待機しない。lockファイルの存在だけではBusyにならず、プロセス終了でOSの排他が解放される。

容量を超える面も先頭192バイトだけでヘッダーを確認し、明確な未知の面バージョンなら旧面への回復・次の保存を拒否する。この先頭読込を全量検証成功とは扱わない。破損した既知の面は元の容量超過原因を保持し、反対面の正常値から回復できる。上限そのものは公開トランザクション入口でもOS操作前に検証する。

公開前のCancelは確定面を変更しない。公開を試みた後の成否不明は`PublicationUnknown`で、Canceledとは表示しない。トランザクションが保持する排他のもとで同じPublishを明示再試行し、実面を検証できる。公開確認後のCancelはfalse。最新正常面の保持と全ファイルシステム・実電源断の耐久性は別であり、後者は証明していない。

取消しの明示操作は自分が作ったtempだけを後始末する。トランザクション破棄は確保なしで排他を解放し、未公開tempが残る場合もロード候補にしない。任意フォルダーの再帰削除を行わない。

OS処理の契約は[Known Folder取得](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shgetknownfolderpath)、[flush](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers)、[名前公開](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw)、[排他共有](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew)を確認した。Windows.hはToolbox privateに限定し、Native OFFでも利用できる。

## 非同期要求の現在の範囲

呼出し側が`FGameSaveRequest`を所有し、既存Dispatcher／Scopeを任意で借りる。RequestSaveへ渡す文書は、安全な採取境界で既に完成したCPU値である。Scene所有のFCheckpointCaptureによる要求時採取を追加した。Sampleの長期GameInstanceが成功境界の採取値を非同期要求へ渡す。Workerはencode／temp I/Oまたは面読込／互換性検査だけを行う。Task CommitはCPU結果に印を付け、ファイル公開やNavigator.Commitを実行しない。通常更新のPollが保存を公開し、または読込値を採用する。

状態はPending、Writing／Reading、Prepared、Publishing、Committed、PublicationUnknown、TransitionRequested、Completed、Failed、Canceled、Supersededを区別する。未公開手動保存がある間の次要求はBusy。先行読込は後続でSupersededにし、最後に採用した正常値は新要求の失敗でも残す。Preparedはゲーム再開完了ではない。現在のMarkTransitionRequested／MarkResumeCompletedは上位adapterが実際のNavigator要求と必須Readyを確認して呼ぶ入口で、保存コアの試験だけでScene再開を確認したとは扱わない。

操作・Poll・終了は構築した所有スレッドのTask／Commit外で行う。退役は対象Scopeだけを待ち、兄弟やRoot全体を待たない。公開の同期処理やファイルI/Oはブロックし得る。完全非ブロックの契約ではない。

`PublicationUnknown`になった要求は、その後のPollで自動再試行しない。`RetryPublication`で同じ保存の再確認を明示要求し、次のPollで確定面を検証する。失敗したI/Oを画面更新ごとに繰り返さない。
## 論理チェックポイント文書

`FCheckpointDocument2D`／`FCheckpointDocument3D` と `FCheckpointCodec` は、個体キー、許可Prefabキー、準備済み定義の指紋、配置、型付きparameter、存在／除去、生成順、4種類のJointの接続・Drive・Limit・目標要求を値として保存する。実行時のBody ID／Joint ID、Frame、Impulseは保存しない。ゲーム固有の進行Objectは `GamePayload` に置き、意味と範囲はゲーム側のcodecで検証する。

保存封筒の形式バージョン、ゲームpayloadのバージョン、チェックポイント構造のバージョンは別の値である。現在のチェックポイント構造は `checkpointVersion: 1`。未知項目、重複個体、除去済み個体へのJoint状態、不正な種類別設定、有限上限の超過を拒否する。

この変換だけではSceneの再開は完了しない。`FCheckpointRestore::Build` が許可カタログと照合してCPU再開計画を作り、後続の所有側処理が資源準備、Scene切替、必須Readyの観察を担当する。ゲーム固有の進行検証・初期姿勢レシピを明示してFCheckpointResumeRequestへ渡し、所有側の準備と通常Navigator要求に接続する。

数値の出力は専用のC localeと17桁の表現を使う。保存世代などの厳密整数には引き続き十進文字列を使い、f64へ変換しない。POSIX側の実行はこのWindows環境では未確認。

## 許可カタログからのCPU再開計画

`FCheckpointCatalog2D`／`3D` はゲームが用意した識別条件、初期Scene、初期個体に対応するPrefabキー、許可済み `FCheckpointPrefabRecipe2D`／`3D` を持つ。保存のキーを実ファイルのパスとして扱わない。SceneとPrefabの実読込指紋を厳格に照合する。初期個体は、存在または除去を明示した記録が必要で、欠落を暗黙の除去とは解釈しない。

`FCheckpointRestore::Build` は保存順に新しいScene値を作る。動的個体を含め、接続先を個体名と宣言されたBody export名から解決し直す。保存データの配列位置や旧Body IDを使わない。接続・Drive・Limitは種類を検証して新定義へ反映する。目標操作は `Plan.Targets` に残し、Ready後のControllerへ渡す。未公開の内部Jointは信頼するContentの初期設定を使う。

公開値が許可済み定義と異なるときは、ゲームが明示した `Recipe.Resolve` のCPU処理を使う。結果の指紋・Root・定義パス・全公開値を検査し、既存のContent検証も再実行する。ゲーム処理は既存パーサーへ許可済み定義と型付きoverrideを渡せるが、保存由来の任意パスやC++型名をロードしてはいけない。共有する元定義は変更しない。

Jointの別名exportは状態が一致した場合に一回だけ反映する。別名間の矛盾、未知の状態、種類違いは拒否する。除去個体への接続は解除済みの場合だけ論理履歴として保持し、新しい実体の接続を作らない。接続中または目標操作中なら拒否する。

CPU計画の生成は旧Scene・保存ファイル・Native資源を変更しない。`PrepareScene(Plan.Scene, Assets)` 以降は既存の所有側経路を使う。計画生成や資源準備の成功を「ゲーム再開成功」と表示してはいけない。任意の動作中Poseをそのまま再現する機能ではなく、開いた扉などの姿勢を再開時から一致させるにはゲームが初期PoseとFrameの整合したレシピを用意する。

## 成功した固定更新後の採取境界

`DPhysicsScene2D`／`3D::OnPhysicsStepCompleted` は、World.Stepと全PostPhysics予約が成功した後に一回だけ呼ばれる。ゲーム側の最後の反映とJointの成功観察が揃った境界である。固定更新0回、Pause、固定更新／Pre／Postの失敗、SceneのDestroy要求後には呼ばない。`IsAtPhysicsCompletionBoundary()` はこのフックの実行中だけtrueで、例外時にもfalseへ戻る。

`GetCompletedPhysicsStepCount()` はWorldと全PostPhysicsが成功した境界を数える。完了フック自身が失敗した場合も物理は既に進んでおり、この数は巻き戻さない。採取結果を公開できたこととは別の値である。FCheckpointCapture2D／3Dは、この境界内だけでReadyとJoint保留要求を検査し、要求された文書を一度採取する。ACheckpointScene2D／3Dがこの境界と通常Navigatorへの接続を提供する。Sampleのゲーム固有Controllerと既存F1メニューへ接続している。

## 個体履歴と要求時の採取

`FCheckpointCapture2D`／`3D` はSceneが所有する小さい採取サービスである。新規ゲームでは許可カタログから初期履歴を作り、再開では検証済み文書を渡す。初期化後に `Bind(Scene)` で個体を一度解決する。保持するhandleは世代付きの非所有参照で、同名の新個体を自動検索し直さない。個体の実体所有は既存Collection、Body／Jointの所有はWorldに残す。Scene終了前に `Retire()` を呼び、後続の採取や再Bindを抑止する。Sceneを越えるcallbackやWorkerへのScene捕捉を保持しない。

`Spawn` は準備済みPrefabと、許可カタログで検証した論理記録から動的個体を受付する。履歴とhandleの必要領域は受付前に確保する。初期化とBody登録は既存Applicationの境界に任せ、Spawn成功をReadyと扱わない。`Destroy` は同じ世代のhandleにだけ破棄要求を送り、除去記録を残す。外部から破棄されて履歴と一致しなくなった個体は採取を待機させる。同じ名前が再利用されても古い参照から追従しない。

`RequestCapture()` は次の成功固定更新へ採取要求をまとめる。引数付きの呼出しは、ゲームが明示した安全なCheckpointキーを設定する。ゲーム・Content・Sceneの識別は変更しない。不正キーは旧要求を保持して拒否する。`HasCaptureRequest()` と `GetLastCapture()` は別の値で、正常文書が残っていても新要求が未採取なら完了と表示しない。

Sceneの `OnPhysicsStepCompleted` で、同じ境界のゲームcodecが検証した進行Objectを `CaptureIfRequested` へ渡す。全個体Ready、公開Jointの接続状態、成功観察、`HasPendingRequest()` を確認する。PostPhysicsでDrive等の新要求が出た場合、その要求を反映する次の成功固定更新まで採取を待つ。接続・解除・種類別Drive／Limitは専任Componentの採用済み設定から読み、追加World.Stepを実行しない。採取時の確保やcodec失敗では最後の正常文書を保持し、例外を実行時契約へ伝える。全Bodyのrollbackは行わない。

固定更新0回やPauseでは最後の正常文書を利用できるが、新しい未処理要求をその文書へ混ぜない。要求なしの呼出しはJSON処理や文書複製を行わない。開扉の初期Poseを選ぶゲームレシピと入力設定は下記Sampleで接続している。通常利用のコンパイル・実行例は [外部CheckpointConsumer](../../Tools/PackageConsumer/CheckpointConsumer.cpp) のScene所有・成功境界・退役経路を参照する。


## 所有側のロードと再開完了

`FCheckpointResumeRequest2D`／`3D` は、長期所有者が既存 `FGameSaveRequest`、`FSceneNavigator`、`FAssetService` を借用して使う任意の窓口である。許可カタログと、型付き文書から文書を返す明示的な純粋ゲーム検証・再開レシピを渡す。ゲームの鍵や得点の意味を保存コアに埋め込まない。保存の全構成をゲーム変換の前後で再検証する。任意のC++構築フックでは、準備済み値と進行から `ACheckpointScene2D`／`3D` の派生Sceneを作り、通常のComponent／Controllerを追加できる。ファイル内のC++型名を実行しない。

通常更新の `Poll()` は面読込の採用、CPU計画、所有側の資源準備、`Navigator.RequestChange` まで進める。`Navigator.Commit` は呼ばず、既存Application境界が確定する。ファイル読込は既存Dispatcherへ任意で委譲するが、現在のCPU計画生成とNative資源準備は所有側Pollで行い、ブロックし得る。すべての準備がWorker上で完了するAPIとは扱わない。

候補の受付は `TransitionRequested`。Scene有効化後も `PendingPhysics` とし、全Body・Jointの成功観察と一貫した初回採取を確認してから通常Pollが `Completed` にする。0固定更新やPauseでは再開完了にしない。`FCheckpointSceneTicket` はSceneポインタを含まないCPU観察を保持する。初期化失敗・候補置換・終了は `Ended` となり、未完了の読込をFailedへ確定する。完了済みTicketは後続Sceneの終了で書き換えない。

候補の資源準備失敗では旧Sceneを変更せず、候補のOnInitialize失敗では既存Navigatorが旧Sceneを保持する。切替後の固定更新／PrePhysics／World.Step失敗は既存実行時停止であり、旧Worldへ巻き戻さない。公開前取消しと構築フック後の最新照合を行い、取消し済み値をNavigatorへ渡さない。切替要求済みの取消しはfalseで、所有者退役は未完了状態をFailedへ確定するが、受付済みSceneの巻き戻しや破棄は行わない。依存先はこの窓口より長く生存させる。

`ACheckpointScene` は進行をコンストラクターから保持し、初期イベント消費前にゲームControllerへ渡せる。`OnCheckpointStepCompleted` でゲームのPost反映を採取より先に行い、`SetGamePayload` の不正JSONでは旧進行を保持する。目標付きRevolute／Prismaticは型付きhandleを一度解決し、成功観察から既存 `ComputeJointTargetDrive` を使って次の有限Motorを要求する。初期Poseの直接上書きや追加World.Stepを行わない。Readyは目標到達を意味せず、初期構成の利用可能を意味する。

最小のコンパイル・実行例は [CheckpointConsumer](../../Tools/PackageConsumer/CheckpointConsumer.cpp) を参照する。両次元で `RequestLoad` → 通常 `Poll` → 既存Navigator確定 → 成功固定更新 → `Completed` を確認する。保存用RootをContentのRootとは別引数で渡す。このConsumerの試験は同一プロセスで、下記Sampleの別プロセス試験とは区別する。

## Sampleの再開レシピ

[FSaveCourseRecipes](../../Examples/GameplaySample/SaveCourseRecipes.h) は保存用の別コースを `Assets/SaveCourse` から読み、許可カタログと純粋なゲーム進行変換を提供する。従来のContentコースは変更しない。`Load2D`／`Load3D` が捕捉するのは読込済みの型付き値であり、`Restore` やPrefabレシピでファイルを再読込しない。公開値の型・完全な集合・数値範囲を検査し、元定義の指紋と資源表を保持する。

`NewGame(Catalog)` は初期構成から文書を作り、初期開扉の公開値をゲーム進行の開閉・解錠にも反映する。`Restore(Document)` はゲームpayloadとcheckpoint識別が一致することを確認してから独立した候補を返す。`FCheckpointRestore::Build` へ渡し、既存の `PrepareScene` と `ACheckpointScene2D`／`3D` で採用する。カタログと初期進行を手作業で別々に組み立てる必要はない。

進行の取得済みキーを除去記録へ反映し、プレイヤーを `start`／`north` の安全地点へ配置する。扉の論理開閉は公開bool `open` へ変換し、再開時は停止モードとする。開扉ではleafと固定されたcargoをレール方向へtravelだけ移す。Bodyに取り付けたLocal FrameとColliderは維持するため、Prismaticの観察値は初回の成功固定更新からtravelとなり、Limitは元の `[0, travel]` を保つ。動的荷物の論理記録・生成順・parameterは既存の再開計画が扱う。

[実Application試験](../../Tests/SaveCourseRecipeTests.cpp) では閉扉の対照と片側だけ開いた再開を両次元で比較する。2Dの90度配置、3Dの120度配置、固定荷物、取得済みと未取得の鍵、動的荷物、全4種類の内部Jointを確認する。追加Step・初期化中の音声再生を使わない。

## Sampleの進行Controller

`ASaveCourseScene2D`／`3D` は既存のチェックポイントSceneを使い、Collection所有の `TSaveCourseController` へゲーム判断を委譲する。コンストラクターに準備済みScene、レシピを通した文書、準備済み荷物、現在の独立設定を渡す。`GetProgress` は現在のゲーム値で、永続化済みや採取済みという意味ではない。

Controllerは鍵・安全地点の型付きSensor exportを一度だけ解決する。相手の完全Body IDをプレイヤーと照合し、成功PostPhysics後に取得済み集合・得点・解錠を確定して取得物を破棄する。終了済みの同名個体へ追従しない。安全地点のStayだけではrevisionを更新せず、同じ進行を繰り返し採取しない。進行変更時だけ `SetGamePayload` と `RequestCapture` を使い、親Sceneが同じ境界の個体・Joint状態と合わせて採取する。

`RequestDoor(0, true)` は解錠済みdoorAへ有限Motorの目標を渡し、doorBの設定・IDを変更しない。論理的な開閉は成功観察の位置・速度が目標へ到達した後に確定する。`RequestCargo()` は次の通常固定更新で動的な荷物を一個生成し、既存の論理履歴へ記録する。BodyのPose・速度を直接代入しない。

`FSaveCourseInput::Defaults` と `Policy` がゲームのActionと予約キーを提供する。完全な `ApplySettings(Settings, RawSnapshot)` は既存プロファイルの検証・強い失敗保証・解放後有効化を使う。ActionはUI消費の後、物理計画の前に一回更新する。0固定更新の押下を保留し、複数固定更新でも一回だけ消費する。Modal終了の押し続け、フォーカス喪失、Pad切断では保留操作を抑止する。UIで作られた偽の解放で解除待ちを終了しない。

取得の効果音はScene有効化後の通常更新で既存AudioScopeへ再生し、準備・初期化・描画では再生しない。音量は起動中のPlaybackと次のPlayへ元の倍率から適用する。設定値の保存成功とは別である。UI倍率は既存表示先の描画とhit testへ同じ値で適用し、Save／Continue UIから要求を渡す。固定入力を使った別プロセスのNative画素試験も追加している。人の操作・聴感確認とは区別する。

## Scene切替を越えるSampleの保存窓口

[ASaveCourseGameInstance](../../Examples/GameplaySample/SaveCourseGameInstance.h) はApplicationが所有し、保存サービスとScope付き要求、現在の設定を保持する。完全修飾の専用SaveRootと信頼するContent読込窓口をコンストラクターへ渡す。`RequestNewGame(2)`／`(3)`、`RequestSave("manual1")`、`RequestLoad(2, "manual1")`、`RequestRetry(2)`、`RequestCancel()` を通常更新で反映する。Sceneを第二の管理器で所有せず、既存Navigatorへ候補を渡す。

手動保存は成功固定更新の採取を待ち、Pause中は最後の正常採取値を使用する。`IsCapturing()` はコア書込前の待機で、`GetSaveTicket()` のCommittedやCompletedとは別である。checkpointの新しい進行revisionだけでautosaveを要求し、同じSensorのStayやContinueで同じ保存を繰り返さない。Scene終了前に採取できなければ旧保存を変更しない。同期読込でも受付FrameにはSceneへ採用せず、取消し・後続要求を照合する境界を残す。

`RequestSettings` は完全な設定を次の所有側境界で適用する。`RequestSettingsSave` は進行とは別のsettingsスロットへ要求する。`IsSettingsDirty` は適用した現在設定が書込成功するまでtrueで、古い進行ロードによって現在のキー・音量・UI倍率を巻き戻さない。設定準備失敗では現在値、初回設定読込失敗では明示した既定値と診断を保持する。

[Session試験](../../Tests/SaveCourseSessionTests.cpp) は実Applicationで同期／既存Dispatcherの両経路を通し、Pause保存、異次元失敗時の旧Scene保持、取消し、旧要求の抑止、独立設定、Scene採用と必須Readyの区別を確認する。[別プロセス入口](../../Tests/SaveCourseProcess.cpp) と [実行器](../../Tools/VerifySaveCourseProcesses.py) は同じexeを再配置し、プロセスAで鍵取得・開扉・荷物生成・checkpoint・設定を保存して終了し、別Application／JobSystem／Worldを持つプロセスBでContinueする。日本語・空白の専用SaveRoot、別CWD、exeと原本AssetsのSHA-256を確認する。実BodyのJumpも保存したKへ変わる。描画・外部I/Oの境界は試験実装で、Native画素や人の操作の証拠ではない。

[Controller試験](../../Tests/SaveCourseControllerTests.cpp) は実Application・World・Sensorで鍵取得、解錠、片側開扉、動的生成、成功境界採取、新Sceneでの二重得点抑止を確認する。入力変更も実BodyのImpulseで検証し、Keyboard／Mouse／Pad、0／複数固定更新、Modal閉鎖、切断・フォーカス喪失を含める。接触を作る試験用配置は試験が明示的にTeleportし、製品Controllerの移動方法とは区別する。同一プロセスのScene再開であり、別プロセス試験の代用にはしない。

## 既存Sampleからの操作

通常GameplaySampleは`ASaveCourseGameInstance`をApplicationに渡し、ユーザー保存領域を`FSaveStore::DefaultRoot("dxfGameplaySample")`で一度確定する。起動直後のキャラクターコースでIを押して相互作用コースへ進み、F1メニューから保存コースのNew／Continueを選ぶ。初期のC++コースとF2のContent読込を保持する。保存コースのF1にはNew 2D／3D、Continue、手動3系列、Save／Load、Checkpoint Retry、公開前Cancel、設定保存、公開結果未確定の明示再確認がある。従来C++コースへ戻る操作も通常Navigatorを使う。

Scene所有の`FSaveCoursePanel`は要求と表示だけを担当する。手動上書きは既存Modalで対象系列を確認し、取消しでは保存しない。空・正常・破損・非互換・保存中・前の正常保存からの回復を表示する。一覧は初回、要求の終端、利用者の明示操作だけで更新し、通常の描画やTickで全ファイルを読み直さない。公開結果未確定を保存成功／Canceledと表示せず、同じトランザクションの再確認を要求できる。

Keyboard／Mouse／4Padの再割当は既存採取と完全プロファイル検証を使い、開始クリック・競合・予約入力・閉鎖Frameの押し続けを抑止する。音量とUI倍率は適用済みと保存済みを別表示する。倍率はドラッグ終了時に適用し、操作途中でつまみの座標を動かさない。ロード候補の構築後に設定を変更した場合も、新Scene採用後の入力更新前に最新番号を照合し、独立設定を反映する。

[UI試験](../../Tests/SaveCourseUiTests.cpp) は既存UIの実際の描画位置をクリックし、1001×501・倍率1.5・スクロール・1／2View、上書き取消し、割当変更後の実Body、設定保存、動的個体の生成・破棄、ロード時の入力抑止を両次元で確認する。文字描画と資源境界は試験実装であり、Native画素・人の操作・聴感の証拠ではない。

## 実DxLibでの別プロセス確認

既存の[NativeGameplaySmoke](../../Tests/GameplaySampleSmoke/SaveCourseSmoke.cpp)へ保存・読込の限定入口を追加した。[実行器](../../Tools/VerifyNativeSaveProcesses.py)が同じexeを再配置し、2D／3Dそれぞれで保存するプロセスとContinueするプロセスを直列起動する。元のAssetsとexeのSHA-256を前後照合し、日本語・空白の専用SaveRootと無関係な作業ディレクトリを使う。

保存する起動では初期構成、鍵を一つ取得して扉を開き荷物を一つ生成した保存状態、その後にもう一つ鍵を取得し別の扉を開いた現在状態を別々に確認する。Continueする起動では保存時の得点10・取得済み一件・荷物一個・開扉したdoorAと閉じたdoorBに戻り、Spaceでは跳ばず、保存したKで実Bodyが跳ぶ。進行と設定は別系列である。

画素は扉二個、取得物二個の存在／除去、動的荷物、2D画像／3Dモデルの領域から、一画像につき一回のGPU読戻しで照合する。保存前・保存時・変更後・Continue後に1画面、2画面、1001×501を使い、24画像を確認する。さらに両次元のContinue後に保存倍率1.5のメニューを通常寸法と1001×501で表示し、Buttonの背景色、実クリック、音量・UI倍率のSlider操作を4画像で確認する。内部の実行器は合計28画像と倍率の診断を必須とする。文字HUDだけで成功にはしない。画素条件、投影範囲外の拒否、読戻し前後の物理状態不変の検査を保持する。

保存コースの2D縮尺は定義値を上限とし、表示先の幅に16mのコースと40画素の余白が収まる値を使う。3Dで定義が一つのカメラを持つ場合は、分割した両表示先でそのカメラを使う。二つのカメラが明示された定義では各カメラを使う。いずれも物理座標・Step回数を変えない。

この試験は実Application・World・AssetService・DxLibと固定Snapshotによる確認で、人の物理入力、聴感、SDKなしPC、電源断の検証ではない。installの公開targetだけを使う外部ゲームConsumerの確認は別工程である。

保存I/Oの故障検査は[専用試験](../../Tests/SaveIoFaultTests.cpp)を参照する。新規一時ファイルのwrite／flush／close／読戻し、公開前後のOS失敗、公開済みか確認できない状態の明示再確認を分ける。正常な最新面の全生バイトを保護し、失敗時に所有していない既存tempを削除しない。checksumは正常なJSONの値が変わった場合も検査し、JSON構文だけで完全性を代用しない。


## 公開配布targetからの別起動

[外部ゲーム処理](../../Tools/PackageConsumer/CheckpointProcess.cpp)は`dxf::game_save`と`dxf::input_profiles`をリンクし、既存NavigatorまたはApplicationを借りる。`RunCheckpointProcess`のwriteは通常成功境界で採取して進行と独立設定を保存し、readは新Sceneの必須Ready後に再開完了を確認する。OS公開処理・終了順・旧実行時ID復活をゲーム側に追加しない。許可したspeedのレシピは準備済み不変定義のコピーだけを変更し、再開時にディスクを読み直して元の指紋を置き換えない。

既存`Consumer write|read 2|3 <ProjectRoot> <SaveRoot>`はCPUの実Scene／World、`NativeApp`の同じ引数は実Applicationと[実画素確認](../../Tools/PackageConsumer/NativeCheckpointProcess.cpp)を使う。定義はData/、資源はAssets/、保存は別Rootへ置く。[配布実行器](../../Tools/VerifyCheckpointConsumers.py)は四つの別プロセス、同一exeのSHA-256と24枚の必要画像を確認する。実際の物体・資源の画素判定はC++入口が行い、ファイルの存在だけを描画成功にしない。これらの例はinstallからコンパイル・実行する外部Consumerでもある。
