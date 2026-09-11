# サンプルアセット

`player.bmp` と `confirm.wav` は、この配布の `Tools/GenerateAssets.py` で生成したサンプルです。再生成に外部ライブラリは不要です。

```sh
python Tools/GenerateAssets.py
```

BMPは64×64の24bit画像、WAVはモノラル／16bit／22,050Hzです。既存ゲームの画像・録音・フォントファイルは含みません。フォント描画は実行環境にインストールされたフォントをDxLib経由で利用します。
