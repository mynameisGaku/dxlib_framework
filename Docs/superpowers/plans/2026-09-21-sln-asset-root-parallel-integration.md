# sln基準アセットRootと並列基盤の本体統合 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** exeの起動方法・CWD・構成によらずsln配置先をProjectRootとしてアセットを解決し、Thread/Job監査・並列Physics・Task・非同期Asset・並列描画を本体へ統合してmainへpushする。

**Architecture:** Toolboxに小さなパス解決型（`FAssetPathResolver`＋`FProjectPaths`設定読込）を追加し、`FAssetService`の全要求を解決済み絶対パスへ通す。Root決定は起動準備で一度だけ行い、Workerは解決済みパスだけ見る。CMakeはexe別`.dxfpaths`をPOST_BUILD生成する。

**Tech Stack:** C++20, CMake 3.24+, Visual Studio 2022+ (vswhere検出) / Ninja, MSVC, DxLib SDK, Python 3 (Toolsのみ)。

**Spec:** ユーザー命令「OpenCode実装命令：main継続・sln基準のアセットRoot・並列実行の本体統合」（2026-09-21受領、開始HEAD 68b4456）。

## Global Constraints

- Source・Examples・TestsでSTL禁止。不足はToolboxへTDDで追加する。`std`型の別名・隠蔽ラッパーも禁止。
- 言語支援ヘッダー例外は既存規約範囲のみ。OS/CランタイムはToolbox実装境界へ。
- 値・設定・サービスは`F`、templateは`T`、interfaceは`I`、`enum`は`E`。既存DObject系維持。新規多態Objectの無関係な一括rename禁止。
- 振る舞い型の値メンバーは`m_`、ポインターは`m_p`、boolは`m_b`。設定・結果・Context・ID等のデータフィールドには付けない。
- 内部処理は末尾`_Internal`。Onフック・コンストラクタ・デストラクタ・演算子は対象外。
- `FORCEINLINE`は短い取得・比較・変換のみ。長い処理・再帰・関数ポインタ呼び出しに付けない。
- ヘッダー宣言は日本語複数行`/** ... */`、関数内とcpp内は`//`。単位・所有・失敗条件・Thread契約を書く。
- Allman、タブ幅4、1行1文、Public/Private分離、固定幅数値型、`python Tools/CheckNoStl.py`を毎回実行。
- force push・履歴書換・branch protection回避禁止。`git push origin HEAD:main`はfast-forward確認後のみ。
- Red捏造禁止。最初から通った追加試験は回帰検査と記録する。

## Baseline (verified 2026-09-21)

- `main == origin/main == 68b4456`, clean tree, backup ref `refs/backup/main-20260921-68b4456` created.
- Toolchain: `C:\Program Files\CMake\bin\cmake.exe`, ninja at `.cmake-deps`, MSVC 19.51 via VsDevCmd 18. `cl` runs.
- Prior session evidence at this exact HEAD content: `dxf_tests.exe` 212/212 passed, NoStl 182 files 0 violations.
- `Build/windows-release/dxf_tests.exe` is up to date (`ninja: no work to do`).

## File Structure

Phase 1 (asset root) touches:

