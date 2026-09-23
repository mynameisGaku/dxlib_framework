# ufbx（同梱）

FBXファイルの解析に使う単一ファイルのC言語ライブラリです。フレームワークは`.fbx`をこのライブラリで読み、
DxLibが直接読めるモデルデータへ変換します（`Source/DxLibSupport/Private/Dxf/ModelImport.cpp`）。
Autodesk FBX SDKは使いません。

| 項目 | 値 |
|---|---|
| 入手元 | https://github.com/ufbx/ufbx |
| 版 | v0.23.0（タグのコミット `fcc5d6ba444cfd3eb80677dba5e37e493941abe5`） |
| ライセンス | MIT または Public Domain（Unlicense）の選択制。`LICENSE`を参照 |
| 同梱ファイル | `ufbx.h`、`ufbx.c`、`LICENSE`（内容は無変更） |

取得時（改行LF）のSHA-256:

- `ufbx.h` `942481725372d2ac4da5e77a062b47c20054a3440e7ee09a6043f99fe1f130ed`
- `ufbx.c` `7d8d6ae4373f71692f295ff49ee0826466306ebcaa80b0e587c13ed047b98cea`
- `LICENSE` `0dd48ebadf52273c736256325c8f078c03c8bb4facee22a4122de0ad3f615391`

作業ツリーの改行はリポジトリの規則に従います（Gitの保存内容は取得時と同じLF）。
更新するときは、上記の版・コミット・ハッシュを書き換え、`Tests/ModelTests.cpp`と`Tools/FbxModelProbe`の結果を確認します。
