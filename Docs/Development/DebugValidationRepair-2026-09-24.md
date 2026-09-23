# 単独Debug検証構成の修復（2026-09-24）

開始HEAD / origin/main: `f110226b7e751bb3359a94d93b739219d16db7d5`。main、作業ツリーはクリーン。最新originをfetchして一致を確認してから実施した。前回のWorld問い合わせ記録は当時の結果として変更していない。対象は本記録と同じcommitの変更。

## 修正前の再現

新規 `Build/DebugValidation-before-440fdbf24e6a40409b6bdb0e9a217739` で、`cmake -S Tools/DebugValidation -B <上記> -A x64` は終了0。`cmake --build <上記> --config Debug --parallel 4 -- /v:normal` は終了1（161警告、26エラー）。失敗後にCTestや古いexeは実行していない。

- `core.lib(Render3DContext.obj)` から `FModelInstance::GetMorphWeight` / `GetNativeTime_Internal` が未解決。
- `DxLibModelBackend.obj` から `Toolbox::operator/(FPath, FPath)` / `ReadFileBytes` が未解決。
- View/Continuation系では `FRenderView3D` のC4324がC2220になった。修正前の実コンパイル行は `/W4 /WX`、通常rootは `/W4` と `DXF_WARNINGS_AS_ERRORS=OFF`。設定差を実ログ・生成プロジェクトで確認した。

ログは `Build/repair-before-configure.log` / `repair-before-build.log`。今回の実ビルドで構成不整合を再現した結果であり、新規ゲーム機能の挙動Redではない。

## 変更内容と警告

`Tools/DebugValidation/CMakeLists.txt` を薄い入口にした。`enable_testing()` を最上位で呼び、rootを `add_subdirectory` で取り込む。正規のToolbox / Support / Physics / Runtime / Gameplay / Debugを使い、coreへの本体cpp列挙と試験の重複登録を廃止した。rootの既定設定は変えていない。

Native、device試験、Viewer、Starter/Sandbox実行ファイル、installは専用入口でOFF。手書きNative境界はテストターゲットだけに限定した。生成プロジェクトから本体ライブラリにFakeDxLib/FakeNativeが混入しないこと、dxf_native / core / GUIサンプルのプロジェクトがないことを確認した。SDKの取得・再構築は行っていない。

従来 `/WX` だった次の4対象は、正規の `/W4` に加えPRIVATEの `/WX /wd4324` を適用した。

- dxf_render_continuation_tests
- dxf_job_fault_tests
- dxf_render_views_tests
- dxf_native_views_tests

FVector3/FQuaternionが `alignas(16)` を持ち、それを含むFRenderView3D等でメンバー間や末尾に意図したパディングが入る。構造体レイアウトは変更していない。C4324は警告の原因をなくしたのではなく、この4対象だけで通知を除外した。他の警告は引き続きエラー。`Build/repair-evidence.log` に最終生成設定のWarningLevel=Level4、TreatWarningAsError=true、DisableSpecificWarnings=4324を記録した。

最初の修復ビルド（`Build/DebugValidation-fixed-e9a81b4d264b46b5919c313806f49b8f`、生成0 / Debugビルド1）では、厳格設定によりデストラクターの `EndView3D()` のnodiscard戻り値破棄がC4834/C2220として検出された。`Source/Native/Private/Dxf/DxLibModelBackend.cpp` の一箇所を `(void)EndView3D()` とし、破棄の理由をコメントした。呼び出し、状態復元、例外時の後始末は同じ。C4834自体を抑止していない。

非MSVCでは旧coreから伝播していた-Werrorも、対応するToolbox/Support/Physics/Debug層と旧Debug/Transparency試験へ保持した。非MSVC実ビルドは未実施。

通常rootと同じ非厳格対象にはC4324、C4056、C4456、C4459、C4996の警告が残る。全ソースを一括で警告エラー化・警告抑止する変更はしていない。

## 修復後の新規ビルドと登録

