# ここからゲームを作り始める

`Starter`は空のウィンドウと最初のシーンだけを用意した開始用プロジェクトです。画像・音声・デモ用オブジェクトは必要ありません。

1. リポジトリのルートで`Setup.cmd`を実行し、DxLib SDKを用意します（初回のみ）。
2. `GenerateProjectFiles.bat`を実行し、ルートの`dxlib_framework.slnx`または`.sln`を開きます。
3. `Starter`をスタートアッププロジェクトに設定し、Debug／x64で実行します。初回生成時の既定も`Starter`です。
4. `Source/BootScene.cpp`に自分のゲームを書き始めます。終了はウィンドウの閉じるボタンです。

| ファイル | 書く内容 |
|---|---|
| `Source/BootScene.h` | シーンが持つ変数や関数の宣言 |
| `Source/BootScene.cpp` | 初期化・更新・描画・終了処理 |
| `Source/WindowsMain.cpp` | ウィンドウの名前・大きさ・背景色、最初のシーン |

フックの引数を使うときは、cpp側でもヘッダーと同じ`Context`という引数名を付けてください。`OnInitialize`は準備が成功したら`{}`、失敗したら`Dxf::TResult<void>::Failure(...)`を返します。`OnTick`では`Context.Input`と`Context.Time`、`OnDraw`では描画用の`Context`を利用します。詳しくは[APIの使い方](../../Docs/API.md)を参照してください。

シーンを追加する場合は同じフォルダーへヘッダーとcppを置き、ルートの`CMakeLists.txt`にある`add_executable(Starter ...)`へcppを追加して再生成します。ゲームのコードはこの`Source`内に置き、フレームワークはルートの`Source`内で管理します。

ゲーム制作には`Starter`、機能の使用例を見たいときは`Sandbox`を選びます。どちらも同じフレームワークを利用します。このフォルダーはリポジトリと一緒に使う構成です。`-Portable`ではDxLibを使う実行プロジェクトは生成しません。

公開APIを組み合わせた実例は[小さいゲームを組む](../../Docs/SmallGame.md)とSandboxを参照してください。Starter自体は画像・音声の準備が不要な空の開始点を保ちます。