- Create: `Source/Toolbox/Public/Toolbox/ProjectPaths.h` — `FProjectRoot`値、`FAssetPathResolver`、`FProjectPathSettings`読込・解決。
- Create: `Source/Toolbox/Private/ProjectPaths.cpp` — 上の実装（OS取得は同ファイル内の`_Internal`境界）。
- Create: `Tests/AssetRootTests.cpp` — 下記テストケース。
- Modify: `Source/Toolbox/Public/Toolbox/Platform.h` — `ExecutableDirectory()`宣言追加（OS境界の既存配置に合わせる）。
- Modify: `Source/Toolbox/Private/Platform.cpp` — `ExecutableDirectory()`のWindows実装（`GetModuleFileNameW`＋長いパス再試行）。
- Modify: `Source/DxLibSupport/Public/Dxf/AssetService.h` — 一度だけ設定する`SetProjectRoot`＋`GetProjectRoot`宣言。
- Modify: `Source/DxLibSupport/Private/AssetService.cpp` — 全Load経路で共通Resolverへ通す。
- Modify: `Source/Runtime/Public/Dxf/Application.h` — `FApplicationSettings::ProjectRoot`（空＝未指定）追加。
- Modify: `Source/Runtime/Private/Application.cpp` — `Start_Internal`でアセット使用前にRoot確定。
- Modify: `Examples/Sandbox/WindowsMain.cpp` — exe別`.dxfpaths`読込→`Settings.ProjectRoot`、無ければexe dir。
- Modify: `Examples/Starter/Source/WindowsMain.cpp` — 同上（Starter名のファイル）。
- Modify: `CMakeLists.txt` — `dxf_runtime_paths(Target)`ヘルパー、`Sandbox`/`Starter`/`NativeSmoke`へPOST_BUILD生成、VS_DEBUGGER_WORKING_DIRECTORYを`$Root`へ。
- Create: `Docs/Assets/Paths.md`, modify `Docs/Threading.md` (later), `Docs/Physics/ParallelExecution.md` (later), `Docs/Development/Continuation.md`.

Later phases touch (recon first, then decide): `Source/Physics/Private/ParallelPhysicsCore.*` (exists, unregistered), `Source/Physics/Public/Dxf/PhysicsExecution.h` (exists), `Tests/Physics/ParallelTests.cpp` (exists, unregistered), `CMake/PhysicsTests.cmake`, root `CMakeLists.txt` lines 145-190, `apply_stage2.py` (reference only — never bulk-apply).

---

### Task 1: 資産Root失敗テストの追加（Red）

**Files:**
- Create: `Tests/AssetRootTests.cpp`
- Modify: `CMakeLists.txt:184` (`dxf_tests` sourcesへ`Tests/AssetRootTests.cpp`追加)

**Interfaces:**
- Consumes: `Toolbox::FPath`, `FString`, existing `FAssetService` 3-arg constructor (unchanged).
- Produces: failing tests naming the missing behavior (no production code yet).

- [ ] **Step 1: テストファイルの作成**

`Tests/AssetRootTests.cpp` に少なくとも次を書く（既存TEST/REQUIREマクロ使用、`Support/Test.h` include）:

```cpp
#include "Support/Test.h"
#include "Toolbox/Platform.h"
#include "Dxf/AssetService.h"
TEST("asset root resolves sln-side originals independent of working directory")
{
	// 期待: ProjectRoot/Assets/player.bmp が読める。
	// 現状: Root概念がなくCWD相対でしか読めないため失敗する。
	REQUIRE(false);
}
TEST("asset root rejects paths escaping the root")
{
	REQUIRE(false);
}
TEST("asset root keeps per-service roots and caches separate")
{
	REQUIRE(false);
}
```

ケース一覧（各1つのTESTへ展開）: CWD=ProjectRootで読める／CWD=無関係フォルダーでも同じ原本／Debug・Release・VS・Ninjaの設定生成位置（CMakeテスト側で検証）／root slnと内部slnの選択／日本語・空白入りパス／原本とexe横コピーの内容差（開発は原本優先）／原本欠落時はNotFound（コピーへ逃げない）／絶対パスはRootを二重付与しない／`.`・Root内`..`の正規化とキャッシュ一致／Root外`..`・drive-relativeの拒否／壊れた設定の明示エラー／別Rootの二サービス分離／StreamとMemoryで別キー。

- [ ] **Step 2: 登録して実行し失敗を確認**

```bash
ninja -C Build/windows-release dxf_tests.exe
.\Build\windows-release\dxf_tests.exe
```

Expected: 新規TESTが`[FAIL]`で落ちる（または全テストが止まらず失敗件数に含まれる）。

- [ ] **Step 3: Commit（テストのみ）**

