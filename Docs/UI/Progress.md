# UI基盤の状態 — 2026-09-26

対象はアップロードされた`17445ee`を引き継いだ更新です。**U1〜U8の全完了ではありません。** 実装、CPU実行、Native実行、配布を分けます。

| 段階 | 今回の状態 | 未完了・保証しないこと |
|---|---|---|
| U1 木・型付き参照・寿命・購読 | 既存実装を保持し、再入・Root／Hostの破棄・例外時回収・購読の通知順を修正。CPU／ASan対象 | 任意スレッドで直接UIを操作する安全性 |
| U2 レイアウト・文字・スタイル・倍率 | 既存実装を維持。静止木・可視行準備・Surfaceの桁あふれ等を確認 | 本更新での実DxLibフォント、OS DPI／リサイズ操作 |
| U3 入力・フォーカス・キャプチャ | 入力前のレイアウト、heldキー／スティック所有、複数表示先、寿命を修正。2D／3D Host試験 | 実物理入力、OSウィンドウ外のCapture |
| U4 コントロール | Button等の未登録cppを接続。Toggle、Choice、Slider、ProgressBar、Image、ScrollView、ListView、Popupを追加 | 横ScrollView、スクロール慣性、高度なテキスト編集 |
| U5 クリップと表示先 | 画面／Viewport／2Dパネル／3D平面、複数Root表示、復帰処理。手書きNative境界も検査 | 実GPU画素、3Dパネルの画素単位半透明合成、実D3D9 |
| U6 データ・スタイル資源 | 一方向BindUiProperty、集約ツール、明示再読込と失敗時保持 | 自動監視、CSS／USS互換、任意の非同期リアクティブ処理 |
| U7 ゲーム側利用 | クラス分割したUISampleと、2D／3D実App＋PhysicsのCPU統合試験 | WindowsのUISample起動、NativeUiSmoke実行・画素確認 |
| U8 検査・測定・配布 | CPU表示検査、Release測定、UiOnly／UiRuntime外部ConsumerとNative OFF配布 | Native ON Debug／ReleaseのUI実行と配布、全環境成功の主張 |

Root全群の既存パス試験2群は、このLinux環境では更新前から失敗しています。UI関連試験だけの成功をRoot全群成功と表記しません。

Native ONの既存配布経路は残していますが、追加したUI外部ConsumerはUI単独とSceneアダプターのCPU試験です。既存NativeAppの成功を、UI入り外部NativeApplicationの成功へ読み替えません。これは次のWindows検証で追加・実行する残作業です。

過去のNativeModelSmokeの偶発終了、SDK未導入PC、公式VC SDK経路、実D3D9、物理入力・聴感、D3D／COM全資源のリーク列挙は、今回のCPU成功によって解決済みになりません。
