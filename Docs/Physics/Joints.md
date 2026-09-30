# 距離Joint（2D／3D）

重心基準の二つのLocal Anchor間の距離を維持する両側拘束です。張る方向だけのRopeやSpring、角度を固定するHingeではありません。少なくとも一端がDynamicである必要があります。PhysicsはWorldと求解、Gameplayは接続要求とComponent寿命、Sampleは線と文字の描画を担当します。

## Physicsだけで使う

`dxf::physics`をリンクします。2D／3DともWorldがBodyとJointの唯一の所有者です。

```cpp
Dxf::FPhysicsWorld2D World;
Dxf::FBodyDescription2D Anchor;
Anchor.Type = Dxf::EBodyType::Static;
const auto A = World.CreateBody(Anchor);
Dxf::FBodyDescription2D Weight;
Weight.Position = {0, -2};
const auto B = World.CreateBody(Weight);
Dxf::FDistanceJointDescription2D Settings;
Settings.Length = 2;
const auto Id = World.CreateDistanceJoint(A, B, Settings);
World.Step(1.0 / 60.0);
const auto State = World.GetDistanceJoint(Id);
(void)World.DestroyJoint(Id);
```

```cpp
Dxf::FPhysicsWorld3D World;
Dxf::FBodyDescription3D Anchor;
Anchor.Type = Dxf::EBodyType::Static;
const auto A = World.CreateBody(Anchor);
Dxf::FBodyDescription3D Weight;
Weight.Position = {0, -2, 0};
const auto B = World.CreateBody(Weight);
Dxf::FDistanceJointDescription3D Settings;
Settings.Length = 2;
Settings.LocalAnchorB = {0, 0.1f, 0.06f};
const auto Id = World.CreateDistanceJoint(A, B, Settings);
World.Step(1.0 / 60.0);
const auto State = World.GetDistanceJoint(Id);
(void)World.DestroyJoint(Id);
```

Local AnchorはBodyの姿勢で回転して重心位置へ加えます。長さはm、角度はradです。`GetDistanceJoint`は指定値・実測距離・誤差を返す読み取りです。生成・起床・Stepを行いません。別World、失効ID、同じBody、両端非Dynamic、不正数値は例外です。破棄系は失効時false。IDのWorld・Index・Generationをまとめて照合します。

## Componentで使う

`dxf::gameplay`または`dxf::framework`をリンクし、同じ`DPhysicsScene2D/3D`のObjectへ追加します。Gameplayは実装cppを持つ静的ライブラリです。Body作成、手動Impulse、終了時の順序制御をゲーム側へ要求しません。

```cpp
Dxf::FDistanceJointComponentDescription2D Settings;
Settings.BodyA = Dxf::FPhysicsBodyReference2D::FromRigidBody(AnchorHandle);
Settings.BodyB = Dxf::FPhysicsBodyReference2D::FromRigidBody(WeightHandle);
Settings.Joint.Length = 2;
const auto Added = Owner.AddComponent<Dxf::DDistanceJoint2DComponent>(Settings);
if (!Added)
{
    throw Toolbox::FException(Added.Error().Message);
}
```

```cpp
Dxf::FDistanceJointComponentDescription3D Settings;
Settings.BodyA = Dxf::FPhysicsBodyReference3D::FromRigidBody(AnchorHandle);
Settings.BodyB = Dxf::FPhysicsBodyReference3D::FromRigidBody(WeightHandle);
Settings.Joint.Length = 2;
Settings.Joint.LocalAnchorB = {0, 0.1f, 0.06f};
const auto Added = Owner.AddComponent<Dxf::DDistanceJoint3DComponent>(Settings);
if (!Added)
{
    throw Toolbox::FException(Added.Error().Message);
}
```

`AnchorHandle`と`WeightHandle`はそれぞれ同次元の`TObjectHandle<DRigidBody...Component>`です。Moverには`FromKinematicMover()`、Worldで直接作ったBodyには`FromBodyId()`を使います。参照は所有せず、名前検索、Registry、自動的なID更新を行いません。MoverにRigidBodyを重ねて生成しません。参照値の保持自体は確保を伴いません。

## 固定更新と状態

全Bodyの固定更新後、既存PrePhysics予約で参照を解決しJointを作ります。JointのObjectを先に登録してもBody登録を待てます。PhysicsScene以外や途中で別Worldへ移したComponentは拒否します。World.Step成功後にだけPostPhysicsが観察値を採取します。

| 状態 | 意味 |
|---|---|
| `PendingBodies` | 有効なComponent参照だがBodyが未登録 |
| `Connected` | 完全な世代付きIDのJointが生存 |
| `Disconnected` | 解除要求が反映済み、または終了済み |
| `EndpointLost` | Body・参照・World側Jointが失効 |

`RequestDisconnect()`と`RequestConnect(Settings)`は要求です。同じ固定更新までの最後の要求を採用します。同じ有効な設定の再要求はIDとWarm Startを維持します。Pauseで固定更新がなければ反映しません。ゲーム側がPause中の要求を禁止する場合は入力側で抑止します（Sampleは抑止）。

