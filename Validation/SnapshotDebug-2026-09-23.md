# Physics Snapshot実World統合・Debug接続・累積回帰 — 2026-09-23

過去のAUDIT / STATUS / Validation配下の既存ログは以前の配布物の記録であり、この文書の結果ではない。
この文書は、同日にこの作業ツリーで実際に実行したコマンドと結果だけを記録する。

## 開始時の状態

| 項目 | 値 |
|---|---|
| リポジトリ | `origin https://github.com/mynameisGaku/dxlib_framework.git`、ブランチ`main` |
| HEAD | `15f5df42571dddee2a0dcaede16e0fa826a9061b`（stage済み変更なし） |
| 未コミット変更 | TaskDispatcher.h/.cppがScope待機分離の**base版**Payloadと一致（Scene統合版ではない） |
| Physics Snapshot | 新規7ファイルはPayloadと一致して配置済み。既存4ファイル（RigidBody2D/3D.h、PhysicsWorld2D/3D.cpp）は未編集で、`integration.py`の基準blobと一致 |
| Scene寿命統合 | 未適用（ApplicationにRetireScope呼出しなし） |
| Application／Renderer統合 | 未適用。Applicationは`FRenderSystem2D`を所有し、root CMakeは`RenderSystem2D.cpp`を登録、`CMake/DebugTools.cmake`を読み込まない |
| AllocationFault.h/.cpp | `15f5df4`で削除され、`CMake/RenderTransparencyTests.cmake`・`Tools/TaskValidation`・`Tests/TaskDispatcherRecoveryTests.cpp`の参照だけが残存 |

環境: Windows 11 Pro 10.0.26200、Visual Studio 18 2026 Community、MSVC 19.51.36257、CMake 4.3.1、Ninja、Python 3.11、DxLib SDK 3.25a（`ThirdParty/DxLib-3.25a`）。

## 実施した変更

1. `apply_physics_snapshot.py --apply`（配布ZIPを短いパスへ展開して実行。長いパスではPythonのMAX_PATH制限で`Package file missing or corrupt`と誤停止した）。
   4ファイルへ16行ずつ追加。退避先: `..\dxlib_framework-snapshot-backup-khvf6omh`。再実行は`already-applied`。
2. `Tools/PhysicsSnapshotValidation/CMakeLists.txt`: MSVCの`/WX`下で既存Physicsヘッダーの`alignas(16)`由来C4324が停止させるため、テスト対象へ`/wd4324`のみ追加。その他の警告はエラーのまま。
3. `Tests/PhysicsSnapshotCoreTests.cpp`: 常に例外を投げる複製関数がMSVC ReleaseでC4702（到達不能）を起こしたため、2件目で失敗させる形へ変更。途中まで複製した結果を公開しないことも確認する、より強い検証になった。
4. `Tests/PhysicsSnapshotWorldTests.cpp`: 実Worldの回帰7件を追加（計16件）。統合後に作成したため、変更前WorldでのRedは確認していない。
5. Debug表示の移行（`Source/Debug`）: 旧`TPhysicsDebugWatch3D`・旧`CapturePhysicsDebugSnapshot3D`テンプレート・`EDebugBodyMotion`を削除。
   `Build/CapturePhysicsDebugSnapshot3D`、`Build/CapturePhysicsDebugSnapshot2D`、`FPhysicsDebugRecorder3D`、`SubmitPhysicsDebugSnapshot2D`を追加。`dxf_debug_tools`は`dxf::physics`へ依存。
6. `Examples/RenderDebug`: Watch配列を廃止し、Recorder経由のWorld採取へ移行。F8観察ON/OFF、F9 2D観察の最小例を追加。
   元から`Dxf/RenderContext.h`を含まず`FRenderContext`が不完全型となる潜在的なコンパイルエラーがあった（root未登録のため未検出）。includeを追加。
7. `Source/Toolbox/Private/Toolbox/Testing/AllocationFault.h/.cpp`: `4868dc9`のblobと同一バイトで復元（テスト専用。`DXF_ALLOCATION_FAULT_TEST_EXECUTABLE`未定義では`#error`、`dxf_toolbox`へは登録しない）。
8. `Tests/TaskDispatcherRecoveryTests.cpp`: `if constexpr`後の`return false`がMSVC `/WX`でC4702となるため`else`へ移動（挙動は同一）。
9. `Tools/DebugValidation/CMakeLists.txt`: 実Physicsソースと新Debugソース、`debug_physics_tests`を追加。
10. 文書: `Docs/Physics/Snapshots.md`、`Docs/Rendering/DebugTools.md`、`Docs/Development/RenderingDebugRoadmap.md`、`README.md`（日付付きの節を追加）。

root `CMakeLists.txt`は変更していない。

## 実行したコマンドと結果

すべてWindows / MSVC。各ログ末尾に`EXIT_CODE`を記録。

### 1. Physics Snapshot（実World構成）— `Validation/PhysicsSnapshotWorld/`

