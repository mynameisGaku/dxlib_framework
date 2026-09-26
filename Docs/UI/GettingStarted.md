# 保持型UI — 中断版からの継続実装

この文書は、アップロードされた `17445ee` のUIを引き継いだ更新について説明します。
**Windows／実DxLibでのビルドと画素検証は、この更新の作成環境では未実施です。** LinuxのCPU検証・手書きNative境界と区別してください。全体の状態は [Progress.md](Progress.md)、実行結果は [検証記録](../Development/UiResume-2026-09-26.md) を参照してください。

## 使う層

| 目的 | リンク先 | 主な入口 |
|---|---|---|
| 要素・配置・入力・スタイル・命令生成だけ | `dxf::ui` | `FUiRoot`、`DUiElement`、各部品 |
| SceneへUI入力を接続する | `dxf::ui_runtime` | `FUiSceneHost` |
| 実DxLibへ描く | 上記と`dxf::native` | 既存の`FRenderContext`へ命令を送る |

`dxf::ui`は`dxf::support`へ依存し、Physics・Gameplay・Runtimeは要求しません。Scene接続は別ライブラリです。ボタン、レイアウト、入力、データ接続は2D／3D共通で、表示面と座標変換だけを分けます。

## 小さい画面を作る

UIを所有するクラスでは、文字計測サービス、Root、購読スコープ、Hostの寿命を保持します。以下は初期化時に行う処理の抜粋です。全体のクラスは `Examples/UiSample/UiTitleScreen.h/.cpp` と `UiSampleShell.h/.cpp` にあります。

```cpp
#include "Dxf/UiRoot.h"
#include "Dxf/UiButton.h"
#include "Dxf/UiSceneHost.h"
#include "Dxf/UiAssetTextService.h"

// AssetsはこのUIが使われる間、生存しているものを借用する。
Dxf::FUiAssetTextService Text(Assets);
Dxf::FUiRootSettings Settings;
Settings.Text = &Text;
Dxf::FUiRoot Root(Settings);
Dxf::FUiScope Scope;
Dxf::FUiSceneHost Host;

auto Button = Root.Create<Dxf::DUiButton>("開始");
Button.Get()->SetWidth(Dxf::FUiLength::Fixed(240));
Button.Get()->SetHeight(Dxf::FUiLength::Fixed(48));
auto Added = Root.AddToLayer(Dxf::EUiLayer::Normal, Button.Cast<Dxf::DUiElement>());
if (!Added)
{
    throw Toolbox::FException(Added.Error().Message);
}
Scope.Add(Button.Get()->OnClicked().Subscribe([]()
{
    // ゲーム固有の開始要求を設定する。Sceneの差替えは既存Navigatorの境界で行う。
}));
Host.AddScreen(Root);
Host.AttachTo(Scene);
```

この例のローカル変数を初期化関数から帰るときに破棄してはいけません。実際の画面ではクラスのメンバーとして保持します。`Host.Draw(Render)`をSceneの描画から呼びます。HostのScene接続により、UIの入力はゲームの更新より先に処理されます。UIを使うために、ゲーム側の各OnTickへ「UI上なら処理しない」を増やす必要はありません。

Rootは要素の記憶領域を一括所有します。木からDetachしただけでは要素を破棄しません。不要になった要素は`Root.Destroy()`または親の破棄で回収します。型付き`TUiRef`は破棄要求の時点から解決できず、メモリの解放は安全な境界へ遅延します。Root／Scene／Hostは相互に寿命を延長しません。文字サービスと借用したAssetServiceは使用中に破棄しないでください。

## 部品

- `DUiPanel`、`DUiStack`、`DUiSpacer`：縦・横・重ね合わせ、余白、gap、固定／内容／Fill、整列。
- `DUiLabel`、`DUiImage`：文字折返し・省略と画像のContain／Cover／Stretch。
- `DUiButton`、`DUiToggle`、`DUiChoice`、`DUiSlider`、`DUiProgressBar`：操作と表示の分離。
- `DUiScrollView`：縦方向のスクロール、つまみのキャプチャ、子が処理しなかったホイール量の親への配送。
- `DUiListView`：固定行高、安定した非ゼロキー、可視行と前後の余裕だけの再利用。
- `DUiPopup`とRootのModal／Tooltip：最前面、フォーカス復元、背後の操作抑止。

`SetValue`、`SetSelectedKey`等によるゲーム側からの表示反映では、操作イベントを発行しません。利用者のクリック・方向操作で値が変わったときだけ通知します。`DUiSlider`は横方向、`DUiScrollView`は縦方向です。スクロール慣性は今回は実装していません。

`DUiListView::SetItems`はデータそのものを値として所有します。「表示要素を仮想化する」ことと「データを逐次読み込みする」ことは別です。1万項目でも常設する行部品は可視範囲分ですが、項目データは1万件保持します。空のキーや重複キーは失敗します。選択は行番号ではなくキーを使います。

## データとの接続

`TUiProperty<T>`は同じ値の代入では通知せず、`Subscribe`時に現在値を自動送信しません。初回表示と接続期間を一緒に扱う場合は、部品の`OnAttach`から次を使います。

```cpp
Dxf::BindUiProperty(GetRef<Dxf::DUiLabel>(), Model.Status,
    [](Dxf::DUiLabel& Label, const Toolbox::FString& Value)
    {
        Label.SetText(Value);
    });
```

