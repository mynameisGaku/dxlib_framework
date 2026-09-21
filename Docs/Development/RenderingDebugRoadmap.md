# Rendering / Debug の継続課題

起点: `86a37b9cdb7e3641e41f7838a8c5694de801679c`。
この表は実装済みと未実装を混同しないための継続表であり、全機能の完成宣言ではない。

| 領域 | 今回の状態 | 完了に向けた次の検証条件 |
|---|---|---|
| 3D基本形状・CPUメッシュ | 既存実装を利用。Applicationが実際に3Dを実行する所有者へ接続を修復 | Windows実画面で形状・奥行き・2D復元を確認 |
| MV1モデル・スキニング・テクスチャ付きモデル | 未実装 | 実SDKで読込・解放・複製Instance・独立アニメーション・材質・テクスチャ・資源寿命の検証 |
| 照明 | 既存のCPUフラット計算。サンプルにNormal/Unlit/LightsOffと方向光切替 | DxLibの実ライト/Material/影の対応状況を能力別に定義し、実SDKで状態漏れを検証 |
| 分割Viewport・複数ウィンドウ | 未実装。描画先全体、ビュー切替で深度初期化の既存制約を維持 | Viewport/Scissor/RenderTarget/Camera/Depthの独立性。複数ウィンドウはBackend能力調査から開始 |
| 透明3D | **基本形状/CPUメッシュのプリミティブ単位ソートを追加**。不透明→透明→Overlay、透明はTestOnly | Windows実画面、実負荷、テクスチャ/MV1。重心ソートは近似で交差面の厳密な解決・OITは未実装 |
| Nativeの任意の外部状態保存・復元 | 全状態の保証なし | ライブラリが管理する状態を列挙し、その範囲だけ保証。未知の外部状態は明示再設定する契約 |
| 物理Snapshot | **明示登録した3D Colliderだけ**をStep後に採取するアダプターを追加 | World内部の全Collider列挙・Shape改訂・固定更新番号を持つ公開の観察API。2D版も必要 |
| 接触・Impulse・Island・CCD診断の観察 | 未実装 | Solverが実際に用いた値を更新境界で採取。別の接触計算から作った値を実測値と表示しない |
| デバッグUI | キー操作付きRenderDebug説明パネルのみ | 汎用メニュー、検索、選択ID連携、View別の設定保存、カテゴリ管理。今回のパネルを汎用エディター扱いしない |
| カメラ操作 | 独立カメラのOrbit/距離/平行移動、物理停止中も操作可 | 正投影、選択物体Focus、マウスPicking、複数View、ゲームカメラとの切替 |
| GPU計測 | 未実装 | GPU timestamp等の実測経路と遅延結果回収。今回表示するのは最後のlive Physics StepのCPU経過時間だけ |
| 記録タイムライン | 3D観察Snapshotの上限付き履歴と停止中のZ/X選択を追加 | 時間軸UI、可変間隔、ファイル保存、イベントとの関連付け。物理の巻戻し・再実行は別機能 |
| 資源寿命・ユーザーコードのThread安全性 | **継続する利用契約**。無制限な同時操作の実装予定ではない | 入力資源を生成終了まで所有、Snapshotは値所有、World変更は外側で直列化、Nativeは所有スレッド |

## 恒久的に維持する境界

描画入口はGet2D/Get3Dだけ。旧ルートDraw/DrawText等やFRenderSystem2Dの互換別名を追加しない。
Physics本体からRendererやDxLibを呼ばない。今回のDebug層が公開読取APIと値Snapshotをつなぐ。
Debug機能を無効にした場合、接触/モデル/全World等の高価な採取を自動開始しない。
サンプルの時計は既存Toolbox固定ステップ計画を利用し、PhysicsSceneの自動固定更新とは重ねない。
サンプルの履歴は観察専用。選択してもWorldへTransformや速度を書き戻さない。
Scope/世代/容量を持つ設計を維持し、失効したIDへポインタで追従しない。

## 次の実装順

1. 今回の移行後にWindows Debug/Releaseとroot全テスト、RenderDebug実画面を確認する。
2. 2D/3D Physics本体の正式な観察Snapshot APIを追加し、登録の重複管理をなくす。
3. 実モデル・Instance・Native照明を対応表に沿って導入する。基本形状の照明とモデル照明を混同しない。
4. Viewport・透明描画・描画パス状態を整理する。
5. 汎用デバッグUI・選択・GPU実測・履歴の時間軸UIへ拡張する。

過去のテスト件数を現在の成果として転記しない。各変更の完了には、実行した環境・コマンド・SHA/差分を記録する。

## 今回の継続更新

透明パスを先行実装した。正式Physics Snapshotは未着手であり、登録型3D観察の制約を維持する。
前回のRenderDebugを累積で収録し、F7半透明パネルを追加。旧root Draw入口は増やしていない。
深度とビュー境界、Native状態復元、所有権の契約を維持する。詳しくはRendering/TransparentPasses.md。
