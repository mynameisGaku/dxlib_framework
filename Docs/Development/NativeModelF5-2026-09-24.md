# NativeModelSmokeのVisual Studio F5起動設定 検証記録

開始SHA: `7dc0cbd0feae672186d36a76d24d95afe283a3dc`。main / origin/main一致、開始時の作業ツリーはクリーン。過去の記録は変更していない。

## 修正範囲

- `CMakeLists.txt`: `if(DXF_BUILD_NATIVE_SMOKE)`内、`NativeModelSmoke`の作成直後に、対象限定で`VS_DEBUGGER_COMMAND_ARGUMENTS`（`"<CMAKE_CURRENT_SOURCE_DIR>" "<CMAKE_CURRENT_BINARY_DIR>/model-smoke-vs-$<CONFIG>"`、各引数を引用符で囲む）と`VS_DEBUGGER_WORKING_DIRECTORY`（`$<TARGET_FILE_DIR:NativeModelSmoke>`）を設定した。`if(DXF_RUN_DEVICE_TESTS)`の外なので、デバイス試験のCTest登録がOFFでも付く。
- 変更していないもの: `Main.cpp`の二引数契約・Usage・終了コード2、画素条件、120秒上限、診断、CTestの二引数・出力先`model-smoke`・登録条件、通常ソリューション、既定の起動対象、Starter／Sandbox／ModelViewer。C++・生成スクリプトの変更はない。生成済みの`.vcxproj`／`.vcxproj.user`は手修正していない。
- `Tools/Tests/test_debugger_settings.py`（新規）: 生成された`.vcxproj`の`LocalDebuggerCommandArguments`／`LocalDebuggerWorkingDirectory`を構成別に読み、二引数・引用符・順序・構成別出力先・未展開の式・作業ディレクトリを調べる。合成データの6件は常に実行する。実生成物の確認1件は、環境変数`DXF_CHECK_VS_BUILD`で生成済みBuildを指定したときだけ実行し（通常の`unittest discover`ではskip）、他のプロジェクトに`model-smoke-vs-`が付いていないこと（対象限定）も確認する。SDKやVisual Studioを全環境へ要求しない。
- `Docs/Testing.md`: F5での起動手順、自動で付く引数と出力先、CTestとの違い、直接起動の二引数契約を追加した。

## 生成設定の確認

生成は`GenerateProjectFiles.bat -Development -NoPause`（終了0）。使用したCMakeはVisual Studio同梱の`C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe`、Generatorは`Visual Studio 18 2026`（x64、インスタンス`C:\Program Files\Microsoft Visual Studio\18\Community`）、`DXLIB_ROOT`は`ThirdParty/DxLib-3.25a/DxLib_VC/プロジェクトに追加すべきファイル_VC用`、`DXF_RUN_DEVICE_TESTS=OFF`。修正前の`Build/VisualStudio-development`は、キャッシュ上のCMAKE_COMMANDが`C:/Program Files/CMake/bin/cmake.exe`だった（同じGenerator）。生成スクリプトは`--fresh`で再構成し、その後のNativeModelSmokeのビルドにも同梱のCMakeを使った。

| 確認 | 修正前 | 修正後 |
|---|---|---|
| `CMakeLists.txt`のNativeModelSmokeの`VS_DEBUGGER_*` | なし（Sandbox・ModelViewer・Starterの作業ディレクトリだけ） | 引数・作業ディレクトリあり |
| 生成された`NativeModelSmoke.vcxproj`の`LocalDebugger*` | 0件（`prefix-NativeModelSmoke.vcxproj`） | Debug／Releaseの各条件付きで引数・作業ディレクトリ |
| `test_debugger_settings.py`の実生成物確認 | 失敗（両構成とも「二つの引用符付き引数がない」。`red-python.log`） | 成功（`green-python.log`） |

修正後の生成値（Debug。Releaseは`Debug`を`Release`に置き換えた値）:

