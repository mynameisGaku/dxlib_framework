# Renderer／Scene統合と累積回帰 — 2026-09-23

指示: `dxf_claude_integration_resume/START_HERE.md`。過去のAUDIT / STATUSや以前のValidationログは今回の結果として扱わない。
最終結果はすべて`Validation/RendererScene/final/`の原ログ（各ログ末尾に`EXIT_CODE`）から転記した。

## 開始時の状態と保護

| 項目 | 値 |
|---|---|
| remote / branch / HEAD | `origin https://github.com/mynameisGaku/dxlib_framework.git` / `main` / `15f5df42571dddee2a0dcaede16e0fa826a9061b` |
| stage済み変更 | なし |
| 退避 | `C:/Users/g0190/dxlib_framework-backups/2026-09-23-before-renderer-scene`（変更・未追跡189ファイル、SHA256.txt、tracked.diff、git-status.txt、HEAD.txt） |
| 以前の退避 | `../dxlib_framework-snapshot-backup-khvf6omh`（保持） |

**前回作業後の上書きを検出した。** 03:12頃、作業ツリーの次のファイルが過去配布物と同一バイトになっていた（SHA-256照合）。

- 86a37b9 debug-tools配布物`Changes/`: `CMake/DebugTools.cmake`、`Examples/RenderDebug/RenderDebugScene.h/.cpp`、`Source/Debug/Private/Dxf/PhysicsDebugDisplay3D.cpp`、`Docs/Development/RenderingDebugRoadmap.md`（いずれもHEADより古い内容）。
  `PhysicsDebugSnapshot3D.h/.cpp`、`Tests/DebugToolsTests.cpp`、`Tests/DebugPhysicsIntegrationTests.cpp`、`Docs/Rendering/DebugTools.md`はHEADと同一へ戻っていた。
- Scene寿命配布物`Payload`（旧Renderer版）: `Application.h/.cpp`、`SceneNavigator.h/.cpp`、`Contexts.h`、Scene回帰と検証入口、`Docs/SceneTaskLifetime.md`。

指示の「Snapshot／Debug改修を保持」「古いDebug実装・RenderDebugで上書きしない」に従い、退避後にHEADを基準として前回の変更を再適用した（旧Watch実装は戻していない）。
Scene側の上書き内容は、統合対象そのもの（base版）だったため起点として利用し、Renderer接続を加えた。

## 変更内容

### Renderer統合（4868dc9相当を現行ソースへ限定統合）

- `Application.h/.cpp`: 所有者を`FRenderSystem`へ統一し、構築後に`SetExecutionJobs(m_ExecutionJobs)`を接続。Scene寿命配布物の`Variants/renderer-integrated`と同一バイトになることを確認。
  `~FApplication`は`Shutdown()`を先に呼び、Rendererは借用Jobを破棄時に使わないため、メンバー破棄順を変える必要はない。
- 旧`FRenderSystem2D`の参照9ファイルを移行し、`RenderSystem2D.h/.cpp`を削除（互換aliasなし）。
- 根`FRenderContext`の`Draw`/`DrawText`/`FillRectangle`呼出し31か所を`Get2D().DrawSprite`等へ移行。対象はコンパイラが「FRenderContextのメンバーではない」と報告した行だけで、`Get3D()`経由の参照や旧API不在を検査する`RenderViewsTests.cpp`は変更していない。
- root CMake: `RenderSystem.cpp`・`RenderPass3D.cpp`を登録（各1回）、`ApplicationRenderIntegrationTests.cmake`を`DXF_BUILD_TESTS`内で1回、`DebugTools.cmake`をNative・export一覧・`dxf_runtime_paths`の定義後、install前に読み込む。
- `Tests/FakeDxLib/DxLib.h`: 包括的な契約用ヘッダーの末尾で幾何用追補`RenderViewsApi.h`を読み込む（限定用FakeNativeを先行includeしない）。実NativeターゲットにはFakeヘッダーを入れていない。

### 統合で見つかった不具合（回帰を先に確認して修正）

- `FRenderSystem::Flush_Internal`が3D命令の有無にかかわらず`ResetState`を呼んでいた。描画先切替の直前Flushで復元が失敗するとフレーム全体を失敗扱いにし、
  既存の`render target backend exception is contained and rollback keeps the frame usable`（Framework）が失敗した（Red: 266/267）。
  3D命令がある場合だけ3D実行と2D状態復元を行うよう修正（`FRenderContext::HasPending3D_Internal`を追加）。3D経路の復元は従来どおり。修正後272/272（ログテスト5件を含む）。

