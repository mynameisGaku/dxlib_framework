# R7 相互作用のCPU測定

既存の `dxf_character_benchmark` に `interaction` 系列を追加しています。既存の時計、確保追跡、5回の中央値・最小・最大の集計を使い、CTestの時間合否には含めません。

```text
dxf_character_benchmark interaction --pilot
dxf_character_benchmark interaction
```

Releaseで実行します。`--pilot` は各繰り返し12回、通常は120回です。両方とも初回1回を別に記録し、その後30回慣らして5回繰り返します。各実行の出力は既存の検証runの新しいファイルへ保存します。`--reference` はイベント生成側の参照経路が公開されていないため拒否します。

2D／3D × Collider総数64／512／1024 × 当事者1／16／64を各系列で測り、合計126行・62列のCSVを出します。

| 系列 | 条件 |
| --- | --- |
| sensor-off / sensor-on | 同じ疎な配置でSensor観測OFF／ON。2Colliderずつ離れた場所へ置く |
| solid-only | Solidの接触イベントのみ。指定した当事者はDynamic、残りはStatic、重力0 |
| sensor-dense | 全Colliderが重なるSensor配置。指定した当事者はKinematic、残りはStatic |
| static-floor | 動く床・イベントを使わないキャラクター計算の基準。床は全てStatic |
| moving-floor-off / moving-floor-on | 2m/sのKinematic床と値のキャラクター計算。イベント観測OFF／ON |

床系列では当事者数と同数のキャラクターを `StepCharacter` の値として計算し、追加のキャラクターColliderは登録しません。Collider総数を指定どおりに保つためです。生成したBodyの区分は全て明示し、静止地形をDynamicとして落下させません。各Stepの後、独立な解析位置と接地を確認してから次へ進みます。

配送は模擬ループではなく、既存の `DContactListener2DComponent`／`3DComponent` と `FPostPhysicsStepQueue` を使います。一つのListenerが全ての動くBodyを監視する条件です。動くBody同士の組は双方への通知を数えるため、配送数は組数と一致するとは限りません。

床系列のイベントONは床同士が接触せず、キャラクターColliderも登録しない静かな観測条件です。実際に通知が届く配送費用はSensor系列とSolid系列から評価します。密集Sensorでは毎Stepの配送数 `M × (N − 1)` も検証します。5回の繰り返しは同じWorldを継続する区間であり、5回の独立した再構築ではありません。

密集条件の真の組数は `M × (N − M) + M × (M − 1) / 2` です。NはCollider総数、Mは動く当事者数です。この全組を保持できる容量を設定し、容量超過や組の脱落を検出したら測定を失敗させます。容量値、容量到達回数、超過回数を別列に残します。

実測するのは構築、イベント有効化の確保、初回Step、慣らし後の `World.Step`、配送、床追従を含む `StepCharacter`、速度設定・配送予約、固定更新全体です。全体時間から内部区間を推定して表示しません。確保は件数であり、バイト数ではありません。

内部区間は、別のビルドディレクトリで `DXF_INTERACTION_BENCHMARK_PROBES=ON` にして測ります。このCMake optionは既定OFFで、公開ヘッダーやWorldのAPIを増やしません。通常版と計測版は同じRelease設定・コンパイラー・CRT・最適化設定で構成し、このoptionだけを変えます。既存の検証runをそれぞれ新規に作り、コマンド、設定、ソース指紋、実行前後のexe SHA-256を保存します。

計測版でも既存の単調時計と確保追跡を使い、呼出しスレッドのスタック上の固定長集計へ記録します。開始時には、既知の時計・確保差分を使って候補→詳細→候補の切替、二重終了の抑止、計測器自体の無確保を検査します。`probe_self_check=passed` はこの検査の成功です。各Stepでも区間の呼出し数を確認します。

| 内部列 | 実際に測る境界 |
| --- | --- |
| `event_candidate_us` / `event_candidate_alloc` | `CollectEvents_Internal` の境界作成・整列・走査・Body/Collisionの絞り込み・確定組の追加。入れ子の `EventTouch_Internal` 中だけ時計と確保の集計を詳細側へ切り替える |
| `event_exact_us` / `event_exact_alloc` | `EventTouch_Internal` の実形状の詳細判定全体 |
| `event_difference_us` / `event_difference_alloc` | `Publish` の真の組の正準順への整列、Begin／Stay／End生成、バッチ発行 |
| `carry_only_us` / `carry_only_alloc` | `StepCharacter` の床追従条件の確認から、支持運動の予測・速度上限・経路検査・離地速度継承まで。歩行・接地問い合わせ全体は含めない。静止床では追従条件の確認だけ |
| `event_candidates` | 同Body・Static同士・境界非重複を除いた後、Collision mask等で絞り込む前の候補数／Step |
| `event_exact_tests_per_step` | 実際に `EventTouch_Internal` を呼んだ回数／Step |
| `probe_clock_reads_per_step` | 内部計測が時計を読んだ回数／Step。時間の補正には使わない |

内部時間列も、5区間それぞれの1Step平均の中央値です。確保・候補数は測定した全Stepの平均です。末尾の `cold_event_*` と `cold_carry_only_*` は、慣らす前の最初の1Stepで同じ区間を測った値です。`cold_event_candidates` と `cold_event_exact_tests` は初回の実数です。

計測版は、特に密集Sensorで組ごとの時計読出しが増えます。その費用は差し引かず、計測版のWorld.Stepと内部時間に含めます。通常版のWorld.Step・配送・StepCharacterの時間を性能比較に使い、計測版を区間分解に使います。二つのrunで確定組数・配送数・床追従数・上限到達・確保数の一致を確認し、異なる実行の数値を一つのrunとして合成しません。

通常版の内部区間列は `NA` です。計測版で機能が無効な区間は呼び出されないため0を記録します。`solver_candidates_per_step` はSolver用候補数であり、イベント候補数ではありません。無効時の組数は未観測として両版とも `NA` を出します。`character_with_carry` はStepCharacter全体であり、追従だけの費用ではありません。

これらのCPU値は実描画やUIの確保数を含みません。既存UIの「3Dパネル4系列に2割当」の項目とは別に評価します。
