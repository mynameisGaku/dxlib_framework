# 参照資料

2026年9月12日に確認した公式資料です。実装はこれらのAPI説明を参照して作成していますが、SDKそのものをこの環境で取得・コンパイルしたわけではありません。

- Epic Games: Epic C++ Coding Standard for Unreal Engine
  https://dev.epicgames.com/documentation/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine
- DxLib: 関数リファレンス
  https://dxlib.xsrv.jp/dxfunc.html
- DxLib: グラフィックデータ制御・描画
  https://dxlib.xsrv.jp/function/dxfunc_graph1.html
- DxLib: 描画先・クリア・ScreenFlip
  https://dxlib.xsrv.jp/function/dxfunc_graph3.html
- DxLib: 入力
  https://dxlib.xsrv.jp/function/dxfunc_input.html
- DxLib: サウンド
  https://dxlib.xsrv.jp/function/dxfunc_sound.html
- DxLib: 文字コードの扱い
  https://dxlib.xsrv.jp/lecture/Android/Android_CharCode.html
- DxLib: Visual Studioでの利用設定
  https://dxlib.xsrv.jp/use/dxuse_vscom2019.html
- DxLib: 公式ダウンロード
  https://dxlib.xsrv.jp/dxdload.html

DxLib SDK、DxLibソース、フォント、外部の画像・音声は同梱していません。AssetsのBMP・WAVは同梱スクリプトで生成したサンプルです。代替DxLibヘッダーは、公式SDKの配布物ではありません。

## 0.2で追加参照した公式資料

- DxLib 3.25a公式VCパッケージ: https://dxlib.xsrv.jp/DxLib/DxLib_VC3_25a.zip （URLの掲載を確認。作成環境へのダウンロードは失敗）
- Microsoft: C++ command-line tools: https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line?view=msvc-170
- Microsoft vswhere: https://github.com/microsoft/vswhere/wiki/Examples
- CMake CMP0112: https://cmake.org/cmake/help/latest/policy/CMP0112.html
- GitHub Actions checkout: https://github.com/actions/checkout
- GitHub Actions upload-artifact: https://github.com/actions/upload-artifact

公開APIの記述と実SDKへのコンパイルは別の検証です。Actionsの設定を同梱していますが、この作業でリモート実行したものではありません。
