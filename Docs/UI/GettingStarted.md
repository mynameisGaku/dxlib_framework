# 保持型UI — 使い方

2D／3D共通の保持型UIの使い方と契約です。Windows／実DxLibでのビルド・画素検査・配布の結果は [検証記録](../Development/UiWindowsCompletion-2026-09-26.md)、機能ごとの状態は [Progress.md](Progress.md) を参照してください。以前のLinux環境での記録は [UiResume-2026-09-26](../Development/UiResume-2026-09-26.md) です。

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
- `DUiLabel`、`DUiImage`：文字折返し・省略と画像のContain／Cover／Stretch。画像はスタイルの`foreground`を色の乗算に使い、組込みの`Image`スタイルは白（元画像の色のまま）です。
- `DUiButton`、`DUiToggle`、`DUiChoice`、`DUiSlider`、`DUiProgressBar`：操作と表示の分離。
- `DUiScrollView`：縦・横・両方向（`SetScrollAxes`）のスクロール、つまみのキャプチャ、軸ごとに使いきれなかったホイール量の親への配送、任意の慣性（`SetInertia`）。
- `DUiListView`：固定行高、安定した非ゼロキー、可視行と前後の余裕だけの再利用。
- `DUiPopup`とRootのModal／Tooltip：最前面、フォーカス復元、背後の操作抑止。

`SetValue`、`SetSelectedKey`等によるゲーム側からの表示反映では、操作イベントを発行しません。利用者のクリック・方向操作で値が変わったときだけ通知します。`DUiSlider`は横方向です。

### スクロールの軸と慣性

`DUiScrollView::SetScrollAxes(EUiScrollAxes::Vertical／Horizontal／Both)`で軸を選びます（既定は縦）。スクロールする軸だけにバーを置き、その軸は内容を上限なしで測ります。縦のホイールは縦、Shiftを押したホイール（`FUiPointerFrame::WheelNotchesX`）は横へ使い、使いきれなかった分は軸ごとに外側のスクロールへ渡します。方向操作は、スクロールする軸の向きだけを使い、他の向きはフォーカス移動へ残します。

慣性は既定で無効です。`FUiScrollInertia{true, DecayPerSecond, SnapDistance}`を設定すると、ホイール・方向操作の移動量は変えずに、到着点までの残りを経過時間だけで`e^(-DecayPerSecond×t)`へ減らします。30／60／144Hzのどれで更新しても同じ時刻の位置は同じで、残りが`SnapDistance`未満になると到着点へ正確に合わせます。`SetScrollOffset`／`SetScrollOffsetX`・つまみのドラッグは残りの移動を捨てます。移動中だけ要素が毎フレームの更新を受けます。

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

押下をUIが取得したキー・ボタンは、UIを閉じた後も物理的に離すまでゲーム側へ新規押下として流しません。押下中にボタンを非表示・破棄・表示先から外しても、離すまで所有を保ちます。Modalを開くとゲームが押し続けていたキーは解放として見え、押したままModalを閉じても新しい押下にはなりません。パッドを切断すると、そのパッドの所有と繰返しは消えます。Modalの外側の押下は、Modalの背景が画面を覆わない場合も背後の要素へ抜けません。Modal中のスティックは中立へ戻るまでゲーム移動へ戻しません。ルート単位のポインターキャプチャはOSのウィンドウ外キャプチャではありません。3Dパネルの外、または設定したViewの外に出た場合、計算可能な座標を使用し、線分を作れない間は最後の座標を保持して誤った原点へのドラッグを防ぎます。

ポーズの切替は、既存SceneClockが次に時間をサンプルする境界から反映され、切替フレームの時間を遡って変更しません。

Host更新はフレーム番号で一度だけです。同じRootを複数表示しても入力・時間は増えません。描画は表示先ごとに行います。寸法が異なる表示先へ同じRootを出す場合は、表示先ごとにその寸法のレイアウトを計算し直します。単一の寸法で同じ部品を描く場合と、独立状態の二つのRootを使う場合を区別してください。

## 画面・Viewport・ワールド

`AddScreen`、`AddViewport`、`AddWorldPanel2D`、`AddWorldPanel3D`が同じRootを表示します。2Dワールドのパネルは拡大・平行移動・Y向きを明示した変換です。3Dは有限の平行四辺形と、指定Viewからの近接面〜遠方面の線分を使います。

3Dの入力に使うView集合は、入力処理前に`Host.SetWorldViews3D(Views)`で設定します。同じ`View.Id`でも異なるカメラ／矩形を保持します。描画から前フレームの最後のカメラを推測して入力へ流用しません。

3Dの描画手順は次です。

1. `Host.RenderWorldPanelTextures(Render)`でUIを中間テクスチャへ描く。呼出し前のRenderTargetへ復帰する。
2. 各Viewのゲーム描画と`Host.DrawWorldPanels3D(Render, View)`を送る。
3. `Host.Draw(Render)`で画面・Viewportの2D UIを重ねる。

