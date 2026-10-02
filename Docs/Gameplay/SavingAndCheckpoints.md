# 保存コアとチェックポイント再開

L0〜L10の実装を進めている。任意リンクの`dxf::save_data`は保存コア、`dxf::game_save`はCPU保存／読込要求と型付きチェックポイント・再開計画を提供する。実際のNavigator要求と必須Readyの完了管理、入力設定、GameplaySampleのContinueはまだ接続していない。初期構成を読み込むBの成果と、今回の論理進行保存を別の機能として扱う。

## 保存する値

`FSaveDocument`は形式バージョン、文書種類、ゲーム識別子、ゲームpayloadバージョン、Content revision、Scene／checkpoint論理キー、次元、厳密な64bit世代、ゲームcodecのJSON Objectを持つ。保存コアはScene、Physics、Nativeを知らない。payloadの個体・仕掛け・進行の意味と移行を検証する責務は上位codecへ残す。任意C++構造体のメモリや実行時IDを保存する入口ではない。

進行と設定には異なる文書種類とslot系列を使う。同じ系列に別種類を書き込む要求は拒否する。設定の封筒はdimension=0、Content revision／Scene／checkpointは空である。進行はdimension=2または3、許可カタログで解決するASCIIの論理キーを要求する。

UTF-8、復号後の重複キー、非有限数値、末尾データ、未知field、未知の形式バージョンを既存Toolbox JSON readerで拒否する。既定上限は8MiB、深度64、文字列64KiB、値128K、論理キー512bytes。世代は符号・先頭ゼロなしの十進文字列であり、f64を経由しない。Writerは復号済み辞書キーをUTF-8バイト順に並べ、配列順と数値の元の検証済み字句を保持する。したがって1と1.0の字句まで同一へ統一する正準化ではない。

`RequireCompatible`はゲーム／Content／Scene／次元／payloadバージョンを厳格に照合する。revisionは呼出し側の明示値であり、保存コアが資源のバイト一致を自動採取したという保証はない。SceneContentのファイル準備結果には、実際に読んだ定義バイトの`ContentFingerprint`がある。再開adapterでこの値とアセットbundle revisionを照合する接続はまだ実装中である。

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

呼出し側が`FGameSaveRequest`を所有し、既存Dispatcher／Scopeを任意で借りる。RequestSaveへ渡す文書は、安全な採取境界で既に完成したCPU値である。Scene所有のFCheckpointCaptureによる要求時採取を追加した。非同期要求とのゲーム側接続は実装中。Workerはencode／temp I/Oまたは面読込／互換性検査だけを行う。Task CommitはCPU結果に印を付け、ファイル公開やNavigator.Commitを実行しない。通常更新のPollが保存を公開し、または読込値を採用する。

状態はPending、Writing／Reading、Prepared、Publishing、Committed、PublicationUnknown、TransitionRequested、Completed、Failed、Canceled、Supersededを区別する。未公開手動保存がある間の次要求はBusy。先行読込は後続でSupersededにし、最後に採用した正常値は新要求の失敗でも残す。Preparedはゲーム再開完了ではない。現在のMarkTransitionRequested／MarkResumeCompletedは上位adapterが実際のNavigator要求と必須Readyを確認して呼ぶ入口で、保存コアの試験だけでScene再開を確認したとは扱わない。

操作・Poll・終了は構築した所有スレッドのTask／Commit外で行う。退役は対象Scopeだけを待ち、兄弟やRoot全体を待たない。公開の同期処理やファイルI/Oはブロックし得る。完全非ブロックの契約ではない。

`PublicationUnknown`になった要求は、その後のPollで自動再試行しない。`RetryPublication`で同じ保存の再確認を明示要求し、次のPollで確定面を検証する。失敗したI/Oを画面更新ごとに繰り返さない。
## 論理チェックポイント文書

`FCheckpointDocument2D`／`FCheckpointDocument3D` と `FCheckpointCodec` は、個体キー、許可Prefabキー、準備済み定義の指紋、配置、型付きparameter、存在／除去、生成順、4種類のJointの接続・Drive・Limit・目標要求を値として保存する。実行時のBody ID／Joint ID、Frame、Impulseは保存しない。ゲーム固有の進行Objectは `GamePayload` に置き、意味と範囲はゲーム側のcodecで検証する。

保存封筒の形式バージョン、ゲームpayloadのバージョン、チェックポイント構造のバージョンは別の値である。現在のチェックポイント構造は `checkpointVersion: 1`。未知項目、重複個体、除去済み個体へのJoint状態、不正な種類別設定、有限上限の超過を拒否する。

この変換だけではSceneの再開は完了しない。`FCheckpointRestore::Build` が許可カタログと照合してCPU再開計画を作り、後続の所有側処理が資源準備、Scene切替、必須Readyの観察を担当する。ゲーム固有の進行検証・初期姿勢レシピと、この所有側接続は実装中である。

数値の出力は専用のC localeと17桁の表現を使う。保存世代などの厳密整数には引き続き十進文字列を使い、f64へ変換しない。POSIX側の実行はこのWindows環境では未確認。

## 許可カタログからのCPU再開計画

