# モデル基本材質・照明の検証（2026-09-23）

FModelMaterial3Dでインスタンスごとの不透明な色倍率と照明の有無を指定する。
モデル照明はDxLibのGPU描画に委ね、CPU基本形状の照明とは分離する。
ライトは必要になったビュー内で再利用し、外部のライト有効状態を保存・復元する。
描画命令には材質を複写するため、受付後の変更が遡及しない。

今回の実行結果:
- Debug / ReleaseのCTestは各23/23成功（NoStlを含む）。
- 実描画で光の反転による明暗差、隣の非照明モデルの画素一致、モデル別の色倍率を確認。
- 外部ライトの影響排除と有効状態の復元、専用ライトの解放、モデル後の2D指定色を確認。
- 境界試験で材質の記録時点の保存と、ライト作成・設定・描画の失敗時の状態復元を確認。
- 実画像model-lit-front.png / model-lit-back.pngを目視確認。
- 配布物のbuild-configure / build / install / consumer-configure / consumer-build / consumer-run / support-only-runの7段階成功。

ログ: Build/FbxContinuation/lights-{debug,release}-{build,tests}.log、lights-package.log。
グローバル環境光は変更しない。任意のNative状態全体の復元を保証するものではない。
追加UV・モーフ・PBR・ファイル内カメラ／ライトの対応は、この変更には含めていない。
SDK未導入環境での検証は未実施のまま。