`DrawWorldPanels3D(Render, View)`の前に、同じViewを`Render.Get3D().SetView(View)`で設定してください（異なるViewでは失敗します）。パネルはそのViewの区間へ、同じ深度で描かれるため、手前の不透明な物体に正しく隠れます。

### 3D平面の合成

`FUiWorldPanel3D::Composition`で選びます。

- `EUiPanelComposition::Opaque`（既定）：`Background`（A=255が必要）の上へ描き、平面全体を不透明に貼ります。
- `EUiPanelComposition::Transparent`：アルファ付きの中間画像を(0,0,0,0)で消去し、**乗算済みアルファ**で描いて、画素ごとの透明度`C = a×F + (1−a)×B`で背後と合成します。α=0の画素は深度を書かず、後から描く物体も隠しません。半透明の画素は、同じViewで先に描いた物体とだけ合成します（パネルは同じViewの物体の後に描き、透明なパネル同士は視点から遠い順に描かれます。交差する透明な形状の厳密な合成は対象外）。表示面の文字は乗算済みの字体、画像は`FTextureLoadOptions::bPremultipliedAlpha`で読んだ画像に限ります。乗算前の字体・画像を乗算済みの合成へ送ると、描画キューが受け付けずに失敗します（黒い縁や二重のαを黙って描かない）。入力は平面の範囲で受け、透明な画素のクリック透過はしません。

2Dワールドのパネルは画面へ直接描くため、中間画像の合成の選択はありません。テクスチャ付き四角形を描けないBackendでは3Dパネルの描画が失敗します。乗算済みの合成への対応を別に問い合わせる窓口はありません。

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

## 描画命令の寿命

描画キューが受け付けた命令は、受付時点の文字・資源を所有します。文字は`FSharedText`（変更しない文字列の共有の所有）で、ラベルの配置の各行を配置時に一度だけ作り、毎フレームの命令は参照数の増加だけで共有します。受付後・実行前に`SetText`や要素の破棄が起きても、その命令は受付時点の文字で描かれ、次のフレームから新しい文字になります。`DrawText`へ`FString`や文字列リテラルを渡すと、その時だけ共有の所有を一度作ります。2Dキューは実行後に命令を破棄し、容量だけを次のフレームへ使い回します。

## 表示崩れの検査

レイアウト後に`InspectUiLayout(Root, &DrawList)`を呼びます。TextFit、ClipFit、LayoutFit、Overlap、ImageResolution、未知のスタイル・不正レイアウトを値で報告します。描画を勝手に再実行せず、採取済みの命令を借用します。

各問題は、要素の番号（`ElementId`、Root内で再使用しない）、世代付きの参照の番号（`Source`、破棄・再利用後はRootで解決できない）、木の位置（`Path`）、期待と実際の矩形、理由、深刻度を持ちます。結果は検査時の表示面（画素範囲・倍率・論理寸法）と表示先の番号も持ちます。ClipFitは各描画命令のクリップを、命令を出した要素の期待するクリップ（表示面・表示先のクリップ・切り抜く祖先の矩形の共通部分を木から独立に求めたもの）と比べ、越えた命令を描画項目の添字付きのエラーとして報告します。出所の要素がない命令も報告します。

Host経由の表示先は`Host.InspectDisplay(DisplayId)`で検査します。その表示先の表示面で配置し、描画と同じ命令と追加のクリップ（2Dワールドのパネルの画面の切り抜き等）を作って検査し、描画はしません。同じRootを倍率の異なる表示先へ出す場合も、表示先ごとに検査してください。

要素の`DeclareCheckAllowance`には理由を記入します。意図したOverlayやスクロール内容だけを除外し、検査全体を止めません。文字計測サービスがテスト用なら、その結果は実フォントの保証ではありません。CPU検査の成功と実画素の成功も別です。

## サンプルと試験

開発用の`UISample`は共通のTitleScreen、PausePanel、ConfirmPanel、SettingsPanel、PlayerHud、ItemBrowser、WorldControlsを組み合わせ、Sceneは生成・接続・遷移を担当します。2D／3Dのキャラクター・地形は既存GameplaySampleを再利用します。元のStarterとSandboxは変更していません。

`UiApplication`は実Application・SceneNavigator・2D／3D Physics・移動Componentを使い、描画／文字／音声の外部境界は代替です。タイトルから2D／3Dの操作、一時停止、設定（実際のSlider・Toggle）、一覧、確認Popup、タイトルへ戻るまでと、一時停止を含む軌跡の比較を行います。`NativeUiSmoke`（CTest名`NativeUiDeviceSmoke`）は同じ流れと表示先ごとの実画素を実DxLibで確認します。再配置したパッケージだけを使う外部の例は`Tools/PackageConsumer/NativeUiApp.cpp`です。
