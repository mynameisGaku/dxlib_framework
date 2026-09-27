# 2D／3D World相互作用の引継ぎ・検証（2026-09-27）

## 作業対象

`dxlib_framework_Claude_dedab2f_2D3D_WorldInteraction_Directive.md` の R0〜R7 を継続した記録です。引継ぎ時の main／origin/main は `c036dc8aaab0b3d6eaca899d7c01fc41eb7f5fa7`。Claude側の自動再開を利用者の指示で無効にし、並行編集を止めてから作業しました。

R0 `d913bb8`、R1 `ea60f46`、R2 `9664048`、R3 `4760c3c`、R4 `2ea0686`、R5 `c036dc8` は引継ぎ前のコミットです。過去の実行件数を今回の成功へ転記していません。未コミットの CMakeLists.txt と Interaction の19ファイルは、`Build/InteractionTakeover-3d9b21fc/initial/` と `initial.json` に実バイト・SHA-256を保存して継続しました。別ブランチ・worktree・reset・clean・stash・旧適用器は使用していません。

環境は同じWindows PC、Visual Studio 18 2026 x64、MSVC 14.51.36231、既存 `ThirdParty/DxLib-3.25a-source`（独自モデル拡張、静的CRT）です。SDK未導入PCの検証ではありません。通常のStarterとSandbox、ufbx・モデル・Viewportの既存機能を保持しています。

## 実装と実証した修正

| 対象 | 修正と根拠 |
| --- | --- |
| Trigger再同期 | 観測設定変更後の完全な集合でBody単位の占有を組み直す。残存BodyへEnterを重複通知せず、観測中断中の消失はObservationResetで退場。Red 6件を確認 |
| 通知Handlerの寿命 | Handler設定時に共有所有し、実行中の自己解除・置換でも捕捉値を生存させる。毎通知の確保は不要。Red 4件を確認 |
| 回転床の離地速度 | 弦の平均速度から、固定更新開始時の支持点の `v + ω × r` へ修正。2D／3DのRed各1件 |
| 回転床の通過防止 | 弧を最大32区間に分け、保守的に太くした円／球で各経路を検査。端点間の直線だけでは漏れる障害物をRed各1件で確認。無制限の回転CCDではない |
| イベント候補の一時確保 | World所有の予約済み境界をHeapSortし、候補配列を作らず走査。真の組はPublishで正準順へ整列。SolverのBroadPhaseは変更しない。確保故障回帰でRed 14件→Green |
| 箱のContact表示 | サンプルの観測距離0.01mがSkinWidth 0.02mより小さいため通知が来なかった。サンプルだけ0.025mへ。Solverの許容値は変更していない |
| 設定Modalの閉じるフレーム | 同時に押されたSpace／EscapeもModalが処理してから閉じる。閉じた直後の偽ジャンプ・終了をRed各次元で確認 |
| 奇数幅の2分割 | 1281pxの描画先で丸めによる隙間を作らず、両端を整数分割する。2D／3DでRedを確認 |
| 公開ヘッダー | TriggerVolumeComponentの単独includeで不足していたBodyType、ColliderResponse、WorldEventを直接include。53翻訳単位の検査で検出 |
| 3D危険領域の表示 | 実Sensorは存在するのに描画がなかった。Sensorと同じ中心・半幅の赤い箱を描く。実画素Red（0,0,0）→Green（110,29,29）を確認。既存のプレイヤー・床の閾値は維持 |

既存18件のWorldイベント試験は件数を増やさず、解析した全pairの完全ID・遷移・理由・順序・余剰欠落、独立World間の全法線成分、観測ON／OFFの角速度・StepIndexを補強しました。イベント候補はQueryIndexと別の走査であり、QueryIndexのON／OFF比較を「イベント索引の比較」と呼んでいません。解析60球の系列はZ=0の配置です。

## ゲームの利用経路

既存GameplaySampleの **I** から2D／3Dの相互作用コースへ入ります。取得物・箱・圧力板・扉・横移動床・昇降床・回転床・チェックポイント・危険領域・HUDは同じライブラリを通常アプリ、CPUの実Application試験、既存NativeGameplaySmokeが使います。設定UIは既存のUI Sceneアダプターと入力配送を再利用しています。新しい公開Managerや常設サンプルは追加していません。

自動実機試験は2D／3D×1／2Viewで各320フレームの固定入力を通し、ゲーム状態・イベント列と一括読戻しによる画素を別々に検証します。仕掛けの入口へ公開Teleportする試験操作を含み、物理キーのみの連続完走ではありません。UI有無・リサイズ・DPI・奇数幅・Modal入力のCPU試験は、実Applicationと正規Scene／ゲームコード、代替描画Backendを使う契約試験です。実DxLibでOSのDPI変更を行った証拠ではありません。