`FCheckpointCatalog2D`／`3D` はゲームが用意した識別条件、初期Scene、初期個体に対応するPrefabキー、許可済み `FCheckpointPrefabRecipe2D`／`3D` を持つ。保存のキーを実ファイルのパスとして扱わない。SceneとPrefabの実読込指紋を厳格に照合する。初期個体は、存在または除去を明示した記録が必要で、欠落を暗黙の除去とは解釈しない。

`FCheckpointRestore::Build` は保存順に新しいScene値を作る。動的個体を含め、接続先を個体名と宣言されたBody export名から解決し直す。保存データの配列位置や旧Body IDを使わない。接続・Drive・Limitは種類を検証して新定義へ反映する。目標操作は `Plan.Targets` に残し、Ready後のControllerへ渡す。未公開の内部Jointは信頼するContentの初期設定を使う。

公開値が許可済み定義と異なるときは、ゲームが明示した `Recipe.Resolve` のCPU処理を使う。結果の指紋・Root・定義パス・全公開値を検査し、既存のContent検証も再実行する。ゲーム処理は既存パーサーへ許可済み定義と型付きoverrideを渡せるが、保存由来の任意パスやC++型名をロードしてはいけない。共有する元定義は変更しない。

Jointの別名exportは状態が一致した場合に一回だけ反映する。別名間の矛盾、未知の状態、種類違いは拒否する。除去個体への接続は解除済みの場合だけ論理履歴として保持し、新しい実体の接続を作らない。接続中または目標操作中なら拒否する。

CPU計画の生成は旧Scene・保存ファイル・Native資源を変更しない。`PrepareScene(Plan.Scene, Assets)` 以降は既存の所有側経路を使う。計画生成や資源準備の成功を「ゲーム再開成功」と表示してはいけない。任意の動作中Poseをそのまま再現する機能ではなく、開いた扉などの姿勢を再開時から一致させるにはゲームが初期PoseとFrameの整合したレシピを用意する。

## 成功した固定更新後の採取境界

`DPhysicsScene2D`／`3D::OnPhysicsStepCompleted` は、World.Stepと全PostPhysics予約が成功した後に一回だけ呼ばれる。ゲーム側の最後の反映とJointの成功観察が揃った境界である。固定更新0回、Pause、固定更新／Pre／Postの失敗、SceneのDestroy要求後には呼ばない。`IsAtPhysicsCompletionBoundary()` はこのフックの実行中だけtrueで、例外時にもfalseへ戻る。

`GetCompletedPhysicsStepCount()` はWorldと全PostPhysicsが成功した境界を数える。完了フック自身が失敗した場合も物理は既に進んでおり、この数は巻き戻さない。採取結果を公開できたこととは別の値である。FCheckpointCapture2D／3Dは、この境界内だけでReadyとJoint保留要求を検査し、要求された文書を一度採取する。ゲーム固有の参加者と通常Navigatorへの接続は、引き続き実装中。

## 個体履歴と要求時の採取

`FCheckpointCapture2D`／`3D` はSceneが所有する小さい採取サービスである。新規ゲームでは許可カタログから初期履歴を作り、再開では検証済み文書を渡す。初期化後に `Bind(Scene)` で個体を一度解決する。保持するhandleは世代付きの非所有参照で、同名の新個体を自動検索し直さない。個体の実体所有は既存Collection、Body／Jointの所有はWorldに残す。Scene終了前に `Retire()` を呼び、後続の採取や再Bindを抑止する。Sceneを越えるcallbackやWorkerへのScene捕捉を保持しない。

`Spawn` は準備済みPrefabと、許可カタログで検証した論理記録から動的個体を受付する。履歴とhandleの必要領域は受付前に確保する。初期化とBody登録は既存Applicationの境界に任せ、Spawn成功をReadyと扱わない。`Destroy` は同じ世代のhandleにだけ破棄要求を送り、除去記録を残す。外部から破棄されて履歴と一致しなくなった個体は採取を待機させる。同じ名前が再利用されても古い参照から追従しない。

`RequestCapture()` は次の成功固定更新へ採取要求をまとめる。引数付きの呼出しは、ゲームが明示した安全なCheckpointキーを設定する。ゲーム・Content・Sceneの識別は変更しない。不正キーは旧要求を保持して拒否する。`HasCaptureRequest()` と `GetLastCapture()` は別の値で、正常文書が残っていても新要求が未採取なら完了と表示しない。

Sceneの `OnPhysicsStepCompleted` で、同じ境界のゲームcodecが検証した進行Objectを `CaptureIfRequested` へ渡す。全個体Ready、公開Jointの接続状態、成功観察、`HasPendingRequest()` を確認する。PostPhysicsでDrive等の新要求が出た場合、その要求を反映する次の成功固定更新まで採取を待つ。接続・解除・種類別Drive／Limitは専任Componentの採用済み設定から読み、追加World.Stepを実行しない。採取時の確保やcodec失敗では最後の正常文書を保持し、例外を実行時契約へ伝える。全Bodyのrollbackは行わない。

固定更新0回やPauseでは最後の正常文書を利用できるが、新しい未処理要求をその文書へ混ぜない。要求なしの呼出しはJSON処理や文書複製を行わない。目標操作や開いた扉の初期Poseを選ぶゲームレシピ、入力設定、SampleのSave／Continue UIは追加中である。通常利用のコンパイル・実行例は [外部CheckpointConsumer](../../Tools/PackageConsumer/CheckpointConsumer.cpp) のScene所有・成功境界・退役経路を参照する。
