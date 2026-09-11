# Windows実機の確認

以下は未実行の確認項目です。「対応済み」や「通過済み」を示す一覧ではありません。

## 準備とビルド

公式DxLib VC版を取得し、DxLib.hと対応するライブラリが同じディレクトリに存在することを確認します。C++20／MSVC x64、CMake、Ninjaを使用します。

```powershell
$env:DXLIB_ROOT = 'C:\Libraries\DxLib_VC\プロジェクトに追加すべきファイル_VC用'
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
.\Build\windows-debug\Sandbox.exe

cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
.\Build\windows-release\Sandbox.exe
```

設定を変更した場合は、別Buildディレクトリにするか、該当のCMakeキャッシュを作り直します。SDKのライブラリとアプリケーションのx64／ランタイムが一致する必要があります。Native有効時の既定はDebug `/MTd`、Release `/MT` です。

## 手動確認

- [ ] Debug・Releaseで実DxLib SDKとのコンパイル／リンクに成功する。
- [ ] 画面、画像、日本語の説明、矩形が表示される。
- [ ] WASDで移動でき、Pで停止・再開できる。SceneのUIはポーズ中も反応する。
- [ ] Spaceを複数回押すと独立した音が鳴り、他の再生を不意に止めない。
- [ ] EnterでSceneが変わり、訪問回数だけが維持される。
- [ ] ウィンドウの閉じるボタンとEscで安全に終了する。
- [ ] フォーカス移動後にキー／マウスの押しっぱなし状態が残らない。
- [ ] 接続・切断を含めてゲームパッドの左スティックとボタンが取得できる。
- [ ] 日本語・空白を含むフォルダーへ配置して、画像／音／フォントが扱える。
- [ ] Assetsのファイルを外すと、半端に起動せずエラーを表示して終了する。
- [ ] RenderTargetを用いた描画先切り替え、再描画、Native区間後の描画状態を確認する。
- [ ] 繰り返しのScene切り替えで、メモリやネイティブ資源が増え続けない。

起動やDxLib関数が失敗した場合は、表示されたエラーとDxLib側のLog.txtを確認してください。接続部のエラーコードだけで、ドライバーやファイル形式などの原因を確定することはできません。