## 試行と最終検証

各試行は `Build/InteractionTakeover-3d9b21fc/` 以下の新規runに保存しています。`runs/` は個別のRed／Green、`mutations/` は変異、`final/` は全構成の最終実行です。各Summaryには実コマンド、cwd、終了コード、時刻、HEAD、実バイトのソースmanifestがあります。ビルド失敗後の古い実行ファイルは最終結果に使いません。

### 最終回帰（HEAD `b6d1573`、直列）

コードは `b6d1573` でコミット済み（文書だけ未コミット）。runの `source_sha256` は全runで同じ `748f01bd…`（`SourceManifest.txt` に実バイトの一覧）で、実行の前後で変化なし。

| 工程 | 結果 | run |
| --- | --- | --- |
| root Debug（構成・全ビルド・登録・CTest `-j 1 -V`） | **33/33**（登録33＝従来32＋InteractionSample）、終了0 | `final/root-Debug/20260927T095735Z-7d207cc6` |
| root Release（同） | **33/33**、終了0 | `final/root-Release/20260927T095857Z-f737e6ed` |
| `Tools/ValidateDebug.py`（SDKなしの単独構成） | Debug 27/27、Release 27/27、終了0 | `final/standalone-Debug/20260927T100024Z-b7fec6df` |
| 配布 Portable Debug／Release | 各10工程成功 | `final/package-Debug/…100155Z`、`final/package-Release/…100207Z` |
| 配布 Native Debug／Release（`--run-device`） | 各13工程成功。NativeUiAppのOS捕捉は2構成×2起動とも `verified` | `final/package-Debug-native/…100222Z`、`final/package-Release-native/…100251Z` |
| No-STL | 621ファイル、違反0 | — |
| Python（条件なし／Development生成・UIビルドの実生成物あり） | 56件成功（skip 3）／56件成功（skip 0） | `claude-resume/python-conditional.log` |
| 通常／Developmentのソリューション生成 | 終了0／終了0。IDEの表示の検査（生成物の読取りだけ）は指摘0 | `ide-run/20260927T100812Z-c79c944f` |
| 変更した公開ヘッダーの単独TU（正逆のinclude順・2D3D併用） | 53単位成功（警告168件、エラー0。`/WX`なしの検査） | `headers-run/…/attempts/20260927T100844Z-1958ba16` |

実機の確認の記録（CTestの `-V` の出力から抽出）: Debug・Releaseとも `window_foreground` 5件と `os_pointer_capture`（Resizable・Stretch）2件がすべて `verified`。`ProcessMessage=-1` は両構成で0件（再発なし。原因未特定のまま）。

注意: `ctest -V` は試験の出力の各行へ「番号: 」を付けるため、この回帰の `Summary.json` の `checks` は空で、上の数は生ログから数えた値です。この取りこぼしは回帰の後に `Tools/ValidationSupport.py` の読取りを直し（Python試験で確認）、コードの回帰（root・ValidateDebug・配布）はその修正の前の `b6d1573` で実行しています。

変異試験は、引継ぎ後に10種類の違反を狙った14件をすべて検出した結果（`mutations/`）で、その後に変更したのは3D危険領域の描画とUIの捕捉の判定だけです（Physicsの変異の対象コードは同じ）。UIの捕捉の修正は下のとおり修正前のRedと修正後のGreenを確認しています。

### 既存UI実機試験の起動条件

最初のroot全群はDebug／Releaseとも32/33、CTest終了8でした。両方とも既存NativeUiDeviceSmokeの `UI control unavailable` だけが失敗し、モデル・追加したGameplay・その他32群は成功しています。この二つの全群を、後の単独試験と合算して成功とは扱いません。

検索名・フレーム・Scene・DxLib active・Win32 foreground/capture・UI内の捕捉を記録すると、Start2Dの押下前から `active=1 foreground=0 capture=0`、押下後はUI内でStart2Dを捕捉してもOS捕捉は0でした。離すフレームでUI捕捉が解除され、SceneはTitleのままです。実フォント・Titleの画素は成功しており、描画失敗でも過去の `ProcessMessage=-1` でもありません。

Nativeの捕捉は自ウィンドウが前面の時だけ取得する既存契約です。固定入力の試験はこの起動前提を確認していませんでした。試験側だけに、自プロセスのウィンドウを起動時に一度だけ前面へ要求し、戻り値と実際の前面一致を検査する準備を追加しました。通常ゲームのfocus/capture契約・入力列・Step回数・画素条件は変更していません。配布側NativeUiAppにも同じ前提検査を追加しています。

