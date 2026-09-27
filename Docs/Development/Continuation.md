> 2026-09-23整理: この記録にある旧ルートの`apply_*.py`などは[履歴保存先](../Archive/IntegrationPackages/README.md)へ移動済み。以下の結果・手順は当時の記録です。

# 開発継続記録

## 現在地（2026-09-27更新）

### 実装済み・このSHAで検証済み

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
- P7完了：並列描画命令生成（入力順一括反映・失敗時不変）。
- S1〜S4完了：カプセル幾何・World統合・キャラクターカプセル・Dynamic押し合い
  （`fa2c762`、`e33577e`、`2fa6043`、`b2f8e3c`、`e4ba140`）。
- S5完了（`fafaee9`）：Solverとイベントの組の候補を問い合わせの索引から集める経路。
  総当たり参照との一致（2D／3D）、疎な1024個での候補削減、索引不可座標でのフォールバックを検証。
- `fafaee9`で記録した「3Dの箱どうしの接触喪失」は、2026-09-28の調査で再現せず、記録した原因も事実でなかった
  （閾値の変更は元へ戻し、回帰試験`BoxWallContactTests`を追加）。詳細は[Physicsの進捗記録](../Physics/Progress.md)の
  「個別記録(KD)」。

### 検証結果（SHA `fafaee9`）

- `dxf_physics_tests.exe`（Release）: 27群すべて成功。3回連続で同じ結果（終了コード0）。
- `dxf_physics_tests.exe`（Debug）: 終了コード0。
- `dxf_tests.exe`（Release）: 364/364。
- `python Tools/CheckNoStl.py`: 645ファイル、違反0。

### 未実施（この指令書の残り）

- S6: GameplaySampleへカプセル・低い通路・しゃがみ／天井・押せる箱・圧力板・扉・移動床＋箱・
  Trigger・Checkpoint・Round／Capsule切替・Push ON／OFFのゲーム経路を追加。
- S7: Benchmark系列（capsule-static／dense／trigger、push-1／16／64、broadphase-off／on／
  sparse／dense、capsule-moving-floor）、配送予約の1 allocation／Step、故障注入、変異。
- S8: 最終回帰（Debug／ReleaseのCTest全群、ValidateDebug、NoSTL、Python、公開ヘッダー単独、
  通常／Development solution、Package Native 4構成、Benchmark、IDE filterの自動検査）。
- このSHAは未push。`origin/main`は`385bd96`、ローカルは6コミット先行。
  backup参照: `refs/backup/main-20260927-e4ba140`（S5着手前）。

## 開始位置（P0系 multipliers の開始時）

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
