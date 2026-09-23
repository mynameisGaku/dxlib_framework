# デバッグ表示・カメラ・物理Snapshot観察

## Renderer接続

Applicationは`FRenderSystem`だけを所有し、構築完了後に所有する共有JobSystemを`SetExecutionJobs`で接続する。
旧`FRenderSystem2D`と、その型の互換aliasや旧ルート描画関数（`Render.Draw`等）は存在しない。描画入口は`Get2D()`/`Get3D()`。
root CMakeは`RenderSystem.cpp`と`RenderPass3D.cpp`を`dxf_support`へ登録し、`CMake/DebugTools.cmake`を読み込む。
3D命令が積まれたフレームでは、3D実行の後に2D状態を復元してから2Dを実行する。3D命令がなければBackend状態を変更しないため、この復元も行わない。

2026-09-23に、旧適用器（`apply_debug_tools.py`）を使わず現行ソースへ限定差分として統合した。
旧適用器は`Renderer.GetContext().Get3D()`経由の参照を根Contextと誤判定し、正しい`Tests/RenderViewsTests.cpp`で停止する。テスト側は変更していない。

## Debug層

`dxf::debug_tools`は描画Supportと、値Snapshotを採取する`dxf::physics`に依存する。Physics本体はDebug・Renderer・DxLibに依存しない。
配置はSource/Debug/Public/DxfとSource/Debug/Private/Dxf。

- FDebugCamera3D: Orbit、Zoom、平行移動、FRenderView3Dの生成。
- FDebugStepController: 物理用の停止・0.25倍等の倍率・一固定tick送り。Toolbox::FFixedStepSchedulerを利用。
- FPhysicsDebugSnapshot3D / FPhysicsDebugSnapshot2D: World自身が採取したSnapshotを表示用のワールド座標へ変換した値。
  World ID・Worldが数えた正常Step数・最後のStep引数・呼出し側の時計の秒数・生存Body数・全Colliderの形状と速度等を値所有する。
- CapturePhysicsDebugSnapshot3D / CapturePhysicsDebugSnapshot2D: 正常Step完了後に`FPhysicsWorld3D/2D::CaptureSnapshot()`を明示的に呼び、変換する。
  採取拒否・上限超過・確保失敗は例外ではなく`TResult`の失敗として返し、`PhysicsDebug`分類の警告ログにも理由を残す。
- BuildPhysicsDebugSnapshot3D / BuildPhysicsDebugSnapshot2D: 取得済みの`FPhysicsSnapshot3D/2D`だけを変換する。Worldへは触れない。
- FPhysicsDebugRecorder3D: 有効時だけ採取し、最新値と履歴を保持する。無効中はWorldへ触れず、全件採取・履歴複製を行わない。
- FDebugSnapshotHistory: 最大240件、各Snapshot最大256形状のリング。取り出す結果は所有する複写。
- SubmitPhysicsDebugSnapshot3D: 同じ3Dビューへ辺・重心・速度を追加する描画側アダプター。
- SubmitPhysicsDebugSnapshot2D / BuildPhysicsDebugCommands2D: 既存の2D線・円命令（FLineCommand2D / FCircleCommand2D）へ変換する。
  メートル・Y上向きから画面ピクセル・Y下向きへの変換は`FPhysicsDebugView2D`で明示する。

旧`TPhysicsDebugWatch3D`と、それを受ける旧`CapturePhysicsDebugSnapshot3D`テンプレート、`EDebugBodyMotion`は削除した。
Collider作成時の定義を表示のために二重登録する必要はない。運動区分はPhysicsの`EBodyType`をそのまま使う。

```cpp
// 固定更新: World更新完了 → 明示的な採取 → 表示用の値変換。
World.Step(StepSeconds);
SampleClock += StepSeconds; // 実際に正常完了したStepへ渡した秒数。Step番号から逆算しない。
auto Frame = Dxf::CapturePhysicsDebugSnapshot3D(World, SampleClock);
if (!Frame)
{
	// 既存のエラー処理へ接続する。切り詰めた結果は返らない。
}

// 描画: 採取済みの値だけを使う。Worldは参照しない。
Dxf::SubmitPhysicsDebugSnapshot3D(Frame.Value(), Settings, Render.Get3D());
```

