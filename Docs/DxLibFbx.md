# .fbxの読み込みとDxLibのソースビルド

.mv1へ事前変換せず、`.fbx`をそのまま読み込みます。利用者はAutodesk FBX SDKをインストールする必要がありません。

## 仕組み

1. フレームワークが`.fbx`を読み、同梱のufbx（`External/ufbx`、MIT／Public Domain）で解析します。
2. 座標系（DxLibと同じ左手系・Y軸上向き）と長さの単位をここで一度だけ変換し、
   DxLibが標準で読めるDirectX `.x`（テキスト）をメモリ上に作ります。骨・スキン・材質・テクスチャ参照・
   アニメーション（クリップごとに全ノードの局所行列を一定間隔で標本化）を含みます。
3. `MV1LoadModelFromMem`へ渡します。テクスチャはコールバックでモデルのディレクトリ（またはFBX埋め込み）から供給するため、
   日本語を含むパスでもDxLibのファイルAPIを通りません。

変換は`Dxf::ImportFbxModel`（`Dxf/ModelImport.h`）だけで行い、ネイティブAPIを呼びません。

## DxLibをソースからビルドする理由

公式VCパッケージの`DxLib_vs2015_x64_MT.lib`（Release・静的CRT）は、MV1の読込部がFBX SDKのシンボルを参照します。
FBX SDKなしでは`MV1LoadModelFromMem`などを使った時点でReleaseのリンクが失敗します（Debugの`_MTd`はFBX非対応のため成功）。
そこで`Setup.cmd`が、公式のDxLibソース（DxLibMake）をFBX読込なしでDebug／Releaseともビルドします。実行時ライブラリは従来どおり`/MTd`・`/MT`です。

## 利用者が行うこと

```bat
Setup.cmd
```

1. 公式VCパッケージ（`DxLib_VC3_25a.zip`）を取得します（従来どおり）。
2. 公式ソース`DxLibMake3_25a.zip`（https://dxlib.xsrv.jp/DxLib/DxLibMake3_25a.zip）を取得し、SHA-256
   `2f09078692d3b64448c6ffe80392d77d0f413ce652115a1d7f0063baf475322a`を照合します。
3. `Tools/DxLibFbx/BuildDxLibFbx.ps1`でビルドし、`ThirdParty/DxLib-3.25a-source`（`include/`、`lib/`、`DxLibFbx.json`）を作ります。
   Visual Studio（C++によるデスクトップ開発）以外に必要なものはありません。数分かかります。

`CMake/FindDxLib.cmake`は、`DXF_DXLIB_CUSTOM_ROOT`が未指定なら`ThirdParty/DxLib-<版>-source`を自動で使います
（`-DDXF_DXLIB_AUTO_SOURCE_BUILD=OFF`で公式パッケージへ戻せます）。公式パッケージだけの構成ではモデル機能を無効にし
（`DXF_DXLIB_MODELS=0`）、読み込みは「Setup.cmdでソースビルドを作る」旨のエラーを返します。リンクエラーにはしません。

作ったゲームの実行ファイルは静的リンクで、Windowsの標準DLL以外に依存しません（利用者のPCにも追加の導入は不要です）。
SDK・ビルド生成物（`ThirdParty/`、`Build/`）はGitへ追加しません。

## アプリケーションから使う

`FAssetService::LoadModel("Assets/Models/SkinnedColumn.fbx")`で共有するモデルを読み、
`CreateModelInstance`で配置・再生状態を独立に持つインスタンスを作ります。
`Play("Bend")`、`Advance(DeltaSeconds)`で秒単位の再生を進め、
`Render.Get3D().DrawModel(Instance)`へ渡します。各戻り値の失敗を確認してください。
読み込み・インスタンス作成・描画は所有スレッドで行います。
モデルの相対パスは従来の[ProjectRoot](Assets/Paths.md)を基準にします。

確認用のModelViewerは`GenerateProjectFiles.bat -Development`で生成する開発用ソリューションに含まれます。
通常のソリューションには追加しません。CMakeからは`-DDXF_BUILD_MODEL_VIEWER=ON`で有効にできます。

変換結果・設定の公開型は`ImportedModel.h`、`ImportedModelTexture.h`、`ImportedModelClip.h`、
`ModelImportOptions.h`へ分離しています。従来どおり`ModelImport.h`からまとめて参照できます。
変換途中で例外が発生してもufbxの解析結果を解放します。

実行結果は[FBX SDK不要化の検証記録](../Validation/FbxSdkFree-2026-09-23.md)を参照してください。

## ビルドの内容

- `Tools/DxLibFbx/CMakeLists.txt`は、公式`DxLibMake.vcxproj`の`ClCompile`一覧（77ファイル）をそのまま読み、x64でビルドします。
  公式プロジェクトはWin32構成だけのため、x64の構成は同じ定義（`WIN32;_LIB;_DEBUG|NDEBUG`）で作ります。
  ソースはShift_JISなので、`/source-charset:.932 /execution-charset:.932`で文字コードを固定します。
