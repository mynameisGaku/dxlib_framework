# Viewportの座標変換と簡易選択

`Dxf/ViewCoordinates.h`の2関数は、指定した`FRenderView3D`と描画先の幅・高さだけから計算する。Native、Application、World、最後に描いたカメラに依存しない。`dxf::support`のみでも利用できる。

```cpp
const auto Point = Dxf::ProjectWorldToScreen(View, Width, Height, WorldPosition);
if (Point && Point.Value().bInsideView)
{
    // DrawText等へ渡す位置。遮蔽物の有無は別途ゲーム側で判断する。
    const Dxf::FVector2 LabelPosition = Point.Value().Screen;
}
const auto Pick = Dxf::MakeViewPickSegment(View, Width, Height, MousePosition);
if (Pick && Pick.Value())
{
    const Dxf::FLine3D& Segment = *Pick.Value();
    // Toolbox/SegmentIntersection.h。非交差は空、交差時は始点0～終点1の割合。
    const auto Hit = Toolbox::IntersectSegment(Segment.Start, Segment.End, Sphere);
}
```

## 座標と失敗

- Screenは描画先全体の左上原点、右が+X、下が+Y。Viewport局所座標ではない。矩形は`[Left,Right) × [Top,Bottom)`で、指定なしは全描画先。領域外の選択は成功した空Optionalであり、端へ補正しない。
- XYは連続値。既存Inputの整数マウス位置をそのまま渡す。一律の半ピクセル補正はしない。画素中心を扱う試験は明示的に`x+0.5, y+0.5`を渡す。
- `FProjectedPoint3D::Depth`は近接面0・遠方面1の投影深度。透視では非線形で、カメラからの距離ではない。正射影では線形。
- `bInsideView`は矩形と近遠面の内側を意味する。遮蔽・透明度・モデルの可視性は判定しない。視野外でも計算可能な点は有限なXY/Depthとfalseを返す。
- 透視の眼平面上・背後は失敗。正射影の背後は計算できるがfalse。背後のラベルを表示しないため、成功だけでなくbInsideViewも確認する。
- 選択線分のStartは近接面、Endは遠方面。カメラ位置始点の無限Rayではなく、この区間外の対象を拾わない。正射影は平行線分、透視は位置ごとに方向が変わる。
- 無効寸法、範囲外/空/逆転矩形、NaN/Inf、Eye=Target、Upが視線と平行、Near<=0、Far<=Near、無効FOV/正射影高さはInvalidArgument。両投影設定を常に検査する。照明は使用も検査もしない。
- 中間計算はf64、公開XY/端点は既存f32。表現範囲超過や近遠端点の丸めによる一致は失敗する。入力と描画状態は変更しない。

カメラの右方向はUp×前方向、画角と正射影高さは矩形の縦方向を基準にする。既存FMatrix4の列ベクトル規約を変更しない。Nativeの設定変更・状態復元やGPU読戻しは不要。

## 球・箱の例

開発用ModelViewerでPを押すと球・箱の選択例へ切り替わる。Vで左右分割、Oで透視/正射影、左クリックで近い形状を選ぶ。選択色は黄色、印と名前は3D中心を投影した位置。領域外や非交差のクリックで解除、Rの再読み込み・EnterのScene切替でも解除する。Starter/Sandboxは変更しない。

OnTickでアニメーションを一度進め、表示用ビューを準備して同じSnapshotのマウス位置で選択する。OnDrawはそのビュー・形状を再利用し、時間を進めない。画面リサイズや前フレーム追跡は追加しておらず、例の描画先は1280×720固定。

`Toolbox::IntersectSegment`は球に既存の半径0のSweep、OBBに既存逆行列と軸ごとの区間交差を使う。内部始点は0、接触を含み、非交差は空。無効形状・非有限入力・f32で表現不能な変位/局所変換はFException。サンプルの`PickExampleShapes`はそれをTResult失敗へ変換し、同距離は先に検査する球を選ぶ。

これは表示と共有した代理形状の選択であり、骨/モーフで変形したモデルの三角形を選択する機能ではない。World全体のRaycastサービス、選択ID管理や新しいManagerは追加しない。

## 検証と関連資料

- [左右Viewport](Viewports.md)、[API索引](../API.md)、[Roadmap](../Development/RenderingDebugRoadmap.md)
- [今回の検証記録](../Development/ViewCoordinates-2026-09-23.md)
- 仕様参照: [DxLibカメラAPI](https://dxlib.xsrv.jp/function/dxfunc_3d_camera.html)、[XMVector3Unproject](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmvector3unproject)。本実装にDirectXMathへの依存はない。
