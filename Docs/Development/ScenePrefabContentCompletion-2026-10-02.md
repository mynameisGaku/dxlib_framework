# Scene／Prefab Contentの実装・検証（2026-10-02）

開始HEAD/origin: `17ec54fadcb5861eddc67aa45c6d9771841e2ed3`（main、開始時clean）。検証コードcommit: `124a97873f87f6fb4e2f18b8ca27e51eae0583af`。最終の文書commit／origin SHAはこの記録を収録したGit履歴とチャット報告を参照。

保存先は `Build/ScenePrefabContent-20261001-b812ec`。各Summaryのcommand、cwd、UTC、exit、SourceManifest、JUnit／登録、exe SHA-256を保存。過去runの上書き・Archive適用・SDK取得／再構築・拒否済み画像フォルダーの操作なし。C:/dxfc下の新しい短い作業先で配布を生成し、日本語・空白を含む再配置先と別CWDで実行した。SDKは既存 `ThirdParty/DxLib-3.25a-source`（DxLib 3.25a source、model extension 3、FBX SDK無効）。MSVC 14.51.36231／VS 18 2026 x64。同じPCでの確認であり新環境ではない。

## 到達した構成

B0: Collection／World／AssetService／Task Scopeの現行所有と登録を照合。B1/B2: 任意静的target `dxf::scene_content`、型付き2D/3D定義、厳格なJSON、論理ID、parameter、exports、入れ子DAGと循環・総上限。B3: 既存Componentで生成し受付／初期化／Physics Ready／失効を分離。B4/B5: 四種類資源、所有側準備、CPU Task、取消し／置換／旧成功値保持。B6/B7: 補間表示、個体モデル、SensorのC++操作と音、既存F1パネル／F2コース。B8: 実World／実Application／実DxLib／故障／変異。B9: 五独立環境の測定と配布四構成。B10: 以下の採用集合・現行文書・通常main pushで区切る。未実施範囲は末尾に分ける。

APIは `FSceneContentSource` → `PrepareScene` → `DContentScene2D/3D` と既存Navigator、または `PreparePrefab` → `Scene.Spawn<DPrefabInstance2D/3D>`。Ready後に `GetRigidBody`／`GetPrismaticJoint`等の型付きexportへ既存要求を渡す。Prefab／Body／資源の別所有Managerはない。原本Starterは空、SandboxとC++仕掛けコースを維持。SampleはF2で定義コース、F1で読込・取消し・再試行・生成・破棄・次回parameterと現在Motor操作を分ける。有限な同梱Scene/Prefab選択であり任意ファイルの編集GUIではない。

最小の公開Scene経路は `Tools/PackageConsumer/ContentNativeConsumer.cpp` の `RunContentNativeConsumer`、CPU寿命の検証コードは同フォルダー `ContentConsumer.cpp`。前者は本体ApplicationのNavigator、後者は公開ヘッダーのLifecycle境界を試験として明示駆動する。利用者は実装Privateヘッダーを必要としない。[schema](../Content/SceneDefinitions.md)・[Prefab](../Content/Prefabs.md)・[読込](../Content/Loading.md)。

Scene準備失敗ではNavigatorへ変更要求を出さず旧Sceneを保持。Pending Scene初期化失敗は既存Navigator契約。切替後の固定更新／World／描画失敗はApplicationへ伝播し、全World rollbackは保証しない。Native Model等の所有側同期取込はブロックし得る。現在値のセーブ／世代ID復元、scale配置、スクリプト、自動監視、任意型factory、編集GUIは対象外。JSON位置が不明な資源・C++値診断は0:0と明示し、修正前Redと修正後Greenを保存。

## 採用する最終集合

全群結果を単独再試験と合算しない。最終の全群はfinal-mover-*、単独入口はStandalone-Final-Mover。先行accepted-*全群も別試行として残す。final-mover-*とStandalone-Final-Mover、関連accepted検査、Package-*-Accepted-Logs、Data-ABC-Accepted、Logs/accepted-benchmarkを最終集合として照合する。別名の旧成功は追加前のコード／診断・pilotであり最終集合へ足さない。

