# アセットパスの解決基準

## Rootの定義

ProjectRootは、ユーザーが開くrootのsln／slnxを配置するディレクトリである。
`Assets/`フォルダーではなく、その親を指す。`Assets/`の二重付与はしない。

```text
<ProjectRoot>/
    dxlib_framework.slnx
    Assets/player.bmp
    Assets/confirm.wav
    Source/
    Build/
```

`Assets.LoadTexture("Assets/player.bmp")`は、どのexe・どの作業ディレクトリから起動しても
`<ProjectRoot>/Assets/player.bmp`の原本を読む。

## 選択の優先順位

Applicationの起動準備で一度だけ確定し、アセット使用とWorker起動に先立つ。

1. `FApplicationSettings::ProjectRoot`の明示設定。完全修飾パスを要求し、
   相対パスなど不正な値は`Start`が`InvalidArgument`で失敗する。低優先へ逃げない。
2. exeと同じ場所の`<Exe名>.dxfpaths`（例`Sandbox.dxfpaths`）。
   中の`ProjectRootRelative`はCurrentDirectoryではなくexeディレクトリから解決する。
3. 開発パス設定がない配布構成ではexeディレクトリをRootにする。
   exe横に`Assets`を置けばsolutionなしで起動できる。

設定ファイルが壊れている・指すRootが存在しない場合、3へフォールバックせず起動失敗にする。

## `.dxfpaths`形式

```text
Version=1
Mode=Development
ProjectRootRelative=../../../
```

- `Version`は1のみ有効。`Mode`は用途表示で解決には使わない。
- 相対値はexe配置先基準。別ボリューム等の絶対値は開発専用としてそのまま使う。
- CMakeの`dxf_runtime_paths`がPOST_BUILDで生成する。手書きの`../..`固定はしない。
- 生成物は内容が変わるときだけ書き換える。

## 設定ファイルの読み取り

`Toolbox::TryReadSettingsFile`が次を区別する（上限4096バイト）。

- 存在しない → 呼び出し側は配布構成（exe配置先）を使う。
- 共有違反・権限・種別・上限超過・読込失敗 → 例外。配布構成へ逃げない。
- ディレクトリ等 → 例外。
- 全文を読み切ってから返す。切り捨てた先頭だけを有効扱いにしない。

設定本文が空・不正UTF-8・未対応Version・欠落／重複キーは解析で拒否する。
`Examples/Shared/ProjectRootEntry.h`の入口はこの読込と
`ResolveDevelopmentRoot`＋`IsDirectory`確認を組み合わせる。

## 解決規則

`Toolbox::FAssetPathResolver`が字句的に解決する。ファイルへ触れない。

- 相対パスはRootと結合して正規化する。`.`・Root内の`..`は正規化し、
  同じ結果は同じキャッシュキーになる。
- 完全修飾の絶対パス（ドライブ付き・UNC）はRootを前置しない。
- Root外へ出る`..`、`C:foo`等のdrive-relative、`\foo`・`/foo`等の曖昧な
  root-relative、空文字、埋め込みNUL、不正UTF-8は`InvalidArgument`で拒否する。
- Rootは解決基準であり隔離境界ではない。symlink先の検証はしない。

## 同期・キャッシュ

- `LoadTexture`・`LoadSound`は共通経路で解決済み絶対パスをLoaderへ渡す。
  キャッシュキーは解決済みパス＋読み込み条件である。
- 別Rootの同名ファイルは別資源になる。サービスごとに解決器を持つ。
- Rootは利用開始後に変更できない。二度目の設定は拒否される。

## 起動別の挙動

| 起動方法 | Root |
|---|---|
| Visual Studio F5（Sandbox/Starter） | `.dxfpaths`の指すroot solution配置先。デバッガ作業ディレクトリも同所 |
| exeダブルクリック（開発ビルド） | 同上 |
| exeダブルクリック（配布・設定なし） | exe配置先 |
| CTest・任意CWDのPowerShell | 同上（CurrentDirectoryを読まない） |
| Debug／Release・Ninja・VS multi-config | 構成ごとの出力先に生成した設定を使う |

## 配布例

配布物に開発パス設定を入れない。exe横に`Assets/player.bmp`等を置く。
`Starter.dxfpaths`がない`Starter.exe`は自配置先をRootにする。

## 起動試験

`dxf_asset_probe`（`Tests/AssetProbe/Main.cpp`）はexe入口と同じ解決を行い、
解決したRootを標準出力へ出す。`Tools/VerifyAssetRoot.py`が開発・競合・欠落・
配布・破損・移動・日本語パス・ロックの配置を作り、CTestの`AssetRootLaunch`
として検証する。実DxLibのSandbox画面・音声の目視は別項目である。

## エラー例

- `Texture path cannot resolve against root <Root>: <要求パス>` —
  Root外・曖昧・不正入力。要求パスと選択Rootを併記する。
- `ProjectRoot must be an absolute directory` — 明示設定が相対パス等。
- `Broken development path settings` — 設定ファイルの形式・版の異常。
- `Project root from development path settings is missing` — 指すRootが存在しない。