- `BuildDxLibFbx.ps1`は、ソースZIPのハッシュ照合、ソースと公式SDKのヘッダー一致の確認（同じ版であること）、
  Debug／Releaseのビルド、公式SDKを変更しない別ディレクトリへの出力、`DxLibFbx.json`の作成を行います。
  マニフェストには各ライブラリのパス・SHA-256・出所、コンパイラ、定義、実行時ライブラリを記録します。
- `FindDxLib.cmake`は`DX_LIB_NOT_DEFAULTPATH`で`DxLib.h`の自動リンクを無効にし、マニフェストのライブラリだけを構成別に明示リンクします
  （設定時にSHA-256を照合）。DxLib同梱の外部ライブラリ（DxUseCLib、DxDrawFunc、Bullet、libtiff、libpng、zlib、libjpeg、
  Ogg/Vorbis/Theora、Opus）は自動リンクと同じ組の`_vs2015_x64_MT(d)`を使います。

### 名前修飾の橋渡し（`Tools/DxLibFbx/DxLibV140AbiBridge.cpp`）

公式の`DxUseCLib_vs2015_*.lib`はVS2015（v140）でビルドされています。`DxMovie.h`は`namespace DxLib`の中で、
未宣言の`struct SETUP_GRAPHHANDLE_GPARAM`を関数引数で初めて参照します。v140はこれを大域名前空間の型として扱い、
現在の規格準拠のMSVCは`DxLib::SETUP_GRAPHHANDLE_GPARAM`として扱います。構造体の定義は同じでも、次の3関数の修飾名が一致しません。

- `Graphics_Image_MakeGraph_UseGParam`
- `Graphics_Image_InitSetupGraphHandleGParam`
- `Graphics_Image_InitSetupGraphHandleGParam_Normal_NonDrawValid`

橋渡しは、旧来の引数型で受けて本物の実装へ転送するだけの3関数です（空実装・代替処理ではありません）。

## 任意: DxLib自身のFBX読込との比較構成

ufbxの変換結果を確かめるため、DxLibをFBX読込有効でビルドする構成も残しています（通常は不要です）。
Autodesk FBX SDK 2020.3.11を**ご自身で**インストールし、付属の利用許諾に同意したうえで次を実行します。

```powershell
powershell -ExecutionPolicy Bypass -File Tools\DxLibFbx\BuildDxLibFbx.ps1 -WithFbxSdk
# 出力: ThirdParty\DxLib-3.25a-fbx。FBX SDKの静的ライブラリとbcrypt.libもマニフェストに記録される。
```

`Tools/FbxModelProbe`は同じ27項目の確認を、ufbx経路（`-DDXF_PROBE_UFBX=ON`）とDxLibのFBX読込経路の両方で実行できます。

## 試験用モデル

- `Tools/FbxSampleGen`: 自作の`Assets/Models/StaticBox.fbx`・`SkinnedColumn.fbx`・`ModelChecker.bmp`を生成した道具です
  （生成にはFBX SDKが必要ですが、生成物はリポジトリに含まれるため利用者は実行不要です）。
- `Tools/FbxModelProbe`: 最小実SDK試験。メッシュ・材質・テクスチャ（64×64の実画像、描画結果の色）、骨階層、クリップ名、
  時刻による姿勢変化、複製の独立した時刻、解放と再読込、日本語パス、不在・破損ファイルを確認します。

DxLibのアニメーション時間は、1秒のクリップが30.0になる単位でした（実測）。フレームワークの公開APIは秒で扱い、
読み込み時にクリップごとにネイティブ側の長さを計測して変換します。

## 制限

未対応データの警告・拒否条件は[FBX対応範囲](FbxSupport.md)を参照してください。

- 対象はx64・MSVC・静的CRT（`/MT`・`/MTd`）です。`/MD`系、x86、ARM64は作っていません。
- DxLibの版を変える場合は、ソースと公式SDKの両方を同じ版にそろえ、期待ハッシュ（`SetupDxLib.ps1`・`BuildDxLibFbx.ps1`）を更新します。
- ufbxの変換はアニメーションを指定間隔以下（既定1/30秒）の行列キーへ標本化します。短いクリップも終端を含めます。
  全ノード・全クリップのキー数見積りが100万を超える入力は変換前に拒否します。ブレンドシェイプ（モーフ）、
  PBR材質、カメラ・ライトは警告して省略します。UVは合計2組まで保持し、頂点カラーは先頭セットに対応します。複数スキンとDual Quaternion方式は拒否します。詳細は[FbxSupport](FbxSupport.md)を参照してください。
