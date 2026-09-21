# デバッグ表示・カメラ・登録済みCollider観察

## Renderer接続の修復

86a37b9にはFRenderSystemとFRenderSystem2Dが共存し、root CMakeとApplicationは旧型を使用している。
旧Flushは2Dキューだけを実行するため、新しいGet3D()で受けた命令は画面提示まで実行されない。
今回の移行器は旧型のファイルと参照を除き、既存のFRenderSystemを唯一の所有者にする。
Application構築後に共有JobSystemをContextへ接続する。型の互換aliasや旧ルート描画関数は作らない。

## 新しいDebug層

`dxf::debug_tools`は描画SupportとToolboxに依存し、Physics本体はDebugに依存しない。
配置はSource/Debug/Public/DxfとSource/Debug/Private/Dxf。

- FDebugCamera3D: Orbit、Zoom、平行移動、FRenderView3Dの生成。
- FDebugStepController: 物理用の停止・0.25倍等の倍率・一固定tick送り。Toolbox::FFixedStepSchedulerを利用。
- FPhysicsDebugSnapshot3D: World ID・固定更新番号・時間・世代付きColliderの形状と速度等を値所有する。
- CapturePhysicsDebugSnapshot3D: 明示登録したColliderを公開World APIから採取するテンプレート。
- FDebugSnapshotHistory: 最大240件、各Snapshot最大256形状のリング。取り出す結果は所有する複写。
- SubmitPhysicsDebugSnapshot3D: 同じ3Dビューへ辺・重心・速度を追加する描画側アダプター。

## サンプルRenderDebug

Starterは空テンプレートのまま、Sandboxのゲーム操作は変えない。デバッグサンプルは別exe。
開発ソリューションではNativeとTestsが有効なら既定で追加される。通常ソリューションへデバッグ用プロジェクトを必須追加しない。
`DXF_BUILD_RENDER_DEBUG=ON`で明示的にも構築可能。ただしNative Backendが必要。
サンプルはApplicationの共有JobSystemを借用し、独自のPoolは作らない。
床・落下する箱・反発する球を実FPhysicsWorld3Dで更新する。描画用に物理を近似し直したサンプルではない。

| キー | 操作 |
|---|---|
| F1 | Solid → Wireframe → Solid+Edges |
| F2 | Normal (CPU flat) → Unlit → LightsOff |
| F3 | 調査用の単一方向光ON/OFF。DxLib全ライト管理ではない |
| F4 | 登録Collider・重心・速度の重ね表示ON/OFF |
| F5 | 速度線ON/OFF（F4がONのとき見える） |
| F6 | 重ね表示の深度無視ON/OFF |
| 矢印 / 右ドラッグ | カメラの周回 |
| WASD / Q・E | カメラの平行移動 / 上下移動 |
| ホイール / Shift | カメラ距離 / 平行移動の加速 |
| R | カメラ初期化 |
| P | 物理だけ停止・再開 |
| N | 停止中に1/60秒の固定更新を1回だけ進める |
| O | 通常速度と0.25倍速を切替 |
| Z / X | 停止中の保存Snapshotを古い方 / 新しい方へ選択 |
| Enter | 物理と履歴の再生成。停止と倍率は保持する |
| Tab / Escape | 説明パネル表示 / 終了 |

3Dは一フレームに一度だけSetViewし、そのビューに重ね表示する。Collider表示のために深度を消去する別Viewへ切り替えない。
2Dの文字とパネルは3D処理後に描画する。既存Nativeの任意の外部状態を完全復元する保証は追加していない。
パネルのCPU値は最後に実行したliveのPhysics Stepの壁時計時間。履歴を選んでもGPU時間や履歴フレームの計測値に読み替えない。

## 採取契約と制約

現在のWorld公開APIには、全Collider列挙やLocalShape読出しがない。
したがって「自動全World Snapshot対応済み」ではない。作成時に保存したID/Shape/Body種別のWatch配列を使う。
サンプルの登録は3件（床・箱・球）。実Worldの位置・姿勢・速度・角速度・休止をStep完了後に取得する。
ローカル形状はAttachCollider時の定義を保持し、採取時に実Body姿勢で中心オフセット・OBB軸を変換する。
同一IDのShape/Body種別を変更するAPIが今後追加された場合、登録更新または正式な読取APIへの置換が必要。
期限切れのColliderはSkippedCountへ記録する。異なるWorldのIDや重複登録、最大256件超過は拒否する。
Worldを並行変更しないこと。基底型のgetterが例外を出す場合や確保失敗は呼出し側へ伝播し得る。

Snapshotはコピーされ、World・Texture・Font・Nativeハンドルへの参照を持たない。
接触点・Impulse・Island・CCD軌跡は未採取。別の接触判定でそれらを作り「実際のSolver値」と表示しない。
2DのSnapshotはまだ未実装。3Dだけ実装して両次元の観察完了と扱わない。

## 時間と履歴

サンプルはDGameSceneで独自に固定計画を実行する。DPhysicsSceneの自動処理と二重適用しない。
1固定tickは1/60秒、フレーム当たり最大8回。過負荷時に破棄したゲーム時間を別の値として記録する。
停止中の経過時間は再開時に取り戻さない。手送り要求は一つだけ保留し、同じ要求をsubstepごとに再適用しない。
通常時は6tickごと（約10Hz）、停止操作と手送り時にも実状態を保存する。120件なので通常約12秒相当だが、手送り時は時間幅が異なる。
履歴の添字0は最新。サンプルでは停止時に最新liveを保存してから古い履歴を選ぶ。
履歴は物理の巻戻し・再実行機能ではない。保存値から描くのみ。
履歴のMove後は移動元を破棄または代入する。並行操作は許可しない。

## カメラの数値条件

注視点の各成分は±100000、距離0.1〜10000。Pitchは約±89度、入力角度は有限範囲。
f32へ丸めたEye/Targetで視線基底が潰れる場合、姿勢の変更を拒否する。
移動入力各軸は[-1,1]、時間は[0,0.25]、速度は[0,1000]。合成方向を制限して斜め移動の過加速を防ぐ。
カメラ操作は物理の停止・倍率を通らないサンプル入力から行う。一般的なエディターカメラの全機能ではない。

## 実機確認表

1. 床・箱・球が表示され、箱と球が落下する。失敗はダイアログとVS出力で確認する。
2. F1/F2/F3で表示を変えても、上部の2D説明文字が正常に描画される。
3. Pで箱・球のみ止まり、矢印/右ドラッグ/WASDは動く。Nを1回押すとlive tickが1増える。
4. F4/F5で実速度方向を表示し、F6で深度テストの差を見る。
5. 停止してZで過去の表示を選んでもlive tickは変化しない。再開はlive状態からで、履歴の位置へ飛ばない。
6. EnterでWorldと履歴を作り直す。何度繰り返しても以前のWorldの図形が残らない。
7. slnから、exe直接、別CWDから起動し、.dxfpathsと既存AssetRoot契約を維持する。

上の表は検証すべき項目であり、このLinux部分チェックアウトでの実施済み項目ではない。
