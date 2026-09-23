# モデル実描画の終了・再起動の診断（2026-09-24）

結論：診断と試験判定の整備は完了。前回の予期しない終了は今回の上限内では再現せず、原因は未特定。終了契約やSDK通知を変える修正は行っていない。成功回数は安定性保証や原因修正の証拠ではなく、記録追加による再現頻度の変化も評価していない。

## 開始状態と既存ログ

開始HEAD / origin/mainは `9321b5b22d75dcb2de0cc9d4bc54ec2f826f1d4d`。main、差分・未追跡・stage済み変更なし。fetch後も一致。Windows x64、VS 18 Community / MSVC 19.51、既存DxLib 3.25a source、D3D11、model extension 3、MT/MTd。SDK・Assets・Archiveは変更していない。

前回の原ログは存在し、`Build/NativeLifecycle-PriorLogs/` にコピーして保存した。

- `repair-root-ctest-debug.log`: SHA-256 `9a4f139b13ec2d8241c0b029117106b9a81fb13413d1e14b119f8dc9f49d0599`
- `repair-root-native-model-retry.log`: SHA-256 `6ce0601bf64213e06f3dbed7149e7c3f75beef4a7f4ee25e46b988b8994f37ba`

原ログは、Application試験が先に成功し、後半の注入ケースのassertion失敗、その次の正常描画・更新・シェーダー数の失敗、最後に `Navigator cannot accept a scene request` の順。各Stepの成功falseと注入到達を記録していなかったため、最初の終了分岐は原ログから確定できない。後続のNavigatorエラーだけを原因と扱わない。

## 実装と責務

- `Tests/Support/ModelSmokeStep.h`: 試験専用の共有判定。通常は成功trueのみ許容。注入ケースは実行フラグ、UserException、モデル失効メッセージの一致を要求する。新しいゲーム公開APIではない。
- `Tests/NativeModelSmoke/Main.cpp`: 各Stepを一度だけ実行し、true / false / error、エラーコードと本文、期待値、Application番号・フレーム、初期化・OnDraw・注入・終了・読戻し回数を記録。結果が期待と違えば直ちに例外でシナリオを終了し、RAIIで後始末して実行ファイルも失敗終了する。以後のScene要求や独立シナリオを実行して原因を上書きしない。
- `Picking.cpp`: 同じApplication通し番号を共有し、選択例の異常フレーム番号と結果も記録する。成功する全フレームの大量ログは追加していない。
- `Application.cpp`: 継続不可を決めた既存分岐、終了要求の保留、Scene→資源→Platform終了の境界にInfo記録を追加。Platform/SDK依存は追加していない。
- `DxLibPlatform.cpp`: Initialize開始、DxLib_Init/Endの一回の実戻り値、ProcessMessageが継続不可を返した一回の値と初期化状態を記録。ログのためにSDKを追加呼出ししない。

既存DXF_LOGの固定長・noexcept・再入抑止の記録処理を利用。追加Manager、OS通知の破棄、固定Sleep、失敗時の再初期化はない。元の同一プロセス順、9個の新規Applicationを含む再起動、注入2ケース、画素条件、読戻し抑止、PBR・モデルハンドル寿命検査、120秒上限を保持した。同じApplicationを再Start可能にはしていない。

## 回帰と開発中の失敗

Frameworkへ2ケースを追加し、内部274→276件。

1. 成功true / 成功false / 正しい注入エラー / 別エラー / 未注入を共有判定で区別。
2. 記録用Platformが終了を返す実Applicationで、Draw・注入へ未到達、Presentなし、Scene終了済み、停止状態を確認。判定失敗後はScene要求へ進まず、最初の成功falseを保持。

既存の正常終了要求、描画中の終了保留、失敗フレームの非提示、Scene・資源の後始末の試験も維持した。これらはCPUの契約確認であり、実DxLibの自然発生を再現した試験ではない。

故意に共有判定を「TResultの成否だけを検査」に戻した変異で、ビルド0 / Framework実行1（275/276）。`Model smoke distinguishes continuation stop and injected error` が成功falseの拒否で失敗した。変更前main上の自然発生Redではない。元の判定へ復元して最終検証した。ログは `Build/lifecycle-mutant-build.log` / `lifecycle-mutant-test.log`。

Debug単独初回では、新規判定が注入失敗のコードをInvalidStateと誤認して失敗（CTest 8）。観測値は注入実行済み、Draw=1、Captured=0、Deinitialized=1、StepはUserException(code=5)、本文は `Model instance was released before drawing`。Render.Nativeへ進む際の描画処理失敗を試験のRequireSuccessが例外にし、ApplicationがUserExceptionへ変換する経路である。判定をこの実経路に合わせて修正した。これは今回追加した試験の期待値の不備であり、過去の偶発終了とは別。最初の失敗は保存した。

## 有限回の実描画試行

全プロセスは同じセッションで構成間も含め直列。全群中の実行も数え、Debug/Release各5プロセスで終了した。成功するまでの自動再試行ではない。Debug初回の既知の判定誤りを直した後、通常系列を維持した残りの試行を行った。

