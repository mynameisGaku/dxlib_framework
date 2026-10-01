# Jointとゲームの仕掛け（2D／3D）

## Distance Joint

重心基準の二つのLocal Anchor間の距離を維持する両側拘束です。張る方向だけのRopeやSpring、一軸回転を残すRevolute/Hingeではありません。少なくとも一端がDynamicである必要があります。PhysicsはWorldと求解、Gameplayは接続要求とComponent寿命、Sampleは線と文字の描画を担当します。

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

JointはCollision Filterから独立しています。接続したBody対のContactを自動で無効にしません。休止・起床はContactとJointを共有したIsland単位で伝播し、共有Static／Kinematicを介して別Dynamic島を結合しません。直列とJob経路の順序はContact→Distance→Revolute→Fixed→Prismaticで共通です。一本の鎖は一Islandであり、レーンを増やして鎖内部を分割する実装ではありません。

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

## Revolute・Fixed・Prismatic

全種類はWorldの共通slotと`FJointId2D/3D`を使用します。`GetJointKind`、`DestroyJoint`、`IsJointAlive`も共通です。種類違いのgetter/setter、別World・失効世代を拒否します。新種類はDistanceのLength=0による近似ではありません。

|種類|2Dで拘束する成分|3Dで拘束する成分|残す自由度|
|---|---|---|---|
|Revolute（3DではHinge）|Anchorの2成分|Anchorの3成分、軸の傾き2成分|FrameのZ軸回りの回転|
|Fixed|Anchorの2成分、相対角|Anchorの3成分、相対姿勢3成分|なし|
|Prismatic|軸直交1成分、相対角|軸直交2成分、相対姿勢3成分|FrameAのX軸方向の移動|

`FJointFrame2D`は重心基準LocalAnchorとLocalAngle、3DはLocalAnchorとLocalRotationです。ワールドFrameはBody姿勢×LocalRotation、取付点は重心+回転したLocalAnchorです。Quaternionの非有限・ゼロノルムを拒否し、受理値を正規化します。q/-qは同じ回転です。`Make*JointDescription`は共通World Anchorと向きから両Local Frameを一度だけ求める読み取りhelperです。Step・Wakeしません。有限入力でもLocal差がf32で表現できなければ拒否します。

3D RevoluteのZ軸は回転軸、X軸は角度0の基準です。相対Quaternionのswing（軸の傾き）とtwist（軸回り）を分離し、twistのW/Z投影から角度を求めます。QuaternionのZ成分そのものを角度とはしません。両Z軸の内積が-0.999999未満の反平行近傍は登録・求解時に拒否します。twist投影ノルムが1e-12未満も拒否します。退化した軸を固定World Zで置き換えません。

角度は両次元で主値(-π,π]、角速度はrad/sです。多回転Motorは使えますがAngleは回転数ではありません。角Limitは折返しを跨がない-π < LowerAngle <= UpperAngle < πのみです。追加の端点余裕は設けていません。Fixedは正準化した最短相対Quaternionから姿勢誤差を求め、Euler角へ往復しません。反復補正には有限誤差があり、長いFixed鎖を完全剛体とは保証しません。

PrismaticのTranslationは`dot(FrameA.X, AnchorB-AnchorA)`です。軸はBody Aに取り付いて回転します。TranslationRateと横拘束はAnchorの速度だけでなく`dot(omegaA×axis, AnchorB-AnchorA)`を含みます。速度0 Motorは有限ブレーキで、位置ロックではありません。回転支点の離散積分誤差をなくす保証はありません。

### Limitと有限Motor

角度・並進のLimitとDriveは別の値型です。全数値は有限、MaxTorque/MaxForceは0以上。0は駆動Impulseなしです。Motor無効と有効・目標速度0の有限ブレーキは区別します。SubStep秒hごとに累積Motor Impulseを±MaxTorque*hまたは±MaxForce*hへ制限し、差分だけを適用します。速度・姿勢への直接代入ではありません。重い荷物や接触障害物では目標速度に達しないことがあります。

LimitはDisabled/Inside/Lower/Upper/Lockedを区別します。下限・上限は片側反力、等値Limitは一座標の両側ロックです。Limitから離れるMotorを止めません。Limit反力やAnchor反力はMotorの努力上限とは別です。

`SetRevoluteJointDrive/Limits`、`SetPrismaticJointDrive/Limits`はWorld.Stepと直列に呼びます。不正入力は旧設定を保持。同値設定はID・基本拘束cache・SleepTimerを変更しません。Drive/Limit変更は該当cacheだけを初期化し、必要な端点を起こします。0速度ブレーキの静止島は休止できます。

### Physicsの最小例（配布Consumerでcompile/run）

以下は同じWorld内の完全ID A/Bが生存し、少なくとも一端がDynamicの場合です。Fixedの現在相対Poseもhelperが一度だけLocal Frameへ保存します。

```cpp
// FPhysicsWorld2D World; FBodyId2D A, B;
auto Settings = World.MakeRevoluteJointDescription(A, B, World.GetPosition(B));
const auto Joint = World.CreateRevoluteJoint(A, B, Settings);
World.Step(1.0 / 60);
const auto State = World.GetRevoluteJoint(Joint);
```

```cpp
// FPhysicsWorld2D World; FBodyId2D A, B;
auto Settings = World.MakeFixedJointDescription(A, B, World.GetPosition(B));
const auto Joint = World.CreateFixedJoint(A, B, Settings);
World.Step(1.0 / 60);
const auto State = World.GetFixedJoint(Joint);
```

```cpp
// FPhysicsWorld2D World; FBodyId2D A, B;
auto Settings = World.MakePrismaticJointDescription(A, B, World.GetPosition(B));
const auto Joint = World.CreatePrismaticJoint(A, B, Settings);
World.Step(1.0 / 60);
const auto State = World.GetPrismaticJoint(Joint);
```