`python Tools/ValidateDebug.py --logs Build/DebugValidationRepairLogs` を実行（終了0）。別の新規作業先 `Build/DebugValidation/run-yaig5eih/Debug` と `Release` からそれぞれ生成・全対象リンク・登録確認・直列CTestを実行した。Windows x64、Visual Studio 18 Community / MSVC 19.51。各工程のコマンド・終了コードは同ログディレクトリに保存。

| 工程 | Debug | Release |
|---|---|---|
| 生成 | 0 | 0 |
| 全ターゲットビルド | 0 | 0 |
| CTest JSON登録確認 | 21群、0 | 21群、0 |
| CTest `-j 1 --no-tests=error` | 21/21、0 | 21/21、0 |
| PhysicsContinuation内部 | 159/159（World問い合わせ7/7含む） | 159/159（同左） |
| DebugPhysicsCapture / RenderViews / NativeViewsTranslation内部 | 11/11、40/40、7/7 | 11/11、40/40、7/7 |

群数と内部ケース数は合算していない。旧13群はすべて保持した。追加8群は正規root登録のPhysicsContinuation、NoStl、Framework、ApplicationRenderIntegration、Models、NativeContract、FrameworkAltCwd、AssetRootLaunch。

旧群はDebugTools、DebugPhysicsCapture、RenderContinuation、JobFault-construction / submission / capture-wait / cross-system / fence-allocation、RenderViews、NativeViewsTranslation、RenderTransparency、RenderTransparencyFault、NativeTransparency。DebugPhysicsCaptureは正規登録のSnapshot選択・実RenderDebugまで含む11ケースになった。

旧 `debug_tests` / `debug_physics_tests` は `dxf_debug_tools_tests` / `dxf_debug_physics_tests` に統一。参照検索で現行ツールから旧実行ファイルへの依存がないことを確認した。トップから21群が見え、作業ディレクトリと実行パスはCTest JSONに保存されている。通常のソリューションへ専用検証プロジェクトを増やしていない。

## 再発防止と計測オプション

`Tools/ValidateDebug.py` は新規作業先で両構成を生成し、全対象リンク後にCTest JSONとCMakeCacheを検査する。旧群とPhysics群の欠落、重複、DISABLED、device登録、Native/GUI/installの有効化を拒否する。JUnitから登録名と実行結果を照合し、欠落・失敗・スキップを拒否する。過去の成功を残さない既存ValidationSupportを再利用した。

`Tools/Tests/test_debug_validation.py` の5件は追加群の許容、登録不備、Native設定不備、結果不一致・失敗・スキップ、リンク失敗時に旧exeを実行しない制御を確認する。PythonからSDKや実C++ビルドを無条件に要求しない。実構成の検証は上記の別実行で行った。

DXF_DEBUG_ASAN / DXF_DEBUG_TSANは、対応する非WindowsのGCC/Clangでコンパイラー・ランタイムのリンク可否を確認し、add_subdirectory前の設定で実装ライブラリ・ufbx・実行ファイルへ計測を適用する。併用と対象外環境は明示的に拒否する。

今回Windows/MSVCでASAN単独・TSAN単独・併用をそれぞれ新規Buildへ指定し、全て意図した生成失敗（終了1）を確認した。ログは `Build/repair-reject-ASAN.log` / `TSAN.log` / `BOTH.log`。非Windows GCC/Clangでの計測付きビルド・実行は未実施で、フラグ継承の構成確認のみ。ONを受理して何も計測しない成功とは扱わない。

## 同じ最終コードのroot回帰と配布

非MSVCの厳格設定を補足する前の最初のroot検証では、既存Native有効 `Build/FbxContinuation` でDebug→Releaseの順にビルド・全群を実行。`ctest -j 1` を使い、別構成や単独再試験を重ねて実行していない。SDKは既存DxLib 3.25a source / D3D11 / model extension 3 / MT・MTd。

