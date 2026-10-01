# Physics並列実行

Physics Worldの公開APIは引き続き**呼び出し側で直列化する契約**です。`CreateBody`、`DestroyBody`、設定変更、`Step`を同じWorldへ複数スレッドから同時に呼ぶAPIにはしていません。

`Step`内部だけ、`FPhysicsExecutionSettings::JobSystem`へ借用した`Toolbox::FJobSystem`を使います。`nullptr`または実行レーン数1では従来どおり同期実行します。

並列化する単位は次です。

1. Dynamic Bodyごとの力・速度積分。
2. Sweep-and-Prune BroadPhaseの固定チャンク。
3. Candidate PairごとのNarrowPhase。各Jobは専用のManifold slotだけを書きます。
4. Dynamicの接触とDistance/Revolute/Fixed/Prismaticから作った独立IslandごとのConstraint Solver。
   直列・Job経路は同じ反復処理を使い、各反復でContact→Distance→Revolute→Fixed→Prismaticの順に解く。
   起床、ContactのWarmStart、起床の再伝播、JointのWarmStartは所有スレッドで済ませる。
   位置補正、休止更新、Contact記録の保存も所有スレッドへ残す。
5. Bodyごとの位置・姿勢積分。

Candidate PairはJob完了順を使わず、最後にCollider indexの辞書順へ戻します。Islandの根は最小Body indexに固定します。このためWorker数やJob完了順でSolver入力順が変わらないようにしています。

Static/KinematicはDynamic Island同士を接続しません。複数Islandが同じStatic/Kinematicへ接触できるため、並列Solver経路では逆質量・逆慣性が0のBodyへ「0を加算する」書き込みも行いません。既に起きている対象への再起床も書き込みません。これにより共有Static/Kinematicは読み取り専用になり、Island完了順によらず結果が一致します。

`ContactSlop`内の近接をNarrowPhaseが接触として扱えるよう、BroadPhaseは各AABBをSlop分だけ膨張させて候補化します。

CCDのTOI反復は現在もWorld単位で順序依存があるため、この段階では並列化しません。
TOI中の`SolveContactNow_Internal`はContactだけのIslandを作り、JointのWarmStartや速度求解を繰り返しません。
Jointは各SubStepの離散速度求解で一度（設定された反復数）解き、CCD後の最終姿勢で位置補正します。
TOIの途中でJointの長さを連続的に保証する契約、AnchorのSweep、Joint専用TOIはありません。

速度が変わらないBodyは、同じ開始姿勢からSubStep内の到達時刻まで既存の積分処理を適用します。
これにより無関係な島のTOI回数が位置・姿勢の丸め回数を増やしません。
接触で速度が変わったBodyはその時点の姿勢を新しい基準にします。保守停止で残した時間を勝手に進める処理ではありません。

## Jointの確定境界

`FDistanceJointSolveState2D/3D`はPrivateの作業値です。Worldは容量を再利用しますが、内容は入力検証後の各`Step`開始時に全スロットを登録値から作り直します。
生存・世代・Kind、新種類の8行累積ImpulseとLimit側、Distanceの蓄積Impulse、保存軸とその有効性を保持し、一回の`Step`内の全SubStepで共有します。
Workerは所属IslandのDynamic Bodyと、そのIslandだけが所有するJoint作業スロットを変更します。
共有Static/KinematicとJointの登録情報へは書き込みません。

全SubStep、CCD、位置補正、休止更新、索引更新、イベント発行が成功した後に、所有スレッドがslot昇順で再利用値を確定します。
生存・世代・Kindが一致しない登録へは戻しません。例外・Job拒否・確保失敗ではJoint記録を確定せず、次の`Step`は最後に成功した登録値から再開します。
**これはBodyの位置・速度、Contact記録、力を巻き戻す保証ではありません。** 途中失敗後の問い合わせ・Snapshotは既存契約どおり拒否され、次の正常な`Step`で回復します。
Jointのgetterは登録設定と現在のBody姿勢から計算し、作業値を公開しません。同時呼出しの安全性も追加しません。

## Thread契約

- `FPhysicsWorld2D/3D`公開変更API: MainThreadOnlyまたは呼び出し側で単独所有。
- `Step`: 同一Worldに対する同時呼び出し禁止。戻る前に投入Jobは全て完了する。
- `FPhysicsExecutionSettings::JobSystem`: 非所有。Worldより長く生存させる。
- Worker: `Slots`/`Colliders`の構造を変更しない。生成・破棄・Attach/Detachは禁止。
- NarrowPhase Worker: 読み取り専用World state + 専用Manifold slotのみ変更。
- Island Solver Worker: そのIslandに所属するDynamic Bodyだけ変更。

借用なし（`nullptr`）と1・2・4・8レーンで、独立Joint島・共有Static/Kinematic・Dynamic対・接触との混在・一本の鎖・休止と部分起床を比較します。
位置、速度、角速度、姿勢の全成分、休止、Joint距離・誤差・生存をbit一致で確認します。
`SolverIslandCount`はJob経路で求解した島の累計なので、CCDなしの一回のSubStepでは`IslandCount`と一致し、複数SubStepでは各分割の数を加算します。Jointだけの島も含みます。
一本の鎖は一つの島であり、この実装で島内部の並列高速化は行いません。

限定回帰は`dxf_physics_tests --joint-j4`、全回帰は引数なしの同じ実行ファイルです。
確保故障注入は既存の`PhysicsOverlapFault`に含めます。詳細な結果と未実施事項は[今回の検証記録](../Development/JointParallelTransactionalCCD-2026-10-01.md)を参照してください。

## ゲーム用Joint Componentと測定

参照解決・接続切替は所有スレッドのPrePhysicsで、観察は成功PostPhysicsで行います。WorkerはComponentへ触りません。型付き参照と寿命は[距離Joint](Joints.md)。`dxf_character_benchmark joint`は独立島・共有支点・Dynamic対・Contact混在・一本の鎖とComponent境界を分離します。一本の鎖の処理を複数Workerへ分割したという測定ではありません。CPU時間・確保の結果と14変異の検出は[検証記録](../Development/JointGameplayCompletion-2026-10-01.md)を参照してください。

新種類のMotor/Limitも同じStep作業値に含みます。種類別求解を最後に全反復する構成ではなく、同じIslandの各速度反復で正準順に処理します。位置行はAnchor/横→姿勢、Motor行6→Limit行7です。共有支点には書き込みません。詳細は[Joints](Joints.md)。
