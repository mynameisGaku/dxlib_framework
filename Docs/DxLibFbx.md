# FBX対応DxLibの再現可能なビルド

.mv1へ事前変換せず、`.fbx`をDxLibの`MV1LoadModel`で直接読み込むために、DxLib本体をFBX読込有効でソースからビルドします。
公式VC SDKの`DxLib_vs2015_x64_MT.lib`だけがFBX読込を有効にしてビルドされており（FBX SDKは同梱されない）、
Debug・MD版はFBXを読めないため、構成ごとの差をなくす目的です。実行時ライブラリは既存どおり`/MTd`・`/MT`です。

## 利用者が行うこと

1. 公式のDxLibソース`DxLibMake3_25a.zip`（https://dxlib.xsrv.jp/DxLib/DxLibMake3_25a.zip）を`ThirdParty/`へ置く。
   SHA-256 `2f09078692d3b64448c6ffe80392d77d0f413ce652115a1d7f0063baf475322a`。
2. Autodesk FBX SDK 2020.3.11（VS2022、Windows）を**ご自身で**インストールし、付属の利用許諾（EULA）に同意する。
   入手元はAutodeskの公式ページ（https://aps.autodesk.com/developer/overview/fbx-sdk）です。
   このリポジトリのスクリプトはSDKのダウンロード・インストール・規約同意を行いません。
3. 次を実行する。

```powershell
powershell -ExecutionPolicy Bypass -File Tools\DxLibFbx\BuildDxLibFbx.ps1
# 出力: ThirdParty\DxLib-3.25a-fbx（include/、lib/、DxLibFbx.json）
cmake -S . -B Build\windows-fbx-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug "-DDXLIB_ROOT=<公式SDK>" `
  "-DDXF_DXLIB_CUSTOM_ROOT=$PWD\ThirdParty\DxLib-3.25a-fbx" -DDXF_REQUIRE_FBX_MODEL=ON -DDXF_BUILD_NATIVE=ON ...
