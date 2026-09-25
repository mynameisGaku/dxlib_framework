# APIの契約と使い方

起動・入力・Scene切替・オブジェクト・音声を組み合わせる入口は[小さいゲームを組む](SmallGame.md)です。

FBXモデルの読み込み・インスタンス・アニメーション再生は[モデルの利用手順](DxLibFbx.md#アプリケーションから使う)を参照してください。
モデルの基本PBR・モーフ・追加UV・カメラ／ライトのAPIと制限は[FBX対応範囲](FbxSupport.md)を参照してください。

現在の2D（円・回転矩形）／3D（球・OBB）への読み取り専用問い合わせは[Physics Worldの最短線分問い合わせ](Physics/WorldSegmentQuery.md)を参照してください。

```cpp
// 2D: 自分のBodyを除いて、Eye→Target（物理ワールド座標、メートル・Y上向き）で最初に当たるCollider。
const auto Hit = World2D.RaycastClosest(Eye, Target, SelfBody);
// 対象カテゴリを絞る場合（2D／3D共通）。カテゴリはColliderDescriptionのQueryCategoryで登録する。
Dxf::FWorldQueryFilter Sight;
Sight.IncludeCategories = ObstacleCategory | CharacterCategory;
const auto Seen = World2D.RaycastClosest(Eye, Target, SelfBody, Sight);
```

範囲（2Dの円／3Dの球）と重なる全Colliderの取得は[範囲問い合わせ](Physics/WorldOverlapQuery.md)を参照してください。

円／球を終点まで動かしたときに最初に接触するColliderと、取得できた場合の接触法線は[スイープ問い合わせ（SweepClosest）](Physics/WorldSweepQuery.md)を参照してください。接触の手前で止めて1回だけ滑らせる移動候補は[ComputeSlideMove](Physics/WorldSlideMove.md)です。

初期重なりの解消・反復滑り・接地・坂・段差・重力・ジャンプを行うキャラクター移動は[キャラクター移動（2D円・3D球）](Physics/CharacterMovement.md)です。Scene内では `DCharacterMovement2DComponent` / `DCharacterMovement3DComponent` を追加して入力を渡すだけで使えます。Physicsだけで使う場合は `StepCharacter` です。機能ごとの2D／3Dの状態は[進捗表](Physics/Progress.md#ゲームプレイ基盤の進捗表)を参照してください。

```cpp
// 自分を除いて、半径8の範囲に接するキャラクターのCollider IDをすべて取得する（スロット昇順、値所有）。
const auto Candidates = World2D.OverlapAll(Toolbox::FCircle2D{Eye, 8.0f}, SelfBody, Characters);
```

これらのWorld問い合わせは、Worldが自動で保つ索引で候補を絞ります。登録・同期の呼出しは不要で、結果・順序・失敗は索引がない場合と同じです。回数・木の訪問数・詳細判定の数・総当たりへの切り替えの回数などの任意の診断（`SetQueryDiagnosticsEnabled`／`GetQueryDiagnostics`／`ResetQueryDiagnostics`、型は`Dxf/WorldQueryDiagnostics.h`の`FWorldQueryDiagnostics`）は[World問い合わせの索引](Physics/QueryAcceleration.md#診断任意)を参照してください。

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
| `OnFixedTick(const FFixedTickContext&)` | 固定時間更新。物理シーンが子階層へ配る |
| `OnDraw(FRenderContext&) const` | 描画要求を出す |
| `OnDeinitialize() noexcept` | 終了と部分初期化の後始末 |
| `OnEnter(...) noexcept` / `OnExit() noexcept` | Sceneを現在の場面として有効化・無効化する |

OnDeinitializeは、**初期化を一度でも試みた場合に、部分失敗でも一度呼びます**。取得していない資源にも安全な実装にしてください。初期化する前にDestroyされた対象には呼びません。資源の実所有はRAIIへ任せるのが基本です。

初期Sceneの失敗はApplication起動失敗です。差し替えSceneの準備失敗は現在のSceneを維持し、`GetLastTransitionError()`へ記録します。一方、実行中に追加したObjectの初期化が失敗し、`CommitObjects()`がエラーを返した場合、この版のApplicationは停止します。エラーを握りつぶして続行する方針にはしていません。

OnTick／OnFixedTick／OnDrawから例外が出ると、固定のライフサイクル入口がTResultのエラーへ変換します。Application経由では、そのエラーを受けて終了処理を実行します。noexceptのフック、デストラクタ、バックエンドの解放処理から例外を出してはいけません。

## 固定更新と物理シーン

`DPhysicsScene2D`／`DPhysicsScene3D` は `FFixedStepScheduler` の計画に従い、
1フレームに0回以上の固定更新を実行します。呼び出し元はシーンだけです。
1回の固定更新は、子階層への `OnFixedTick` 配布、物理ワールドの更新の順序です。
オブジェクトの生成・破棄の確定は描画フレームの境界で行い、物理の力・姿勢変更は
固定更新の境界で適用します。`OnTick` はfinalで固定更新の駆動に使うため、
物理シーン利用者は毎刻みの処理を `OnFixedTick` へ書きます。

`FFixedTickContext` の保持入力は毎更新で有効です。押下・解放のエッジは、
フレーム内最初の更新と、固定更新が0回だったフレームの複写だけで有効になり、
二重に発火しません。可変更新のSnapshotは消費も変更もせず、音声は固定更新で扱いません。

`DRigidBody2DComponent`／`DRigidBody3DComponent` はDynamicの物理状態を正とし、
Kinematicはゲーム側の速度指示を物理へ渡します。描画位置は前回・今回の物理姿勢と
補間割合から求め、物理状態へ書き戻しません。`Teleport` は補間履歴と接触記録を破棄します。

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

`Render.Native(Callback)` は、前の要求を実行してからコールバックを呼び、描画先と対応する2D状態を再設定します。エラーや例外でも復元を試みます。0.3以降はコールバックが失敗した場合も、復元結果にかかわらずそのフレームを中止します。Native区間を跨いだ並べ替えは行いません。

復元対象は、描画先・描画領域・ブレンド・輝度・フィルタ・この接続部が設定するZバッファフラグ・シェーダー選択です。DxLibの全グローバル状態を保存・復元する機能ではありません。カメラ、ライティング、描画先に対する特殊設定など、これ以外を変更したらコールバック側で戻してください。

コールバック内からContextのDrawを再登録したり、Nativeを再入呼び出ししたりすることは拒否します。管理中のハンドルの削除、DxLib_Init／DxLib_End、管理外のセッション操作は行わないでください。

通常の利用ではDxLib.hは不要です。Native区間でDxLibを直接呼ぶ利用側のcppには、別途そのSDKのincludeディレクトリを設定します。

## 音と終了

`FSound`は音データ、`FPlaybackHandle`は一回の再生です。Playのたびに独立した再生を作り、特定のハンドルだけ停止・音量変更できます。Memory音声は複製、Stream音声は読み込み直しで再生インスタンスを作ります。

Scene所属の効果音には `FPlaybackOptions::Scope = Context.AudioScope` を指定します。0はApplication全体に属する音として扱い、Sceneの切り替えだけでは停止しません。Scopeを自動で推測する仕組みではありません。

サービス／資源／Objectの操作はメインスレッド限定です。低レベル部品を自分で組み合わせる場合も、再生停止・要求破棄・資源無効化をDxLibSessionの終了前に済ませ、バックエンドが利用者より長く存在するようにしてください。

## 0.2の追加API

### 型付きの検索

`FindComponent<T>()`／`GetComponents<T>()`、`FindObject<T>()`／`GetObjects<T>()` は非所有の型付きハンドルを返します。Findは最初の一致、Getは一致する全件のスナップショットです。Pending中の対象も含みますが、破棄要求済みの対象は除きます。順序はストレージ走査順であり、更新優先度順ではありません。

### 再割り当て可能な入力

```cpp
Dxf::FInputMap Actions;
Actions.Bind("Confirm", Dxf::EKey::Space);
Actions.BindMouse("Confirm", Dxf::EMouseButton::Left);
Actions.BindPad("Confirm", 0, 0); // Pad 0、ボタン0（PAD_INPUT_1）
Actions.Bind("Left", Dxf::EKey::A);
Actions.Bind("Right", Dxf::EKey::D);
// 毎回の新しいInputSnapshotに対して、一度だけ更新します。
Actions.Update(Context.Input);
const Toolbox::f32 Direction = Actions.GetAxis("Left", "Right");
```

Bind系は妥当な割り当てでtrue、範囲外のキー／デバイス／ボタンや空のAction名ではfalseです。複数のボタンを同じActionへ割り当てるとORとして扱い、一つを押したまま別のボタンへ移っても二重にPressedを出しません。UnbindでAction全体を解除し、Clearで全割り当てと状態を除去します。Updateは一つのSnapshotにつき一度呼ぶ契約です。

### スプライトComponent

`DSpriteRendererComponent(Texture, Position, Options)` をAddComponentすると、基盤から自動的に描画します。Texture未設定の間は何も描きません。PositionとOptionsはこのComponentが持つ独立した2D値です。親GameObjectへ暗黙にTransformを追加したり、親座標へ自動追従したりはしません。

### 入力検証と失敗後の扱い

パス・フォント名・ウィンドウ名・描画文字はUTF-8です。埋め込みNUL・過長・不正なシーケンスを拒否します。空の文字列は文字描画・既定フォント・ウィンドウ名では許可、資源パスでは拒否します。

音声保存形式の列挙値はキャッシュ参照より前に検証します。別のISoundBackendで作られたSoundをAudioPlayerへ渡すと拒否します。

描画キューの実行が失敗したフレームでは、その後のEndFrameも失敗してPresentしません。次のフレームは改めてBeginFrameから開始できます。0.3以降はClearTargetやNativeコールバックの失敗もフレーム内に保持します。最初のエラーはEndFrameまたはCancelFrameまで保持し、そのフレームをPresentしません。一方、無効な引数による要求の拒否や、元のTargetへの復元に成功した切り替え失敗は、呼び出し元が結果を確認して扱う契約です。すべてのAPIエラーを一律にフレーム全体の失敗へ変換するわけではありません。

Objectのコンストラクタ中に所属Collectionへ終了が要求された場合、Spawnは失敗し、そのObjectを登録しません。終了要求を取り消して同じCollectionを再利用するAPIはありません。

## 0.3の終了・再入契約

`RequestQuit()`は取り消せない停止要求です。現在および準備中のSceneと子階層を停止予約し、実行中のコールバックが返るまでそのオブジェクトは破棄しません。残りの子の初期化・更新・描画は実行しません。`FApplication`は実行順序の確認点で終了へ進みます。単独で`FSceneNavigator`を使う場合は、利用側が`Shutdown()`またはデストラクタで後始末してください。

通常の別々の`RequestChange()`は最後の要求を採用します。ただし、Pending Sceneを置き換える際のデストラクタ内からの差し替えやCommitは拒否します。デストラクタからShutdownされた場合も、現在実行中のスタックを即座に破棄せず、差し替え操作が戻る境界で確定します。

`FAudioPlayer::Play()`中に音声バックエンドから終了された場合、部分的に確保した再生を回収してInvalidStateを返します。バックエンドが例外を投げる場合はBackendFailureです。`Tick()`中にShutdown／Stopで他の再生が無効化されても、ハンドルを解決し直して無効な対象を飛ばします。ポーリングに失敗した対象は停止し、そのTickはエラーを返します。

`FResourceRegistry::Shutdown()`は冪等です。解放対象を内部リストから切り離してから解放コールバックを実行します。停止後に新しいResourceをRegisterした場合は、それを解放してfalseを返す従来の契約を維持します。バックエンドの寿命とnoexceptの解放契約は引き続き利用側の責任です。

## ログ

`Toolbox/Log.h`の`DXF_LOG_VERBOSE` / `DXF_LOG_INFO` / `DXF_LOG_WARNING` / `DXF_LOG_ERROR`で、分類名とprintf形式の本文を記録します。
既定の出力先はVisual Studioの出力ウィンドウと標準エラーで、`SetLogSink`で差し替えられます。詳細は[Logging.md](Logging.md)を参照してください。

## ビュー座標と簡易選択

`Dxf/ViewCoordinates.h`の`ProjectWorldToScreen`と`MakeViewPickSegment`はNativeなしでも使える。[座標・失敗条件と最小例](Rendering/ViewCoordinates.md)を参照。球/OBBとの交差は`Toolbox/SegmentIntersection.h`。

## Physics観察値の選択

`Dxf/PhysicsDebugPicking3D.h`の`PickPhysicsDebugSnapshot3D(Snapshot, Segment)`は、保存した球/OBBの最近接交点を返すDebug層の値問い合わせ。[契約とRenderDebug操作](Rendering/PhysicsSnapshotPicking.md)。既存support単独の座標変換はDebugに依存しない。