```cpp
// FPhysicsWorld3D World; FBodyId3D A, B;
auto Settings = World.MakeRevoluteJointDescription(A, B, World.GetPosition(B));
const auto Joint = World.CreateRevoluteJoint(A, B, Settings);
World.Step(1.0 / 60);
const auto State = World.GetRevoluteJoint(Joint);
```

```cpp
// FPhysicsWorld3D World; FBodyId3D A, B;
auto Settings = World.MakeFixedJointDescription(A, B, World.GetPosition(B));
const auto Joint = World.CreateFixedJoint(A, B, Settings);
World.Step(1.0 / 60);
const auto State = World.GetFixedJoint(Joint);
```

```cpp
// FPhysicsWorld3D World; FBodyId3D A, B;
auto Settings = World.MakePrismaticJointDescription(A, B, World.GetPosition(B));
const auto Joint = World.CreatePrismaticJoint(A, B, Settings);
World.Step(1.0 / 60);
const auto State = World.GetPrismaticJoint(Joint);
```

### Componentと目標操作

`DRevoluteJoint2D/3DComponent`、`DFixedJoint2D/3DComponent`、`DPrismaticJoint2D/3DComponent`は同じ型付きBody参照・接続状態・寿命契約を使います。`EJointConnection`は既存EDistanceJointConnectionの互換別名です。古い型・値を消していません。

Revolute/Prismaticの`RequestDrive`/`RequestLimits`は値を保持し、PrePhysicsで反映します。最後のConnect/Disconnectが意図を決め、その後のDrive/Limitはその意図の設定を更新します。後からのConnectはDrive/Limitも置換。Disconnect後のDriveだけでは再接続しません。要求集合は検証後に反映し、同じEndpoint/FrameならIDと基本拘束cacheを保持します。

```
FRevoluteJointComponentDescription2D Settings;
Settings.BodyA = FPhysicsBodyReference2D::FromRigidBody(Support);
Settings.BodyB = FPhysicsBodyReference2D::FromRigidBody(Door);
Settings.Joint.FrameA.LocalAnchor = {0, 1};
const auto Joint = Owner.AddComponent<DRevoluteJoint2DComponent>(Settings);
Joint.Value().Get()->RequestDrive({true, 1, 10});
```

3Dは同名の3D型、Fixed/Prismaticは種類別Description/Componentへ置き換えます。6種類の実例は再配置ConsumerのMechanismConsumer.cppです。Bodyを生成/破棄する責務は既存RigidBody/Moverだけにあります。

`ComputeJointTargetDrive`は角度用/並進用Settings、現在座標と速度、Limit、固定dtから有限Motor要求を返します。目標、最大速度、減速距離、位置・速度許容、最大努力を指定します。位置と速度の両方が許容内ならReached、座標だけ到達して動いていればStopping。負荷で止まっても目標から離れていればDriving。Limit外目標、非有限、dt<=0は例外として拒否します。角度は折返しを跨ぐ指令を提供しません。

```cpp
FAngularJointTargetSettings Target;
Target.TargetAngle = 0.5;
Target.MaxTorque = 10;
const auto State = Joint.Get()->GetObservation()->State;
const auto Command = ComputeJointTargetDrive(Target, State.Angle, State.AngularSpeed, Joint.Get()->GetDescription().Joint.Limits, 1.0 / 60);
Joint.Get()->RequestDrive(Command.Drive);
```

目標制御は前回の成功観察をJoint Componentの今Step無効化より前に読みます。Sampleの制御Objectを装置より先に生成し、一固定Step一要求で処理します。観察通し番号と描画補間Poseを混同しません。任意負荷・任意dtの完全収束は保証しません。

### 既存コースの操作

GameplaySampleの相互作用コースに、手動扉、電動扉、スライド扉、回転するKinematic支点の昇降機、固定連結物、昇降機にFixedで取り付けた荷物とDistance吊り下げを配置します。Tabで2D/3D、F1パネルで対象・目標・最大速度・最大Torque/Force・Limit・接続・運転/停止・一度の力積を操作します。装置ごとに意図を保持し、Modal中は要求を送らず再開後の固定更新で反映します。UIは既存Button/Slider/Toggle/Label。空StarterとSandboxは保持します。

3Dの斜め軸・非Identity姿勢、荷物による有限Forceの停止と再開を確認する試験を用意しています。Dynamic昇降機へのキャラクター床追従は未対応のままです。描画は補間BodyとFrame、全画面2D文字は成功観察を使い、表示数で物理更新を増やしません。

### 数学・統合の境界

拘束行はJv/K/Impulseの符号を揃え、異なる両Anchorの腕を使用します。方向回転R vと慣性R I Rᵀを区別します。小さい行の反復求解を採用し、位置修正ごとにFrame/J/Kを再計算します。角度位置補正はワールド微小回転で正規化し、速度へ擬似Impulseを加えません。Kは非負項の和です。K=0の有効な可動項なしは省略、非有限/負は失敗にします。巨大連立行列の逆行列失敗を0成功にする経路はありません。

TOI即時処理はContactのみです。新Joint・Motorは正規SubStepで求解し、TOI回数分のMotor予算を追加しません。Anchor Sweep/Joint TOI/任意障害物を絶対に貫通しない位置補正は提供しません。受理した設定と成功Stepだけの求解履歴は別で、Body rollbackは保証しません。

新回帰、変異K-M01〜24、Distance M01〜14、費用と試行結果は[仕掛け検証記録](../Development/MechanismJointsCompletion-2026-10-01.md)へまとめます。現時点の最終検証状況は[継続点](../Development/MechanismJointContinuation.md)を参照してください。