`BindUiProperty`は現在値を一度反映し、変更購読を対象のAttachスコープへ登録します。Detach時に購読を解除し、再Attach時にはその時点の現在値を反映します。操作をゲームへ戻す通知は別に登録してください。汎用の双方向バインディングや任意スレッドからのUI変更は提供しません。別スレッドからは既存のRoot Post窓口等で所有スレッドへ渡します。

## 入力

入力取得は従来どおり1フレームに1回です。HostはRaw入力を事実として保持し、UIが処理した操作を除いた`FInputSnapshot`を値で返します。Sceneと子の更新はこの入力を使います。

押下をUIが取得したキー・ボタンは、UIを閉じた後も物理的に離すまでゲーム側へ新規押下として流しません。Modal中のスティックは中立へ戻るまでゲーム移動へ戻しません。ルート単位のポインターキャプチャはOSのウィンドウ外キャプチャではありません。3Dパネルの外、または設定したViewの外に出た場合、計算可能な座標を使用し、線分を作れない間は最後の座標を保持して誤った原点へのドラッグを防ぎます。

ポーズの切替は、既存SceneClockが次に時間をサンプルする境界から反映され、切替フレームの時間を遡って変更しません。

Host更新はフレーム番号で一度だけです。同じRootを複数表示しても入力・時間は増えません。描画は表示先ごとに行います。寸法が異なる表示先へ同じRootを出す場合は、表示先ごとにその寸法のレイアウトを計算し直します。単一の寸法で同じ部品を描く場合と、独立状態の二つのRootを使う場合を区別してください。

## 画面・Viewport・ワールド

`AddScreen`、`AddViewport`、`AddWorldPanel2D`、`AddWorldPanel3D`が同じRootを表示します。2Dワールドのパネルは拡大・平行移動・Y向きを明示した変換です。3Dは有限の平行四辺形と、指定Viewからの近接面〜遠方面の線分を使います。

3Dの入力に使うView集合は、入力処理前に`Host.SetWorldViews3D(Views)`で設定します。同じ`View.Id`でも異なるカメラ／矩形を保持します。描画から前フレームの最後のカメラを推測して入力へ流用しません。

3Dの描画手順は次です。

1. `Host.RenderWorldPanelTextures(Render)`でUIを中間テクスチャへ描く。呼出し前のRenderTargetへ復帰する。
2. 各Viewのゲーム描画と`Host.DrawWorldPanels3D(Render, View)`を送る。
3. `Host.Draw(Render)`で画面・Viewportの2D UIを重ねる。

**今回の3D平面の中間画像は不透明背景です。** `FUiWorldPanel3D::Background.A`は255を要求します。部品の不透明度はその背景へ合成しますが、平面全体を画素単位の半透明画像として後方の3Dモデルへ合成する機能は未対応です。既存のクリア／アルファ契約へ、未検証の透明RenderTargetの規約を暗黙に追加しないための制限です。

平面の描画深度と入力の遮蔽は別です。`Panel.IsOccluded`を設定しない場合、他の物体の後ろに隠れた平面をクリックしない保証はありません。UIはPhysicsへ依存せず、必要なゲーム側の判定を借用します。

## スタイルの明示再読込

既定の型付きスタイルだけでも動きます。外部資源の構文は版1の小さな形式で、CSS／USS互換ではありません。

```text
 dxfui-style 1
 token Accent = #3388cc
 style Button {
     background = @Accent
     hover.background = #55aaff
     font-size = 20
 }
```

実ファイルでは先頭の空白は不要です。各キーは`=`で区切り、CSSの`;`は使いません。`Tools/MergeUiStyles.py`は部品別`.dxfui`を決定的な順で集約し、元ファイルを変更しません。

```powershell
python Tools/MergeUiStyles.py Examples/UiSample/Styles Build/UiStyles.bundle.dxfui
```

`FUiStyleResource::Attach(Root)`と`ReloadFile`で、すべての対象へ同じ有効版を反映します。解析・確保の失敗では前の版とrevisionを保持します。これは**明示再読込**で、自動監視やUnityのホットリロードではありません。UIの構造・フォーカス・スクロールは作り直しません。ゲーム実行にPythonや監視プロセスは不要です。

## 表示崩れの検査

レイアウト後に`InspectUiLayout(Root, &DrawList)`を呼びます。TextFit、ClipFit、LayoutFit、Overlap、ImageResolution、未知のスタイル・不正レイアウトを値で報告します。描画を勝手に再実行せず、採取済みの命令を借用します。

要素の`DeclareCheckAllowance`には理由を記入します。意図したOverlayやスクロール内容だけを除外し、検査全体を止めません。ClipFitは現在、DrawList全体の項目番号とRootを報告します（個々の描画命令に要素IDを保存する機構は未対応）。文字計測サービスがテスト用なら、その結果は実フォントの保証ではありません。CPU検査の成功と実画素の成功も別です。

## サンプルと試験

開発用の`UISample`は共通のTitleScreen、PausePanel、SettingsPanel、PlayerHud、ItemBrowser、WorldControlsを組み合わせ、Sceneは生成・接続・遷移を担当します。2D／3Dのキャラクター・地形は既存GameplaySampleを再利用します。元のStarterとSandboxは変更していません。

Linuxで実行した`UiApplication`は実Application・SceneNavigator・2D／3D Physics・移動Componentを使いますが、描画／文字／音声の外部境界は手書き代替です。`NativeUiSmoke`は実DxLib用の別入口として追加しましたが、この環境ではSDKビルド・実行を行っていません。
