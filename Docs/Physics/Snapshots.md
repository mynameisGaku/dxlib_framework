# 2D / 3D World Snapshot

## 公開API

`FPhysicsWorld2D::CaptureSnapshot()` と `FPhysicsWorld3D::CaptureSnapshot()` は、現在生存している全Body・全Colliderを値として返します。`Dxf/RigidBody2D.h` / `Dxf/RigidBody3D.h` に宣言され、戻り値は `FPhysicsSnapshot2D` / `FPhysicsSnapshot3D` です。

```cpp
#include "Dxf/RigidBody3D.h"

Dxf::FPhysicsSnapshot3D Snapshot;
{
    Dxf::FPhysicsWorld3D World;
    World.CreateBody({});
    World.Step(1.0 / 60.0);

    Dxf::FPhysicsSnapshotLimits Limits;
    Limits.MaxBodies = 512;
    Limits.MaxColliders = 1024;
    Snapshot = World.CaptureSnapshot(Limits);
}
// Worldを破棄しても、Snapshotの配列は保持される。
```

BodyはId、Type、Position、Rotation、Velocity、AngularVelocity、bSleeping、bUseContinuousを持ちます。Rotationは2Dならラジアン、3Dなら単位四元数です。ColliderはId、LocalShape、Friction、Restitutionを持ち、Id.BodyでBodyの識別子に対応します。コライダーなしのBodyも含み、複合Bodyには取り付けたCollider全件を含みます。

LocalShapeは重心相対です。描画先の座標系やピクセル単位への変換をPhysics本体では行いません。順序は各登録スロット番号の昇順で、削除済みの穴は結果配列へ含みません。IDはWorld・Index・Generationを含む既存の世代付き型をそのまま保持します。

## 更新番号と形状の世代

StepIndexは正常完了した公開Step呼出しの回数です。内部SubStepsや描画フレームの番号ではありません。外側が固定刻みごとにStepを一度呼ぶ場合、その固定更新回数に対応します。LastDeltaSeconds / LastSubStepsは最後に正常完了したStepの引数で、初回前は0です。Snapshot採取自体やSetBodyTransformは番号を増やしません。

現在のWorldには、同じCollider IDのままShapeを書き換える公開APIはありません。そのため架空のShapeRevisionを作らず、取り外し・再登録はCollider IDの世代変更で識別します。将来のin-place Shape更新API・その改訂番号は別作業です。

Stepの入力検証エラーは観察状態を変更しません。有効な引数で開始したStepが途中で例外終了した場合、次の正常Step完了までCaptureSnapshotは例外で拒否します。これは物理状態を巻き戻す処理ではなく、途中状態を正常な観察値として渡さないための制限です。

## 所有・スレッド・失敗の契約

採取はWorldの所有スレッド、または利用者が単独アクセスを保証する区間で行います。同じWorldのStep・生成・削除・各変更APIと並行実行できません。実行中フラグは再入検出用で、Mutexやスレッド安全性を提供するものではありません。採取後のSnapshotはWorldを借用しません。Workerへ渡す場合も公開後は読み取りだけにするか、利用者がSnapshot自体の書込を同期してください。

省略時の保持上限はBody / Colliderそれぞれ65,536件です。上限は生存件数に対して適用します。超過時は切り詰めずToolbox::FExceptionで失敗し、一時結果を破棄します。確保・複製が例外終了しても、採取元Worldや代入先の以前のSnapshotは変更しません。採取の計算量は削除済みを含む登録スロット数に比例し、上限は走査時間の制限ではありません。

自動採取、接触の再計算、Solver Impulse / Islandの記録、GPU計測、ファイル保存、物理の巻戻しは追加していません。

## Debug表示からの利用

表示側は `Source/Debug` の `CapturePhysicsDebugSnapshot3D` / `CapturePhysicsDebugSnapshot2D` を通じて、このAPIの戻り値だけを入力にします。旧来の表示専用Watch登録（Collider作成時のShapeとBody種別を外側に二重登録する方式）は廃止しました。詳細は [DebugTools.md](../Rendering/DebugTools.md) を参照してください。Physics本体はRenderer・DxLib・Debugに依存しません。

## 検証の区分

共通の列挙・値複製・上限・Step観測処理の20件は、テスト用の登録配列を使う単体テストです。本物のWorld/Solverのテストではありません。

実FPhysicsWorld2D/3Dへリンクする `Tests/PhysicsSnapshotWorldTests.cpp` は、配布時の9件に、実Worldへの統合後に追加した回帰7件（停止済みJob Systemによる実Stepの途中失敗と採取拒否・解除の2D/3D、World識別子の区別、2D箱と3D OBBのローカル形状とBody姿勢の分離、休止フラグ、採取の有無で数値が一致すること）を加えた16件です。

2026-09-23、完全な作業ツリー（基準 `15f5df4` + 本統合）で `Tools/PhysicsSnapshotValidation` を `DXF_SNAPSHOT_CORE_ONLY=OFF`（実 `dxf::physics` へリンク）で生成し、Windows / MSVC 19.51（Visual Studio 18 2026）のDebug / Releaseで共通20件＋実World16件の計36件が通過しました。追加回帰7件は統合後に作成したもので、変更前WorldでのRedは確認していません。GCC / Clang・Sanitizerでの実World構成は今回実行していません。ログは `Validation/PhysicsSnapshotWorld/` にあります。