```powershell
cmake -S .\Tools\PhysicsSnapshotValidation -B .\Build\PhysicsSnapshot -DDXF_SNAPSHOT_CORE_ONLY=OFF   # 0
cmake --build .\Build\PhysicsSnapshot --config Debug --target dxf_physics_snapshot_core_tests dxf_physics_snapshot_world_tests   # 0
ctest --test-dir .\Build\PhysicsSnapshot -C Debug --output-on-failure --no-tests=error   # 0
cmake --build .\Build\PhysicsSnapshot --config Release --target dxf_physics_snapshot_core_tests dxf_physics_snapshot_world_tests # 0
ctest --test-dir .\Build\PhysicsSnapshot -C Release --output-on-failure --no-tests=error # 0
```

Visual Studio 18 2026（複数構成）。Debug・Releaseとも36/36通過。内訳は共通20件（テスト用登録配列）＋実World16件（実`dxf::physics`へリンク）。
初回（変更2・3の前）はDebugで29/29通過、ReleaseはC4702でビルド失敗した。

### 2. Debug表示（実World→採取→変換→描画キュー）— `Validation/PhysicsDebug/`

```powershell
cmake -S Tools\DebugValidation -B Build\PhysicsDebug   # 0
cmake --build Build\PhysicsDebug --config <Debug|Release> --target debug_tests debug_physics_tests   # 0 / 0
ctest --test-dir Build\PhysicsDebug -C <Debug|Release> -R "^(DebugTools|DebugPhysicsCapture)$" --output-on-failure --no-tests=error   # 0 / 0
```

| 実行ファイル | Debug | Release | 内容 |
|---|---|---|---|
| debug_tests | 26/26 | 26/26 | 値変換（手作成のFPhysicsSnapshot3D/2D）、2D/3D表示命令、履歴、カメラ、時計、描画統合 |
| debug_physics_tests | 6/6 | 6/6 | 実FPhysicsWorld3D/2Dから採取した値が3D/2D描画キューへ届く統合、複数Collider・回転・削除後保持・途中失敗の拒否・上限超過・観察無効時に採取しない・停止中の履歴・1/4レーン一致 |

同プロジェクトの`dxf_render_views_tests`等は既存`RenderView3D.h`のC4324がモジュール側`/WX`で停止するため対象外（この作業では変更していない）。

### 3. RenderDebugのコンパイル — `Validation/PhysicsDebug/renderdebug-*-compile.log`

RenderDebugはroot CMakeに登録されていないため、`RenderDebugScene.cpp`・`TransparencyDemo.cpp`・`WindowsMain.cpp`を
フレームワークと同じ`cl /std:c++20 /W4 /permissive- /utf-8 /EHsc /GR`と実DxLib SDKのincludeでコンパイルした（Debug: `/MTd /Od`、Release: `/MT /O2`）。両方とも終了コード0、C4324以外の警告なし。**リンク・実行・実画面確認は未実施。**

### 4. TaskDispatcher / Scope分離 — `Validation/TaskRegression/`

```powershell
cmake -S Tools\TaskValidation -B Build\TaskRecovery ; cmake --build ... --config <Debug|Release> ; ctest ... -C <Debug|Release>
cmake -S Tools\ScopeTaskValidation -B Build\ScopeTask ; cmake --build ... --config <Debug|Release> ; ctest ... -C <Debug|Release>
```

| 構成 | Debug | Release |
|---|---|---|
| TaskDispatcher回帰（td_） | 26/26 | 26/26 |
| Scope待機分離（scope_） | 16/16 | 16/16 |

初回のTaskValidationはAllocationFaultの復元後もC4702でビルド失敗し、26件がNot Run。変更8の後に上記の結果。

### 5. root全体（Tools/BuildWindows.ps1と同じ生成引数、別Buildディレクトリ）— `Validation/Cumulative/`

```text
cmake -S . -B Build\Cumulative-<debug|release> -G Ninja -DCMAKE_CXX_COMPILER=cl -DCMAKE_BUILD_TYPE=<Debug|Release>
  -DDXLIB_ROOT=<SDK> -DDXF_BUILD_NATIVE=ON -DDXF_BUILD_EXAMPLE=ON -DDXF_BUILD_TESTS=ON -DDXF_BUILD_NATIVE_SMOKE=ON
  -DDXF_RUN_DEVICE_TESTS=OFF -DDXF_INSTALL=ON                      # 0 / 0
cmake --build Build\Cumulative-<cfg> --parallel 8 -- -k 0          # 2 / 2（失敗は下表。成功可能なターゲットはすべてビルド）
ctest --test-dir Build\Cumulative-<cfg> --output-on-failure --no-tests=error --timeout 600   # 8 / 8
```