修正後の単独試行では `accepted=0 foreground=0` でWindowsに拒否され、明確な `UI startup foreground request denied` として停止しました。OSの制限を迂回するキー注入、固定Sleep、成功までの再試行は行っていません。前面化は常に許可される操作ではありません（[Microsoft SetForegroundWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow)）。成功経路の実機確認とOS側の前面拒否は区別します。

**原因の訂正（再開後、`16475df`）:** 押下が取り消された原因は前面化の不足ではなく、UIの入力の仲介の判定でした。「捕捉を求めたのに今は持っていない」ことを、一度も取得していない場合も「喪失」とみなして押下を取り消していました（前回のP5の実装）。前面でないウィンドウではOSの捕捉は取得できないため、固定入力でも、非アクティブなウィンドウへの実際のクリックでも決定できませんでした。求めている間に一度取得した捕捉を失った場合だけ取り消すよう直し、UiRuntimeに「取得しなかった捕捉では押下を取り消さない」試験を追加しました（旧判定へ戻すとRed、戻すとGreen、原本のハッシュ一致を確認）。

これに合わせ、実機のUIの試験と配布のNativeUiAppの「前面でなければ失敗」を取りやめ、起動時に一度だけ前面を要求して、前面ならOS捕捉まで確かめ、拒否されたら `DXF_CHECK …=not_exercised` を記録して続ける形に戻しました（前面でないことだけでUIの操作そのものを失敗にしない）。最終回帰の実行では前面化は許可され、OS捕捉は `verified` でした。

また、計測専用CMake生成の最初の試行は、PowerShellへ渡したSDKパスの引用不足で失敗しました。失敗のSummary更新時には `Summary.json.tmp` の置換でWinError 5も発生しています。最初のCMake終了1の生ログと `FailureNote.json` を保存し、旧Summaryを成功へ書き換えていません。引用を直した別runは終了0です。WinError 5の根本原因は特定していません。

## 測定の条件

`dxf_character_benchmark interaction` はRelease、2D／3D×Collider 64／512／1024×動く当事者1／16／64×7系列、計126行です。初回と30Stepの慣らしを分け、120Stepを5区間測定します。5区間は同じWorldを継続するもので、独立した5プロセスではありません。時間は合否の閾値に使いません。

引継ぎ後の予備測定では、N=1024／M=64の毎Step確保がSensor OFF 0、Sensor ON 1060、移動床ON 1025、密集Sensor 64575でした。これがイベント候補生成と並べ替えの一時確保を修正した根拠です。予備測定中は別のビルドも行っていたため、時間値を最終比較へ使いません。

### 最終の測定（`b6d1573`、Release）

通常版 `final/benchmark-Debug/20260927T100353Z-e09cfa50` と計測版 `final/benchmark-Debug/20260927T100511Z-4273fa06`（フォルダー名のDebugはrunnerの既定値の名前で、実行したのはどちらもReleaseの実行ファイル。計測版は `DXF_INTERACTION_BENCHMARK_PROBES=ON` の専用生成）。両方とも126条件・62列、全数値が有限、条件の重複・欠落なし、実行の前後で実行ファイルとソースのハッシュが不変。19列の数・確保の比較で両版の差0件、計測版のイベントの内部区間（候補・詳細・差分・追従）の確保は全条件で0件、容量超過0・追従の停止0（解析：`final/resumed/benchmark-final-analysis.json`）。

Collider 1024・当事者64（静止床は移動Body 0・キャラクター64）の通常版。時間はμs／Stepで5区間の中央値、その他はStepあたり。時間は合否の閾値に使っていません。