|工程|結果|工程exit|秒|source SHA-256|
|---|---|---|---:|---|
|Data-ABC-Accepted|passed; -|0,0|3.96|`5f9403e623fe2b49bb15bd305b1770bb8858bc889d7a067d2fce5acb5b1b8009`|
|accepted-benchmark|passed; -|0|104.90|`617d24e35259bedba59c3ffd22df6e9875918e4f38007e88a4d96f3a45e776c3`|
|accepted-crlf-fault-debug|passed; 1/1 fail=0 skip=0|0|0.87|`617d24e35259bedba59c3ffd22df6e9875918e4f38007e88a4d96f3a45e776c3`|
|accepted-crlf-fault-release|passed; 1/1 fail=0 skip=0|0|0.19|`617d24e35259bedba59c3ffd22df6e9875918e4f38007e88a4d96f3a45e776c3`|
|accepted-crlf-python-ide|passed; -|0|4.29|`617d24e35259bedba59c3ffd22df6e9875918e4f38007e88a4d96f3a45e776c3`|
|accepted-headers2|passed; -|0|8.29|`5f9403e623fe2b49bb15bd305b1770bb8858bc889d7a067d2fce5acb5b1b8009`|
|accepted-ide-development|passed; -|0|5.59|`5f9403e623fe2b49bb15bd305b1770bb8858bc889d7a067d2fce5acb5b1b8009`|
|accepted-ide-normal|passed; -|0|4.19|`5f9403e623fe2b49bb15bd305b1770bb8858bc889d7a067d2fce5acb5b1b8009`|
|accepted-native-prepare-1|passed; -|0|2.04|`5f9403e623fe2b49bb15bd305b1770bb8858bc889d7a067d2fce5acb5b1b8009`|
|accepted-native-prepare-2|passed; -|0|1.91|`5f9403e623fe2b49bb15bd305b1770bb8858bc889d7a067d2fce5acb5b1b8009`|
|accepted-native-prepare-3|passed; -|0|1.95|`5f9403e623fe2b49bb15bd305b1770bb8858bc889d7a067d2fce5acb5b1b8009`|
|accepted-native-prepare-4|passed; -|0|1.93|`5f9403e623fe2b49bb15bd305b1770bb8858bc889d7a067d2fce5acb5b1b8009`|
|accepted-native-prepare-5|passed; -|0|2.02|`5f9403e623fe2b49bb15bd305b1770bb8858bc889d7a067d2fce5acb5b1b8009`|
|accepted-python-ide|passed; -|0|4.35|`5f9403e623fe2b49bb15bd305b1770bb8858bc889d7a067d2fce5acb5b1b8009`|
|accepted-python-normal|passed; -|0|3.63|`5f9403e623fe2b49bb15bd305b1770bb8858bc889d7a067d2fce5acb5b1b8009`|
|final-mover-native-debug-build|passed; -|0|2.44|`83156fdeb701927a8f6dc2b1e50bb6feb856bd9e77aca2771879b4accda19e89`|
|final-mover-native-debug-tests|passed; 36/36 fail=0 skip=0|0|211.06|`83156fdeb701927a8f6dc2b1e50bb6feb856bd9e77aca2771879b4accda19e89`|
|final-mover-native-release-build|passed; -|0|2.91|`83156fdeb701927a8f6dc2b1e50bb6feb856bd9e77aca2771879b4accda19e89`|
|final-mover-native-release-tests|passed; 36/36 fail=0 skip=0|0|67.07|`83156fdeb701927a8f6dc2b1e50bb6feb856bd9e77aca2771879b4accda19e89`|
|final-mover-no-stl|passed; -|0|0.45|`83156fdeb701927a8f6dc2b1e50bb6feb856bd9e77aca2771879b4accda19e89`|
|final-mover-portable-debug-build|passed; -|0|1.37|`83156fdeb701927a8f6dc2b1e50bb6feb856bd9e77aca2771879b4accda19e89`|
|final-mover-portable-debug-tests|passed; 30/30 fail=0 skip=0|0|158.05|`83156fdeb701927a8f6dc2b1e50bb6feb856bd9e77aca2771879b4accda19e89`|
|final-mover-portable-release-build|passed; -|0|2.46|`83156fdeb701927a8f6dc2b1e50bb6feb856bd9e77aca2771879b4accda19e89`|
|final-mover-portable-release-tests|passed; 30/30 fail=0 skip=0|0|27.88|`83156fdeb701927a8f6dc2b1e50bb6feb856bd9e77aca2771879b4accda19e89`|
|Package-Off-Debug-Accepted-Logs|passed; -|0,0,0,0,0,0,0,0,0,0|39.63|`5f9403e623fe2b49bb15bd305b1770bb8858bc889d7a067d2fce5acb5b1b8009`|
|Package-Off-Release-Accepted-Logs|passed; -|0,0,0,0,0,0,0,0,0,0|42.70|`5f9403e623fe2b49bb15bd305b1770bb8858bc889d7a067d2fce5acb5b1b8009`|
|Package-On-Debug-Accepted-Logs|passed; -|0,0,0,0,0,0,0,0,0,0,0,0,0|33.69|`5f9403e623fe2b49bb15bd305b1770bb8858bc889d7a067d2fce5acb5b1b8009`|
|Package-On-Release-Accepted-Logs|passed; -|0,0,0,0,0,0,0,0,0,0,0,0,0|38.03|`5f9403e623fe2b49bb15bd305b1770bb8858bc889d7a067d2fce5acb5b1b8009`|
|Standalone-Final-Mover|passed; 30/30 fail=0 skip=0; 30/30 fail=0 skip=0|0,0,0,0,0,0,0,0|267.03|`83156fdeb701927a8f6dc2b1e50bb6feb856bd9e77aca2771879b4accda19e89`|

登録／実行／成功／失敗／skipはJUnitとRegistrationへ個別保存。Contentは最終43ケース、Content確保故障5、Framework413、Physics588、InteractionSample48。CTest群数と内部case数・配布工程数・変異数は合算しない。公開ヘッダー45単位（単独・正順・逆順・両次元、/utf-8）、IDE通常／Developmentのfilter・二重cppなし・従来F5設定を検査。GUIの目視や人のF5操作とは別。

## 同一exeのデータA/B/C

```json
{
  "binary_sha256": "ce6a774f4ea423ad067e868fa885b528f5de5787afa94d7f3bc46c71e3dacc1f",
  "source_sha256": "5f9403e623fe2b49bb15bd305b1770bb8858bc889d7a067d2fce5acb5b1b8009",
  "world_trials": {
    "A": [
      [
        "2",
        "3",
        "-3.000000000",
        "1.000000000",
        "0.800000000"
      ],
      [
        "3",
        "3",
        "-3.000000000",
        "1.000000000",
        "0.800000000"
      ]
    ],
    "B": [
      [
        "2",
        "4",
        "-4.000000000",
        "1.000000000",
        "-0.400000000"
      ],
      [
        "3",
        "4",
        "-4.000000000",
        "1.000000000",
        "-0.400000000"
      ]
    ]
  }
}
```
Aは3個体・x=-3・doorB速度0.8、Bは4個体・x=-4・色変更・doorB速度-0.4／上限12・2D画像差替え。3Dも配置／色／Motor／個体数の差を実Worldと実画素で検査。3Dモデル自体の差替えはこのA/Bでは行っていない。同じexeの前後hash、定義コピーのmanifest、A/B各プロセスのWorld値、通常／左右／1001×501のBody色と画像／モデルの領域を保存。Cは未知schemaの壊れた定義で、同じ起動セッションの旧Scene維持と正常定義への再試行を検査した。原本Assetsを上書きしていない。

