# 0.2から0.3への移行

## 通常のゲームコード

`Run<TScene>()`・`Spawn<T>()`・`AddComponent<T>()`・On系フックのシグネチャは変更していません。ソースを置き換えて全体を再ビルドしてください。0.2の静的ライブラリと0.3のヘッダーを混在させないでください。

インストール済みパッケージを使う場合は、`find_package(dxlib_framework 0.3 CONFIG REQUIRED)`へ更新します。0.2を要求した利用側へ0.3を自動で互換品として提供しない設定です。

## 描画失敗の扱い

以前はNativeコールバックに失敗があっても、状態復元に成功すればEndFrameを成功扱いにできました。0.3では、すでに描かれた画素が不完全な可能性があるため、そのフレームをPresentしません。

Renderの各TResultを確認してください。低レベルにFRenderSystem2Dを所有している利用側は、EndFrameで失敗を受け取るか、CancelFrameで中止してから、必要に応じて新しいBeginFrameを始めます。FApplication経由では描画エラーを受けて終了処理へ進みます。

無効引数による登録拒否や、元のTargetへの復元に成功したTarget切り替え失敗まで、すべてフレーム失敗にするものではありません。

## 終了要求

RequestQuit以降は、同じSceneの残りの子の初期化・更新・描画を進めません。終了要求後に別ObjectのTickが必ず呼ばれることを前提にせず、必要な後始末はOnDeinitializeやRAIIへ置いてください。

Pending Sceneを置き換える時のデストラクタから、さらにSceneを差し替えたりCommitしたりしないでください。この再入はInvalidStateで拒否します。通常のOnTickなどからのRequestChangeは、従来どおり次の境界への要求です。

## 独自の低レベル拡張

ILifecycleGroupを自作している場合、`void RequestStop_Internal() noexcept`を実装する必要があります。役割は自身と子階層を停止予約することだけです。その中でユーザーフックやメモリ破棄を実行せず、従来の安全な境界でShutdown_Internalを実行してください。標準のGameObject／ComponentのCollectionでは実装済みです。

## 検証結果の読み方

`Docs/Validation/Summary.json`と`Docs/Validation/Package/Summary.json`は、開始時にstatusがrunningになり、終了時にpassedまたはfailedになります。run_id・started_utc・finished_utcも確認してください。途中で落ちた検証の横に古いログが残っていても、その古いログを現在の成功として扱わないでください。

実SDK・Windows・実機の確認は、引き続き別のTools\Build.cmd／Tools\Validate.cmdの対象です。この版を受け取っただけで、その確認が完了するわけではありません。