```

SDK本体・ビルド生成物（`ThirdParty/`、`Build/`）はGitへ追加しません。

## ビルドの内容

- `Tools/DxLibFbx/CMakeLists.txt`は、公式`DxLibMake.vcxproj`の`ClCompile`一覧（77ファイル）をそのまま読み、x64でビルドします。
  公式プロジェクトはWin32構成だけのため、x64の構成は同じ定義（`WIN32;_LIB;_DEBUG|NDEBUG`）で作ります。
  ソースはShift_JISなので、`/source-charset:.932 /execution-charset:.932`で文字コードを固定します。
- `DX_LOAD_FBX_MODEL`はDxLib本体のコンパイル時に定義します。アプリ側で定義しても既存libの機能は変わりません。
  静的な`libfbxsdk-mt.lib`を使うため`FBXSDK_SHARED`は定義しません。
- `BuildDxLibFbx.ps1`は、ソースZIPのハッシュ照合、ソースと公式SDKのヘッダー一致の確認（同じ版であること）、
  Debug / Releaseのビルド、公式SDKを変更しない別ディレクトリへの出力、`DxLibFbx.json`の作成を行います。
  マニフェストには各ライブラリのパス・SHA-256・出所、FBX SDKの版と場所、コンパイラ、定義、実行時ライブラリを記録します。

## リンクの構成

`CMake/FindDxLib.cmake`は`DXF_DXLIB_CUSTOM_ROOT`が指定されると次のように動きます（未指定時は従来の公式SDKの自動リンク）。

- `DX_LIB_NOT_DEFAULTPATH`を定義し、`DxLib.h`の自動リンクを無効にする（公式のDxLib本体を二重にリンクしないため）。
- マニフェストに記録したライブラリだけを構成別に明示リンクし、設定時にSHA-256を照合する。選択したライブラリは設定ログへ出す。
  - 本体: ソースからビルドした`DxLib_fbx_x64_MT(d).lib`。
  - DxLib同梱: 自動リンクと同じ組（DxUseCLib、DxDrawFunc、Bullet、libtiff、libpng、zlib、libjpeg、Ogg/Vorbis/Theora、Opus）の`_vs2015_x64_MT(d)`。
  - FBX SDK: `libfbxsdk-mt.lib`、`libxml2-mt.lib`、`zlib-mt.lib`（readmeの指示どおり明示リンク）。
  - Windows: `bcrypt.lib`（FBX SDK同梱のlibxml2 2.15.3が`BCryptGenRandom`を使う。リンク時の未解決シンボルで確認）。
- `DXF_DXLIB_HAS_FBX=1|0`を定義し、`DXF_REQUIRE_FBX_MODEL=ON`ではFBX対応ビルドでなければ設定段階でエラーにする。

DxLib同梱のzlib 1.2.12とFBX SDKのzlib 1.3.2は、Debug / Releaseとも重複定義エラーなくリンクできることを確認しました。

### 名前修飾の橋渡し（`Tools/DxLibFbx/DxLibV140AbiBridge.cpp`）

公式の`DxUseCLib_vs2015_*.lib`はVS2015（v140）でビルドされています。`DxMovie.h`は`namespace DxLib`の中で、
未宣言の`struct SETUP_GRAPHHANDLE_GPARAM`を関数引数で初めて参照します。v140はこれを大域名前空間の型として扱い、
現在の規格準拠のMSVCは`DxLib::SETUP_GRAPHHANDLE_GPARAM`として扱います。構造体の定義は同じでも、次の3関数の修飾名が一致しません。

- `Graphics_Image_MakeGraph_UseGParam`
- `Graphics_Image_InitSetupGraphHandleGParam`
- `Graphics_Image_InitSetupGraphHandleGParam_Normal_NonDrawValid`

橋渡しは、旧来の引数型で受けて本物の実装へ転送するだけの3関数です（空実装・代替処理ではありません）。
v140以降のMSVCの静的ライブラリはMicrosoftが相互のリンク互換を保証しており、今回の差はこの名前解決だけです。
`DxUseCLib`をソースから再ビルドする方法もありますが、Bullet 2.75（名前変更版）・libpng・zlib・libjpeg・Ogg/Vorbis/Theora・Opus・libtiff・Live2Dの
外部ソースが必要になるため採用していません。

## 試験用モデルと最小実SDK試験

- `Tools/FbxSampleGen`: FBX SDKで自作の`Assets/Models/StaticBox.fbx`・`SkinnedColumn.fbx`・`ModelChecker.bmp`を生成します（第三者のモデルは使いません）。
  テクスチャの参照は相対名だけで、書き出し時の絶対パスや利用者名を埋め込みません。
- `Tools/FbxModelProbe`: `.fbx`を`MV1LoadModel`へ直接渡し、メッシュ・材質・テクスチャ（64×64の実画像であること、描画結果に色が出ること）、
  骨階層（Root→Bone1）、クリップ名（Bend・Twist）、時刻変更による骨姿勢の変化、複製の独立した再生時刻、解放と再読込、
  日本語パス、不在・破損ファイルを確認します。FBX非対応のDxLibにリンクすると読込が失敗し、試験も失敗します。

DxLibのアニメーション時間は、1秒のクリップが30.0になる単位でした（実測）。フレームワークの公開APIは秒で扱い、この変換を内部で行います。

## 制限

- 対象はx64・MSVC・静的CRT（`/MT`・`/MTd`）です。`/MD`系、x86、ARM64は作っていません。
- DxLibの版を変える場合は、ソースと公式SDKの両方を同じ版にそろえ、`BuildDxLibFbx.ps1`の期待ハッシュを更新します。
- FBX SDKのDebugライブラリにはPDBが同梱されないため、リンク時にLNK4099の警告が出ます。
- 生成物の再配布条件は、採用したFBX SDKのEULAを別途確認してください。