| 構成・試行 | 結果 | CTest終了コード | 時間 |
|---|---|---|---|
| Debug単独1 | 新規判定の期待コード誤り、上記の注入エラーを誤拒否 | 8 | 5.09秒（CTest全体） |
| Debug単独2 / 3 / 4 | 各1/1成功、予期しない終了なし | 各0 | 約12.63 / 12.59 / 12.45秒（採取処理込み） |
| Debug全群内（5プロセス目） | モデル群成功 | 0（全群も成功） | モデル群13.72秒 |
| Release単独1 / 2 / 3 / 4 | 各1/1成功、予期しない終了なし | 各0 | 約12.13 / 14.66 / 13.02 / 14.00秒（採取処理込み） |
| Release全群内（5プロセス目） | モデル群成功 | 0（全群も成功） | モデル群13.05秒 |

単独ログは `Build/NativeLifecycle-Attempts/<構成>/attempt-N.log`、成功時を含む全出力は `attempt-N-full.log`、直後のSDKログは `attempt-N-dxlib.log`。SDKログはそのプロセスの最後の初期化区間が中心で、全セッションに対応する記録とはみなさない。

Debug2～4の後にPicking側の番号・異常フレーム記録を補足し、その後のビルドを最終版とした。Debug2～4を最終版の3回成功へ読み替えない。Release単独4回と両構成の全群は最終コード。Debug初回は判定誤りの中間版。

実行ファイルは `Build/FbxContinuation/<構成>/NativeModelSmoke.exe`。

| 実行対象 | SHA-256 |
|---|---|
| Debug2～4の中間版 | `7c6a7de6dc41a43b4f1b84950e735f1e6c449f6e1e34f6a50d18bbd3b49b22df` |
| 最終Debug | `24043e06537686e43d534a74529407d8e2616c745aa775b68a503aca636b946e` |
| 最終Release | `0b95735bc5eccb2b206908e194ee96363e308606c4be20648a101dde98db6dcb` |

## 同じ最終コードの全検証

| 対象 | 結果 | 終了コード |
|---|---|---|
| 正規単独入口・新規作業先 | Debug/Release生成・全ビルド・登録・CTest各21/21 | 全工程0 |
| root Debug | ビルド0、登録25、全群25/25、23.35秒 | 0 |
| root Release | ビルド0、登録25、全群25/25、18.99秒 | 0 |
| Framework内部（通常/別CWD） | 両構成276/276、追加2件含む | 0 |
| Python | 24/24 | 0 |
| No-STL | 310ファイル、違反0 | 0 |
| 配布 | Native無効・Debug、PhysicsOnlyを含む8段階 | 全工程0 |
| 差分・文字コード | git diff --check、UTF-8/CRLF、既存BOM保持 | 0 |

単独入口は `python Tools/ValidateDebug.py --logs Build/NativeLifecycle-StandaloneLogs`、作業先 `Build/DebugValidation/run-uwo0vz3b`。rootの構成・SDK・device登録は既存FbxContinuationを確認し維持。各ビルド成功後に `ctest --test-dir Build/FbxContinuation -C <構成> -j 1 --no-tests=error --output-on-failure` を実行した。登録/ビルド/CTest/全出力ログは `Build/lifecycle-final-*-debug.log` とrelease側に保存。全群の成功は単独試験との合算ではない。

配布はx64開発者環境で `python Tools/ValidatePackage.py --logs Build/NativeLifecycle-PackageLogs`。`Build/PackageValidation/run-c9e8v54c/ConsumerBuild/PhysicsOnly.exe` がdxf::physicsだけの生成・登録・問い合わせ・削除を実行して終了0。既存8段階の結果を保持し、Native有効やRelease配布へ拡大解釈していない。

## 初発原因と未解決事項

今回自然発生の予期しない終了は得られず、元の初発原因は未特定。根拠を伴って観測できた最初の異常は、上記Debug1の「正しい注入エラーを新規判定が誤拒否」であり、これは修正済み。CPUの終了注入では成功false→注入未到達→Scene要求前の打ち切りを確認した。自然発生・故障注入・変異・静的読み取りを混同しない。

次回ProcessMessageが継続不可なら、その実値と初期化状態→Runtimeのplatform-false→Stepの結果と注入到達→終了順を追える。SDK内部やOS通知の起源は、この記録だけで自動的に特定できるものではない。今回その起源を証明していないため、外部実装の原因修正は行っていない。

SDK未導入PC、開発ツール未導入PC、実D3D9、物理キー操作、聴感、全D3D/COM資源のリーク列挙、非Windows Sanitizer実行は未実施。ハンドル数の既存検査は全リーク不存在の証明ではない。Physics、RaycastClosest、Snapshot、ufbx/モデル機能、Scene/Scope寿命、Starter/Sandbox、公開APIを保持した。

[次回の採取手順](../Testing.md#モデル実描画の終了再起動の診断)。ログ・画像・SDK・実行ファイルはコミットしない。今回の限定調査は診断整備として完了し、追加反復や新機能へは進めない。
