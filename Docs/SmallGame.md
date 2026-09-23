# 小さいゲームを組む

フレームワークの中心は、ゲーム固有の処理を書きやすくすることです。Sandboxに「タイトル→プレイ→結果→リトライ」の小さい例を置き、Starterは空の開始点として保持します。モデルやPBRの知識は不要です。

## 動かす

通常のソリューションでSandboxを起動します。タイトルでEnterを押し、WASDで右の緑色の帯へ移動すると結果画面になります。結果のEnterで新しいプレイ、Spaceでタイトルへ戻ります。プレイ中はSpaceで効果音、Pでポーズ、Escで終了します。タイトルのSpaceから従来の機能サンプル（Enterで配色の異なるSceneへ切替）も起動できます。

画像と音声は既存の`Assets/player.bmp`と`Assets/confirm.wav`を使います。ProjectRootは起動時に一度設定し、ゲーム側では`Assets/...`の相対パスを使います。各Sceneにソリューションの絶対パスを渡し直す必要はありません。

## ゲーム側に書くもの

| 処理 | 使用する公開API | 今回の例 |
|---|---|---|
| 起動と終了 | FApplication、FAppRunner | WindowsMain.cppで最初のSceneを渡す |
| タイトル・結果 | DScene、OnTick、OnDraw | SandboxMenuScene。結果値だけを受け取る |
| プレイ中の所有 | DGameScene::Spawn | SandboxGameの既存DPlayerを再利用 |
| 表示部品 | DGameObject::AddComponent | 既存SpriteComponentがプレイヤー位置を描画 |
| 入力と時間 | Context.Input、Context.Time | 移動・ゴール判定。ポーズを反映するScene時計を利用 |
| Scene切替 | Context.Scenes->RequestChange<T>() | 切替要求だけを書く。停止・破棄・初期化の順序は組み直さない |
| 読み込み | Context.Assets.LoadTexture/LoadFont/LoadSound | 結果を確認し、必要な共有ハンドルを保持 |
| 音声 | Context.Audio->Play、Context.AudioScope | Sceneに所属する再生は退役時に自動停止 |
| 読込失敗から再試行 | GetLastTransitionError | 元のSceneが保持されるのでタイトル上に失敗を表示して再試行 |

タイトル・結果はオブジェクト集合を必要としないDScene、プレイはDGameSceneを使います。共通の画面表示は同じメニューScene型の別インスタンスで共有します。結果へプレイヤーや内部資源を渡さず、秒数だけを渡します。

画像・音声・フォントのハンドルはメンバーとして保持するだけで、例に手動Delete、キャッシュ掃除、終了順序の組み替えはありません。画像の共有ハンドルとモデルの個体は役割が異なりますが、どちらもAssetServiceから取得し、公開描画窓口へ渡す原則を維持します。今回、新しい管理層や共通Subsystemは追加していません。

## Starterで同じ流れを作る

StarterのWindowsMainはそのまま利用し、Runnerへ渡す最初のSceneを自分のタイトルSceneへ変更します。Sceneは`Source`へ追加し、StarterのCMake対象へcppを登録します。Sandboxをそのまま試作の土台にする場合は`SandboxMenuScene.h/.cpp`と`SandboxGame.h/.cpp`をStarterのSourceへコピーし、WindowsMainで`SandboxMenuScene.h`をinclude、最初の型を`Dxf::Sandbox::ASandboxMenuScene`に変更します。2つのcppをStarterのソース一覧へ追加します。

起動時のProjectRoot設定は既存のものを使います。独自GameInstanceはこの小さいゲームの必須条件ではありません。Sandboxの訪問回数表示だけが任意のGameInstanceを参照しています。初期化が不要なSceneへ空のOnInitialize、後始末が不要なSceneへ空のOnDeinitializeを追加する必要もありません。

## 確認範囲

Starterの空起動試験、Sandbox本体を使うApplication試験、実DxLibのNativeSandboxDeviceSmokeを区別しています。入力を固定した実描画試験は開発用構成だけに含め、通常のソリューションには追加しません。画面の保存とゴール帯の画素確認を行います。OSからの物理キー入力と音を耳で聞く確認は自動試験の対象外です。

今回の根拠と残る改善観点は[利用経路の検証記録](Development/SmallGame-2026-09-23.md)を参照してください。