| 実行 | 結果 | 終了コード |
|---|---|---|
| root Debugビルド | 成功 | 0 |
| root Debug全群 | 24/25、NativeModelDeviceSmoke失敗 | 8 |
| root Releaseビルド | 成功 | 0 |
| root Release全群 | 25/25 | 0 |
| Debug NativeModelDeviceSmoke単独再試験 | 1/1、13.76秒 | 0 |
| No-STL | 309ファイル、違反0 | 0 |
| Python | 24/24（既存19＋新規5） | 0 |
| 配布 | Native無効・Debug、8段階成功 | 各0 |
| git diff --check / UTF-8・CRLF・BOM | 成功 | 0 |

rootの最初の失敗ログは `Build/repair-root-ctest-debug.log`。同一テスト内のApplication試験が一部成功した後、後半の終了・再起動を含む経路で「real Application model draw failure shuts down scene before capture」から失敗し、最後は `Navigator cannot accept a scene request`、failures=6。前回記録の `The window was closed` やDxLib_Init失敗・タイムアウトと同一事象とは扱っていない。

試験のRunApplication_Internal、FDxLibPlatformのInitialize / PumpEvents / Shutdown、FApplication::Step_Internalを確認した。PumpEventsの継続不可や終了要求でフレーム処理を行わず終了する経路はあるが、今回のログだけではその原因を特定できない。コード・画素判定・時間上限を変更せず対象だけを単独再実行し成功した（`repair-root-native-model-retry.log`）。全群24/25と単独1/1を全群25/25へ合算しない。原因は未特定であり、競合・人手操作・環境問題とは断定しない。無期限の再試験は行っていない。

Releaseの全群ログは `repair-root-ctest-release.log`。全ビルドログは `repair-root-build-debug.log` / `release.log`。配布は `python Tools/ValidatePackage.py --logs Build/DebugValidationRepairPackageLogs` をx64開発者環境で実行。`Build/PackageValidation/run-6ybmjmfy` の移動済みパッケージを使い、`ConsumerBuild/PhysicsOnly.exe` が `dxf::physics` だけで生成・登録・問い合わせ・削除を実行して終了0。従来のframeworkとsupport単独Consumerも成功した。

## 最終版での再検証

最終レビューで非MSVCの旧core由来の-Werrorも明示的に保持した。この最後の構成変更後、次を新しいログ先で再実行した。途中失敗の記録を上書きしていない。

| 最終版の工程 | 結果 | 終了コード |
|---|---|---|
| 単独生成・全ビルド・登録・CTest | 新規 `Build/DebugValidation/run-7seexk9t/Debug` と `Release`、各21/21 | 全工程0 |
| root Debugビルド・全群 | 25/25、23.06秒 | 各0 |
| root Releaseビルド・全群 | 25/25、22.41秒 | 各0 |
| Python / No-STL | 24/24、309ファイル違反0 | 各0 |
| 配布 | Native無効・Debug、8段階成功、PhysicsOnly含む | 全工程0 |

単独は `Build/DebugValidationRepairFinalLogs/`、rootは `Build/repair-root-final-build-debug.log` / `repair-root-final-ctest-debug.log` と対応するreleaseログ、配布は `Build/DebugValidationRepairFinalPackageLogs/`。全群の途中失敗・単独再試験・最終版の全群成功はそれぞれ別実行であり、合算ではない。

## 完了範囲・未実施

単独入口の構成修復、正規実装・現行試験の両構成実行、制御ロジックの回帰、現行手順更新、配布確認まで完了。最終版のroot両構成も全群成功したが、途中で発生した上記モデル試験の失敗原因は残課題。

SDK未導入PC、開発ツール未導入PC、非WindowsのASan/UBSan/TSan実行、実D3D9、物理入力・聴感、全D3D/COM資源のリーク列挙は未実施。配布のNative有効・Release確認へ読み替えない。RaycastClosest、Snapshot選択、モデル/物理の挙動・ABI、Assets、空のStarterとSandboxは保持した。新しいゲームAPI・描画機能は追加していない。

[現行の再実行手順](../Testing.md#単独debug検証入口現行の正規ターゲット)。生成物・ログ・SDKはコミット対象外。
