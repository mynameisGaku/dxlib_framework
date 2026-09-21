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

## 非同期読み込み

`LoadTextureAsync`・`LoadSoundAsync`は要求をWorkerの準備と所有側の取込に分ける。

- 要求受付時に論理パスを絶対パスへ解決し、WorkerはRootを再評価しない。
- Workerはファイル読み・形式検証（非圧縮BMP・PCM16 WAVE）を行い、原本バイト列を保持する。
- 所有側は準備済みバイト列を`LoadTextureMemory`・`LoadSoundMemory`で取り込む。
  同じファイルを読み直さない。Stream保持の非同期は明示的に拒否する。
- チケットは完了・失敗・期限切れを確定する。取り消しや破棄で反映しなかった
  要求は未完了のまま残るため、無期限に待たないこと。
- Nativeハンドルの生成・解放は所有側で行う。共有参照の破棄は呼び出し側が
  所有スレッドで行う契約である（同期APIと同じ）。
- 同期キャッシュと資源を共有する。解決済み同一パスは再利用される。

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

## 描画命令の並列生成

`FRenderQueue2D::SubmitGenerated`は借用Job Systemで命令を並列生成し、
入力順に一括で追加する。通常の`OnDraw`や即時操作は所有スレッドのままである。

- 各添字は専用領域へ一度だけ書き込み、入力順に統合する。
  同順位の命令は入力順を保つ（後のソートも安定順である）。
- 一つでも生成・検証に失敗したらキューを変更しない。
- 生成中は資源の寿命を呼び出し側で保つ。最終検証は所有スレッドで行う。
- `RenderTarget`変更・`Clear`・即時`Native`・`ScreenFlip`は所有スレッドで実行し、
  並べ替えの境界とする。

## エラー例

- `Texture path cannot resolve against root <Root>: <要求パス>` —
  Root外・曖昧・不正入力。要求パスと選択Rootを併記する。
- `ProjectRoot must be an absolute directory` — 明示設定が相対パス等。
- `Broken development path settings` — 設定ファイルの形式・版の異常。
- `Project root from development path settings is missing` — 指すRootが存在しない。