```bash
git add Tests/AssetRootTests.cpp CMakeLists.txt
git commit -m "test: add sln-rooted asset resolution expectations"
```

### Task 2: `FProjectRoot`・`FAssetPathResolver`のToolbox実装（Green）

**Files:**
- Create: `Source/Toolbox/Public/Toolbox/ProjectPaths.h`
- Create: `Source/Toolbox/Private/ProjectPaths.cpp`
- Modify: `Source/Toolbox/Public/Toolbox/Platform.h` (`ExecutableDirectory`宣言)
- Modify: `Source/Toolbox/Private/Platform.cpp` (`ExecutableDirectory`実装)

**Interfaces:**
- Consumes: `Toolbox::FPath` (`Normalize`, `Parent`, `operator/`), `FString`, `FWideString`.
- Produces:
  - `Toolbox::FPath ExecutableDirectory();` — exe配置ディレクトリ（OS境界）。
  - `struct Toolbox::FProjectPathSettings { uint32 Version; FString Mode; FString ProjectRootRelative; };`
  - `TResult<FProjectPathSettings> ParseProjectPathSettings(const FString& Text);`
  - `class Toolbox::FAssetPathResolver` — `SetRoot(const FPath&)`一度だけ、`Resolve(const FString& LogicalPath)`。

契約（ヘッダー日本語コメントへ明記）:
- `Resolve`は字句結合＋`Normalize`し、Root外へ出る相対パス・drive-relative（`C:foo`）・root-relative（`\foo`）・空・埋め込みNUL・不正UTF-8を拒否。完全修飾の絶対パス（ドライブ付き・UNC）はRootを前置せずそのまま正規化して返す。Rootはsandboxではないと文書化。
- `SetRoot`は二度目の呼び出しを拒否（Root不変性）。スレッド契約: 構築・設定は所有スレッド、`Resolve`は不変参照でJobSafe。
- `.dxfpaths`形式: `Version=1` / `Mode=Development|Packaged` / `ProjectRootRelative=<exe dir基準の相対パス>`。壊れた内容・存在しないRootはエラー（フォールバックしない）。

- [ ] **Step 1: `ProjectPaths.h`の作成**（1主要型1ファイル原則に従いResolver中心＋設定struct同梱）
- [ ] **Step 2: `Platform::ExecutableDirectory`の宣言・実装**（Windows: `GetModuleFileNameW`＋512→倍増→32768上限、既存`WindowsMain.cpp:15-39`の流儀を移植）
- [ ] **Step 3: `ProjectPaths.cpp`の最小実装**（Task 1のテストが通る範囲のみ）
- [ ] **Step 4: 実行**

```bash
ninja -C Build/windows-release dxf_tests.exe
.\Build\windows-release\dxf_tests.exe
python Tools/CheckNoStl.py
```

Expected: 新規TESTがPASS、既存212件もPASS、NoStl違反0。

- [ ] **Step 5: Commit**

```bash
git add Source/Toolbox/Public/Toolbox/ProjectPaths.h Source/Toolbox/Private/ProjectPaths.cpp Source/Toolbox/Public/Toolbox/Platform.h Source/Toolbox/Private/Platform.cpp
git commit -m "feat: resolve asset paths against sln-placed project root"
```

CMakeの`dxf_toolbox` sourcesへ`ProjectPaths.cpp`追加を忘れないこと（Task 1時点で未登録ならビルド失敗するので同Taskで登録）。

### Task 3: `FAssetService`・`FApplication`・exe入口の接続

**Files:**
- Modify: `Source/DxLibSupport/Public/Dxf/AssetService.h`, `Source/DxLibSupport/Private/AssetService.cpp`
- Modify: `Source/Runtime/Public/Dxf/Application.h`, `Source/Runtime/Private/Application.cpp`
- Modify: `Examples/Sandbox/WindowsMain.cpp`, `Examples/Starter/Source/WindowsMain.cpp`

