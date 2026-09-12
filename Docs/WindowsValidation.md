# Windows／実DxLib SDKの検証

この文書はWindowsで実行する手順です。**配布作成環境では未実行であり、成功ログは同梱していません。** `Docs/Validation` のLinux結果や代替SDKテストとは区別します。

## 必要な環境

Windows x64、Visual StudioのC++デスクトップ開発、Windows SDK、CMake 3.24以上、Ninja、公式DxLib VC版。スクリプトはPowerShell 5.1以上を想定しています。実機Smokeには描画可能なWindowsデスクトップが必要で、音声検査には音声環境も必要です。

## 入口

通常のPowerShellでプロジェクトのルートから実行します。

```powershell
.\Setup.cmd
.\Build.cmd
.\Validate.cmd
```

Setupは公式VC版3.25aをHTTPSで取得します。既存のSDKを使う場合はSetupの代わりに `DXLIB_ROOT` を設定します。SDKはGit対象外のThirdPartyだけに置き、システムのPATHや恒久的な実行ポリシーを変更しません。cmdのExecutionPolicy Bypassは、その起動プロセスだけに指定します。

取得ファイルのSHA-256とURLを `ThirdParty/dxlib-sdk.json` へ記録します。独立に確認した値がある場合は `Setup.cmd -ExpectedSha256 <64桁>` で照合できます。期待値なしの取得はTLSに依存しており、記録したハッシュだけで公式ファイルの真正性を証明したとは扱いません。

既存バージョンの上書きには `Setup.cmd -Force` が必要です。ダウンロードが失敗したら成功として進まず、その時点でエラーにします。

## BuildとValidateの違い

`Build.cmd` はコンパイラ環境の準備、CMake設定、Debugビルド、非実機CTest、ケース数集計、installを行います。Sandbox.exeとNativeSmoke.exeが実SDKでリンクされていることも検査します。

`Validate.cmd` は同じ処理をDebug／Release両方で行い、それぞれNativeSmokeを音声込みで実行します。すべて成功するまでSummaryのpassedはfalseです。

```powershell
# 音声再生のAPI検査なし。DxLib自体の音声初期化は無効化しません。
.\Build.cmd -AllConfigurations -RunDeviceSmoke

# CIなどの非対話環境。実SDKコンパイル・リンクと非実機テストのみ。
.\Build.cmd -AllConfigurations
```

## NativeSmokeの内容

標準アセットのBMPを一時ディレクトリの日本語ファイル名へコピーし、DxLibを初期化します。画像・システムフォント・RenderTargetを作り、入力を取得し、画像・矩形・日本語文字を12フレーム描画して終了します。全APIの結果を検査します。

音声を有効にすると同じSoundから2つの再生を開始し、一方の停止と他方の音量変更を検査します。短い効果音を使うため、長時間の再生試験ではありません。

終了前に、再生停止、描画要求の破棄、資源の解放・無効化、DxLib終了の順序を確認します。一時ファイルはRAIIで削除します。

Smokeは返り値とフレーム数を検査します。**画素比較・実際に聞こえた音・日本語字形・物理的な入力ボタン操作は自動検査していません。** 次の手動確認が別途必要です。

| 手動確認 | 確認する内容 |
|---|---|
| Sandboxを起動 | 画像・文字の欠け、透明度、ちらつき、日本語表示 |
| WASD／Space／P | 移動、音、ポーズと復帰 |
| Enterを繰り返す | Scene切り替え、訪問回数の保持、古いSceneの音の停止 |
| 最小化・フォーカス変更 | キーの押しっぱなしが残らないこと |
| ウィンドウを閉じる／Esc | 通常終了と資源解放 |
| 使用予定のPadを接続 | 実ボタン番号・左スティック・切断後の状態 |

## 結果の読み方

`Build/WindowsValidation/Summary.json` は次を別々に記録します。

- `real_sdk_compiled_and_linked`: 実SDKでのビルドとリンクが成功したか。
- `real_sdk_api_smoke_passed`: 実SDK用APIスモークテストを実行して成功したか。
- `audio_api_smoke_passed`: 音声API検査も実行したか。
- `visual_output_inspected`／`audible_output_inspected`: スクリプトは確認しないためfalse。

代替SDKの14ケースを成功しても、これらの実SDKフラグは上がりません。GitHub ActionsのWindowsジョブは実機Smokeを実行しない設定です。

失敗時は `*-configure.log`、`*-build.log`、`*-real-sdk-smoke.log` と、実行ディレクトリのDxLibの `Log.txt` を確認します。コード／SDK／グラフィックドライバーによる問題を、単にテスト無効化で通過扱いにはしません。
