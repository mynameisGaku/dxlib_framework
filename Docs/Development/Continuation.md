# 開発継続記録

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

- 日本語ロケールの`/showIncludes`をNinjaが解析できず、ヘッダ編集時に
  依存先が再ビルドされない。ヘッダ編集後は対象ターゲットを
  `ninja -t clean <targets>`してからビルドする。
  （今回`SupportTests.cpp.obj`等9/13の stale でODR不整合の停止を経験）
- 失敗例：`ninja -C Build/windows-release -t clean dxf_tests dxf_support
  dxf_toolbox dxf_runtime dxf_gameplay dxf_foundation dxf_physics`

## よく使うコマンド

```powershell
$env:VSLANG = 1033
cmake -S . -B Build/windows-release -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C Build/windows-release dxf_tests.exe
python Tools/CheckNoStl.py
.\GenerateProjectFiles.bat
cmake --build Build/VisualStudio --config Debug --target Sandbox Starter
```
