# キャラクター移動（2D円・3D球）

2D（円対円・回転矩形）と3D（球対球・OBB）のキャラクター移動です。初期重なりの解消、接触面に沿う反復滑り、接地・坂・下向きの吸い付き、段差上り、重力、ジャンプ、着地を扱います。同じ契約を2D／3Dで提供します。

使い方は二通りです。

| 使い方 | 入口 | リンク先 |
|---|---|---|
| Componentで使う（推奨） | `DCharacterMovement2DComponent` / `DCharacterMovement3DComponent`（`Dxf/CharacterMovementComponent2D.h` / `3D.h`） | `dxf::gameplay`（`dxf::framework`） |
| Physicsだけで使う | `StepCharacter`・`MoveAndSlide`・`ProbeCharacterGround`・`ResolveCharacterOverlap`（`Dxf/CharacterMovement2D.h` / `3D.h`） | `dxf::physics` |

動く床への追従、剛体との押し合い（キャラクターが箱を押す・押される）、しゃがみ・カプセル形状、ネットワーク同期は提供しません。キャラクターは現在の姿勢のColliderを障害物として扱うだけです。

## Componentで使う（最小の例）

`DPhysicsScene2D`／`DPhysicsScene3D`の中のGameObjectへ追加し、入力を渡すだけです。位置の計算・Bodyの登録・物理Stepとの順序はComponentが行います。

```cpp
#include "Dxf/CharacterMovementComponent2D.h"

class DPlayer final : public Dxf::DGameObject
{
protected:
    Dxf::TResult<void> OnInitialize(const Dxf::FInitContext&) override
    {
        Dxf::FCharacterMovementDescription2D Description;
        Description.Center = {0, 0.52f};             // 円の中心（メートル、Y上向き）。床の上面y=0から半径0.5＋接触余裕0.02
        auto Added = AddComponent<Dxf::DCharacterMovement2DComponent>(Description);
        if (!Added)
        {
            return Dxf::TResult<void>::Failure(Added.Error());
        }
        m_Character = Added.Value();
        return {};
    }
    void OnTick(const Dxf::FTickContext& Context) override
    {
        auto* Character = m_Character.Get();
        const auto& Input = Context.Input;
        const Toolbox::f32 X = (Input.IsDown(Dxf::EKey::D) ? 1.0f : 0.0f) - (Input.IsDown(Dxf::EKey::A) ? 1.0f : 0.0f);
        Character->SetMoveInput({X, 0});             // 長さ1で最大速度。デバイスに依存しない要求
        if (Input.WasPressed(Dxf::EKey::Space))
        {
            Character->RequestJump();                // 次に実行される固定更新で一度だけ試す
        }
    }

private:
    Dxf::TObjectHandle<Dxf::DCharacterMovement2DComponent> m_Character;
};
// 描画は GetRenderCenter()（直前と直近の固定更新の間の補間）を使う。
```

3Dは`FCharacterMovementDescription3D`・`DCharacterMovement3DComponent`で、`SetMoveInput`へUpに直交する方向（既定はXZ平面）を渡します。完成した操作例は開発用ソリューションの`Examples/GameplaySample`（2D／3Dを[Tab]で切替）です。

### Componentの契約

- **固定更新**: 自分の`OnFixedTick`では移動を予約するだけです。同じ固定更新のすべての登録の**後**、物理Stepの**直前**に1回だけ`StepCharacter`を実行します（`FFixedTickContext::PrePhysicsStep`）。同じ固定更新で後から登録された壁も見落としません。計算が成功するまで状態は変えません。
- **Body**: `bRegisterBody`（既定true）なら、最初の固定更新でKinematicのBodyと円／球のColliderを一つだけ作り（`BodyQueryCategory`、既定1）、破棄時に解放します。位置はこのComponentだけが`SetBodyTransform`で決めます。自分のBodyは問い合わせから除外します。複数のキャラクターは互いのBodyを障害物として見ます。
- **同じオブジェクトの`DRigidBody2D/3DComponent`とは併用できません**（位置の決定権が重なるため、最初の固定更新で例外）。
- **ジャンプ要求**: 固定更新0回のフレームは次へ持ち越し、1フレームに複数回の固定更新があっても一度しか使いません。空中の要求は跳ばずに消費します。一時停止中の要求は破棄します。
- **一時停止**: 固定更新が進まないので移動もしません。再開で続きから進みます。
- **Teleport**: 中心を直接移し、速度・足元・補間履歴を初期化します（登録したBodyも移します）。
- **状態の取得**: `GetCenter`・`GetVelocity`・`GetGround`・`IsGrounded`・`GetLastStep`（着地・離地・天井・段差・吸い付き・停止理由）・`GetStepCount`・`GetRenderCenter`・`GetBodyId`。
- 描画の数（1画面・2画面）で固定更新・移動の回数は変わりません。

## Physicsだけで使う

```cpp
#include "Dxf/CharacterMovement3D.h"

Dxf::FCharacterMoveSettings3D Settings;   // 既定値は下表。Upは既定(0,1,0)
Dxf::FCharacterState3D State;
State.Center = {0, 0.52f, 0};
State.Ground = Dxf::ProbeCharacterGround(World, State.Center, Settings, SelfBody);

// 固定更新ごと。Worldは変更しない。採用するかは呼出し側が決める。
Dxf::FCharacterMoveInput3D Input;
Input.Move = {1, 0, 0};
Input.bJump = bJumpPressedThisStep;
const auto Step = Dxf::StepCharacter(World, Settings, State, Input, 1.0 / 60.0, SelfBody);
State = Step.State;
World.SetBodyTransform(SelfBody, State.Center, Toolbox::FQuaternion{});   // 自分のBodyを登録している場合
```