### Scene寿命統合（Scope待機分離を維持）

- `TaskDispatcher.h`へ`IsOwnerThread()`・`CanSynchronize()`だけを追加（`Reference/ScopeIsolationBase_to_SceneCompatible.patch`、`git apply --check`後に適用。`TaskDispatcher.cpp`は無変更）。
- 追加回帰2件（実Application、条件変数のゲート＋5秒の監視スレッドでゲートを開放しテスト自身が終了する）:
  - `scene_task_switch_completes_while_root_and_sibling_prepares_are_blocked`
  - `scene_task_exit_waits_for_gated_scene_prepare_and_capture_release`
- 変異確認: 作業ツリー外の複製で`RetireCurrentScope_Internal`の前に`WaitForPrepares()`を挿入（旧「Dispatcher全体待ち」）すると、前者は`bSwitchedWhileBlocked`で失敗し5.01秒で終了した（`Validation/RendererScene/mutation-*.log`）。後者は順序検査のためこの変異では通過する。
- MSVC専用のテスト修正: Scene回帰の`if constexpr`後の到達不能`return`（C4702）を`else`へ移動。Scene回帰ターゲットに`/wd4324`（Rendererヘッダーの`alignas(16)`通知）のみ追加。

### ログ（利用者の追加要望）

- `Toolbox/Log.h`・`Log.cpp`: `DXF_LOG_VERBOSE/INFO/WARNING/ERROR`、コンパイル時・実行時の重要度、差替え可能な出力先、既定はVisual Studio出力（UTF-16変換）＋標準エラー。仕様は`Docs/Logging.md`。
- 記録箇所: Application（開始・フレーム失敗・終了）、SceneNavigator（有効化Scope・退役失敗・遷移失敗）、Debug採取拒否、RenderDebugのF8/F9/再生成。
- `Tests/LogTests.cpp`（5件、`dxf_tests`）。当初のスレッド試験はVerboseを使い、Release（Verboseをコンパイル時除去）で失敗したためInfoへ修正（ログ機構の不具合ではない）。

### 文字コード

利用者の指示により、今回変更・追加したSource／Tests／Examples／CMake／Tools／Docsのテキスト65ファイル＋文書をCRLF・UTF-8（既存BOMは維持）へ統一した。

## 最終結果（同一作業ツリー、Windows / MSVC 19.51、実DxLib 3.25a）

root: `Tools/BuildWindows.ps1`と同じ生成引数（Ninja、`DXF_BUILD_NATIVE/EXAMPLE/TESTS/NATIVE_SMOKE=ON`）、`Build/Integration-<debug|release>`。

| 対象 | 入口 | Debug | Release |
|---|---|---|---|
| root CTest登録 | `ctest -N` | 20件 | 20件 |
| root CTest | `ctest --no-tests=error` | 20/20 通過（exit 0） | 20/20 通過（exit 0） |
| Framework（内部ケース） | `dxf_tests.exe` | 272/272 | 272/272 |
| NativeContract（内部ケース） | `dxf_native_contract_tests.exe` | 15/15 | 15/15 |
| Application描画統合 | `dxf_application_render_integration_tests.exe`（CTestは1件） | 8/8 | 8/8 |
| Physics（全9群） | `dxf_physics_tests.exe` | 152/152 | 152/152 |
| Debug値変換・表示・履歴 | `dxf_debug_tools_tests.exe` | 26/26 | 26/26 |
| Debug実World→採取→描画キュー | `dxf_debug_physics_tests.exe` | 6/6 | 6/6 |
| Snapshot（共通20＋実World16） | `Tools/PhysicsSnapshotValidation`、`CORE_ONLY=OFF` | 36/36 | 36/36 |
| TaskDispatcher回帰 | `Tools/TaskValidation` | 26/26 | 26/26 |
| Scope待機分離 | `Tools/ScopeTaskValidation` | 16/16 | 16/16 |
| Scene（実Application、既存18＋追加2） | `Tools/SceneTaskValidation -A x64`、`-L scene-task` | 20/20 | 20/20 |
| NoStl / `git diff --check` | | 0 / 0 | |