数値不正は構築／要求時に拒否します。同一Body・別World・失効参照・両端非DynamicはPrePhysicsで拒否し、初期待機には読み替えません。再接続は新Jointの生成成功後に旧Jointを解放します。検証／確保が失敗しても旧接続を先に壊しません。失敗は例外として上位へ返り、Applicationの失敗フレームはPresentしません。EndpointLost後は明示的な接続要求が必要です。同slotの新世代に自動接続しません。

`GetConnectionState()`、`GetJointId()`、`GetObservation()`は読み取りです。破棄要求を受けたハンドルや外部破棄も検出し、古いConnectedを有効なIDとして返しません。失敗した固定更新でComponentまで到達していれば観察値を無効化します。配送がComponent到達前に失敗した場合とは区別します。

`FDistanceJointObservation2D/3D`は、Joint ID、Componentの成功PostPhysics通し番号、指定長、実測長、誤差、物理姿勢のAnchorを値として返します。再接続しても通し番号をリセットしません。これはWorld全体のフレーム番号ではありません。`GetRenderAnchors(A,B)`はRigidBody／Moverの補間姿勢を使い、物理の観察値と区別します。明示ID参照には補間履歴がないため現在のWorld姿勢を使います。

## 寿命・接触・休止・並列

Body破棄は接続Jointを失効させます。Collider DetachだけではJointを破棄しません。Component終了はJointだけを解放し、両Bodyを残します。PhysicsSceneは子Componentの終了までWorldを保持します。Jointの解放で確保しないよう、登録時に空きslot領域を先に予約します。

JointはCollision Filterから独立しています。接続したBody対のContactを自動で無効にしません。休止・起床はContactとJointを共有したIsland単位で伝播し、共有Static／Kinematicを介して別Dynamic島を結合しません。直列とJob経路の順序はContact→DistanceJointで共通です。一本の鎖は一Islandであり、レーンを増やして鎖内部を分割する実装ではありません。

Joint作業値は全SubStep・索引更新・イベント発行が成功してから確定します。失敗時にJointの再利用値を確定しません。Body・外力・Contactまで巻き戻す保証はありません。詳細は[並列実行](ParallelExecution.md)。

## 退化とCCDの制限

Anchorが一致して方向も履歴軸もない場合、方向を捏造せずその拘束のImpulseと補正を省きます。Length=0かつ一致する配置では方向拘束になりません。非一致配置で距離0を指定することも、完全なPoint-to-Pointや角度拘束の保証にはなりません。

Jointは各SubStepの離散速度求解とCCD後の位置補正で処理します。Anchor Sweep、Joint TOI、高速運動中の連続した長さ保証は未対応です。SubSteps増加による誤差の単調減少も保証しません。Worldの保守停止で未処理時間が残った場合、無関係Bodyだけを全時間進める保証はありません。

## 既存Sampleと外部利用

DevelopmentのGameplaySampleでIを押して相互作用コースへ入り、Tabで次元を切り替えます。紫の吊り下げ物、4体の鎖、動く支点と荷物を表示します。Jは力積、Kは解除、Lは再接続、Nは仕掛け再生成、Bは支点の停止／再開です。Pause／Modalと、そのModalを閉じるフレームは操作を抑止します。Vで1／2画面、PでPause。描画回数は物理・接続の更新回数へ影響しません。Starterは空のままです。

`Tools/PackageConsumer/PhysicsJoint.cpp`はPhysics単独、`JointConsumer.cpp`はComponentの初期化・解除・再接続・Body再生成・Scene終了を使う実例です。`NativeApp.cpp`は同じSceneを実描画します。再配置インストールは元のソース／Buildを直接参照しません。SDK未導入PCでの確認とは別です。

## 測定・回帰

`dxf_character_benchmark joint --pilot`の後、`joint`で本測定します。2D／3D、接続なし・独立支点・Dynamic対・共有Static／Kinematic・Contact混在・鎖・Component維持／切替、同期／4レーンを比較します。通常16/64/256接続、鎖8/32/128。各条件は新規Worldを5回作り、各回で初回、慣らし30、測定120Stepを連続運転します。

Component解決・予約、Pre、World.Step、Postを別々に計測します。Component測定は明示Body ID参照で、所有階層の配送と描画を含みません。最大Island Body数は既知の配置から導いた値であり、一般Worldの最大島を測る新APIではありません。時間と確保の中央値／最小／最大、Body／Joint／Contact／島／レーン／休止数をCSVへ出します。Component部分の確保0をゲーム全体の確保0やSolver確保0と読み替えません。

CPU回帰はFramework／InteractionSample／PhysicsContinuation／JointComponentFault、実描画はNativeGameplayDeviceSmokeです。[今回の試行別記録](../Development/JointGameplayCompletion-2026-10-01.md)を参照してください。