## 故障と変異

確保countdownは0から順に一地点ずつ試した。初回非注入成功はparse2D=608、parse3D=644、root受付=6／初期化=18／初回固定更新=45（両次元）、再読込2D=269／3D=271、所有側部分資源=53。先行する各countdownは実注入・失敗・outstanding delta 0、次の正常環境で回復。成功後の終了はcountdown0で非注入・古いhandle失効。失敗StepでBody全体のrollbackは要求していない。

B-M01〜18は独立にbuild 0 / Red 1 / restore build 0 / Green 0。保存時と復元時の全対象生バイトhashが一致し、更新時刻と再コンパイル・exe hashを保存。初回B-M15はRed0で未検出だったため、種類違いの明確な診断を検査して補強。最終18件再実行後、診断位置修正に関係するB-M12/17をもう一度確認した。B-M07はEndpointLost観察の除去、B-M08は未知field無視を代表変異とした。Physics数式・求解／TOI cppを変更していないため、既存Distance14／K24変異は再実行せず通常全群で保持を確認。

## 測定

content系列は1/32/256個体、Jointなし／四種類、共有定義／別typed定義、試験用Texture共有／別条件。各条件5新規環境。ファイル読取・parse・展開は一つの区間、純粋検証は別区間。複数typed定義は読み取った一つの定義の値コピーへ個体名を与える条件で、256別ファイルのI/O測定ではない。所有側資源準備は試験用Backendの費用。実Native取込は別の実デバイス診断に記録する。

定常は30更新の慣らし後、通常contentは120更新を計測。Content状態読取とWorld単独（追加Stepの比較系列）、Componentを含む固定更新全体を分ける。World.Stepの確保を0と主張しない。初回固定更新はReadyまでの登録、終了は別区間。時間はμs、中央値／最小／最大、確保、Body/Joint数、owner load数をCSV形式で生ログへ保存。並行実行したpilot／旧Release測定値は最終性能値として採用しない。

## 途中失敗の扱い

製品: PendingInitialization個体へのDraw受付を抑止して生成受付と初期化を分離。資源／C++値診断の架空1:1を0:0へ修正。試験/harness: 一レーンのinline Job投入をブロックする待機、Shapes未対応のFake、App.Start前のRoot、奇数寸法の未指定window size、初期キーNと既存接続解除の衝突を修正。Windows入口のargvをUTF-16→UTF-8へ変換して日本語Rootを検査。

環境/実行器: SDK option引用、実target名、MSBuildの長いtlogパス、ログ先の既存再利用拒否、wrapperの未build登録hashを整理。既存ログは上書きしていない。実描画: 基本色を比較するfixtureへ全ambientを明示し、B配置で独立に観察できる領域へ変更。既存色許容±35、121画素中>50、画像／モデルの最低画素数は弱めていない。故障試験の大きいreload fixtureは探索上限に達したため小さい要求fixtureへ分け、既存四種類Joint故障を保持。成功準備後の試験用Backend解放へ未使用countdownを持ち込まないよう注入境界を限定した（timeout二試行を保存、再実行5/5）。停止時の実スタックは取れておらず、timeoutの初発箇所まで断定した結果ではない。

## 試行別結果（生ログはBuild内）

以下は途中成功・失敗を含む全保存run。失敗の後の成功で過去行を消さない。工程欄が空のfailedは外部コマンド開始前の検証器エラー。秒は保存した子工程の合計。

