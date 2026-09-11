# 設計と責務

## 依存の方向

FoundationはDxLibを知りません。DxLibSupportはGameObjectを知りません。Runtimeは実際のDxLib関数を直接呼びません。GameplayはRuntime上にGameObjectとComponentを構成します。Nativeは入力・資源・描画などの抽象インターフェースを、DxLibの呼び出しへ変換します。

公開Nativeヘッダーには `Windows.h` と `DxLib.h` を含めません。したがって基盤のテストや公開ヘッダーの利用は、Windows SDKなしでも可能です。

## 所有関係

```text
FDxLibBackends                         Applicationより長く存在
  └─ 各種DxLibバックエンド

FApplication
  ├─ FDxLibSession                     DxLibの初期化・終了
  ├─ FInputSystem                      入力の取得とSnapshot構築
  ├─ FAssetService                     Loader・Cache・Registryの組み合わせ
  ├─ FRenderSystem2D                   描画要求とフレーム処理の組み合わせ
  ├─ FAudioPlayer                      再生インスタンスの所有
  ├─ DGameInstance                     ゲーム固有の長期状態
  └─ FSceneNavigator
       ├─ FSceneStorage                現在と準備中のSceneを所有
       └─ FSceneLifecycle              準備・有効化・終了の手順

DGameScene
  └─ FGameObjectCollection
       ├─ FGameObjectStorage           実体の唯一の所有者
       ├─ FGameObjectLifecycle         生成・破棄・初期化・終了
       ├─ FGameObjectUpdater           更新対象と更新順
       └─ FGameObjectDrawDispatcher    OnDrawの呼び出し

DGameObject
  └─ FComponentCollection
       ├─ FComponentStorage
       ├─ FComponentLifecycle
       ├─ FComponentUpdater
       └─ FComponentDrawDispatcher
```

Object系とComponent系で同じアルゴリズムを二重実装しないよう、Storage／Lifecycle／Updater／DrawDispatcherはそれぞれ専任のテンプレートを共有し、型付きの名前で公開しています。細分化の単位は処理の判断と規則です。単にファイル数を増やすための同一コードの複製はしていません。

## 継承関係

```text
DObject
  └─ DLifecycleObject
       ├─ DGameInstance
       ├─ DScene
       │    └─ DGameScene
       ├─ DGameObject
       └─ DGameObjectComponent
```

`DObject`はRTTIと多態的な破棄の基底です。`DLifecycleObject`はユーザーフックを呼ぶ固定手順を共有します。`DGameScene`・`DGameObject`は子のCollectionを構成として持ち、基盤の入口が子の処理を実行します。ユーザーが親のOnTickを呼ばなくても、自動更新が消える構造にはしていません。

汎用のComponent基底、全Objectを所有する巨大Manager、GameObjectへの座標の強制付与は行っていません。GameObjectと無関係な部品は通常のC++クラスとして作れます。

## 更新・生成・破棄

所有ストレージのコンテナは外へ直接公開しません。操作はCollectionのSpawn／Destroyへ集約します。全階層の確定対象を先にスナップショット化し、その後にユーザーの初期化・終了処理を呼びます。既存オブジェクトへ初期化処理の途中で追加された子は、別の確定境界まで参加しません。

例外は、新しく初期化するオブジェクトの構築手順です。そのOnInitializeで追加した子は、親の初期化トランザクションに参加します。子が失敗すると、親も終了処理を行って除去します。

破棄要求を受けた対象は、その場でハンドル解決を無効化し、後続のTick／Draw対象から外します。メモリの解放は境界へ遅延します。スナップショットにはポインタではなくハンドルを入れ、実行直前に解決し直します。

## Scene切り替え

RequestChangeは要求の受付だけです。複数要求は最後の要求を採用します。次の境界で新Sceneを初期化し、成功した場合だけ旧Sceneを終了して差し替えます。失敗時は準備中のSceneだけを片付け、現在のSceneを維持します。

準備中のOnInitializeにはAssetServiceだけを渡します。BGM開始やGameInstanceへの反映などはOnEnterに置きます。ただしC++コードから外部の状態へアクセスすることを物理的に禁止する仕組みではないため、利用者が準備中に外部へ副作用を起こした場合の巻き戻しまでは保証しません。

## 資源と描画

Loaderは読み込み、Cacheは条件ごとの再利用、Resourceはネイティブハンドルの所有、RegistryはApplication終了時の無効化を担当します。キャッシュは弱参照なので、不要な資源をキャッシュだけが永久に保持しません。

描画側は、Contextが要求受付、Queueが保持・検査・安定ソート、RendererがDxLib呼び出し、Presenterがフレームのクリア・提示を担当します。GameObjectのOnDraw順と、最終的な描画順は同じ責務にしません。

ContextからTarget切り替えやNative描画へ進む場合も、専用の `IRenderControl` に委譲します。ContextがApplication全体へアクセスすることはありません。

## 1フレーム

1. 終了要求・メッセージを確認し、時間と入力を取得します。
2. Scene切り替えとObject／Componentの追加・破棄を確定します。
3. GameInstanceを実時間側のFrameTimeで、SceneをSceneClock適用後の時間で更新します。
4. 再生終了した音を回収し、描画先をクリアします。
5. Sceneの描画フックから要求を集め、並べて実行し、ScreenFlipします。
6. 不要な弱参照を整理します。

途中で終了が要求された場合、実行中のユーザーコールバックが返るまで破棄せず、後続の描画や提示は取りやめます。

## 終了順序

Sceneとその子 → GameInstance → 音声再生の停止 → 未実行描画の破棄 → 全資源の解放・無効化 → DxLib終了、の順です。共有Textureなどの参照だけが外部に残っても、後からDxLib終了後の削除を行わないようにしています。
