# 入力プロファイルとユーザー設定

任意リンクの `dxf::input_profiles` は既存 `FInputMap` へ完全な割当を適用し、入力・音量・UI倍率を設定文書へ変換する。SceneContent、Physics、UI、Nativeを要求しない。OS入力は既存Snapshotを使う。設定値の永続化とActionの操作を外部Consumerの別プロセスで検証した。GameplaySampleの設定画面、実音量・UI倍率への反映はL作業で接続中である。

## 全体の検証と適用

`FInputProfilePolicy` にゲームが許可するAction名と予約入力を明示する。完全プロファイルには許可Actionを一度ずつ含める。空のBindingsは明示的な未割当である。不明Action、同一Actionの重複、同一入力の重複、異なるAction間の入力競合、予約入力を全体拒否する。勝手に他Actionを解除しない。

Action名は大文字小文字を区別する `[A-Za-z_][A-Za-z0-9_-]*`、最大128bytes。既定上限は128Action、割当総数512、一つのActionに64、文書256KiB、JSON深度8、文字列128bytes、値4096。Policyで件数・バイト上限を下げられる。未知field、復号後の重複key、不正UTF-8、末尾データ、非有限数値は拒否する。

`FInputProfileCodec::Apply` は全検証後に候補Mapを作り、必要確保が完了してから例外を出さない移動で採用する。検証・確保失敗時は旧MapとAction履歴を保持する。現MapをClearしてから構築しない。

```cpp
Dxf::FInputProfilePolicy Policy;
Policy.AllowedActions = {"Jump"};
Dxf::FInputBindingDescription Jump;
Jump.Key = Dxf::EKey::K;
Dxf::FInputProfile Profile;
Profile.Actions = {{"Jump", {Jump}}};
Dxf::FInputProfileCodec::Apply(Profile, Policy, Map, OriginalSnapshot);
```

`Map` はゲームが所有する既存Map、`OriginalSnapshot` はUI消費前のSnapshotである。完全なコンパイル・実行例は [InputProfileOnly](../../Tools/PackageConsumer/InputProfile.cpp)。

## 保存名とバージョン

形式は `profileVersion: 1` と `actions`。Actionは `id` と `bindings` を持つ。装置は `Keyboard`、`Mouse`、`Gamepad`。キーボード名は既存 `EKey` と明示的に対応する `A`〜`Z`、`Num0`〜`Num9`、`Space`、`Escape`、`Enter`、`Tab`、`Backspace`、矢印、左右Shift／Control、`F1`〜`F12`。マウス名は `Left`／`Right`／`Middle`。Padはslot整数0〜3と `Button1`〜`Button16`（Snapshotの番号0〜15へ対応）を使う。enumの生の数値やWindowsのABI値を保存しない。

```json
{"actions":[{"bindings":[{"device":"Keyboard","key":"K"}],"id":"Jump"}],"profileVersion":1}
```

整数項目は小数・指数表記を許さない。未知バージョンは `UnsupportedVersion`。WriterはAction名と割当表現を正準順序に並べ、共有元を変更しない。このJSON例は手作りの期待バイト列を使うcodec試験に対応する。

## 押し続けの抑止と再割当

一括適用と `FInputMap::SuppressUntilRelease` はAction履歴をリセットし、既に押されていた割当を解放まで抑止する。Pad切断・失焦は解放観察に数えず、接続復帰後の解放を待つ。既存Trackerの復帰時抑止とも併用する。Modal終了時も元Snapshotを渡す。UIで消費してfalseにしたSnapshotだけで抑止を決めない。

`FInputRebindCapture` は指定装置の全ボタンを一度解放してから、最初の単一の新しい押下を採取する。開始クリックを採用しない。失焦・指定Pad切断・複数同時押下・明示取消しで `Canceled` へ終端する。`Ready` は採取の完了だけで、Policy検査・適用・保存は別である。採取待ちの取消しは完成済みの値や設定を巻き戻さない。

Mapの `Update` は新しいSnapshotごとに一回。採取の `Poll`、描画View、保存Poll、固定更新数を理由に追加Updateしない。固定更新への押下配送は既存の保留入力経路へ接続する。新しいプロファイルを使わない従来のBind運用は変更しない。

## 設定と進行の分離

`FUserSettings` は `SettingsVersion`、完全なInput、有限なVolume 0〜1、有限なUiScale 0.25〜8を持つ。設定バージョン、入力プロファイルのバージョン、ゲーム側の互換バージョンは別である。

`FUserSettingsCodec::Encode` はSettings種類・次元0の `FSaveDocument` を返し、Content／Scene／checkpointキーを空にする。設定は `settings` 等の専用系列へ `FSaveStore` で保存する。進行のmanual／autosaveとは別系列で、古い進行のLoadで現在の設定を戻さない。Decodeは現在のMap・音声・UIへ副作用を出さず、全検証後に値を返す。壊れた設定やゲーム不一致では現在の値を維持できる。

Volumeは元の再生音量へ一度だけ掛け、既存Playbackへの `SetVolume` と次回PlayのOptionsへ反映する。UiScaleは既存表示先のOptionsへ渡し、描画とhit testを同じ座標変換にする。この反映処理は現在Sampleへ接続中である。設定を適用したことと保存公開の成功を区別し、書込失敗を保存済み表示にしない。

現在の確認範囲は [L作業の試行記録](../Development/GameSaveCompletion-2026-10-02.md)。別プロセスConsumerで値の永続化と実Action操作を確認したが、実音量・画面倍率・人の入力・聴感の確認結果ではない。