**Interfaces:**
- Consumes: Task 2の`FAssetPathResolver`。
- Produces:
  - `bool FAssetService::SetProjectRoot(const Toolbox::FPath& Root);` — 一度だけ成功、以後`false`。未設定時は従来どおりCWD相対（既存65箇所のテスト構築を壊さない）。
  - `FApplicationSettings::ProjectRoot` (`FString`、空＝未指定)。
  - exe入口: `<Exe>.dxfpaths`（例`Sandbox.dxfpaths`）があればexe基準で解決して設定、無ければexe dirをPackaged Rootに。

`LoadTexture`/`LoadSound`の共通経路: 入力検証→`Resolver.Resolve`→解決済み絶対パスでCacheキー・Loader呼び出し。フォント識別子はResolverへ通さない。失敗時は要求パス・選択Root・解決後パス・選択元をエラー文へ含める。

- [ ] **Step 1: `FAssetService`へRoot保持と解決経路を追加**（キャッシュキーは解決済みパス＋条件のまま）
- [ ] **Step 2: `FApplication::Start_Internal`でScene開始前にRoot確定**（明示設定のみ。`.dxfpaths`探索はexe入口の責務でApplicationに持たせない）
- [ ] **Step 3: Sandbox/Starterの`WindowsMain`で`.dxfpaths`読込**（exe別名で上書き防止。壊れ・Root不存在はダイアログエラーで終了、フォールバック禁止）
- [ ] **Step 4: 実行** — Task 1の全ケース＋既存全テスト＋NoStl
- [ ] **Step 5: Commit** — `feat: wire project root through application and exe entries`

### Task 4: CMakeの`.dxfpaths`生成とデバッガ作業ディレクトリ

**Files:**
- Modify: `CMakeLists.txt`（`dxf_runtime_paths`ヘルパー、Sandbox/Starter/NativeSmokeへ適用、`dxf_sample_assets`の開発コピー必須化を解除、VS_DEBUGGER_WORKING_DIRECTORYを`$Root`へ）

**Interfaces:**
- Consumes: `$<TARGET_FILE_DIR:...>`、`CMAKE_CURRENT_SOURCE_DIR`。
- Produces: `dxf_runtime_paths(TargetName ExeName)` — POST_BUILDで`<TARGET_FILE_DIR>/<ExeName>.dxfpaths`を生成。相対パス計算は`cmake -P`スクリプトで行い、未評価の生成式を実パス扱いしない。VS multi-config・Ninja・RUNTIME_OUTPUT_DIRECTORYに対応。

- [ ] **Step 1: ヘルパーと適用を追加**
- [ ] **Step 2: 検証**

```powershell
GenerateProjectFiles.bat
cmake --build Build/VisualStudio --config Debug --target Sandbox
# Debug/Sandbox.dxfpaths の ProjectRootRelative が repo root を指すことを確認
GenerateProjectFiles.bat -Development
cmake --build Build/VisualStudio-development --config Debug --target Sandbox
```

- [ ] **Step 3: 起動別検証表の記録** — F5／exeダブルクリック／別CWDのPowerShell／CTest／Debug・Release・Ninja・VS。Sandbox起動で`LoadGraph failed`が出ないこと。
- [ ] **Step 4: Commit** — `build: generate per-exe development path settings`

### Task 5: Thread/Job基盤の継続監査

- [ ] **Step 1: 既存実装の再読**（`JobSystem.cpp`祖先Fence検査・`Threading.cpp` Yield対策を維持）
- [ ] **Step 2: 不足の再現テスト追加** — Fence破棄と最終通知の競合反復／ParallelFor途中失敗時の受理済みJob退役／Worker生成失敗時のJoin保証／デストラクタ再入／別System Fence（`Tests/ThreadingTests.cpp`へ、1件ずつRed→Green）
- [ ] **Step 3: 全回帰＋NoStl＋Commit** — `test: harden job system lifetime and reentry`

