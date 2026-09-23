# FBX対応DxLibの再現可能ビルドと最小FBX実証 — 2026-09-23（第1段階）

起点: `main` = `origin/main` = `a40e523da64dafd9528fcf538e9f6bd428194123`（作業開始時、stage・未コミット変更なし）。
環境: Windows 11 Pro 10.0.26200、Visual Studio 18 2026、MSVC 19.51.36257（x64）、CMake 4.3.1、Ninja。
過去の記録（`Validation/RendererSceneIntegration-2026-09-23.md`等）は書き換えていない。

## 取得・導入したもの

| 対象 | 入手元 | 確認 |
|---|---|---|
| DxLib 3.25a ソース `DxLibMake3_25a.zip` | https://dxlib.xsrv.jp/DxLib/DxLibMake3_25a.zip（公式配布ページの掲載URL） | SHA-256 `2f09078692d3b64448c6ffe80392d77d0f413ce652115a1d7f0063baf475322a`。ソースの`DxLib.h`等5ヘッダーは使用中の公式VC SDKと同一バイト |
| Autodesk FBX SDK 2020.3.11 VS2022 Windows | Autodesk公式ページ掲載の`damassets.autodesk.net/.../fbx2020311_fbxsdk_vs2022_win.exe` | SHA-256 `ad2f2724850c2b54ea97453e02249c449a7e46d061640d59bc894d1bc0080ad6`、Authenticode有効（Autodesk, Inc.）。**インストールとEULAへの同意は利用者が実施** |

どちらもGitへは追加していない（`ThirdParty/`はGit対象外）。

## 実施内容と判明した事項

1. `Tools/DxLibFbx`: 公式`DxLibMake.vcxproj`の77ソースをx64の`/MTd`・`/MT`でビルドするCMakeと、ハッシュ照合・出力・マニフェスト作成の`BuildDxLibFbx.ps1`。
2. 現行MSVCでビルドした本体と、公式の`DxUseCLib_vs2015_*`（v140）の間で3関数の修飾名が一致しない（`SETUP_GRAPHHANDLE_GPARAM`の名前空間解釈の差）。
   公式本体は`...GPARAM@@`、ソースビルドは`...GPARAM@1@`。本物の実装へ転送する3関数の橋渡しを追加（`DxLibV140AbiBridge.cpp`）。
3. `DX_LIB_NOT_DEFAULTPATH`で無効になる自動リンクの代わりに、DxLib同梱の外部ライブラリを`DxDataTypeWin.h`と同じ組で明示リンク。
4. FBX SDK 2020.3.11の`libxml2-mt.lib`が`BCryptGenRandom`を参照（未解決シンボルで確認）。Windowsの`bcrypt.lib`をマニフェストの`system_libraries`へ記録。
5. `CMake/FindDxLib.cmake`: `DXF_DXLIB_CUSTOM_ROOT`・`DXF_REQUIRE_FBX_MODEL`を追加。未指定時の公式SDK経路は従来どおり。
6. `Tools/FbxSampleGen`で自作の試験モデルを生成。最初の生成物はテクスチャの相対名が`..\..\ModelChecker.bmp`になり、
   文書URLに利用者名を含む絶対パスが入っていた。出力先を現在ディレクトリにし、文書情報を明示して修正（生成物に`Users`・利用者名を含まないことを確認）。
7. `Tools/FbxModelProbe`: 最小実SDK試験。当初「テクスチャハンドルが非負」だけを確認しており、8×8の既定画像を誤って合格にしていた（上記6の不具合）。
   画像寸法（64×64）と描画結果の色の読み戻しを追加して失敗を再現してから修正した。

## 結果（原ログ: `Validation/ModelFbx/`）

### DxLibのビルド

| 構成 | 結果 |
|---|---|
| FBXなし（パイプライン確認） Debug / Release | 成功。`DxLib_fbx_x64_MTd.lib` 22.1MB / `DxLib_fbx_x64_MT.lib` 14.1MB、FBX参照0件 |
| FBXあり Debug / Release | 成功。FBX参照 2065件 / 1548件（公式MT版以外の公式libは0件） |

### 最小FBX試験 `FbxModelProbe`（`probe-debug-run.log`、`probe-release-run.log`）

Debug・Releaseとも終了コード0、全27項目PASS。主な値:

- 静的な箱: meshes=1 textures=1 materials=1、テクスチャ64×64、描画結果の色付き画素569（160×160領域を4px間隔で採取）、アニメーション0、解放と再読込。
- 骨付きの柱: frames=3、Root→Bone1の親子、clips=2（Bend=0、Twist=1）、Bendの長さ30.0（1秒のクリップ）、
  時刻0と中間で骨の行列が異なる、複製が独立した時刻を持つ、複製の削除後も元が有効、解放。
- 日本語パス（`日本語パス/箱モデル.fbx`）の読込、不在ファイルと破損ファイルは-1。
- 描画画像: `Validation/ModelFbx/probe-images/{Debug,Release}/`（箱のチェッカー、Bend中間の曲げ）。

対照: FBXなしのDxLibへリンクした同じ試験は`.fbx`の読込3件がFAILし終了コード1（`probe-nofbx-release-run.log`）。

### フレームワーク全体（FBX対応DxLib、`DXF_REQUIRE_FBX_MODEL=ON`、`Validation/ModelFbx/fbx-root/`）

| 項目 | Debug | Release |
|---|---|---|
| root設定・ビルド（`Tools/BuildWindows.ps1`と同じ引数＋カスタムSDK） | 0 / 0 | 0 / 0 |
| root CTest | 20/20 | 20/20 |
| NativeSmoke / Sandbox / Starter / RenderDebug のリンク | 成功 | 成功 |
| NativeSmoke 実行 | 終了コード0、`REAL_SDK_API_SMOKE_PASSED` | 同左 |
| RenderDebug・Sandbox 起動（画面キャプチャ） | 終了コード0 | 終了コード0 |

公式SDKの既定経路（カスタムSDK未指定）もroot CTest 20/20（Debug、`Validation/ModelFbx/official-default/`）。

### RenderDebugの未確認項目（FBXなしのソースビルド、Debug、`screens-renderdebug-history/`）

- 停止中のZ×3で表示Stepが322→306、X×3で322へ戻る。サンプル時計（5366ms）と履歴件数は変わらず、Worldへ書き戻していない。
- 停止中のEnterで両Worldを再生成: Step 0・時計0・履歴1件、箱が初期位置へ戻る。再開後は新しいWorldが0から進む（Step 176で停止状態へ）。
- Sandbox・Starter・RenderDebugをDebug / Releaseで起動し描画を確認（`screens-launch/`）。Starterは空テンプレートでEscの処理がなく、スクリプトが終了させた。

画面キャプチャは他ウィンドウが写り込まないよう、クライアント領域（1280×720）へ切り抜いて保存した。

## 未実施・制限

- `/MD`系・x86・ARM64のDxLibは作っていない。GCC / Clang・Sanitizerはこの環境にない。
- FBX SDKのDebugライブラリはPDBが同梱されず、LNK4099警告が出る。
- フレームワークのモデルAPIは第2段階（この記録の対象外）。
