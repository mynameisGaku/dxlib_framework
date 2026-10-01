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
