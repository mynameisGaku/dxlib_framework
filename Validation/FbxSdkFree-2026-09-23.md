# FBX SDK不要化の検証（2026-09-23）

既存の未コミットのufbx統合を引き継いで検証した。過去の検証結果は書き換えていない。
Windows / MSVC 19.51、x64、DxLib 3.25a、ufbx 0.23.0を使用。

## 実装

- `.fbx`をufbxで読み、メモリ上のDirectX `.x`をDxLibへ渡す。外部テクスチャ・骨・スキン・複数クリップを扱う。
- `Setup.cmd`の標準経路はFBX SDKを使わずDxLibをソースビルドし、CMakeが自動選択する。
- 追記ごとの出力全体の再確保を取り除いた。ufbxの所有型で例外時も解析結果を解放する。
- 短いクリップでも最終姿勢を含むように標本化する。過大なキー数は変換前に拒否する。
- 変換結果の公開型を型別ヘッダーへ分離。ModelViewerは開発用ソリューションだけに追加する。
- ソース配布にufbx本体とライセンスを追加。外部のMSVC利用先へUTF-8設定を伝える。

## 実行結果

| 確認 | 結果 |
|---|---|
| フレームワーク全体 Debug / Release | CTest各23/23成功 |
| モデル専用 | Debug / Release各18/18成功（短いクリップ終端・過大なキー数の2件を追加） |
| 実DxLibのufbx経路 | FbxModelProbe Debug / Release成功 |
| フレームワーク経由の実描画 | NativeModelDeviceSmoke Debug / Release成功 |
| 配布・移動・外部利用 | ValidatePackageの全7段階成功。support単独でImportFbxModelもリンク・実行 |
| 依存関係 | NativeModelSmokeのリンク一覧にAutodesk FBX SDKなし。Debug実行ファイルの直接DLL依存はKERNEL32、USER32、ADVAPI32のみ |
| ツール回帰 | 19/19成功（ufbx同梱・旧資料と生成ログの配布除外を含む） |
| 通常ソリューション | 再生成しSandbox / StarterのDebug・Releaseビルド成功 |
| アセットRoot | AssetRootLaunch成功。モデルもProjectRoot経由 |

モデル実描画では2体の独立した再生、一時停止、速度変更、再読込、日本語パス、テクスチャ、解放後の描画失敗を確認した。
保存画像でも、片側の柱だけが曲がることと両側のテクスチャを確認した。
テストの画素検査を画素ごとのGPU読出しから一括読戻しに変え、同じ判定でDebugのモデル実描画試験が47.56秒から1.19秒になった。

主なログは`Build/FbxContinuation/`、描画画像は同所の`model-smoke/`に保存する（Git管理外）。

## 再実行

```powershell
cmake -S . -B Build/FbxContinuation -A x64 -DDXF_BUILD_NATIVE=ON -DDXF_BUILD_TESTS=ON -DDXF_BUILD_EXAMPLE=ON -DDXF_RUN_DEVICE_TESTS=ON
cmake --build Build/FbxContinuation --config Debug --parallel 6
ctest --test-dir Build/FbxContinuation -C Debug --output-on-failure
cmake --build Build/FbxContinuation --config Release --parallel 6
ctest --test-dir Build/FbxContinuation -C Release --output-on-failure
python Tools/CheckNoStl.py
python -m unittest discover -s Tools/Tests -v
```

`Tools/ValidatePackage.py`はC++開発者コマンドプロンプトから実行する。
実SDK試験はWindowsの描画環境が必要。標準の生成処理では自動実行しない。

## 検証の限界

このPCには比較用のAutodesk FBX SDKが導入済み。未導入の別PCでは未検証だが、今回のビルドとリンクはSDKのヘッダー・ライブラリを使用していない。
同梱モデルによる検証であり、任意のDCCツールが出力する全FBXの互換性は保証しない。
モーフ、複数UV、頂点カラー、PBR材質、カメラ・ライトは対象外。詳細は[利用手順](../Docs/DxLibFbx.md)を参照。