| 関数 | 内容 |
|---|---|
| `ResolveCharacterOverlap(World, Center, Settings, Excluded, Filter)` | 重なり（負の符号付き距離）から、最も深い面の法線方向へ接触余裕の位置まで押し出す候補。経路はSweepで確認する |
| `MoveAndSlide(World, Center, Displacement, Settings, Excluded, Filter)` | 変位を接触面に沿って反復的に解決する（3Dは二つの面の稜線に沿い、三つの面で止まる） |
| `ProbeCharacterGround(World, Center, Settings, Excluded, Filter)` | 足元の支持（接触余裕の2倍以内・Up側を向く面、歩ける面を優先） |
| `StepCharacter(World, Settings, State, Input, DeltaSeconds, Excluded, Filter)` | 1回の固定更新: 重なりの解消→足元→速度（加減速・ジャンプ・重力）→水平移動と段差上り→Up方向の移動→吸い付き→足元の再確認 |

すべて読み取り専用で、同じ入力・状態・Worldなら同じ結果です。通常経路で配列を確保しません（隔離した故障注入試験で確認）。入力・設定・World状態の異常は`Toolbox::FException`で、部分的な結果は返しません。同じWorldの変更・Stepとは呼出し側で直列化してください。

## 単位と設定（`FCharacterMoveTuning`、2D／3D共通）

距離は物理ワールドの距離単位（メートルを想定）、時間は秒、角度はラジアン。既定値は人型程度の例です。

| 項目 | 既定 | 意味 |
|---|---|---|
| `Radius` | 0.5 | 円／球の半径 |
| `SkinWidth` | 0.02 | 表面から法線方向に保つ接触余裕。移動は表面へこの距離まで近づいて止まる（ComputeSlideMoveの経路上の後退距離とは別） |
| `MinMoveDistance` | 1e-4 | これより短い残り移動は処理しない |
| `MaxIterations` | 8 | 1回のMoveAndSlideの反復上限 |
| `MaxQueries` | 128 | 1回の呼出しのWorld問い合わせ上限（超える前に`QueryLimit`で止まる） |
| `MaxRecoveryIterations` / `MaxRecoveryDistance` | 4 / 0.25 | 重なり解消の反復・合計距離の上限 |
| `MaxSlopeAngle` | π/4 | 歩ける床の最大傾斜。`dot(法線, Up) >= cos(MaxSlopeAngle)` |
| `StepHeight` | 0.3 | 接地中に上れる段差の高さ（0で無効） |
| `GroundSnapDistance` | 0.1 | 接地を保つための下向きの吸い付き距離 |
| `MaxSpeed` / `Acceleration` / `Deceleration` | 5 / 40 / 40 | 入力の大きさ1での目標速さと加減速 |
| `AirControl` | 0.3 | 空中の加減速の倍率 |
| `JumpSpeed` / `Gravity` / `MaxFallSpeed` | 6 / 20 / 30 | ジャンプ初速、キャラクターの重力（Worldの重力とは独立）、落下速度上限 |
| `Up`（`FCharacterMoveSettings2D/3D`） | (0,1) / (0,1,0) | 上方向（単位ベクトル） |

## 結果と停止理由

| `ECharacterMoveStop` | 状況 |
|---|---|
| `NoMovement` | 要求がMinMoveDistance未満 |
| `Completed` | 接触なしにすべて移動した |
| `Slid` | 接触面に沿って向きを変え、補正後の移動を最後まで行った |
| `Blocked` | これ以上進める成分がない（壁・角・稜線・囲まれた場所） |
| `MissingNormal` | 法線を得られず接触の手前で止めた |
| `AmbiguousContact` | 開始時の接触の方向を決められない（移動しない） |
| `PrecisionLimit` | f32への丸めで進めない |
| `IterationLimit` / `ContactLimit` / `QueryLimit` | 反復・接触の保持（8件）・問い合わせの上限 |

`ECharacterRecoveryStatus`は`NoOverlap`・`Resolved`・`Ambiguous`（同心・同距離の面）・`TooDeep`・`Blocked`（挟み込み）・`IterationLimit`・`ContactLimit`・`QueryLimit`です。解消できなかった場合は元の中心のまま移動しません。`ECharacterGroundState`は`Airborne`・`Walkable`・`Steep`（急坂は歩けず、上れない）です。

`FCharacterStepResult2D/3D`は`State`・`Recovery`・`Horizontal`／`Vertical`の移動結果・`bJumped`・`bLanded`・`bLeftGround`・`bHitCeiling`・`bSteppedUp`・`bSnapped`・`Queries`を返します。

## 限定仕様

- 段差上りは「接地中に歩けない面で止められた」ときだけ、上→前→下の順に試します。上方向で天井に当たる場合、実際に上がらない場合、進みが増えない場合は採用しません（低い天井の下の段差は上りません）。
- 急坂（MaxSlopeAngleを超える面）は水平移動では壁として扱い、上りません。
- ジャンプは歩ける床に接地しているときだけです。
- World問い合わせは現在、全Colliderを順に調べます（BroadPhaseなし）。1回の固定更新あたり約7回問い合わせるため、費用はCollider数に比例します（下の測定を参照）。
- 同じWorldを複数のスレッドから同時に変更・Stepしながら呼ばないでください。

## 性能測定

`dxf_character_benchmark`（`Tools/CharacterBenchmark`、CTest外、Releaseで手動実行）。結果と条件は[検証記録](../Development/Gameplay-2026-09-25.md#性能測定)を参照してください。

検証の範囲と未解決事項は[ゲームプレイ基盤の検証記録](../Development/Gameplay-2026-09-25.md)、機能ごとの状態は[進捗表](Progress.md#ゲームプレイ基盤の進捗表)を参照してください。
