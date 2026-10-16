# フレームワーク全体の次の棚卸し

内部の責務を分け、利用側はゲーム固有の処理へ集中する。各拡張を引き渡した後に別機能を自動実装しない。

|分野|確認した現在仕様|次の候補・未確認|
|---|---|---|
|Application/Scene/Component|既存所有・固定更新・Scene終了を新装置へ適用。常設Manager追加なし|実ゲームで重複準備が生じた箇所だけ改善|
|描画/モデル/アニメーション|ufbx、骨、CPUモーフ、UV1、頂点色、基本PBR、静止カメラ/ライト、2Viewportを保持。[FBX仕様](../FbxSupport.md)|GPUモーフ、追加材質マップ、影、カメラ/ライトのアニメーションは別候補。実制作データの不足から選ぶ|
|アセット|ProjectRoot/.dxfpaths、LoadTexture/Model/Soundの既存資源経路を保持。[パス仕様](../Assets/Paths.md)|更新監視や任意制作形式の保証は今回追加なし|
|音声|FSoundと独立PlaybackHandle、Scene AudioScopeを保持。保存した音量を既存再生と次回再生へ適用。[API](../API.md)|新しいミキサーは追加していない。聴感・人の操作は自動試験へ含めない|
|UI/入力/Window|Button/Slider/Toggle/Labelで装置と保存を操作。完全な入力割当、再割当と解放後の有効化、保存したUI倍率、Modalと閉鎖Frameの抑止、1/2表示を保持|UI基盤の再設計なし。3D UIパネル確保は従来の残課題|
|Physics/仕掛け|新3種類・両次元、有限Motor/Limit、6Componentと目標速度操作|Spring/Soft、Rope/Gear/Wheel、Joint破断、Joint TOIは未実装。今回終了後に自動追加しない|
|配布/導入|公開target、再配置Consumer、4構成を検証する範囲|同PCの既存ソースSDK利用。SDKなしPC・開発ツールなしPCは未実施|

新しい検証点と完了/未実施の状態は[今回記録](MechanismJointsCompletion-2026-10-01.md)で判定する。過去記録は当時の結果として保持する。

## Scene／Prefabのデータ駆動単位

任意SceneContent層で、両次元の型付き初期構成、bounded JSON、個体別parameterとexport、既存Component生成、AssetService準備、Scoped要求・取消し・再読み込み、既存GameplaySampleとUIを接続しています。実装の存在と最終検証の採用範囲は[Scene／Prefabの試行別記録](ScenePrefabContentCompletion-2026-10-02.md)で区別します。

SceneContent自身が提供しないものは実行状態の保存、任意C++型のserialization、スクリプト、編集GUI、自動監視、親Transform追従です。次の候補はアニメーション制御のゲーム利用、必要な描画拡張です。論理チェックポイントと入力設定は下記の任意保存層が担当します。候補の記録だけで実装を開始しません。従来NativeModel早期終了原因、3D UIパネル確保、Dynamic床追従、Joint連続拘束、SDKなしPC、TSan、全D3D/COMリーク証明も残します。

## L0〜L10：保存・チェックポイント・入力設定（実装・採用検証済み）

SceneContentの初期構成とは別に、論理チェックポイントとユーザー設定の継続を追加しています。現在は任意SaveDataの厳格な封筒、世代付き二面保存、専用Root、OS排他と回復、GameSaveのScoped CPU要求と明示移行、両次元の型付きチェックポイント・許可カタログからのCPU再開計画を実装しました。内部と個体間の全4Jointを新Worldへ登録する代表試験もあります。成功固定更新後の採取境界、世代付き個体履歴と要求時採取を追加しています。Scene所有の進行と通常Navigator要求、必須Ready後の再開完了、既存の有限目標操作を追加しました。任意InputProfilesでは完全な割当の一括適用、解放後の有効化、再割当の入力採取、独立した設定文書を追加し、保存と別プロセスからの読込・実Action判定を確認しました。ゲーム固有Controller、既存Sample UI、音量・UI倍率の実適用と別Applicationプロセスの両次元再開を接続しました。CPU試験で実Sensor・実BodyとUIクリック、両次元の別Nativeプロセスと保存前・変更後・Continue後の画素を確認しました。I/O・確保故障とL-M01〜20、独立installゲームConsumerの両次元別プロセス／実画素を追加しています。Releaseの16／64／256個体・各5環境の測定、最終配布4構成、Portable／Nativeと独立構成の最終累積を確認しました。定常Poll／入力更新の追加確保0とWorld全体の確保を区別します。大きい構成では採取・codec・所有側資源準備に秒単位の費用があり、今後の棚卸し対象です。[Lの試行記録](GameSaveCompletion-2026-10-02.md)で完成範囲を判定し、Bの結果をLの成功へ転用しません。
