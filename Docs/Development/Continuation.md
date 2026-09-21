# 開発継続記録

## 現在地（2026-09-22更新）

- P0完了：`/showIncludes`接頭辞の文字化け注入を廃止し、CMake自動検出へ。
  fixtureと実ツリー＋隔離worktreeで増分再ビルドを検証。
- P1完了：`.dxfpaths`の不存在と読込失敗の区別、`dxf_asset_probe`起動試験。
- P2完了：Physics検証統一とWorld実行設定の接続。
- P3完了：Thread/Job監査（投入失敗・他System待機・全Worker子待ち・破棄再入・
  二重Shutdownの回帰を追加、契約文書を更新）。
  未実施：Worker生成失敗の故障注入（注入点なし、停止・Join経路は目視監査のみ）。
- P4完了：Island Solver並列化（静的書込の除去、1/N一致、SolverIslandCount）。
  残りは起床伝播の追加回帰とCCD方針の明文化。
- P5完了：Application共有Task基盤（Dispatcher・Scope・Step反映・終了順序）。
- P6完了：非同期Asset（CPU検証・メモリ取込・期限切れチケット）。
- P7進行中：並列描画命令生成（入力順一括反映・失敗時不変）。
- 残り：P8 最終検証。

## 開始位置

- 開始HEAD: `68b4456e4686af7f33f3be54f35615c03fb94dd6`（main、origin/mainと一致、作業前クリーン）
- 作業前backup参照: `refs/backup/main-20260921-68b4456`
- ベースライン（同HEAD内容・前回検証）: `dxf_tests` 212/212、NoStl 182ファイル0違反

## 実装済み（未push）

1. sln基準アセットRoot（`Tests/AssetRootTests.cpp` 17件）
   - `Toolbox::FAssetPathResolver`、`ParseProjectPathSettings`、
     `ResolveDevelopmentRoot`（`Source/Toolbox/Public/Toolbox/ProjectPaths.h`、
     `Source/Toolbox/Private/Toolbox/ProjectPaths.cpp`）
   - `Toolbox::ExecutableDirectory`、`Toolbox::IsDirectory`
    （`Platform.h`／`Platform.cpp`）
   - `FAssetService::SetProjectRoot`／`GetProjectRoot`、全Load経路の共通解決
   - `FApplicationSettings::ProjectRoot`と`Start_Internal`での確定
   - `Examples/Shared/ProjectRootEntry.h`、Sandbox／Starterの`WindowsMain`接続
   - CMake `dxf_runtime_paths`＋`CMake/WriteDxfPaths.cmake`、
     `VS_DEBUGGER_WORKING_DIRECTORY`をrootへ
2. JobSystem監査の副産物
   - 祖先Fence検査は68b4456に取得済み。今回は完了カウンタをFence通知より先に
     確定し、`Wait`帰還後の件数観測を安定させた
    （`Tests/ThreadingTests.cpp`「ten thousand jobs」高負荷時の失敗を修正）。

## 検証結果

- `dxf_tests.exe`（Ninja Release）: **228/228 passed**（資産17＋既存211）
- `python Tools/CheckNoStl.py`: 186ファイル0違反
- VS Debug: `Sandbox`・`Starter`のビルド成功、警告0、
  `Build/VisualStudio/Debug/Sandbox.dxfpaths`・`Starter.dxfpaths`の生成を確認
  （内容`ProjectRootRelative=../../../`）
- Sandbox／Starterの実起動確認は未実施（次項へ）。

## 残課題

- Task 5: Thread/Job基盤の継続監査（Fence寿命・確保失敗・再入の再現検査）
- Task 6: 並列Physicsの本体統合（`apply_stage2.py`は参考のみ、一括適用禁止）
- Task 7: Application Task基盤（Prepare/Commit・Scope）
- Task 8: 非同期Asset・並列描画命令
- Task 9: 終了順序・Thread契約監査、Sanitizer、性能測定、push
- 実機確認：F5／exeダブルクリック／別CWD／配布配置でのSandbox起動表

## 次の失敗テスト

- `.dxfpaths`ありのSandboxが別CWDからroot原本を読む起動試験
- 原本欠落時に古いexe横コピーへ逃げずNotFoundになる試験

## 注意（ビルド運用）

- ヘッダー依存追跡の停止原因を特定した。`BuildWindows.ps1`がPowerShell経由で
  採取した`/showIncludes`接頭辞が文字化けし、`DXF_MSVC_INCLUDE_PREFIX`経由で
  `rules.ninja`の`msvc_deps_prefix`へ焼き込まれていた。Ninjaのバイト比較が
  一致せず、ヘッダー依存が一件も記録されない（`ninja -t deps`で`#deps 0`）。
  対策としてスクリプト側の測定・注入を廃止し、空の既定値ではCMakeの自動検出を
  使う。`DXF_MSVC_INCLUDE_PREFIX`は手動上書き専用に残す。
- ヘッダー編集後は初回だけ対象ターゲットを`ninja -t clean`して
  poisoned時代の依存記録を流すこと。以後は通常の増分再ビルドでよい。
  例：`ninja -C Build/windows-release -t clean dxf_tests dxf_support
  dxf_toolbox dxf_runtime dxf_gameplay dxf_foundation dxf_physics`
- `ninja -d explain`と`ninja -t deps <obj>`で増分動作を確認できる。

## よく使うコマンド

```powershell
$env:VSLANG = 1033
cmake -S . -B Build/windows-release -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C Build/windows-release dxf_tests.exe
python Tools/CheckNoStl.py
.\GenerateProjectFiles.bat
cmake --build Build/VisualStudio --config Debug --target Sandbox Starter
```
