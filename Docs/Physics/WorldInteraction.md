# 接触・Trigger・動く床（2D／3D）

ゲーム処理は `dxf::gameplay`（または `dxf::framework`）、Worldと値の計算だけなら `dxf::physics` を使います。描画・Debug・Nativeは物理イベントの生成に不要です。単位はm・s・radです。

## Worldで観測する

`FWorldEventSettings` の `bEnabled=true` を `SetEventSettings()` へ渡します。既定は無効です。`Step()` が成功した後に `GetEventBatch()` を読みます。バッチは非消費で、次のStep・設定変更・World終了まで借用できます。長く保存するなら値をコピーします。毎StepのSnapshot採取は不要です。

次は登録済みの2D Worldで観測を開始し、1Step後のTriggerの有無を読む最小例です。実ゲームでは設定は開始時に一度行い、バッチ読取りだけを各Step後に行います。Colliderの生成から確かめる例は配布検証の [PhysicsInteraction.cpp](../../Tools/PackageConsumer/PhysicsInteraction.cpp) にあります。同じ流れで3Dも検証しています。

```cpp
#include "Dxf/RigidBody2D.h"
#include "Toolbox/Utility.h"

bool ObserveOneStep(Dxf::FPhysicsWorld2D& World)
{
    Dxf::FWorldEventSettings Settings;
    Settings.bEnabled = true;
    World.SetEventSettings(Settings);
    World.Step(1.0 / 60.0);
    const auto& Batch = World.GetEventBatch();
    if (!Batch.bPublished || Batch.bOverflowed)
    {
        throw Toolbox::FException("World events were not published");
    }
    for (const auto& Event : Batch.Events)
    {
        if (Event.Kind == Dxf::EWorldEventKind::Trigger)
        {
            return true;
        }
    }
    return false;
}
```

- `EColliderResponse::Solid` は物理応答あり、`Sensor` は検知だけです。SensorはSolver・CCDの応答・キャラクター移動の障害物へ入りません。
- `FColliderCollisionFilter` は双方のCategory／Maskが相手を許可した場合だけ有効です。問い合わせ用の `QueryCategory` とは別です。
- ContactはSolid同士で少なくとも一方Dynamic、TriggerはSensorを含み少なくとも一方非Staticです。自己Bodyの組は除外します。Triggerは固定更新完了時点の重なりで、途中で通り抜けた形状の連続検出ではありません。
- Begin／Stay／EndはColliderの完全な世代付きIDごとに、成功したStepで一度だけ確定します。Solver反復数では増えません。Bodyに複数Colliderがある場合は複数の組になります。
- 通常のEnd理由はSeparated／Removed／FilterChanged。区分の変更では旧種類のEndの後、新種類のBeginを発行します。Contactの法線はBからAへ向き、同心などで決まらない場合は空です。
- `ContactMargin` は観測の距離だけです。キャラクターのSkinWidthより小さいと、衝突で止まってもContact表示が出ない場合があります。GameplaySampleはSkinWidth 0.02mに対して0.025mを指定し、Solverの値は変えていません。
- 有効化・設定変更の初回は `bReset` となります。`MaxPairs` 超過は `bOverflowed` と `RequiredPairs` を確認し、部分的な成功として扱いません。失敗したStepは `bPublished=false` です。以前に記録した集合との遷移と、新規観測を混同しないでください。

## ゲームの通知と寿命

`DPhysicsScene2D`／`DPhysicsScene3D` のGameObjectに `DContactListener2DComponent`／`3DComponent` を追加し、既存のBodyを監視します。`DTriggerVolume2DComponent`／`3DComponent` はSensor一つを所有し、相手Bodyごとの入場・退場と占有数を提供します。ゲーム側で同じ形状を二重登録する必要はありません。