2Dも同じ所有契約で`CapturePhysicsDebugSnapshot2D`→`SubmitPhysicsDebugSnapshot2D(Frame, View, Settings, Render.Get2D())`と呼ぶ。
2Dの履歴型は用意していない。汎用エディター・選択UIも対象外。

## サンプルRenderDebug

Starterは空テンプレートのまま、Sandboxのゲーム操作は変えない。デバッグサンプルは別exe。
開発ソリューションではNativeとTestsが有効なら既定で追加される。通常ソリューションへデバッグ用プロジェクトを必須追加しない。
`DXF_BUILD_RENDER_DEBUG=ON`で明示的にも構築可能。ただしNative Backendが必要。
サンプルはApplicationの共有JobSystemを借用し、独自のPoolは作らない。
床・落下する箱・反発する球を実FPhysicsWorld3Dで更新する。描画用に物理を近似し直したサンプルではない。
F9で、床・傾いた箱・円を持つ実FPhysicsWorld2Dの2D観察を画面右下へ表示する（2D入口の最小使用例）。
F8・F9・Enterの操作は`RenderDebug`分類のInfoログとしてVisual Studioの出力へ記録される（[Logging.md](../Logging.md)）。

| キー | 操作 |
|---|---|
| F1 | Solid → Wireframe → Solid+Edges |
| F2 | Normal (CPU flat) → Unlit → LightsOff |
| F3 | 調査用の単一方向光ON/OFF。DxLib全ライト管理ではない |
| F4 | 採取したCollider・重心・速度の重ね表示ON/OFF |
| F5 | 速度線ON/OFF（F4がONのとき見える） |
| F6 | 重ね表示の深度無視ON/OFF |
| F7 | 半透明三枚の比較表示 |
| F8 | 3D物理観察ON/OFF。OFF中は採取・履歴保存を行わず、図形も描かない（Worldの更新は継続） |
| F9 | 2D物理観察の表示ON/OFF。OFF中は2D Worldを採取しない |
| 矢印 / 右ドラッグ | カメラの周回 |
| WASD / Q・E | カメラの平行移動 / 上下移動 |
| ホイール / Shift | カメラ距離 / 平行移動の加速 |
| R | カメラ初期化 |
| P | 物理だけ停止・再開 |
| N | 停止中に1/60秒の固定更新を1回だけ進める |
| O | 通常速度と0.25倍速を切替 |
| Z / X | 停止中の保存Snapshotを古い方 / 新しい方へ選択 |
| Enter | 物理と履歴の再生成。停止・倍率・観察ON/OFFは保持する |
| Tab / Escape | 説明パネル表示 / 終了 |

3Dは一フレームに一度だけSetViewし、そのビューに重ね表示する。Collider表示のために深度を消去する別Viewへ切り替えない。
2Dの文字とパネルは3D処理後に描画する。既存Nativeの任意の外部状態を完全復元する保証は追加していない。
パネルのCPU値は最後に実行したliveのPhysics Stepの壁時計時間。履歴を選んでもGPU時間や履歴フレームの計測値に読み替えない。
パネルのStepは3D Worldが数えた正常Step数。時計（ms）はサンプルが正常完了したStepへ渡した秒数の合計で、`StepIndex × LastDeltaSeconds`ではない。

## 採取契約と制約

