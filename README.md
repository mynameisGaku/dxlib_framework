# UIのVisual Studio表示・フィルター登録の修正

前回の `dxlib_framework_17445ee_ui_continuation_update.zip` を適用したソース、または同じ全ソースZIPに追加する修正です。UIを実装する新しい指令書ではなく、生成設定の修正コードです。

## 直した登録漏れ

1. `Tools/SolutionHelpers.ps1` の通常ソリューションの表示対象に `dxf_ui` と `dxf_ui_runtime` がありませんでした。この2つを追加しました。開発用のサンプル・試験を通常版へ混ぜる変更ではありません。`.sln` と `.slnx` の両分岐が同じ対象一覧を使います。
2. `dxf_ui_sample` の実装cppは登録されていましたが、ヘッダーとスタイルのIDE登録がありませんでした。対応するh・`.dxfui`を登録し、既存の `dxf_ide_headers` によって実フォルダーと同じグループへ配置します。スタイルは表示専用で、C++としてコンパイルしません。
3. UI Runtime／Application／故障注入の試験、UIベンチマーク、NativeUiSmokeにも、関連ヘッダーとグループの登録を追加しました。

`Source/Ui` と `Source/UiRuntime` 自体のヘッダー登録は前回からありました。今回の「通常ソリューションからプロジェクトが落ちる問題」と「サンプル・試験のヘッダーが未登録の問題」を区別しています。

## 適用

ZIPをリポジトリ外へ展開し、`dxf_ui_filters_fix` フォルダーを置いてください。以下は、そのフォルダーをDownloads直下へ置いた場合です。**コマンドを実行する場所はリポジトリのルートです。**

```powershell
$Fix = "$HOME\Downloads\dxf_ui_filters_fix\apply_ui_filters.py"
python "$Fix" --root .
if ($LASTEXITCODE -ne 0) { throw "検査で停止しました。強制上書きしないでください。" }
python "$Fix" --root . --apply
if ($LASTEXITCODE -ne 0) { throw "適用で停止しました。表示された理由と退避先を確認してください。" }
.\GenerateProjectFiles.bat -Development -Open -NoPause
if ($LASTEXITCODE -ne 0) { throw "再生成に失敗しました。生成ログを確認してください。" }
```

`-Development`で生成した、ルートの `dxlib_framework-development.sln` または `.slnx` を開きます。通常版の古いソリューションを開いたままでは開発用プロジェクトを確認できません。再読み込みを求められたら、生成後の内容を読み込みます。

通常版にUIライブラリだけを表示する場合は `GenerateProjectFiles.bat -Open -NoPause` で通常版も再生成します。通常版に `UISample` がないのは意図した構成です。

### 表示場所

```text
dxf_ui
  Source/Ui/Public/Dxf
  Source/Ui/Private/Dxf

dxf_ui_runtime
  Source/UiRuntime/Public/Dxf
  Source/UiRuntime/Private/Dxf

開発用ソリューションのみ:
dxf_ui_sample
  Examples/UiSample           ← 画面・パネルなどのh/cpp
  Examples/UiSample/Styles    ← Button.dxfui、Tokens.dxfui
  Examples/GameplaySample     ← 共用する地形・キャラクター
UISample
  Examples/UiSample           ← WindowsMain.cpp等、実行ファイル側の入口
```

サンプル本体は `dxf_ui_sample` でコンパイルし、`UISample` がリンクする構成を保持しています。表示するためだけに同じcppを `UISample` へ再登録して二重コンパイルしません。UI試験と `NativeUiSmoke` のヘッダーは各プロジェクトの `Tests` 以下です。

`.vcxproj`、`.vcxproj.filters`、`.vcxproj.user` の手編集・削除、`.vs`削除、SDKの再Setupは、この修正手順に含みません。

## 適用器の保護

変更は既存3ファイルと、新規の検証用Pythonファイル1つです。C++製品コードは変更しません。

- 既定は検査だけ。`--apply` を付けた場合だけ書き込みます。
- 前回の継続更新とハッシュが一致するファイルだけを更新します。LF/CRLFの差だけは比較時に許容し、元の実バイトを退避します。
- 前回の更新が未コミットでも、対象が前回の配布内容と一致し、stage済みでなければ適用できます。独自編集・stage済み変更は停止します。
- 変更前ファイルと復元用の一覧はリポジトリの隣の `*-ui-filter-backup-*` へ保存します。失敗時の復元を試み、競合したものは上書きせず記録します。
- Gitの履歴・index・リモートを変更しません。commit/push、reset/clean/stashは実行しません。
- 旧17445eeのまま、または別のUI更新が入っている場合は一致しないため停止します。古いZIPを適用し直して合わせるのではなく、現行差分を確認してください。

## 確認したこと

実行環境はLinux、CMake 3.31.6／Ninja／GCCです。

- 修正前にCMakeを生成し、`dxf_ui_sample`等のヘッダー数が0、ソースが既定の `Source Files` グループであることを確認しました。
- 新しい試験は修正前に失敗。修正後はCMakeの実生成情報で、登録されたヘッダー・スタイル・グループを確認しました。
- 修正前後の33ターゲットで、コンパイルするcpp/cの一覧とリンク依存関係は一致しました。
- Native無効の通常構成は生成成功。UI製品2ターゲットはあり、UISample・UIサンプル本体はありません。
- Native無効・DebugのUI4群は4/4成功。
- Python全体47件中40件成功、7件skip。生成情報を指定した新規試験は7件中4件成功、3件skip。
- No-STLは544ファイル、違反0。
- 適用器15件成功。前回の全ソースZIPに対して検査・適用・再適用を実行し、対象4ファイルだけが変わり、Payloadとバイト一致しました。

**Windows／Visual Studioの実プロジェクト生成、GUIでのフィルター表示、PowerShellでの `.sln`／`.slnx` 変換の実行は、この環境では未実施です。** その試験はskipとして残しています。CMakeの生成情報の検査を実際のVisual Studio表示確認に読み替えていません。Release・実DxLib・配布・既存全群も今回再実行していません。前回のUI更新で明示した残課題を解決済みにする修正ではありません。

### Windowsで実際のフィルターを検査する場合

再生成した開発用ソリューションに対応するBuildディレクトリを指定します。これは任意の追加確認です。

```powershell
$env:DXF_CHECK_VS_BUILD = (Resolve-Path .\Build\VisualStudio-development).Path
python -m unittest discover -s Tools/Tests -p test_ui_ide_visibility.py -v
```

実際の `.vcxproj.filters` から、各ヘッダーとスタイルの所属フィルターを確認します。`DXF_CHECK_UI_BUILD`を設定する別の試験はCMake File API用のため、未指定ならskipします。

参照: CMake公式 `source_group` と `cmake-file-api(7)`。本修正はこのリポジトリの既存 `dxf_ide_headers` を再利用しています。

- https://cmake.org/cmake/help/v3.31/command/source_group.html
- https://cmake.org/cmake/help/v3.31/manual/cmake-file-api.7.html
