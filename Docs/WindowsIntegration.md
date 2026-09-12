# Windows統合検証 — 2026-09-12

受領した `dxlib_framework_0.3.0.zip` のDistributionManifestにある275ファイルのサイズとSHA-256を照合し、既存リポジトリへ取り込みました。0.2.0の履歴と今回のNative修正は保持しています。配布元のLinux検証をこの端末で再実行した結果として扱いません。

## 結果

| 対象 | 結果 |
|---|---|
| Windows x64 / MSVC 19.51.36257 / Debug | 基盤138件＋接続契約15件、計153件通過 |
| 実DxLib VC 3.25a / Debug | SandboxとNativeSmokeのコンパイル・リンク、インストール成功 |
| 実DxLib / Debug API確認 | NativeSmokeが12フレームで正常終了 |
| MSVC Release / SDKなし / 警告をエラー扱い | 基盤138件＋接続契約15件、計153件通過 |
| Python配布・検証ツール | 9件通過 |
| 実DxLib VC 3.25a / Release | NativeSmokeのリンク失敗。FBX関連105件の未解決参照 |
| 画素、聴感、物理キー・Pad操作 | 未確認。今回のSmokeは音声再生検査なし |

Releaseの失敗には `DxLib_vs2015_x64_MT.lib(DxModelLoader1.obj)` からの `fbxsdk::FbxAllocSize` などの参照が含まれます。未解決参照を無視する設定やDebugライブラリへの置き換えは行っていません。SDK・リンク構成の追加調査が必要です。

使用SDKの取得記録にあるZIP SHA-256は `508fea81c65963bdb6ea6dd54e23995f753b54d2a3638e8a25b4fc72b62b2cd1` です。これは独立した真正性確認ではありません。

## 修正と再現

NativeApi.hでSDK取り込み後のCreateFont／DrawTextマクロを除去しました。公開API名は変えていません。回帰テストはSDK取り込み時と同様のマクロ導入を再現します。Windows.hを先に含める利用側全体のマクロ互換性まで保証する修正ではありません。

BuildWindows.ps1の `-Clean` は既存オブジェクトを削除してCMakeを再検出します。ZIPの古いファイル日時による旧版との混在を防ぎます。Ninjaの依存追跡には、実ビルドと同じ `/utf-8` を指定したコンパイラから採取した表示接頭辞を渡します。PowerShell 5.1で日本語コメントを読めるよう、変更したスクリプトはUTF-8 BOM付きです。

```powershell
.\Tools\Build.cmd -Configuration Debug -Clean -RunDeviceSmoke
.\Tools\Build.cmd -Configuration Release -Clean

cmake -S . -B Build/portable-msvc -G "Visual Studio 18 2026" -A x64 -DDXF_BUILD_NATIVE=OFF -DDXF_BUILD_TESTS=ON -DDXF_WARNINGS_AS_ERRORS=ON
cmake --build Build/portable-msvc --config Release --parallel 4
ctest --test-dir Build/portable-msvc -C Release --output-on-failure
```

現在の端末にはVS 2026があるため、非実機検証のコマンドではそのジェネレーターを指定しています。Windowsビルドスクリプトはvswhereでインストール済みのC++環境を探索します。

実行ログは `Build/WindowsValidation`、保存したDebugログは `Build/evidence-0.3-debug` にあります。要約と主要ログは `Docs/Validation/WindowsIntegration` に保存します。

## 次の開発候補

1. Releaseの実SDKリンク失敗を、最小のDxLib利用プログラムでも調べる。
2. 外部利用側のWindows.h取り込み順による公開APIマクロ衝突を検証する。
3. Sandboxの実画面・音声・入力操作を確認してから、2Dカメラと座標変換へ進む。