root CTestの20件には、Framework・FrameworkAltCwd・NativeContract・ApplicationRenderIntegration・PhysicsContinuation・NoStl・AssetRootLaunch・DebugTools・DebugPhysicsCapture・Render/Transparency/JobFault/Views群が含まれる。
CTest登録数と各実行ファイル内のケース数は別に記載しており、合計していない。

### 実SDK実行ファイル（ビルド／リンク／起動／実画面を区別）

| 実行ファイル | Debugビルド・リンク | Debug起動 | Debug実画面 | Releaseリンク |
|---|---|---|---|---|
| RenderDebug | 成功 | 成功（Escで終了コード0） | 確認（下記） | 失敗（FBX） |
| NativeSmoke | 成功 | 未実施 | 未実施 | 失敗（FBX） |
| Sandbox | 成功 | 未実施 | 未実施 | 失敗（FBX） |
| Starter | 成功 | 未実施 | 未実施 | 失敗（FBX） |

RenderDebug（Debug）の実画面キャプチャ: `Validation/RendererScene/screens-debug/`。
3D描画と奥行き・2D文字の復元、Snapshotからの表示（Body=3 Collider=3）、F4の採取値の辺・重心、F9の2D観察、P停止（step 435）→N手送り（436、時計+16.7ms、履歴74→75）、
F8 OFFで図形が消え採取・履歴保存停止（step "-"・履歴75のまま）、F8 ONで現在Stepから再開を確認した。
06番（手送り直後）の画像はキャプチャスクリプトで保存されなかったため、手送りの確認は05・08番の数値差による。Z/X履歴閲覧・Enter・F1〜F3・F5〜F7は操作していない。

### Release実SDKリンク失敗の原因

- 失敗: `NativeSmoke.exe`・`Sandbox.exe`・`Starter.exe`・`RenderDebug.exe`、各105件の未解決外部参照（`fbxsdk::FbxString`、`FbxProperty`、`FbxAMatrix`、`FbxMalloc`等）。参照元は`DxLib_vs2015_x64_MT.lib(DxModelLoader1.obj)`。
- 選択されたライブラリ: SDKの`DxDataTypeWin.h`の自動リンク指定（静的CRT `/MT`、x64、VS2015以降）に従う`DxLib_vs2015_x64_MT.lib`。`CMake/FindDxLib.cmake`はSDKのincludeディレクトリをリンク探索先にするだけで、ライブラリを列挙していない。
- SDK: `ThirdParty/DxLib_VC3_25a.zip`のSHA-256 `508fea81c65963bdb6ea6dd54e23995f753b54d2a3638e8a25b4fc72b62b2cd1`は記録済みの取得値と一致し、展開済み`.lib`はZIP内と同一バイト。
- x64のDxLib系ライブラリ50個のうち、FBX SDK参照（`fbxsdk@@`）を含むのは`DxLib_vs2015_x64_MT.lib`だけ（705件）。MD / MTd / 旧名称版はすべて0件。SDKにFBX SDKのライブラリは同梱されておらず、`DxCompileConfig.h`の`DX_LOAD_FBX_MODEL`は既定で無効。
- 結論: プロジェクトの依存登録・構成選択の不備ではなく、公式SDKのMT版ライブラリがFBX読込を有効にしてビルドされていることが原因。ダミー定義や根拠のないライブラリ追加は行っていない。
- 解決には利用者の判断が必要: (1) Autodesk FBX SDKの取得と利用許諾・リンク、(2) Releaseの実行時ライブラリを`/MD`へ変更（全体の方針変更）、(3) 別版のDxLibの利用。
  利用者から「MV1変換ではなくFBXからモデル・アニメーションを直接読み込みたい」との要望があり、モデル作業の範囲決定と合わせて判断する。

## 以前の記録の訂正

`Validation/SnapshotDebug-2026-09-23.md`末尾へ訂正追記した。NativeSmokeは前回、Debug・Releaseともコンパイル段階で失敗しており、「Debugリンク成功」はSandbox / Starterだけに当てはまる。

## 実行していないもの

- GCC / Clang・ASan / UBSan / TSan（この環境にない。導入していない）。
- Release構成の実SDK実行ファイルのリンク以降（上記の原因による）。
- NativeSmoke・Sandbox・StarterのDebug起動と実画面確認、RenderDebugの一部操作（上記）。
- 追加したScene回帰2件のうち、`scene_task_exit_waits_for_gated_scene_prepare_and_capture_release`の変更前Red（統合後に作成）。
- モデル・ライト・Viewport・GPU計測（指示により対象外）。
