# 保存コアとチェックポイント再開

L0〜L10の実装を進めている。現在利用できるのは任意リンクの`dxf::save_data`と、`dxf::game_save`のCPU保存／読込要求である。SceneContentからチェックポイントを再構築するadapter、入力設定、GameplaySampleのContinueはまだ接続していない。初期構成を読み込むBの成果と、今回の論理進行保存を別の機能として扱う。

## 保存する値

`FSaveDocument`は形式バージョン、文書種類、ゲーム識別子、ゲームpayloadバージョン、Content revision、Scene／checkpoint論理キー、次元、厳密な64bit世代、ゲームcodecのJSON Objectを持つ。保存コアはScene、Physics、Nativeを知らない。payloadの個体・仕掛け・進行の意味と移行を検証する責務は上位codecへ残す。任意C++構造体のメモリや実行時IDを保存する入口ではない。

進行と設定には異なる文書種類とslot系列を使う。同じ系列に別種類を書き込む要求は拒否する。設定の封筒はdimension=0、Content revision／Scene／checkpointは空である。進行はdimension=2または3、許可カタログで解決するASCIIの論理キーを要求する。

UTF-8、復号後の重複キー、非有限数値、末尾データ、未知field、未知の形式バージョンを既存Toolbox JSON readerで拒否する。既定上限は8MiB、深度64、文字列64KiB、値128K、論理キー512bytes。世代は符号・先頭ゼロなしの十進文字列であり、f64を経由しない。Writerは復号済み辞書キーをUTF-8バイト順に並べ、配列順と数値の元の検証済み字句を保持する。したがって1と1.0の字句まで同一へ統一する正準化ではない。

`RequireCompatible`はゲーム／Content／Scene／次元／payloadバージョンを厳格に照合する。revisionは呼出し側の明示値であり、現在の保存コアが資源のバイト一致や準備済みContentの指紋を自動採取したという保証はない。準備に使った定義バイトの指紋保持と型付きpayload移行は後続の再開adapterで実装する。

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

公開前のCancelは確定面を変更しない。公開を試みた後の成否不明は`PublicationUnknown`で、Canceledとは表示しない。トランザクションが保持する排他のもとで同じPublishを明示再試行し、実面を検証できる。公開確認後のCancelはfalse。最新正常面の保持と全ファイルシステム・実電源断の耐久性は別であり、後者は証明していない。

取消しの明示操作は自分が作ったtempだけを後始末する。トランザクション破棄は確保なしで排他を解放し、未公開tempが残る場合もロード候補にしない。任意フォルダーの再帰削除を行わない。

OS処理の契約は[Known Folder取得](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shgetknownfolderpath)、[flush](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers)、[名前公開](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw)、[排他共有](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew)を確認した。Windows.hはToolbox privateに限定し、Native OFFでも利用できる。

## 非同期要求の現在の範囲

呼出し側が`FGameSaveRequest`を所有し、既存Dispatcher／Scopeを任意で借りる。RequestSaveへ渡す文書は、安全な採取境界で既に完成したCPU値である。安全境界の参加者採取は未接続。Workerはencode／temp I/Oまたは面読込／互換性検査だけを行う。Task CommitはCPU結果に印を付け、ファイル公開やNavigator.Commitを実行しない。通常更新のPollが保存を公開し、または読込値を採用する。

状態はPending、Writing／Reading、Prepared、Publishing、Committed、PublicationUnknown、TransitionRequested、Completed、Failed、Canceled、Supersededを区別する。未公開手動保存がある間の次要求はBusy。先行読込は後続でSupersededにし、最後に採用した正常値は新要求の失敗でも残す。Preparedはゲーム再開完了ではない。現在のMarkTransitionRequested／MarkResumeCompletedは上位adapterが実際のNavigator要求と必須Readyを確認して呼ぶ入口で、保存コアの試験だけでScene再開を確認したとは扱わない。

操作・Poll・終了は構築した所有スレッドのTask／Commit外で行う。退役は対象Scopeだけを待ち、兄弟やRoot全体を待たない。公開の同期処理やファイルI/Oはブロックし得る。完全非ブロックの契約ではない。

`PublicationUnknown`になった要求は、その後のPollで自動再試行しない。`RetryPublication`で同じ保存の再確認を明示要求し、次のPollで確定面を検証する。失敗したI/Oを画面更新ごとに繰り返さない。
