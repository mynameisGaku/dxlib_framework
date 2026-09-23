# 同じ描画先への左右2ビュー

`FRenderView3D`の`bViewport`と`Viewport`を指定し、既存の`Get3D().SetView()`で描画する。未指定（`bViewport=false`）は従来の全描画先。Starter、Sandboxやゲーム側の資源管理は変更しない。

```cpp
// 640×480の描画先。OnDraw内で既存のモデル個体を描く例。
auto Require = [](Dxf::TResult<void> Result)
{
    if (!Result)
    {
        throw Toolbox::FException(Result.Error().Message);
    }
};
Dxf::FRenderView3D View;
View.bViewport = true;
View.Viewport = {0, 0, 320, 480};
Require(Render.Get3D().SetView(View));
Require(Render.Get3D().DrawModel(Model));
View.Viewport = {320, 0, 640, 480};
View.Eye = {4, 2, -5};
View.LightDirection = {1, -1, 0};
Require(Render.Get3D().SetView(View));
Require(Render.Get3D().DrawModel(Model));
Require(Render.Get2D().DrawText(Font, "Player 1 / Player 2", {220, 20}));
```

更新・物理Step・アニメーションAdvanceは通常の更新経路で一度だけ実行する。描画のためにSceneのTickを繰り返さない。ModelViewerは1280×720の固定描画先を左右640×720へ分割し、Vキーで通常表示に戻せる。異なるカメラ・ライトを使い、UIは全画面に重ねる。

## 矩形と受付

- `FIntRect`は描画先左上を原点とする整数ピクセルの半開区間`[Left,Right) × [Top,Bottom)`。幅と高さの加算をしない終端指定なので整数加算の桁あふれを避けられる。
- 始点は非負、終端は始点より大きく、描画先の寸法以下。ゼロサイズ・逆転・負数・範囲外は補正せず失敗する。無効矩形を「未指定」と解釈しない。
- RenderSystemが知る寸法でSetViewと各命令受付時に検証する。RenderTarget切替・次フレームの寸法変更後も、保持したビューを再検証する。単独Contextで寸法不明ならNative開始時に検証する。
- Nativeでは現在の描画先を問い合わせ、バックバッファには画面寸法、テクスチャにはGetGraphSizeを使用する。DxLibのGetDrawScreenSizeはテクスチャ描画先の寸法ではない。
- 無効なSetViewは前の設定・受付済み命令を変えない。矩形・カメラ・照明は命令へ複写する。Idが同じでもSetViewごとに別の区間であり、異なる矩形をまとめない。
- 透視のVerticalFov、正射影のOrthographicHeightは矩形の縦方向を基準にする。正方形の画素比を維持し、中心は矩形中心へ移す。

## 深度、順序、状態

各SetView区間の開始で、その矩形内の深度だけを遠方面へ初期化する。色とアルファは保持する。矩形外の色・深度を変更しない。矩形が重なる場合や同じ矩形へ戻る場合も、新しい区間の矩形内深度は初期化される。前区間との深度共有や重複領域の合成は提供しない。今回の受入対象は左右2つの非重複矩形。

DxLib 3.25aソースのD3D11深度クリアはClearRectを無視する。[D3D11のClearDepthStencilView](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-cleardepthstencilview)もViewport/Scissorに従わない。このため分割時は、そのAPIを使わず、DxLibのDESTCOLOR（RGB・アルファとも元の値を保持）、深度比較ALWAYS、深度書込み1.0で矩形を一度描く。全画面の中間画像や追加シェーダーは確保しない。全画面時の既存クリアは保持する。SDK拡張バージョンは3のままで変更しない。

SetDrawAreaの後にカメラ・投影・画面中心を設定する。全描画先基準の投影行列のX/Y倍率を矩形高／描画先高で補正するため、透視・正射影とも画素の縦横比が一定になる。[DxLibカメラAPI](https://dxlib.xsrv.jp/function/dxfunc_3d_camera.html)の設定順に従う。

3Dは受付区間順、続く2Dは全描画先へ描く。成功・失敗とも終了時に描画範囲、投影、2D深度、ブレンド、所有ライトをフレームワーク管理状態へ戻す。既存のPBR終了処理は維持する。実行失敗は最初のエラーを保持しPresentを抑止する。既に書いたGPU画素を取り消す契約ではない。2Dだけのフレームでは3D復元を増やさない。

## 対応境界と検証

- 矩形ビューのNative対応はDirect3D11。その他は明確な失敗。既存の全画面描画の対応範囲は変更しない。
- 独自Backendは`SupportsViewports3D()`が既定false。対応を宣言する場合は矩形投影・範囲内の深度初期化・終了時復元を実装する。
- 既存ufbx、骨、CPUモーフ、UV1、頂点色、基本PBR、モデルライトを保持。GPUモーフ、追加材質マップ、影、複数ウィンドウ、カメラ／ライトのアニメーションは今回追加しない。
- [今回の実行記録](../Development/Viewport-2026-09-23.md)に、手作成入力・境界ダブル・実World/Application・実DxLib画素判定と未実施範囲を分けて記載する。