```text
LocalDebuggerCommandArguments: "C:/Users/g0190/OneDrive/Desktop/frame/dxlib_framework" "C:/Users/g0190/OneDrive/Desktop/frame/dxlib_framework/Build/VisualStudio-development/model-smoke-vs-Debug"
LocalDebuggerWorkingDirectory: C:/Users/g0190/OneDrive/Desktop/frame/dxlib_framework/Build/VisualStudio-development/Debug
```

生成物の絶対パスはこのPCの`CMAKE_CURRENT_SOURCE_DIR`／`CMAKE_CURRENT_BINARY_DIR`から展開されたもので、CMakeLists.txtには固定パスを書いていない。未展開の`$<`／`${`はない。`model-smoke-vs-`を含むプロジェクトはNativeModelSmokeだけだった。既存の`NativeModelSmoke.vcxproj.user`は空の`PropertyGroup`だけで、生成設定を上書きしていない（削除していない）。デバイス試験ON（`Build/FbxContinuation`、`DXF_RUN_DEVICE_TESTS=ON`）でも同じ設定が生成された（`device-on-settings.log`）。Native無効の新規Build（`Build/NativeModelF5-NativeOff`、`DXF_BUILD_NATIVE=OFF`）は構成・生成とも終了0で、NativeModelSmokeのプロジェクトは作られない（存在しないターゲットへの設定で失敗しない）。

## 起動の確認

Visual StudioのGUIでのF5操作は、この作業環境では実行していない。代わりに、生成された`NativeModelSmoke.vcxproj`から実行ファイル・引数・作業ディレクトリを読み出し、同じ条件でプロセスを直接起動した（`launch.py`）。記録上の扱いは「生成設定確認＋同条件での直接起動」で、実際のVS F5操作の確認済みではない。NativeModelSmokeのDebug／Releaseビルドは同梱のCMakeで各終了0。起動はすべて直列。

| 起動 | 引数 | 終了コード | 最終RESULT |
|---|---|---|---|
| 生成設定どおり Debug | 生成値（出力`model-smoke-vs-Debug`、40ファイル作成） | 0 | `REAL_SDK_MODEL_SMOKE_PASSED failures=0` |
| 生成設定どおり Release | 生成値（出力`model-smoke-vs-Release`、40ファイル作成） | 0 | `REAL_SDK_MODEL_SMOKE_PASSED failures=0` |
| 引数なし Debug | なし | 2（期待どおり） | なし。Usageを表示 |
| 空白を含む出力先 Debug（1回目） | 二段の新規パス`model smoke space test/out Debug` | 1 | `FAILED failures=1`（`Cannot create directory`） |
| 空白を含む出力先 Debug（2回目） | 既存の親の下の新規`model smoke vs space Debug`、引用符付きの一つのコマンド行 | 0 | `REAL_SDK_MODEL_SMOKE_PASSED failures=0`（40ファイル作成） |

空白を含む出力先の1回目の失敗は**検証手順の誤り**で、製品・設定の不具合ではない。親ディレクトリも存在しない二段の新規パスを渡したが、`Toolbox::CreateDirectory`は既存の契約どおり一段だけを作る。引数はそのまま届いていた（失敗はディレクトリ作成）。理由を記録してから、親が存在する一段の新規ディレクトリで、生成設定と同じ引用符付きのコマンド行文字列（Pythonの自動引用に頼らない）として1回だけやり直した。日本語を含む出力先での起動は実行していない（ソースルートや生成時のSDKパスに日本語が含まれることとは別で、出力先の日本語は未検証）。

## 回帰

デバイス試験ONの既存Build（`Build/FbxContinuation`。PATHの`C:/Program Files/CMake/bin/cmake.exe`で既存と同じGenerator、登録一覧に`NativeModelDeviceSmoke`あり）で、同じ最終CMake・ソースのroot全群をDebug→Releaseの直列で各1回実行した。

