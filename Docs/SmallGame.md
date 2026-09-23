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

元のStarterは空のまま残し、**リポジトリ全体のソース構成を保った作業用コピー**で行います。Starterフォルダーだけのコピーでは、共有の起動ヘッダー・フレームワーク・Assets・ルートCMakeが不足します。既存のコピー先を上書きせず、新しいディレクトリを使ってください。`.git`、Build、ThirdParty、旧Archiveは移植用ソースに含めません。

1. コピー側の`Examples/Starter/Source`へ、同じコピーの`Examples/Sandbox`から`SandboxMenuScene.h/.cpp`と`SandboxGame.h/.cpp`を配置します。
2. `Examples/Starter/Source/WindowsMain.cpp`に`#include "SandboxMenuScene.h"`を加え、Runnerへ渡す`Toolbox::MakeUnique<Starter::ABootScene>()`を`Toolbox::MakeUnique<Dxf::Sandbox::ASandboxMenuScene>()`へ変更します。ProjectRootの既存行は変更しません。
3. コピー側のルート`CMakeLists.txt`でStarterの登録を次へ変更します。元のSandboxのcppを直接登録せず、コピーしたStarter側を指定します。

```cmake
add_executable(Starter WIN32
    Examples/Starter/Source/WindowsMain.cpp
    Examples/Starter/Source/BootScene.cpp
    Examples/Starter/Source/SandboxMenuScene.cpp
    Examples/Starter/Source/SandboxGame.cpp)
```

4. 同じPCの既存ソース版DxLibを使う場合、コピー側ルートで以下を実行します。`C:/Existing/dxlib_framework/ThirdParty/DxLib-3.25a-source`は実際の既存SDKパスに置き換えます。新しいBuildディレクトリを指定し、各工程の終了コード0を確認してから次へ進んでください。

```bat
cmake -S . -B Build/Starter -A x64 -DDXF_BUILD_NATIVE=ON -DDXF_BUILD_STARTER=ON -DDXF_BUILD_EXAMPLE=OFF -DDXF_BUILD_TESTS=OFF -DDXF_BUILD_NATIVE_SMOKE=OFF -DDXF_DXLIB_CUSTOM_ROOT=C:/Existing/dxlib_framework/ThirdParty/DxLib-3.25a-source
cmake --build Build/Starter --config Debug --target Starter
cmake --build Build/Starter --config Release --target Starter
Build\Starter\Debug\Starter.exe
Build\Starter\Release\Starter.exe
```

このCMake経路のソリューションは`Build/Starter`に生成されます。アセットの起点はコピー側のルートです。通常のルートソリューションを生成する場合は、既存のGenerateProjectFilesを使い、既存SDK利用時は環境変数`DXLIB_ROOT`と`DXF_DXLIB_CUSTOM_ROOT`を適切に設定します。今回の確認で使ったのは上の明示的なCMake経路です。

生成された構成別`Starter.dxfpaths`をexeの横に残してください。これがコピー側のProjectRootを指すため、別の作業ディレクトリから起動してもゲーム側に絶対パスは不要です。同じPCの既存SDKを参照する確認であり、SDK未導入環境の検証ではありません。

独自GameInstanceはこのゲームの必須条件ではありません。Sandboxの訪問回数表示だけが任意のGameInstanceを参照しています。初期化・後始末が不要なSceneへ空のフックを追加する必要もありません。

### 今回の移植確認を再実行する

```bat
python Tools/ValidateStarterCopy.py --work-parent C:/Validation --sdk-root C:/Existing/dxlib_framework/ThirdParty/DxLib-3.25a-source
```

`--work-parent`はリポジトリ外の既存ディレクトリを指定します。スクリプトはそこへ新規コピーを作り、上記の限定編集・通常Starterの両構成ビルドと起動を行います。その後、コピーの入口だけへ`Tools/StarterCopyProbe.h/.cpp`を接続して固定入力で再ビルドします。ゲーム4ファイルと元リポジトリは変更せず、公開API・実DxLibを使用します。恒久的なサンプルや通常ソリューションの検証プロジェクトは追加しません。

固定入力版はコピー側の画像1枚を一時退避し、失敗理由の表示、同じApplicationでの復旧、ポーズ中の移動停止、2秒のポーズ有無による結果時間の画素一致、リトライ、終了を確認します。各構成をコピーのルートと無関係な作業ディレクトリから起動します。退避した画像は失敗時にも復元します。画像・工程別終了コード・実行ファイルのハッシュ・Starterへの登録ソース・元ソースのハッシュはコピー内の`validation`へ保存します。

検証の最後にコピー側の入口とCMake登録を通常版へ戻し、両構成を再ビルドします。残った`Starter.exe`は普通に遊べるゲームです。固定入力版は同じフォルダーの`Starter-probe.exe`に分けて保存します（再試験時はスクリプトを使って画像欠落の条件も準備してください）。

実行先を再利用せず、新しいコピーを作る検証専用スクリプトです。SDKや開発ツールを導入していないPC、物理キー操作、音声の聴感の代替にはなりません。

## 確認範囲

Starterの空起動試験、Sandbox本体を使うApplication試験、実DxLibのNativeSandboxDeviceSmokeを区別しています。入力を固定した実描画試験は開発用構成だけに含め、通常のソリューションには追加しません。画面の保存とゴール帯の画素確認を行います。OSからの物理キー入力と音を耳で聞く確認は自動試験の対象外です。

今回の根拠と残る改善観点は[利用経路の検証記録](Development/SmallGame-2026-09-23.md)を参照してください。

Starter移植先での今回の結果は[別ディレクトリでの検証記録](Development/StarterCopy-2026-09-23.md)を参照してください。過去の記録の未実施項目は書き換えていません。
