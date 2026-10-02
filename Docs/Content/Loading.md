# 準備要求・Scene遷移・再読み込み

FSceneContentSourceはファイルI/O、JSON、参照展開、型検証だけを行い、WorldやNativeを変更しません。PreparePrefab／PrepareSceneは所有側で既存AssetServiceへTexture／Model／Sound／Fontを要求します。同じパス・条件・Rootは既存Cacheを共有します。

必須資源一つの失敗でも準備は失敗し、今回取得した共有参照だけを回収します。白矩形や空モデルで成功扱いにせず、旧個体が共有する資源を無効化しません。Scene共通資源は個体が空でも必須です。

## 要求の所有

呼出し側がFSceneContentRequestを所有し、任意の既存TaskDispatcherと親Scopeを借用します。DispatcherはRequestより長く生存させ、操作・Poll・破棄は構築した所有スレッドのTask／Commit外で行います。同期はDispatcherを渡さず、非同期は既存Dispatcherを渡します。

WorkerはCPU Prepareだけ、Task CommitはCPU結果の値公開、通常更新のPoll(Assets)は所有側の資源採用です。SceneNavigatorの確定境界をCommitから再入実行しません。

|状態|意味|
|---|---|
|Pending／Preparing|受付済み／CPU処理中|
|Ready|所有側の必須資源準備・採用成功|
|Failed|診断を保持して失敗|
|Canceled|明示取消しで終端。CPUスレッド強制終了ではない|
|Superseded|後続要求により置換された終端|

要求番号を照合し、A開始→B開始→B完了→遅れてA完了でもAを採用しません。取消しで永久Pendingを残しません。GetPrepared2D/3Dは最後の成功値を保持し、新しい失敗・取消しでは破棄しません。TicketのReadyとGetAcceptedSequenceを確認し、通常更新からNavigatorへScene要求を渡します。

Retireは対象Scopeだけを退役・待機して入力寿命を保ち、兄弟／Root全体を無条件に待ちません。Sceneをまたぐ要求は遷移を渡すまで生存する所有者へ置きます。Model等の同期LoaderはPollをブロックし得ます。完全ノンブロッキングや単一Loaderの途中取消しは保証しません。

## 旧Sceneを保持する範囲

|失敗位置|結果|
|---|---|
|JSON／schema／参照／資源準備|変更要求を出さず、旧Scene・旧成功定義を維持。診断と明示再試行|
|Pending SceneのOnInitialize|既存Navigatorが候補を終了して旧Sceneを保持|
|切替後のFixedTick／PrePhysics／World.Step|既存Applicationの実行時停止。旧Sceneへの無条件rollbackなし|
|描画Backend|失敗フレームをPresentしない既存経路|

明示再読み込みだけを提供します。準備に成功した新しいバージョンの定義は新生成／Scene再開始に使い、生存中のInstanceは以前の不変定義を保持します。再開始は物理状態を初期化し、角度・速度・取得物を保存復元しません。

## GameplaySampleと再現

従来C++コースを保持し、F2からContentコースへ入り、F1パネルでScene次元の選択、読込／壊れた定義／取消し、個体選択・生成・破棄、型付きJointの運転を操作します。次回生成のspeed／travel／effort／colorと現在個体の目標操作は別欄です。UI変更だけで毎Frame再構築しません。

同梱選択肢は2D／3Dコースと対応扉Prefabです。任意ファイルを列挙するエディターではありません。Pause、Modal、閉じたFrameの入力抑止は既存UI経路を使い、1／2Viewは描画だけを増やします。

`Tools/ValidateContentData.py --exe <NativeGameplaySmoke.exe> --logs <新規保存先>`は日本語・空白パスへAssets検証用コピーを作ります。同じexeを別CWDでA／B定義から実行し、Worldの個体数・配置・Motor、実画素の差、壊れたCから同一セッション再試行を保存します。原本Assetsを変更せず、exe SHA-256を前後照合します。

`Tools/ValidateContentMutations.py --build <PortableBuild> --logs <新規保存先>`はB-M01〜18を一つずつ適用し、build成功、挙動Red、バイト復元、再compile、Green、指紋一致を記録します。失敗や未検出も残し、compile失敗を検出件数へ含めません。


## 論理チェックポイントからの再開

SceneContent自身は初期構成を扱う。任意の `dxf::game_save` を利用する場合は、保存文書と許可カタログから新しい初期構成を作り、既存の資源準備とNavigatorを使ってチェックポイント進行を再開できる。初期構成の再読み込みと進行ロードは別操作である。

`FCheckpointResumeRequest2D`／`3D` は保存の読解・参照検査・純粋なゲーム変換・再検証をCPU準備で行う。既存TaskDispatcherを渡す場合はWorkerが不変のカタログと要求ごとの値だけを扱い、SceneやNative資源を捕捉しない。所有側の通常更新でPollし、資源準備とNavigatorへの切替要求を行う。`ACheckpointScene2D`／`3D` の有効化・成功PostPhysics・全必須Readyを確認して完了する。Preparedや切替受付だけを再開成功と表示しない。失敗保証は上表を引き継ぎ、切替後のWorld全体のrollbackは追加しない。詳細と実際の公開利用例は [保存とチェックポイント](../Gameplay/SavingAndCheckpoints.md) を参照する。既存SampleのSave／Continue UIと、2D／3Dそれぞれの別プロセス再開を利用できる。

Scene要求から得た埋込Prefabの指紋は、その要求が実際に読んだScene・共有資源宣言・全依存定義のバイト集合を含む。単独 `LoadPrefab2D`／`3D` の依存集合とは区別する。これは保守的な互換性確認で、無関係なScene定義の変更でも同じ要求のPrefab指紋は変わり得る。資源ファイルの全内容の指紋や、後日のディスク再読込結果を意味しない。
