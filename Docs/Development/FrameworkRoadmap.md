# フレームワーク全体の次の棚卸し

内部の責務を分け、利用側はゲーム固有の処理へ集中する。今回の仕掛け拡張の後に別機能を自動実装しない。

|分野|確認した現在仕様|次の候補・未確認|
|---|---|---|
|Application/Scene/Component|既存所有・固定更新・Scene終了を新装置へ適用。常設Manager追加なし|実ゲームで重複準備が生じた箇所だけ改善|
|描画/モデル/アニメーション|ufbx、骨、CPUモーフ、UV1、頂点色、基本PBR、静止カメラ/ライト、2Viewportを保持。[FBX仕様](../FbxSupport.md)|GPUモーフ、追加材質マップ、影、カメラ/ライトのアニメーションは別候補。実制作データの不足から選ぶ|
|アセット|ProjectRoot/.dxfpaths、LoadTexture/Model/Soundの既存資源経路を保持。[パス仕様](../Assets/Paths.md)|更新監視や任意制作形式の保証は今回追加なし|
|音声|FSoundと独立PlaybackHandle、Scene AudioScopeの既存公開APIを保持。[API](../API.md)|今回変更なし。聴感・物理操作は今回の自動試験へ含めない|
|UI/入力/Window|Button/Slider/Toggle/Labelで装置操作。Modalと閉じたフレームの抑止、1/2表示を保持|UI基盤の再設計なし。3D UIパネル確保は従来の残課題|
|Physics/仕掛け|新3種類・両次元、有限Motor/Limit、6Componentと目標速度操作|Spring/Soft、Rope/Gear/Wheel、Joint破断、Joint TOIは未実装。今回終了後に自動追加しない|
|配布/導入|公開target、再配置Consumer、4構成を検証する範囲|同PCの既存ソースSDK利用。SDKなしPC・開発ツールなしPCは未実施|

新しい検証点と完了/未実施の状態は[今回記録](MechanismJointsCompletion-2026-10-01.md)で判定する。過去記録は当時の結果として保持する。

## Scene／Prefabのデータ駆動単位

任意SceneContent層で、両次元の型付き初期構成、bounded JSON、個体別parameterとexport、既存Component生成、AssetService準備、Scoped要求・取消し・再読み込み、既存GameplaySampleとUIを接続しています。実装の存在と最終検証の採用範囲は[Scene／Prefabの試行別記録](ScenePrefabContentCompletion-2026-10-02.md)で区別します。

今回提供しないものは実行状態のsave、任意C++型のserialization、スクリプト、編集GUI、自動監視、親Transform追従です。次の候補はアニメーション制御のゲーム利用、入力設定／セーブ、必要な描画拡張です。候補の記録だけで実装を開始しません。従来NativeModel早期終了原因、3D UIパネル確保、Dynamic床追従、Joint連続拘束、SDKなしPC、TSan、全D3D/COMリーク証明も残します。