|run|状態|CTest成功/実行・失敗・skip|exit|秒|manifest先頭|
|---|---|---|---|---:|---|
|Data-ABC-1|failed|-|1|0.68|`4f42bc60b68f`|
|Data-ABC-2|failed|-|0,1|3.59|`2524857adeba`|
|Data-ABC-3|passed|-|0,0|3.98|`47570549b50b`|
|Data-ABC-Accepted|passed|-|0,0|3.96|`5f9403e623fe`|
|accepted-benchmark|passed|-|0|104.90|`617d24e35259`|
|accepted-crlf-fault-debug|passed|1/1 fail=0 skip=0|0|0.87|`617d24e35259`|
|accepted-crlf-fault-release|passed|1/1 fail=0 skip=0|0|0.19|`617d24e35259`|
|accepted-crlf-python-ide|passed|-|0|4.29|`617d24e35259`|
|accepted-headers|failed|-|1|0.35|`5f9403e623fe`|
|accepted-headers2|passed|-|0|8.29|`5f9403e623fe`|
|accepted-ide-development|passed|-|0|5.59|`5f9403e623fe`|
|accepted-ide-normal|passed|-|0|4.19|`5f9403e623fe`|
|accepted-native-debug-build|passed|-|0|19.61|`5f9403e623fe`|
|accepted-native-debug-tests|passed|36/36 fail=0 skip=0|0|200.03|`5f9403e623fe`|
|accepted-native-prepare-1|passed|-|0|2.04|`5f9403e623fe`|
|accepted-native-prepare-2|passed|-|0|1.91|`5f9403e623fe`|
|accepted-native-prepare-3|passed|-|0|1.95|`5f9403e623fe`|
|accepted-native-prepare-4|passed|-|0|1.93|`5f9403e623fe`|
|accepted-native-prepare-5|passed|-|0|2.02|`5f9403e623fe`|
|accepted-native-release-build|passed|-|0|49.84|`5f9403e623fe`|
|accepted-native-release-tests|passed|36/36 fail=0 skip=0|0|67.78|`5f9403e623fe`|
|accepted-no-stl|passed|-|0|1.13|`5f9403e623fe`|
|accepted-portable-debug-build|passed|-|0|8.46|`5f9403e623fe`|
|accepted-portable-debug-tests|passed|30/30 fail=0 skip=0|0|167.90|`5f9403e623fe`|
|accepted-portable-release-build|passed|-|0|44.36|`5f9403e623fe`|
|accepted-portable-release-tests|passed|30/30 fail=0 skip=0|0|28.33|`5f9403e623fe`|
|accepted-python-ide|passed|-|0|4.35|`5f9403e623fe`|
|accepted-python-normal|passed|-|0|3.63|`5f9403e623fe`|
|adopted-native-debug-build|passed|-|0|2.20|`82beafc1ad4b`|
|adopted-native-debug-tests|passed|36/36 fail=0 skip=0|0|200.69|`82beafc1ad4b`|
|adopted-native-release-build|passed|-|0|58.74|`82beafc1ad4b`|
|adopted-no-stl|passed|-|0|0.66|`82beafc1ad4b`|
|adopted-portable-debug-build|passed|-|0|9.63|`82beafc1ad4b`|
|adopted-portable-debug-tests|passed|30/30 fail=0 skip=0|0|172.51|`82beafc1ad4b`|
|adopted-portable-release-build|passed|-|0|5.59|`82beafc1ad4b`|
|adopted-portable-release-tests|passed|30/30 fail=0 skip=0|0|28.60|`82beafc1ad4b`|
|adopted-python-normal|passed|-|0|4.09|`82beafc1ad4b`|
|b0-json-debug-build|failed|-|-|0.00|`d6a2cdc71859`|
|b0-json-debug-build2|failed|-|-|0.00|`d6a2cdc71859`|
|b0-json-debug-build3|passed|-|0|20.43|`d6a2cdc71859`|
|b0-json-debug-tests|passed|-|0|12.91|`d6a2cdc71859`|
|b1-schema-configure|passed|-|0|1.09|`f3a245ce81d0`|
|b1-schema-debug-build|failed|-|1|2.44|`f3a245ce81d0`|
|b1-schema-debug-build2|failed|-|1|0.83|`f3a245ce81d0`|
|b1-schema-debug-build3|passed|-|0|1.00|`bdba6882b6ef`|
|b1-schema-tests|passed|-|0|0.07|`40d24c089d80`|
|b1-schema-tests-build|passed|-|0|2.08|`40d24c089d80`|
|b2-expansion-build|passed|-|0|4.67|`93a0d956eecd`|
|b2-expansion-source-build|passed|-|0|1.32|`c89b519f320d`|
|b2-nested-tests|passed|1/1 fail=0 skip=0|0|0.19|`5700b12a765c`|
|b2-nested-tests-build|failed|-|1|1.43|`704449694b16`|
|b2-nested-tests-build2|passed|-|0|1.35|`5700b12a765c`|
|b2-shape-build|passed|-|0|2.60|`51f3e667cfee`|
|b2-source-build|passed|-|0|2.00|`c8ce6356128b`|
|b2-strict-build|passed|-|0|9.58|`07564ce545d4`|
|b3-all-joints-build|passed|-|0|1.63|`8f6f2f3396cf`|
|b3-all-joints-test|passed|-|0|0.13|`8f6f2f3396cf`|
|b3-file-tests|passed|-|0|0.13|`8e2d9d10cac3`|
|b3-file-tests-build|passed|-|0|2.35|`8e2d9d10cac3`|
|b3-resources-build|passed|-|0|2.36|`1d5df9563ddd`|
|b3-resources-test|passed|-|0|0.10|`1d5df9563ddd`|
|b3-runtime-build|failed|-|1|4.79|`40c28d8c4b05`|
|b3-runtime-build2|failed|-|1|1.56|`59079f699d45`|
|b3-runtime-build3|failed|-|1|1.36|`f2993ce63157`|
|b3-runtime-build4|failed|-|1|1.15|`3468a13ef4ac`|
|b3-runtime-build5|passed|-|0|1.91|`d2d6db8fa482`|
|b3-scene-build|passed|-|0|2.38|`0516075719b5`|
|b3-world-build|failed|-|1|0.99|`2ecc71217542`|
|b3-world-build2|passed|-|0|1.39|`d078dd874dbc`|
|b3-world-test|passed|1/1 fail=0 skip=0|0|0.10|`d078dd874dbc`|
|b5-application-build|failed|-|1|8.47|`aea4c442ea34`|
|b5-application-build2|passed|-|0|4.71|`cb4fe3032f6e`|
|b5-application-tests|passed|1/1 fail=0 skip=0|0|0.67|`cb4fe3032f6e`|
|b5-request-build|passed|-|0|2.16|`77cb3fc56f75`|
|b5-request-tests|failed|0/1 fail=1 skip=0|8|60.04|`b1768809d5dd`|
|b5-request-tests-build|passed|-|0|2.13|`b1768809d5dd`|
|b5-request-tests-build2|passed|-|0|1.13|`c7d55318b071`|
|b5-request-tests-build3|passed|-|0|1.18|`d3caae881071`|
|b5-request-tests2|failed|0/1 fail=1 skip=0|8|0.23|`c7d55318b071`|
|b5-request-tests3|passed|1/1 fail=0 skip=0|0|0.23|`d3caae881071`|
|b6-drawing-build|failed|-|1|17.47|`41132107b464`|
|b6-drawing-build2|passed|-|0|9.19|`2c5e203a4543`|
|b6-dynamic-placement-build|passed|-|0|8.86|`cbf26953e26a`|
|b6-render-observer-build|passed|-|0|23.18|`2ef96b12e310`|
|b6-render-observer-tests|passed|1/1 fail=0 skip=0|0|0.72|`2ef96b12e310`|
|b6-scene-views-build|passed|-|0|13.64|`ddb22539c509`|
|b6-scene-views-tests|failed|0/1 fail=1 skip=0|8|0.81|`ddb22539c509`|
|b6-sensor-dynamic-build|failed|-|1|6.06|`4c67080f5f8c`|
|b6-sensor-dynamic-build2|passed|-|0|5.58|`6d9956ebb0c1`|
|b6-sensor-dynamic-tests|passed|1/1 fail=0 skip=0|0|0.74|`6d9956ebb0c1`|
|b6-view-matrix-build|passed|-|0|10.46|`4737c905c8dd`|
|b6-view-matrix-tests|running|1/1 fail=0 skip=0|0|0.71|`cbf26953e26a`|
|b7-connections-regression-build|passed|-|0|15.59|`a89415194535`|
|b7-connections-regression-tests|passed|2/2 fail=0 skip=0|0|40.25|`a89415194535`|
|b7-data-course-build|failed|-|1|23.20|`56fa09821404`|
|b7-data-course-build2|passed|-|0|7.77|`cf40081aca9d`|
|b7-definition-validation-build|failed|-|1|7.77|`c5dc343e4fdf`|
|b7-existing-course-tests|failed|1/2 fail=1 skip=0|8|33.74|`cf40081aca9d`|
|b7-odd-window-build|passed|-|0|7.20|`d040fe5fb129`|
|b7-odd-window-tests|passed|2/2 fail=0 skip=0|0|39.39|`93b589eed3be`|
|b7-owner-preparation-tests|failed|1/2 fail=1 skip=0|8|34.06|`d88f0d71a4fe`|
|b7-pending-draw-build|failed|-|1|5.01|`e695fcd7521e`|
|b7-pending-draw-build2|passed|-|0|8.25|`8e203b27108d`|
|b7-pending-draw-tests|failed|1/2 fail=1 skip=0|8|32.45|`8e203b27108d`|
|b7-sample-path-build|passed|-|0|11.38|`1283a23db6a7`|
|b7-sample-path-tests|failed|1/2 fail=1 skip=0|8|32.87|`1283a23db6a7`|
|b7-scene-connections-build|failed|-|1|10.34|`f88c97e4a0e6`|
|b7-scene-connections-build2|passed|-|0|28.18|`73c69a5c7ff0`|
|b7-shared-assets-build|passed|-|0|27.38|`d88f0d71a4fe`|
|b7-shortcut-fix-build|passed|-|0|8.09|`07068c140d0d`|
|b8-content-common-build|passed|-|0|33.26|`a2ea7e270f2a`|
|b8-content-fault-tests|passed|2/2 fail=0 skip=0|0|0.86|`7297476f87ef`|
|b8-fault-build|passed|-|0|24.78|`7297476f87ef`|
|b8-native-asset-build|passed|-|0|1.67|`390e7410cc9f`|
|b8-native-config|failed|-|1|34.27|`767c06d87e51`|
|b8-native-config2|passed|-|0|4.68|`a2ea7e270f2a`|
|b8-native-content-build|failed|-|1|72.22|`a2ea7e270f2a`|
|b8-native-content-build2|passed|-|0|32.00|`79c081a9ee75`|
|b8-native-content-build3|passed|-|0|5.53|`955470751a41`|
|b8-native-content-trial1|failed|-|1|5.09|`7297476f87ef`|
|b8-native-content-trial2|passed|-|0|2.06|`955470751a41`|
|b8-native-content-trial3|passed|-|0|1.94|`390e7410cc9f`|
|b8-native-data-build|passed|-|0|2.31|`06357ba0e932`|
|b8-reserved-destroy-build|passed|-|0|1.75|`44bccfc39c01`|
|b8-reserved-destroy-tests|failed|-|-|0.00|`44bccfc39c01`|
|b8-reserved-destroy-tests2|passed|1/1 fail=0 skip=0|0|0.10|`44bccfc39c01`|
|b8-scene-validation-build|passed|-|0|14.78|`fd4015007958`|
|b8-ui-build|passed|-|0|9.61|`0f64f71b79b5`|
|b8-ui-tests|passed|3/3 fail=0 skip=0|0|23.84|`0f64f71b79b5`|
|b8-validation-style-build|passed|-|0|13.85|`de02adac45c1`|
|b8-validation-style-tests|passed|3/3 fail=0 skip=0|0|25.46|`a992965252b9`|
|b9-benchmark-build|passed|-|0|18.55|`281e12d23cd9`|
|b9-content-benchmark-full|passed|-|0|26.14|`281e12d23cd9`|
|b9-content-benchmark-pilot|passed|-|0|5.78|`281e12d23cd9`|
|b9-data-projection-build|passed|-|0|1.68|`47570549b50b`|
|b9-native-ui-build|failed|-|1|0.08|`4f42bc60b68f`|
|b9-native-ui-build2|passed|-|0|10.30|`4f42bc60b68f`|
|b9-unicode-native-build|passed|-|0|1.74|`2524857adeba`|
|benchmark-pilot2|passed|-|0|359.98|`82beafc1ad4b`|
|benchmark-release-final|passed|-|0|105.32|`82beafc1ad4b`|
|diagnostic-green-build|passed|-|0|4.03|`5f9403e623fe`|
|diagnostic-green-tests|passed|1/1 fail=0 skip=0|0|0.36|`5f9403e623fe`|
|diagnostic-red-build|passed|-|0|1.38|`85e9a5b4140a`|
|diagnostic-red-tests|failed|0/1 fail=1 skip=0|8|0.34|`85e9a5b4140a`|
|equivalence-build|passed|-|0|1.43|`8c30cffbe4cd`|
|equivalence-tests|passed|1/1 fail=0 skip=0|0|0.44|`8c30cffbe4cd`|
|fault-fix-build|failed|-|1|0.08|`82beafc1ad4b`|
|fault-fix-build2|passed|-|0|3.55|`82beafc1ad4b`|
|fault-fix-tests3|passed|1/1 fail=0 skip=0|0|0.98|`82beafc1ad4b`|
|final-content-benchmark-pilot|failed|-|1|0.03|`44c69bab89b7`|
|final-expanded-release-build|passed|-|0|26.36|`664a1cabc017`|
|final-fault-expanded-build|passed|-|0|8.12|`ba3f720e3fe2`|
|final-fault-expanded-build2|passed|-|0|8.00|`44c69bab89b7`|
|final-fault-expanded-tests|failed|0/1 fail=1 skip=0|8|60.05|`ba3f720e3fe2`|
|final-fault-expanded-tests2|failed|0/1 fail=1 skip=0|8|60.04|`44c69bab89b7`|
|final-headers|passed|-|0|6.40|`df933104da0b`|
|final-ide-development|passed|-|0|6.04|`df933104da0b`|
|final-ide-development2|passed|-|0|5.74|`44c69bab89b7`|
|final-ide-normal|passed|-|0|4.71|`df933104da0b`|
|final-ide-normal2|passed|-|0|4.30|`44c69bab89b7`|
|final-mover-native-debug-build|passed|-|0|2.44|`83156fdeb701`|
|final-mover-native-debug-tests|passed|36/36 fail=0 skip=0|0|211.06|`83156fdeb701`|
|final-mover-native-release-build|passed|-|0|2.91|`83156fdeb701`|
|final-mover-native-release-tests|passed|36/36 fail=0 skip=0|0|67.07|`83156fdeb701`|
|final-mover-no-stl|passed|-|0|0.45|`83156fdeb701`|
|final-mover-portable-debug-build|passed|-|0|1.37|`83156fdeb701`|
|final-mover-portable-debug-tests|passed|30/30 fail=0 skip=0|0|158.05|`83156fdeb701`|
|final-mover-portable-release-build|passed|-|0|2.46|`83156fdeb701`|
|final-mover-portable-release-tests|passed|30/30 fail=0 skip=0|0|27.88|`83156fdeb701`|
|final-native-debug-build|passed|-|0|26.07|`f3b4b531cd09`|
|final-native-debug-tests|passed|36/36 fail=0 skip=0|0|201.82|`df933104da0b`|
|final-native-full-configure|passed|-|0|1.34|`44c69bab89b7`|
|final-native-full-debug-build|passed|-|0|19.83|`44c69bab89b7`|
|final-portable-debug-build|passed|-|0|16.04|`f3b4b531cd09`|
|final-portable-debug-tests|passed|30/30 fail=0 skip=0|0|155.37|`df933104da0b`|
|final-portable-release-build2|passed|-|0|25.60|`44c69bab89b7`|
|final-python-ide|failed|-|1|4.35|`df933104da0b`|
|final-python-ide2|passed|-|0|4.31|`82beafc1ad4b`|
|frozen-native-debug-build|passed|-|0|20.49|`cad6926a3d17`|
|frozen-portable-build|passed|-|0|15.84|`cad6926a3d17`|
|locked-native-debug-build|passed|-|0|14.71|`4a24c6bd6948`|
|locked-portable-debug-build|passed|-|0|11.11|`4a24c6bd6948`|
|mover-debug-build|failed|-|1|0.07|`83156fdeb701`|
|mover-debug-build2|passed|-|0|1.42|`83156fdeb701`|
|mover-debug-test|passed|1/1 fail=0 skip=0|0|0.37|`83156fdeb701`|
|slice-collider-debug-build|passed|-|0|10.63|`5fea8de4e955`|
|slice-collider-debug-tests|passed|2/2 fail=0 skip=0|0|13.21|`5fea8de4e955`|
|slice-no-stl|passed|-|0|0.53|`b389de75167a`|
|slice-portable-debug-build|passed|-|0|20.90|`b389de75167a`|
|slice-portable-debug-tests|passed|29/29 fail=0 skip=0|0|163.23|`b389de75167a`|
|slice-portable-release-build|passed|-|0|42.53|`5fea8de4e955`|
|slice-portable-release-tests|passed|29/29 fail=0 skip=0|0|29.40|`5fea8de4e955`|
|slice-python|passed|-|0|5.40|`b389de75167a`|
|Package-Off-Debug-1-Logs|failed|-|0,0,0,1|10.76|`4f42bc60b68f`|
|Package-Off-Debug-2-Logs|passed|-|0,0,0,0,0,0,0,0,0,0|15.59|`47570549b50b`|
|Package-Off-Debug-Accepted-Logs|passed|-|0,0,0,0,0,0,0,0,0,0|39.63|`5f9403e623fe`|
|Package-Off-Debug-Final-Logs|passed|-|0,0,0,0,0,0,0,0,0,0|17.05|`82beafc1ad4b`|
|Package-Off-Release-Accepted-Logs|passed|-|0,0,0,0,0,0,0,0,0,0|42.70|`5f9403e623fe`|
|Package-Off-Release-Final-Logs|passed|-|0,0,0,0,0,0,0,0,0,0|21.26|`82beafc1ad4b`|
|Package-On-Debug-1-Logs|passed|-|0,0,0,0,0,0,0,0,0,0,0,0,0|33.93|`47570549b50b`|
|Package-On-Debug-Accepted-Logs|passed|-|0,0,0,0,0,0,0,0,0,0,0,0,0|33.69|`5f9403e623fe`|
|Package-On-Release-Accepted-Logs|passed|-|0,0,0,0,0,0,0,0,0,0,0,0,0|38.03|`5f9403e623fe`|
|Standalone-Accepted|passed|30/30 fail=0 skip=0; 30/30 fail=0 skip=0|0,0,0,0,0,0,0,0|293.20|`5f9403e623fe`|
|Standalone-Final|passed|30/30 fail=0 skip=0; 30/30 fail=0 skip=0|0,0,0,0,0,0,0,0|264.72|`82beafc1ad4b`|
|Standalone-Final-Mover|passed|30/30 fail=0 skip=0; 30/30 fail=0 skip=0|0,0,0,0,0,0,0,0|267.03|`83156fdeb701`|