| 工程 | 結果 | 終了コード |
|---|---|---|
| root構成 | TESTS・NATIVE・STARTER・EXAMPLE・MODEL_VIEWER・RENDER_DEBUG・NATIVE_SMOKE・RUN_DEVICE_TESTSを明示ON | 0 |
| root Debugビルド／CTest（`-j 1 --no-tests=error --output-on-failure`） | 26/26。`NativeModelDeviceSmoke`は`REAL_SDK_MODEL_SMOKE_PASSED failures=0` | 0／0 |
| root Releaseビルド／CTest（同上） | 26/26。`NativeModelDeviceSmoke`は`REAL_SDK_MODEL_SMOKE_PASSED failures=0` | 0／0 |
| 単独 `python Tools/ValidateDebug.py --logs Build/NativeModelF5-StandaloneLogs`（Native無効） | Debug 22/22、Release 22/22、8工程PASS | 0 |
| Python `unittest discover -s Tools/Tests` | 31件中30件成功、1件skip（実生成物確認。環境変数なし） | 0 |
| No-STL | 328ファイル、違反0 | 0 |
| 配布 `python Tools/ValidatePackage.py --logs Build/NativeModelF5-PackageLogs`（x64開発者環境、Native無効・Debug） | 8段階PASS | 0 |
| `git diff --check` | 問題なし | 0 |

コード指紋（`git diff -- . ':!Docs'`とDocs以外の未追跡ファイルの内容のSHA-256）: 上表の実行の前後とも`44512ef3c060beed0cb2f698e67e9c0929e393ac4c790dcc881f7bdb1f7a0405`で一致。その後、新規の`test_debugger_settings.py`がLF改行で作成されていたため、内容を変えずにCRLFへ変換した（最終指紋`e83e99794d6724d941a602f3843cc33a7620bbbbe994f85c48f2030be82e29d0`）。このファイルはroot・単独のビルド・CTestと配布には含まれないため、影響するPython unittest（31件中30件成功・1件skip）と、両Buildでの実生成物確認（各終了0）、`git diff --check`を変換後に再実行した。改行はPythonでバイト検査した（変更・追加したファイルはUTF-8・CRLF・BOMなし）。既存の`test_solution.py`の改行混在は今回の変更対象外で、そのままにした。

## モデル実描画の偶発終了との関係

今回の全群（Debug・Release各1回）と直接起動4回では、`ProcessMessage=-1`による早期終了は発生しなかった。これは限られた回数の結果であり、前回（`WorldSweepNormals-2026-09-24.md`）のroot Release 25/26を書き換えるものではなく、F5設定で偶発終了が直ったことを示すものでもない。偶発終了の起源は引き続き特定していない。引数不足による終了コード2は、偶発終了・DxLib初期化失敗・画素不一致・時間切れとは別の、意図した引数検査の結果である。

## ログ

`Build/NativeModelF5-Logs/`（修正前の状態・プロジェクト、生成・ビルドの出力と終了コード、Red／Greenの設定確認、起動ログ`launch-*.log`と`launch-summary.log`、root・単独・No-STL・Python・配布のコンソール出力、`*-LastTest.log`、指紋、実行スクリプト）、`Build/NativeModelF5-StandaloneLogs/`、`Build/NativeModelF5-PackageLogs/`、Native無効の生成確認は`Build/NativeModelF5-NativeOff/`。F5用の出力は`Build/VisualStudio-development/model-smoke-vs-Debug`／`-Release`。生成物・ログ・SDKはコミットに含めない。

## 未実施

VS GUIでの実際のF5操作、日本語を含む出力先での起動、SDK未導入PC、開発ツール未導入PC、実D3D9、物理入力・聴感、D3D全資源のリーク列挙は未実施。配布はNative無効・Debugであり、Native有効・Release配布の検証へ読み替えない。