| CTest | Debug | Release | 備考 |
|---|---|---|---|
| PhysicsContinuation | Passed | Passed | `dxf_physics_tests`の直接実行は両構成152/152（衝突・Sweep・固定Step・剛体・接触・Solver・CCD・安定性・並列の9群） |
| NoStl | Passed | Passed | |
| AssetRootLaunch | Passed | Passed | |
| Framework / FrameworkAltCwd | Not Run | Not Run | `dxf_tests`がコンパイル不能（下記） |
| NativeContract | Not Run | Not Run | `dxf_native_contract_tests`がコンパイル不能（下記） |

コンパイル失敗はすべて**この作業で変更していないファイル**で、Renderer移行の未適用に起因する:
`Source/Gameplay/Public/Dxf/SpriteRendererComponent.h`、`Tests/SupportTests.cpp`、`Tests/RenderFailureTests.cpp`、`Tests/HardeningTests.cpp`、`Tests/UsabilityTests.cpp`（削除済みの根`FRenderContext::Draw`/`DrawText`/`FillRectangle`呼出し）、
`Tests/NativeTests.cpp`、`Tests/NativeSmoke/Smoke.cpp`、`Source/Native/Private/Dxf/DxLibGeometryBackend.cpp`（FakeDxLibへの幾何追補が未適用で`DxLib::VECTOR`等が未定義）。
Release構成ではさらに`Sandbox.exe`・`Starter.exe`のリンクがDxLibのFBX SDK依存シンボルで失敗する。同じエラーは以前の`Build/WindowsValidation/release-build.log`にもあり、既存の環境問題。

### 6. 規則・差分

```text
python Tools/CheckNoStl.py   # 0: No-STL check: 255 files, 0 violations
git diff --check             # 0
```

新規・全面改稿したDebugソースと`Tests/DebugPhysicsIntegrationTests.cpp`へ、リポジトリの`.clang-format`（VS同梱clang-format）を適用。既存ファイルの全体整形は行っていない。

## 既存の警告（変更していない）

- C4324（`FVector3`/`FQuaternion`/`FRenderView3D`の`alignas(16)`パディング）: Physics・Support・Debugの各所。
- C4456（`PhysicsWorld3D.cpp` 471/472行、`AxisA`/`AxisB`の隠蔽）。
- C4459（`Platform.cpp` 535行、`Out`の隠蔽）。

## 実行していないもの

| 項目 | 理由 |
|---|---|
| Scene＋Scope分離の実Application経由18件 | Scene寿命統合がこの作業ツリーに未適用。Scope分離base版が先に適用済みで、引継ぎの指示により旧Scene適用器を後から重ねない |
| root `Framework`/`NativeContract`等のCTest | 上記のRenderer移行未適用によるコンパイル失敗 |
| Application／Renderer統合テスト（`ApplicationRenderIntegrationTests`） | root CMakeに未登録で、Application側の移行も未適用 |
| RenderDebugのリンク・起動・実画面確認 | root CMakeに未登録。Applicationの`FRenderSystem2D`は3D命令を実行しないため、移行前に起動しても3D表示を確認できない |
| 実DxLib SDKのNativeSmoke起動・Sandbox起動 | NativeSmokeは上記の理由でコンパイル不能。Sandbox/StarterはDebugのみ実SDKへリンク成功したが、起動・画面確認は実施していない |
| GCC / Clang・ASan / UBSan / TSan | この環境にGCC / Clangがない |
| 追加回帰7件の変更前Red | 統合後に作成した |

## 訂正追記（2026-09-23、後続作業での原ログ確認）

この記録の本文は書き換えず、以下を追記する。

- NativeSmoke: `Validation/Cumulative/debug-build.log`・`release-build.log`の`FAILED`行で、Debug・Releaseとも`Tests/NativeSmoke/Smoke.cpp`のコンパイル失敗（根`FRenderContext::Draw`等）を確認した。
  NativeSmokeはどちらの構成でもリンクまで到達していない。報告文中の「Debugリンク成功」はSandbox / Starterだけに当てはまる。
- Sandbox / Starter: Debugはリンク成功（`Build/Cumulative-debug`に02:51付けの実行ファイル）。Releaseはリンク失敗。
- Releaseのリンク失敗の原因: 公式DxLib 3.25a ZIP（SHA-256 `508fea81…`、展開物と一致）に含まれる`DxLib_vs2015_x64_MT.lib`だけが、FBX読込を有効にしてビルドされている（FBX SDK参照705件。他のx64版はすべて0件）。
  このため「以前のBuildでも出た既存の環境問題」ではなく、SDKライブラリの構成に起因する。詳細は`Validation/RendererSceneIntegration-2026-09-23.md`。
- 2026-09-23 03:12頃、この記録の作成後に、作業ツリーのDebug関連ファイルが86a37b9配布物の`Changes/`とScene寿命配布物の`Payload`（旧Renderer版）で上書きされていた。
  後続作業で退避（`C:/Users/g0190/dxlib_framework-backups/2026-09-23-before-renderer-scene`）の後、本記録の変更内容を復元した。
