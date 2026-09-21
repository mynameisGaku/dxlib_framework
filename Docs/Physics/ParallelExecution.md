# Physics並列実行

Physics Worldの公開APIは引き続き**呼び出し側で直列化する契約**です。`CreateBody`、`DestroyBody`、設定変更、`Step`を同じWorldへ複数スレッドから同時に呼ぶAPIにはしていません。

`Step`内部だけ、`FPhysicsExecutionSettings::JobSystem`へ借用した`Toolbox::FJobSystem`を使います。`nullptr`または実行レーン数1では従来どおり同期実行します。

並列化する単位は次です。

1. Dynamic Bodyごとの力・速度積分。
2. Sweep-and-Prune BroadPhaseの固定チャンク。
3. Candidate PairごとのNarrowPhase。各Jobは専用のManifold slotだけを書きます。
4. Dynamic接触グラフから作った独立IslandごとのConstraint Solver。
   WarmStartは全多様体へ直列に済ませ、StoreCacheも直列に戻す。
5. Bodyごとの位置・姿勢積分。

Candidate PairはJob完了順を使わず、最後にCollider indexの辞書順へ戻します。Islandの根は最小Body indexに固定します。このためWorker数やJob完了順でSolver入力順が変わらないようにしています。

Static/KinematicはDynamic Island同士を接続しません。複数Islandが同じStatic/Kinematicへ接触できるため、並列Solver経路では逆質量・逆慣性が0のBodyへ「0を加算する」書き込みも行いません。既に起きている対象への再起床も書き込みません。これにより共有Static/Kinematicは読み取り専用になり、Island完了順によらず結果が一致します。

`ContactSlop`内の近接をNarrowPhaseが接触として扱えるよう、BroadPhaseは各AABBをSlop分だけ膨張させて候補化します。

CCDのTOI反復は現在もWorld単位で順序依存があるため、この段階では並列化しません。TOI中に呼ばれる離散接触解決は同じBroadPhase/NarrowPhase/Island経路を利用できます。

## Thread契約

- `FPhysicsWorld2D/3D`公開変更API: MainThreadOnlyまたは呼び出し側で単独所有。
- `Step`: 同一Worldに対する同時呼び出し禁止。戻る前に投入Jobは全て完了する。
- `FPhysicsExecutionSettings::JobSystem`: 非所有。Worldより長く生存させる。
- Worker: `Slots`/`Colliders`の構造を変更しない。生成・破棄・Attach/Detachは禁止。
- NarrowPhase Worker: 読み取り専用World state + 専用Manifold slotのみ変更。
- Island Solver Worker: そのIslandに所属するDynamic Bodyだけ変更。

`WorkerCount=1`相当のJob Systemと複数Workerで同じ入力列を比較する回帰テストを維持します。借用なし（`nullptr`）では従来の直列経路をそのまま通り、新旧経路の等価性も検証します。`SolverIslandCount`が`IslandCount`に一致することで、Island SolverがJob経路を通ったことを確認します。
