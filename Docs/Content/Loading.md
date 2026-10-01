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

明示再読み込みだけを提供します。成功新版は新生成／Scene再開始に使い、旧Instanceは旧不変定義を保持します。再開始は物理状態を初期化し、角度・速度・取得物を保存復元しません。

## GameplaySampleと再現

従来C++コースを保持し、F2からContentコースへ入り、F1パネルでScene次元の選択、読込／壊れた定義／取消し、個体選択・生成・破棄、型付きJointの運転を操作します。次回生成のspeed／travel／effort／colorと現在個体の目標操作は別欄です。UI変更だけで毎Frame再構築しません。

同梱選択肢は2D／3Dコースと対応扉Prefabです。任意ファイルを列挙するエディターではありません。Pause、Modal、閉じたFrameの入力抑止は既存UI経路を使い、1／2Viewは描画だけを増やします。

`Tools/ValidateContentData.py --exe <NativeGameplaySmoke.exe> --logs <新規保存先>`は日本語・空白パスへAssets検証用コピーを作ります。同じexeを別CWDでA／B定義から実行し、Worldの個体数・配置・Motor、実画素の差、壊れたCから同一セッション再試行を保存します。原本Assetsを変更せず、exe SHA-256を前後照合します。

`Tools/ValidateContentMutations.py --build <PortableBuild> --logs <新規保存先>`はB-M01〜18を一つずつ適用し、build成功、挙動Red、バイト復元、再compile、Green、指紋一致を記録します。失敗や未検出も残し、compile失敗を検出件数へ含めません。