配布OFFは旧10工程（build-configure、build、install、consumer-configure、consumer-build、consumer-run、support-only-run、physics-only-run、ui-only-run、ui-runtime-run）。export-pathsは別の非コマンド検査として保持。ONはNativeAppとNativeUi builtin/styledの3実行を追加した13工程。新Contentは既存Consumer内で実行し、工程件数のために旧Consumerを削除していない。

## 未実施・保持する限界

SDK未導入PC／開発ツール未導入PC、TSan、人の物理キー・聴感・Visual Studio目視、全D3D/COM資源リーク証明は未実施。Worker内部の人工例外注入は行っておらず、Job投入拒否とCPU読解例外・退役は別に検査した。過去NativeModel早期終了の根本原因、UI 3Dパネル確保、Dynamic床追従、Joint連続拘束は今回の成功で解消扱いにしない。Scene共通接続は四種類の製品経路を持つが、独立したcross-instance実World回帰はFixedを代表にした。新しい種類／描画拡張／セーブ等へ自動で進まない。

## 最終コードと採用manifestの照合

最初のaccepted全群・配布・A/B/Cはmanifest 5f9403e623fe2b49bb15bd305b1770bb8858bc889d7a067d2fce5acb5b1b8009。その後は試験用JSON四つとPython試験一つのCRLF正規化、およびSceneContentTests.cppのKinematicMover直接回帰二件だけが変更された。製品・CMake・Sample・Consumer・正規Assetsの生バイトは同一で、配布・変異・ヘッダー・測定の製品入力を照合した。製品変更後の未検証を旧結果で埋めたものではない。

