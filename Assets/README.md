# サンプルアセット

`player.bmp` と `confirm.wav` は、この配布の `Tools/GenerateAssets.py` で生成したサンプルです。再生成に外部ライブラリは不要です。

```sh
python Tools/GenerateAssets.py
```

BMPは64×64の24bit画像、WAVはモノラル／16bit／22,050Hzです。既存ゲームの画像・録音・フォントファイルは含みません。フォント描画は実行環境にインストールされたフォントをDxLib経由で利用します。

## モデル（Assets/Models）

`StaticBox.fbx`・`SkinnedColumn.fbx`・`ModelChecker.bmp`は、`Tools/FbxSampleGen`がAutodesk FBX SDKで生成した自作の試験用モデルです。
第三者のモデルは含みません。再生成には利用者がインストールしたFBX SDKが必要です（[Docs/DxLibFbx.md](../Docs/DxLibFbx.md)）。

- `StaticBox.fbx`: 骨・アニメーションなしの一辺100cmの箱。`ModelChecker.bmp`を貼る。
- `SkinnedColumn.fbx`: 高さ200cmの柱を骨Root・Bone1でスキニング。クリップ`Bend`（Z回転0→60→0度、1秒）と`Twist`（Y回転0→90度、1秒）。
- 単位はセンチメートル、Y軸上向きの右手系。テクスチャは相対名だけで参照し、絶対パスは含みません。