Sceneがイベントを有効にします。通知はWorld.Step内ではなく、同じ固定更新が成功した直後に予約順で届きます。固定更新が0回なら届かず、複数回なら各Stepのバッチを届けます。通知対象または所有者にDestroyが要求されていれば、その後の通知は行いません。所有者の実解放は既存の境界で行います。通知内から取得物をDestroyし、Scene遷移を予約できます。通知の例外はApplicationの失敗として終了し、残りをPASS扱いにしません。

Triggerの占有数は相手Body単位です。同じBodyのColliderが二つ入っても一人で、最後の組が消えた時に退場します。観測設定を変更した場合は完全な新バッチから再構築し、残存BodyのEnterを重複発行しません。中断期間を挟んで消えたBodyは `ObservationReset` で退場します。この理由はGameplayの再同期であり、Worldが物理的な離脱原因を特定した意味ではありません。容量超過中は以前の占有を保ちます。

Handlerを通知中に解除・置換しても、その呼び出しが終わるまで捕捉値の寿命を保ちます。共有所有の確保はHandler設定時で、通知のたびにコピー・確保する方式ではありません。

## 動く床

`DKinematicMover2DComponent`／`3DComponent` にColliderと初期Poseを渡し、`SetPath()` の固定更新累計秒数から目標Poseを返すか、`SetTarget()` で次Stepの目標を渡します。Componentは制限内の並進・角速度を決め、Worldが一度だけ積分します。床の登録順やView数で時間が増えません。

`Teleport()` は瞬間移動で、速度と補間履歴をリセットします。乗員を過去の変位で運ぶ操作には使いません。通常の移動経路で許容速度を超える要求は黙ってClampせず失敗します。

キャラクターの追従・支持点速度・回転経路の上限は[キャラクター移動](CharacterMovement.md)を参照してください。Dynamicとの双方向押し合い・カプセルは実装済みです。距離拘束は[Joint](Joints.md)を参照してください。回転形状全般の厳密CCDとMeshは未対応です。

## 既存GameplaySampleで試す

`GenerateProjectFiles.bat -Development` のGameplaySampleから **I** で相互作用コースへ入り、Tabで2D／3Dを切り替えます。通常のStarterは空のまま、Sandboxも従来のゲーム例を維持しています。

| 操作 | 動作 |
| --- | --- |
| A/D、3DはW/Sも | 移動 |
| Space | 次の固定更新で一度ジャンプ |
| R | 最後のチェックポイントへ復帰 |
| M | 現在乗っている移動床をDestroy |
| V | 1／2画面の切替 |
| P | Scene時計のポーズ／再開 |
| J／K／L／N／B | Jointへ力積／解除／再接続／再生成／動く支点の停止・再開。Pause・Modal中は抑止 |
| F1 | 設定Modal。物理を止め、既存UIボタンで画面数を変更して戻る |
| H | 状態UIの表示切替（入力と物理更新は継続） |
| I／Escape | 従来のキャラクター例へ戻る／終了 |

箱のContact、取得物の一度だけの破棄、複数Bodyを数える圧力板、遅れて閉じる扉、横移動・昇降・回転床、チェックポイントと危険領域を同じ規則で構成しています。各責務はInteraction*.h/.cppに分け、Sceneは配置と接続を行います。実行ファイル・CPUのApplication試験・Native描画試験は同じ `dxf_gameplay_sample` をリンクします。

自動試験のコース移動には、仕掛けの開始位置への公開Teleportも含みます。全区間を物理キーのみで連続完走した記録ではありません。試行別結果・配布・測定・未実施事項は[今回の検証記録](../Development/WorldInteraction-2026-09-27.md)を参照してください。

## 仕掛けの操作と既存Trigger

TriggerやUIの値はゲームコントローラーへ渡し、固定更新からJoint ComponentのRequestConnect/Disconnect/Drive/Limitsへ送ります。UIからWorld.Step/Pose変更を直接行いません。既存GameplaySampleはF1の既存操作部品で扉・直動・昇降・固定連結を操作します。Modal/PauseではSampleの要求を抑止し、Componentの次の固定更新までの保留と区別します。キャラクターのDynamic床追従は新保証に含みません。[Joint契約](Joints.md)。