CRLF正規化後の故障D/RとPython IDEを再実行し、Mover回帰後はPortable/Native D/R全build・全CTest、正規独立入口、No-STLを再実行した。再配置四構成・同一exe・変異・測定は同一製品コードに対する既採用結果。最後の追加回帰は、定義からKinematicMoverを生成し、型付きexportのSetTarget/SetPath、Joint ID維持、別個体の不変性、片側破棄を両次元の実Worldで確認する。

|先行集合から変わった試験ファイル|
|---|
|Tests/Data/SceneContent/fault-scene2d-a.dxfscene.json|
|Tests/Data/SceneContent/fault-scene2d-b.dxfscene.json|
|Tests/Data/SceneContent/fault-scene3d-a.dxfscene.json|
|Tests/Data/SceneContent/fault-scene3d-b.dxfscene.json|
|Tests/SceneContentTests.cpp|
|Tools/Tests/test_content_mutations.py|

Tests/とTools/Tests/を除く入力集合のSHA-256は`de4c6758cbbfdde48cd56e5e9b06cf243779bd9d920a6c693e4197424e0b9223`。配布4構成、同一exe試験、測定、ヘッダー検査と最終集合で一致し、照合表はFinalProductInputComparison.jsonへ保存した。

## 費用の代表値（独立Release、五環境）

