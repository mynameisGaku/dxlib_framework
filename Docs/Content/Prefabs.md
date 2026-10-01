# Prefabを既存Sceneへ配置する

CPUのFSceneContentSourceで型付き定義を読み、所有側でPreparePrefab／PrepareSceneへ既存FAssetServiceを渡します。任意target `dxf::scene_content`をリンクし、ProjectRoot相対の定義と必要Assetsを配置します。新しいglobal RegistryやPhysics Worldを用意する必要はありません。

最小の実コードは`Tools/PackageConsumer/ContentConsumer.cpp`です。両次元の準備→Spawn→初期化待ち→固定更新→Ready→型付き公開先→Drive変更→Destroyをコンパイル・実行します。Native資源と描画を含む例は同じフォルダーのContentNativeConsumer.cppです。

## 所有と生成順

Object／Componentは既存Collection、Body／Collider／Jointは既存World、Native資源は既存Resource経路が所有します。Prefabは不変定義・共有資源参照と世代付き非所有Component handleを保持する任意GameObjectです。全GameObjectへTransformを強制しません。

Scene.Spawn<DPrefabInstance2D/3D>の返却は受付です。親OnInitializeの子は既存初期化トランザクションへ参加し、FixedTickで全Bodyを登録し、PrePhysicsで専任Jointを接続し、成功PostPhysics後にReadyを公開します。Prefab自身はWorld.Stepを呼びません。

|状態|意味|
|---|---|
|PendingInitialization|受付済み、Object初期化待ち|
|PendingPhysics|子は初期化済み、Body／必須Jointの登録と成功観察待ち|
|Ready|必須構成の成功観察が揃った。非物理Prefabは初期化後にReady|
|EndpointLost|外部破棄により必須Body／Collider／Component／Jointが失効|
|Destroyed|破棄要求済み。通常は既存handle自体が失効|

初期化後に型付きexportを取得できることとBody登録済みは別です。GetRigidBody／GetKinematicMover／各Joint getter／GetContactListener／各資源getterは不在・種類違い・初期化前・失効を拒否します。Ready後は既存ComponentへRequestDrive／RequestLimits／RequestConnect／RequestDisconnectを渡します。ゲーム判断は通常のC++ Controllerへ残します。

## 動的配置・表示・音

DContentScene2D/3D::SpawnPrefabは動的描画追跡も登録し、生存中の同名IDを拒否します。手書きPhysicsSceneでは通常SpawnとDrawContentを明示利用できます。Destroyは既存要求を使い、予約消費の境界まで実体を保ちます。他個体の同名部品・共有資源を強制解放しません。古いhandleが同slot新世代へ追従することもありません。

表示はBody／Moverの補間Poseを読み、2D画像／図形、3Dモデル／図形、Fontラベルを既存描画APIへ受付します。モデル個体は更新側で一回Advanceし、View追加で再生回数を増やしません。モデルscaleは物理寸法・慣性を自動変更しません。

Sound exportはGetSoundから既存AudioPlayer／AudioScopeへ渡す参照です。PrepareとOnInitializeでは鳴らしません。SampleのSensor Controllerは有効化後の通知で再生要求を出します。自動API試験と人の聴感は別です。

## 失敗境界

C++で直接作った定義もPrepareの純粋検証で全体の値・参照・配置を確認してからNative資源を要求します。実Body登録後でなければ確定できない条件と実行時確保失敗は残ります。

親初期化失敗はそのPrefabの追加物を既存終了経路で回収します。動的Spawn初期化・FixedTick・World.Stepの失敗はApplication停止契約へ伝わります。全WorldのBody rollbackや、生存Sceneの無停止維持を保証しません。[準備失敗](Loading.md)とは区別します。
