# 次元別描画とデバッグ図形

起点: `1bb654826cb6b23de4837d744ab2634b25fb9979`。
この変更は破壊的API変更である。FRenderContextのDraw/DrawText/FillRectangle/SubmitGeneratedは削除し、互換転送を残さない。通常描画の入口はGet2D/Get3Dだけにする。

## 所有と入口

フレーム全体の所有者を `FRenderSystem`（旧FRenderSystem2Dから改名）とする。`FRenderQueue2D`は二次元専用の内部実行担当として残る。Applicationの構築完了時に所有JobSystemをRendererへ結び付ける。各描画でJobSystemを取り出したり、SceneごとにThreadPoolを作ったりしない。

```cpp
// FRenderContext& Renderが渡されるOnDraw内。
auto Result = Render.Get2D().DrawLine({10, 10}, {200, 100});
if (!Result)
{
    throw Toolbox::FException(Result.Error().Message);
}
```

| 窓口 | 実装した操作 |
|---|---|
| Get2D() | DrawSprite、DrawText、DrawLine、DrawRectangle、FillRectangle、DrawCircle、FillCircle、DrawTriangle、FillTriangle、Submit、SubmitGenerated |
| Get3D() | SetView、DrawLine、DrawTriangle、DrawBox、DrawSphere、DrawMesh、Submit、SubmitGenerated |
| Root Context | 描画先・Clear・Nativeなど、次元をまたぐパス制御のみ |

2D位置はピクセル、3D位置はワールド座標。3Dの基本形状は `Toolbox::FOBB` / `Toolbox::FSphere`、三角形メッシュは所有配列 `FGeometry3D` を使う。`DrawMesh`はCPU上の線・三角形を描くもので、MV1モデル、テクスチャ付きメッシュ、スキニング、アニメーションモデルではない。

## ビューと表示モード

```cpp
Dxf::FRenderView3D View;
View.Id = 1;
View.Eye = {4, 3, -6};
View.Target = {0, 0, 0};
View.Debug.Surface = Dxf::ESurfaceMode3D::SolidWithEdges;
View.Debug.Lighting = Dxf::ELightingMode3D::Unlit;
auto Set = Render.Get3D().SetView(View);
if (!Set)
{
    throw Toolbox::FException(Set.Error().Message);
}
auto Drawn = Render.Get3D().DrawBox(Toolbox::FOBB{});
if (!Drawn)
{
    throw Toolbox::FException(Drawn.Error().Message);
}
```

各命令は受付時のビューを値で保存する。後でビューを変更しても、既存命令に遡って変更しない。`SetView`は明示的なパス境界となり、実行時にカメラ設定と深度初期化を行う。未指定なら描画先全体を使う。`bViewport`と`Viewport`で矩形を指定できる。現在の契約と左右表示の例は[分割Viewport](Viewports.md)を参照。別ウィンドウの多画面UIは未対応。

- Solid: 塗りつぶした三角形。
- Wireframe: 明示したワイヤー辺、または三角形から得た辺。
- SolidWithEdges: 塗りつぶしに辺を重ねる。

箱のワイヤーは12本の幾何学的な辺、球は緯線・経線である。これは基本形状の調査用表示であり、モデルの全三角形分割やGPUのラスタライザWireframeを再現するものではない。`DrawMesh`へ辺を指定しない場合は各三角形の3辺を使う（共有辺の重複除去は行わない）。線だけのGeometryはSurfaceモードにかかわらず線として描く。

### 照明の範囲

**基本形状のNormalはCPUで計算する「一つの方向光＋環境色」のフラットライティングです。** 準備した色を二重評価しないため、基本形状を描く区間ではDxLibのLightingを無効にします。モデルは`FModelMaterial3D::bLit`で別途GPU照明を有効にでき、同じビューの方向光と環境色を使います。モデルの描画直後にはLightingを無効へ戻します。詳細は[FBX対応範囲](../FbxSupport.md)を参照してください。

- Normal: LightDirection / LightColor / AmbientColorを評価する。
- bLightEnabled=false: 方向光の寄与だけを除く（環境色は残る）。
- Unlit: 元の描画色をそのまま使う。
- LightsOff: 環境色も含め光の寄与を除き、塗りつぶしを黒にする。

元のGeometryやビューのライト値を書き換えない。線・調査用の辺は見失わないよう非照明色で表示する。影の切替、個別ライトの選別、法線・深度・UVの専用表示、オーバードロー／GPU負荷は未実装。未定義の列挙値は失敗する。

## 実行順と状態

一回のFlush内は「3D（ビュー受付順）→2D（Layer/Order/同順位の入力順）」。透明3D面の距離ソートはまだ行わない。半透明を使う場合は順序と `EDepthMode3D::TestOnly` を呼び出し側で明示する。