256個体、512 Body、四種類1024 Joint、共有定義・資源なし。単位はμs、五環境の中央値［最小,最大］。活動・休止別のSolver性能を測定した系列ではなく、Content生成と状態読取の費用である。

|次元|工程|μs 中央値［最小,最大］|追加確保 中央値|
|---|---|---:|---:|
|2D|read-parse-expand|177.100 [173.900,222.300]|889.000|
|2D|pure-validation|4.100 [3.700,4.300]|23.000|
|2D|owner-prepare|5.100 [4.400,6.700]|42.000|
|2D|spawn-accept|91.500 [85.800,95.600]|1287.000|
|2D|initialize|748.200 [741.900,820.500]|4104.000|
|2D|first-fixed-ready|6957.600 [6932.000,7057.200]|5497.000|
|2D|fixed-total|6605.323 [6599.135,6618.178]|5387.000|
|2D|content-state-only|130.791 [127.786,133.237]|0.000|
|2D|world-only|5251.634 [5243.203,5253.273]|1291.000|
|2D|shutdown|735.600 [711.000,891.800]|0.000|
|3D|read-parse-expand|186.900 [177.600,195.000]|925.000|
|3D|pure-validation|5.400 [5.000,5.800]|23.000|
|3D|owner-prepare|5.700 [5.600,6.200]|42.000|
|3D|spawn-accept|180.600 [173.400,189.500]|1287.000|
|3D|initialize|913.400 [879.500,928.400]|4104.000|
|3D|first-fixed-ready|12178.300 [12153.300,12214.900]|5497.000|
|3D|fixed-total|11575.343 [11569.672,11593.074]|5387.000|
|3D|content-state-only|130.762 [129.137,131.583]|0.000|
|3D|world-only|9429.266 [9420.328,9435.778]|1291.000|
|3D|shutdown|1018.200 [867.900,1083.600]|0.000|