### Task 6: 並列Physicsの本体統合（`apply_stage2.py`は参考のみ・一括適用禁止）

- [ ] **Step 1: 差分精査** — `apply_stage2.py`の各hunkを現mainの対応箇所と突き合わせ、適用済み・未適用・陳腐化を分類
- [ ] **Step 2: 登録** — `ParallelPhysicsCore.cpp`を`dxf_physics`へ、`ParallelTests.cpp`を`CMake/PhysicsTests.cmake`へ（＋Private include dir）。`PhysicsExecution.h`は存在確認
- [ ] **Step 3: 1/N比較テスト実行** — 実World 2D/3D×1・2・4・Nレーンで位置・姿勢・速度・Sleep・接触順を比較＋診断カウンタで並列経路通過を証明
- [ ] **Step 4: CCD・Sleep・共通床・Island結合/分離の回帰** — 未対応は明示残課題化、許容値緩和禁止
- [ ] **Step 5: Commit群** — `feat: register parallel physics core`, `test: ...`, `feat: connect ...`

### Task 7: Application Task基盤（Prepare/Commit・Scope）

- [ ] **Step 1: `FApplicationExecution`等の既存有無を再確認**（あれば監査再利用、無ければToolboxへ小さなDispatcher新設）
- [ ] **Step 2: Prepare/Commit分離・Scope世代・親子キャンセル伝播のTDD**
- [ ] **Step 3: Scene寿命・Commit順序・再入のテスト**
- [ ] **Step 4: Commit**

### Task 8: 非同期Asset・並列描画命令

- [ ] **Step 1: 共通Resolver再利用の非同期読込API**（BMP/PCM16のCPU準備に限定、未対応形式は明示返却、Native取込は所有スレッド）
- [ ] **Step 2: 破損・上限・キャンセル・Shutdown競合のテスト**
- [ ] **Step 3: 描画Snapshot→chunk別バッファ→入力順統合**（Layer→Order→入力順維持、失敗時は既存キュー不変）
- [ ] **Step 4: Commit群**

### Task 9: 終了順序・Thread契約監査、文書、検証、push

- [ ] **Step 1: 三観点セルフ監査** — (a)所有・例外・終了 (b)競合・ordering・再入 (c)公開API・CMake・配布・パス。発見は再現テスト→修正→全回帰
- [ ] **Step 2: 文書保存** — `Docs/Assets/Paths.md`、`Docs/Threading.md`、`Docs/Physics/ParallelExecution.md`更新、`Docs/Development/Continuation.md`、`Docs/Validation/<RunId>/`（環境・SHA・コマンド・終了コード・件数・測定値）。成功件数の転記禁止
- [ ] **Step 3: commit前検査**

```powershell
git diff --check
git status --short
git diff --stat
```

- [ ] **Step 4: push手順**

```powershell
git fetch origin
git merge-base --is-ancestor origin/main HEAD
git push origin HEAD:main
git rev-parse HEAD
git ls-remote origin refs/heads/main
```

non-fast-forward時は相手差分を確認して統合・再検証。認証・保護ルール失敗は原文報告。

## Self-Review

1. Spec coverage: §3 Root→Task 1-4。§4 Job→Task 5。§5 Physics→Task 6。§6 Task→Task 7。§7 非同期Asset→Task 8前半。§8 描画→Task 8後半。§9 終了→Task 9。§10 ゲート→各Task内＋Task 9。§11 規約→Global Constraints。§12 文書/push→Task 9。§2 環境→Baseline済み。
2. Placeholder scan: Task 6-8はrecon起点を明示し、代表コードと判定基準を記載。`apply_stage2.py`の一括適用を禁止事項として明記。
3. Type consistency: `FAssetPathResolver::SetRoot/Resolve`、`ParseProjectPathSettings`、`ExecutableDirectory`、`SetProjectRoot`、`FApplicationSettings::ProjectRoot`を全Taskで同一名使用。

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-09-21-sln-asset-root-parallel-integration.md`. Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