| 次元 | 系列 | World μs | World 確保/Step | 配送 μs | StepCharacter μs | Prepare 確保/Step | 組数/Step | 配送数/Step | 追従数/Step |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 2D | sensor-off | 566.206667 | 0.000000 | 0.000000 | 0.000000 | 0.000000 | NA | 0.000000 | 0.000000 |
| 3D | sensor-off | 675.580833 | 0.000000 | 0.000000 | 0.000000 | 0.000000 | NA | 0.000000 | 0.000000 |
| 2D | sensor-on | 638.338333 | 0.000000 | 1.668333 | 0.000000 | 1.000000 | 32.000000 | 64.000000 | 0.000000 |
| 3D | sensor-on | 802.031667 | 0.000000 | 1.883333 | 0.000000 | 1.000000 | 32.000000 | 64.000000 | 0.000000 |
| 2D | solid-only | 1739.053333 | 69.000000 | 1.931667 | 0.000000 | 1.000000 | 32.000000 | 64.000000 | 0.000000 |
| 3D | solid-only | 7606.646667 | 69.000000 | 2.340833 | 0.000000 | 1.000000 | 32.000000 | 64.000000 | 0.000000 |
| 2D | sensor-dense | 9845.020833 | 0.000000 | 2862.426667 | 0.000000 | 1.000000 | 63456.000000 | 65472.000000 | 0.000000 |
| 3D | sensor-dense | 15794.031667 | 0.000000 | 3267.694167 | 0.000000 | 1.000000 | 63456.000000 | 65472.000000 | 0.000000 |
| 2D | static-floor | 584.533333 | 0.000000 | 0.000000 | 71.271667 | 0.000000 | NA | 0.000000 | 0.000000 |
| 3D | static-floor | 694.880833 | 0.000000 | 0.000000 | 128.208333 | 0.000000 | NA | 0.000000 | 0.000000 |
| 2D | moving-floor-off | 720.220833 | 0.000000 | 0.000000 | 126.516667 | 0.000000 | NA | 0.000000 | 64.000000 |
| 3D | moving-floor-off | 700.245833 | 0.000000 | 0.000000 | 162.938333 | 0.000000 | NA | 0.000000 | 64.000000 |
| 2D | moving-floor-on | 642.675000 | 0.000000 | 0.108333 | 99.923333 | 1.000000 | 0.000000 | 0.000000 | 64.000000 |
| 3D | moving-floor-on | 1008.221667 | 0.000000 | 0.189167 | 182.216667 | 1.000000 | 0.000000 | 0.000000 | 64.000000 |

- Sensor・床・密集のWorld.Stepは確保0。Solidの系列の69件／Stepは既存のSolverを含むWorld.Step全体の値で、今回の変更の範囲外。
- イベントを有効にした系列のPrepare（速度の設定と配送の予約）に1件／Stepの確保が残る。したがって固定更新全体を「確保0」とは報告しない。
- 密集Sensorは63,456組・65,472通知／Stepを保持し、容量超過0。

## 再実行

通常／Developmentの生成、root Debug／Release全群、`Tools/ValidateDebug.py`、No-STL、Python、配布を実行します。実デバイス試験は構成間も直列です。詳細は [Testing](../Testing.md)、[計測手順](../../Tools/CharacterBenchmark/Interaction.md)、[外部利用](../../Tools/PackageConsumer/README.md) を参照してください。明示した `--logs` は新規または空のrunディレクトリを指定し、過去runを再利用しません。

```powershell
cmake --build Build/FbxContinuation --config Debug --parallel 4
ctest --test-dir Build/FbxContinuation -C Debug -j 1 -V --no-tests=error
cmake --build Build/FbxContinuation --config Release --parallel 4
ctest --test-dir Build/FbxContinuation -C Release -j 1 -V --no-tests=error
python Tools/ValidateDebug.py
python Tools/CheckNoStl.py
python -m unittest discover -s Tools/Tests -v
# Visual Studio x64開発者環境で、各構成を別runとして実行する。
python Tools/ValidatePackage.py --config Debug
python Tools/ValidatePackage.py --config Release
python Tools/ValidatePackage.py --config Debug --native --sdk-root ThirdParty/DxLib-3.25a-source --run-device
python Tools/ValidatePackage.py --config Release --native --sdk-root ThirdParty/DxLib-3.25a-source --run-device
```

## 保持する未解決・対象外

- 過去の `ProcessMessage=-1` の原因は未特定。今回の成功で解消と扱わない。
- 既存UIの3Dパネル4系列の2割当／フレームは別の未解決項目。「漏れ65」は初回割当と反復非増加の観測であり、製品リーク修正と読み替えない。
- SDK未導入PC、開発ツール未導入PC、複数DPIモニター、物理キー操作、耳での音声確認、実F5操作は未実施。
- 全D3D／COM資源のリーク不在は証明していない。失われた過去の生ログは復元していない。
- 任意形状の連続Trigger、回転形状全般の厳密CCD、Dynamic剛体との双方向の押し合い、Joint、Mesh、カプセル、GPU計測は今回の対象外。

## 完了の範囲

R0〜R7の実装・CPU試験・実App（実DxLib・固定入力）・配布4構成・測定を、同じ最終コード `b6d1573` で確認しました（文書とPythonの読取りの修正は後から）。2D／3D別の状態は [Physicsの進捗表](../Physics/Progress.md) のI〜Lの行にまとめています。

実施していないこと: SDK・開発ツール未導入PCでの起動、複数DPIの実モニター、人による物理キー・マウス操作と実F5、耳での確認、D3D／COM全資源のリーク列挙、非Windows・Sanitizer。