状態読取と終了は両次元0確保。固定更新全体は5387回、追加比較のWorld単独は1291回であり全体無確保ではない。1/32/256と資源共有／別条件の全480行はaccepted-benchmark/command.logへ保存。試験用Textureの共有時owner load=1、別条件では個体数分。

|実Native準備（五独立プロセス）|CPU読解・展開 μs|所有側資源取込 μs|
|---|---:|---:|
|2D|6810.2 [6372.6,6886.9]|5183.9 [5164.1,5333.4]|
|3D|7765.0 [6929.8,10239.7]|34849.8 [34301.4,34941.3]|

実Native値は同梱Scene一件の定義CPU準備と所有側PrepareSceneの区間。Application起動、モデル個体作成、GPU、描画時間を含めず、所有側同期取込を非ブロックと説明しない。

## 変異復元の指紋

|変異|build / Red / 復元build / Green exit|対象・復元SHA-256|
|---|---|---|
|B-M01|0/1/0/0|Source\SceneContent\Private\Dxf\PrefabRuntime.h: 0208e6a5f1e4e2f818bc52f4ff1687c0979ea04033cc6cc2d523d42eb15dfbbf|
|B-M02|0/1/0/0|Source\SceneContent\Private\Dxf\PrefabRuntime.h: 0208e6a5f1e4e2f818bc52f4ff1687c0979ea04033cc6cc2d523d42eb15dfbbf|
|B-M03|0/1/0/0|Source\SceneContent\Private\Dxf\PrefabRuntime.h: 0208e6a5f1e4e2f818bc52f4ff1687c0979ea04033cc6cc2d523d42eb15dfbbf|
|B-M04|0/1/0/0|Source\SceneContent\Private\Dxf\PrefabRuntime.h: 0208e6a5f1e4e2f818bc52f4ff1687c0979ea04033cc6cc2d523d42eb15dfbbf|
|B-M05|0/1/0/0|Source\SceneContent\Private\Dxf\SceneContentParser.cpp: 5f404be22372ab1958c91e872dcd6005bc372ac94949e7421ed30626190ec3bb|
|B-M06|0/1/0/0|Source\SceneContent\Private\Dxf\ContentSchemaExtras.inl: 4839a550790881593cbf5e2cc450d7c012ae7c5c33396c26f2c41f2be4c5d826|
|B-M07|0/1/0/0|Source\SceneContent\Private\Dxf\PrefabRuntime.h: 0208e6a5f1e4e2f818bc52f4ff1687c0979ea04033cc6cc2d523d42eb15dfbbf|
|B-M08|0/1/0/0|Source\SceneContent\Private\Dxf\ContentSchemaReader.cpp: 4da4181d3f5926928c0ff83cc63c1e5f77dfed416ec895cf1e12cb3eff4c3690|
|B-M09|0/1/0/0|Source\SceneContent\Private\Dxf\SceneContentSource.cpp: cc179662445de36133ed4b2226e7400c8aa7d6f6b07ae3300c65afea341f973d|
|B-M10|0/1/0/0|Source\SceneContent\Private\Dxf\SceneContentRequest.cpp: 38aedd306ba16dbfbe3150fee914e690df0bce358f437f6f41eb2bb478bada34|
|B-M11|0/1/0/0|Source\SceneContent\Private\Dxf\SceneContentRequest.cpp: 38aedd306ba16dbfbe3150fee914e690df0bce358f437f6f41eb2bb478bada34|
|B-M12|0/1/0/0|Source\SceneContent\Private\Dxf\ContentResources.cpp: b31d90f733a91a425138419cc4a29dc54c4c688f5ed23bf86067d9bce3f74063|
|B-M13|0/1/0/0|Source\SceneContent\Private\Dxf\PrefabInstance2D.cpp: 78ea8593b3a491cb95e5dc2ebd21f431a801948b56c115733b73e66d22b3ae93<br>Source\SceneContent\Private\Dxf\PrefabInstance3D.cpp: f1ad459f772d04bd83041cb2d553a2bc6bb7761ba0abc959719ae8423f4705af|
|B-M14|0/1/0/0|Source\SceneContent\Private\Dxf\ContentVisuals.cpp: ac4d11713c307cbc64cd89a7d53ffa37fcd7d84a4491f8826d5e408574344dfb|
|B-M15|0/1/0/0|Source\SceneContent\Private\Dxf\PrefabRuntime.h: 0208e6a5f1e4e2f818bc52f4ff1687c0979ea04033cc6cc2d523d42eb15dfbbf|
|B-M16|0/1/0/0|Source\SceneContent\Private\Dxf\SceneContentSource.cpp: cc179662445de36133ed4b2226e7400c8aa7d6f6b07ae3300c65afea341f973d|
|B-M17|0/1/0/0|Source\SceneContent\Private\Dxf\SceneContentRequest.cpp: 38aedd306ba16dbfbe3150fee914e690df0bce358f437f6f41eb2bb478bada34|
|B-M18|0/1/0/0|Source\SceneContent\Private\Dxf\PrefabRuntime.h: 0208e6a5f1e4e2f818bc52f4ff1687c0979ea04033cc6cc2d523d42eb15dfbbf|