採取は`FPhysicsWorld3D/2D::CaptureSnapshot()`の戻り値だけを使う。生存Body・Colliderを全件含み、Colliderを持たないBodyは`BodyCount`で数える。
変換は表示側で行う。`LocalShape`の中心へBody姿勢を一度だけ適用し、3D OBBの軸を回転し、2D矩形の角度はBody角とローカル角の和にする。
BodyとColliderの対応はWorld・Index・Generationを含むIDで確認し、一致しない場合は失敗にする。
1回の観察で変換するColliderは最大256件、Bodyは最大4096件。超過はWorldの採取段階から失敗させ、先頭だけを残した結果を全件として返さない。
Worldの途中失敗Step後は、次の正常Step完了まで採取を拒否する（Physicsの契約）。この拒否も`TResult`の失敗になる。
採取はWorldの所有スレッドでStepと重ならない区間に行う。Worldを並行変更しないこと。

Snapshotはコピーされ、World・Texture・Font・Nativeハンドルへの参照を持たない。World削除・破棄後も採取済みの値を描画できる。
接触点・Impulse・Island・CCD軌跡・GPU時間は未採取。別の接触判定でそれらを作り「実際のSolver値」と表示しない。
同一Collider IDのままShapeを更新するAPIはPhysicsにないため、改訂番号も表示しない。

## 時間と履歴

サンプルはDGameSceneで独自に固定計画を実行する。DPhysicsSceneの自動処理と二重適用しない。
1固定tickは1/60秒、フレーム当たり最大8回。過負荷時に破棄したゲーム時間を別の値として記録する。
停止中の経過時間は再開時に取り戻さない。手送り要求は一つだけ保留し、同じ要求をsubstepごとに再適用しない。
通常時はWorldのStep番号が6の倍数のとき（約10Hz）、停止操作と手送り時にも実状態を保存する。120件なので通常約12秒相当だが、手送り時は時間幅が異なる。
停止中に再採取しても、同じStep番号は履歴へ二重保存せず、Step番号も増えない。
履歴の添字0は最新。サンプルでは停止時に最新liveを保存してから古い履歴を選ぶ。
履歴は物理の巻戻し・再実行機能ではない。保存値から描くのみで、Worldへ位置や速度を書き戻さない。
履歴のMove後は移動元を破棄または代入する。並行操作は許可しない。

## カメラの数値条件

注視点の各成分は±100000、距離0.1〜10000。Pitchは約±89度、入力角度は有限範囲。
f32へ丸めたEye/Targetで視線基底が潰れる場合、姿勢の変更を拒否する。
移動入力各軸は[-1,1]、時間は[0,0.25]、速度は[0,1000]。合成方向を制限して斜め移動の過加速を防ぐ。
カメラ操作は物理の停止・倍率を通らないサンプル入力から行う。一般的なエディターカメラの全機能ではない。

## 実機確認表

| # | 確認項目 | 2026-09-23 Debug（実DxLib 3.25a・実画面キャプチャ） |
|---|---|---|
| 1 | 床・箱・球が表示され、箱と球が落下する | 確認（落下後に床上で休止） |
| 2 | 2D説明文字が3Dの後に正常に描画される | 確認（F1〜F3は未操作） |
| 3 | Pで物理停止、Nでworld stepが1増える | 確認（435→436、時計+16.7ms、履歴74→75） |
| 4 | F4で採取値の辺・重心を表示 | 確認（F5/F6は未操作） |
| 5 | 停止中のZ/X履歴選択 | 未実施 |
| 6 | F8 OFFで図形が消え採取停止、ONで現在Stepから再開 | 確認（OFF中step表示"-"・履歴75のまま、ON後step=436・履歴75） |
| 7 | F9で2D観察が右下へ表示 | 確認（床・箱・円、円の半径線と重心十字） |
| 8 | Enterで再生成 | 未実施 |
| 9 | sln・exe直接・別CWDからの起動 | 未実施（ビルドディレクトリから起動） |

Release構成の実SDK実行ファイルは、公式SDKの`DxLib_vs2015_x64_MT.lib`がFBX SDKを要求するためリンクできない（Validation記録を参照）。

## 採取済みColliderの選択

RenderDebugは左クリックで表示Snapshotの球・箱を選択し、採取時のID・重心・速度・休止状態を表示する。[問い合わせAPI、履歴と観察OFFの契約](PhysicsSnapshotPicking.md)を参照。