SetRenderTarget/SetBackBuffer/NativeはFlush境界として働く。3D→2Dで深度・ブレンド状態を復元し、失敗時に部分フレームをPresentしない。EndView失敗が重なっても最初の描画エラーを保つ。Native拡張が変更する任意のシェーダー定数・モデル材質等まで自動復元する契約ではない。

Native Backendは追加2D図形と3D基本形状の対応を宣言する。既存の別Backendは既定では未対応を返し、成功したふりをしない。実画面、線のにじみ、ワイヤーの深度精度、実SDKのコンパイルは別途実機検証が必要。

2Dの新図形は安全なint32範囲を検証してからNativeでピクセルへ丸める。線幅・アンチエイリアス指定はこの段階では持たない。3Dの保存座標はf32、中間の距離・外積・照明はf64。大座標で形状が潰れてしまう場合は拒否し、失った精度を復元したふりをしない。

## 並列生成

```cpp
// Application経由のRenderは共有実行器へ接続済み。
auto Generated = Render.Get2D().SubmitGenerated(100,
    [](Toolbox::size_t Index, Dxf::FRenderCommand& Output) -> Dxf::TResult<void>
    {
        Dxf::FRectangleCommand Command;
        const Toolbox::int32 X = static_cast<Toolbox::int32>(Index) * 2;
        Command.Rectangle = {X, 0, X + 1, 10};
        Output = Toolbox::Move(Command);
        return {};
    });
```

Generatorは確定した入力だけを読み、自分の出力スロットだけを書く。同じCallableが複数Workerから呼ばれる。通常のOnDrawを自動的にWorkerへ移すものではない。生成中は同じContextの別次元も含め、追加・設定変更・Native制御を拒否する。同期JobもJob区間として扱う。

Texture/Font等の入力参照は所有スレッド側で生成完了まで保持する。任意のGameObject/AssetServiceをJobから安全に変更できるAPIではない。生成に成功しても、フレーム表示成功までは保証しない。

低レベルテストでは `FRenderContext(Queue, Control, &Jobs)` または `FRenderSystem::SetExecutionJobs(Jobs)` で借用先を設定する。未設定の並列生成は明示的に失敗する。通常の描画メソッドはJobSystem未設定でも使える。

3Dは一回のFlushまでに65,536命令かつ65,536線・三角形まで。バッチの生成結果をすべて検証し、予算超過・失敗時は既存キューを変更しない。無制限のFrameメモリを許さない。

## デバッグ記録とアダプター

`FDebugDrawStore2D`（2D線）と `FDebugDrawStore3D`（所有Geometry）はNative資源もPhysics Worldも所有しない。

- カテゴリのbitmask、所有者のWorld/Id/Generation、Scopeを記録する。
- 寿命はFrame / Seconds / Persistent。秒はGame / Realを分ける。
- Advanceは新フレームの開始で一回呼ぶ。初回はFrame=0でもよい。
- ゲーム停止中はGameDelta=0、RealDeltaは実経過を渡す。
- ClearScopeは世代まで照合。Clearですべて解除できる。
- Snapshotはビューごとのカテゴリ／選択フィルターで独立コピーを返す。
- 記録数とプリミティブ数を制限し、超過件数をGetDroppedCountで返す。
- 生成前にWantsCategoryを確認すれば、無効時の高価な採取自体を省略できる。

`SubmitDebugDraw(Store, Filter, Render.Get3D())`等は、値のSnapshotを通常の次元別窓口へ送るアダプター。通常描画の別経路やグローバルDraw関数ではない。

**Physics Worldからの自動採取、実際の接触点／Impulse／Islandの可視化、デバッグUI、選択ツール、自由カメラ、一時停止ボタン、履歴タイムラインは今回の実装範囲外。** 確定済み物理SnapshotからGeometryを作るアダプターを後から追加する。Colliderローカル座標を、変換せずワールド座標として渡してはいけない。

## 検証と移行

古い `Render.Draw(...)` は `Render.Get2D().DrawSprite(...)`、文字・矩形もGet2Dへ移す。FRenderSystem2Dという所有型も残さない。

同梱の適用器は1bb6548のclean checkoutに限定し、ソース・CMake・既存サンプル／テストの型と呼び出しを計画してから適用する。知らないSubmitGeneratedパターンや予想外のファイルを検出したら変更前に停止する。過去のTDDログは書き換えない。

この作業環境での検証は、実際の変更対象ソースと依存ヘッダーによる部分ビルド。フレームワーク全体のWindowsビルド済みとは扱わない。

技術確認先: DxLib公式の3D描画・ライト・カメラ・描画先制御の関数リファレンス。テスト用DxLib.hは手書きの変換ダブルであり、実SDKのシグネチャ・ABI・デバイス動作の保証ではない。
