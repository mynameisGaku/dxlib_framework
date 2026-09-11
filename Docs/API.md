# APIの契約と使い方

## GameObject／Componentの作成

`DGameScene::Spawn<T>(引数...)` と `DGameObject::AddComponent<T>(引数...)` は、コンストラクタ引数を転送してインスタンスを登録し、`TResult<TObjectHandle<T>>` を返します。型は対応する基底を継承してください。

**Spawnの成功は、OnInitializeの成功を意味しません。** 通常は次のフレーム境界で初期化します。返されたハンドルはPending中も解決できるので、初期設定を与える用途に使用できますが、`Get() != nullptr` と `IsInitialized()` は別です。

```cpp
auto Created = Spawn<DEnemy>(EnemySettings);
if (!Created)
{
    return Dxf::TResult<void>::Failure(Created.Error());
}
m_Enemy = Created.Value();
```

ハンドルを保存し、必要な時点で解決してください。`Get()`の戻り値を長期間保存しても寿命は延びません。Destroy要求、初期化失敗、Scene終了、スロットの再利用を跨いだハンドルは解決に失敗します。

Destroyは即時deleteではありません。予約された対象をそれ以降の処理から外し、確定境界で終了・解放します。所属が異なるCollectionへハンドルを渡しても削除しません。

## フックと失敗

| フック | 用途 |
|---|---|
| `OnInitialize(const FInitContext&)` | 資源の取得とローカルな構築。TResultで成功・失敗を返す |
| `OnTick(const FTickContext&)` | 更新。入力・時間・必要なゲーム実行サービスを使う |
| `OnDraw(FRenderContext&) const` | 描画要求を出す |
| `OnDeinitialize() noexcept` | 終了と部分初期化の後始末 |
| `OnEnter(...) noexcept` / `OnExit() noexcept` | Sceneを現在の場面として有効化・無効化する |

OnDeinitializeは、**初期化を一度でも試みた場合に、部分失敗でも一度呼びます**。取得していない資源にも安全な実装にしてください。初期化する前にDestroyされた対象には呼びません。資源の実所有はRAIIへ任せるのが基本です。

初期Sceneの失敗はApplication起動失敗です。差し替えSceneの準備失敗は現在のSceneを維持し、`GetLastTransitionError()`へ記録します。一方、実行中に追加したObjectの初期化が失敗し、`CommitObjects()`がエラーを返した場合、この版のApplicationは停止します。エラーを握りつぶして続行する方針にはしていません。

OnTick／OnDrawから例外が出ると、Applicationがエラーに変換して終了処理を実行します。noexceptのフック、デストラクタ、バックエンドの解放処理から例外を出してはいけません。

## 入力と時間

```cpp
Context.Input.IsDown(Dxf::EKey::D);
Context.Input.WasPressed(Dxf::EKey::Space);
Context.Input.WasReleased(Dxf::EKey::Space);
```

Snapshotはフレーム内で何度読んでも変わらず、押下情報を消費しません。フォーカスを失った場合はキー・ボタンを解放扱いにします。マウスとPadにも同様の問い合わせがあります。ゲームパッドは最大4台・ボタン16個・左スティックが対象です。ボタン番号はDxLibのPAD_INPUT_1～16に対応し、コントローラー共通のA/B表記へは変換しません。

`DeltaSeconds`は最大差分制限とSceneの時間倍率を適用する時間です。`UnscaledDeltaSeconds`は実際のフレーム差分です。SceneClockに対するポーズ／倍率の変更は、次に時間を適用する更新から有効です。

Scene自身はポーズ中もTickする既定値です。通常Objectは停止し、`SetTickWhenPaused(true)` のComponentなどは動かせます。

## 描画と資源

`FAssetService::LoadTexture`／`LoadSound`／`LoadFont` は、条件が同じ資源への参照を再利用します。相対パスの字句的な正規化は行いますが、実体の同一性を調べるためのシンボリックリンク解決やファイル更新監視は行いません。

`FRenderContext` のDraw／DrawText／FillRectangleは、その場でDxLibを呼ぶのではなく要求を登録します。失敗を確認してください。Layer → Order → 同条件の登録順で表示します。TransformのPivotは画像内ピクセル座標、PositionはそのPivotを配置する画面座標、角度はラジアンです。

RenderTargetを読みながら同じTargetへ書く操作は拒否します。切り替える前には、それ以前の描画を実行します。

```cpp
// OnDrawへ渡されたContextだけで、明示的な描画先切り替えができます。
auto Changed = Render.SetRenderTarget(m_RenderTarget);
// エラーを確認してから、そのTargetへのDraw等を行います。
auto Returned = Render.SetBackBuffer();
// エラーを確認してから、RenderTarget.AsTexture()を描画します。
```

Native接続部のClearはRGBのみです。FColorのAは255固定で、それ以外はエラーになります。**A=255がRenderTargetのアルファを255へ塗る契約ではありません。** RenderTargetのクリア時のアルファはDxLibの動作に従い、任意アルファ値を指定したクリアは未対応です。

## Native区間

`Render.Native(Callback)` は、前の要求を実行してからコールバックを呼び、描画先と対応する2D状態を再設定します。エラーや例外でも復元を試み、復元失敗時はフレームを中止します。Native区間を跨いだ並べ替えは行いません。

復元対象は、描画先・描画領域・ブレンド・輝度・フィルタ・この接続部が設定するZバッファフラグ・シェーダー選択です。DxLibの全グローバル状態を保存・復元する機能ではありません。カメラ、ライティング、描画先に対する特殊設定など、これ以外を変更したらコールバック側で戻してください。

コールバック内からContextのDrawを再登録したり、Nativeを再入呼び出ししたりすることは拒否します。管理中のハンドルの削除、DxLib_Init／DxLib_End、管理外のセッション操作は行わないでください。

通常の利用ではDxLib.hは不要です。Native区間でDxLibを直接呼ぶ利用側のcppには、別途そのSDKのincludeディレクトリを設定します。

## 音と終了

`FSound`は音データ、`FPlaybackHandle`は一回の再生です。Playのたびに独立した再生を作り、特定のハンドルだけ停止・音量変更できます。Memory音声は複製、Stream音声は読み込み直しで再生インスタンスを作ります。

Scene所属の効果音には `FPlaybackOptions::Scope = Context.AudioScope` を指定します。0はApplication全体に属する音として扱い、Sceneの切り替えだけでは停止しません。Scopeを自動で推測する仕組みではありません。

サービス／資源／Objectの操作はメインスレッド限定です。低レベル部品を自分で組み合わせる場合も、再生停止・要求破棄・資源無効化をDxLibSessionの終了前に済ませ、バックエンドが利用者より長く存在するようにしてください。
