# Rendering / Debug の継続課題

起点: `86a37b9cdb7e3641e41f7838a8c5694de801679c`。
この表は実装済みと未実装を混同しないための継続表であり、全機能の完成宣言ではない。

| 領域 | 今回の状態 | 完了に向けた次の検証条件 |
|---|---|---|
| 3D基本形状・CPUメッシュ | 既存実装を利用。Applicationが実際に3Dを実行する所有者へ接続を修復 | Windows実画面で形状・奥行き・2D復元を確認 |
| MV1モデル・スキニング・テクスチャ付きモデル | ufbx経路、骨・モーフ・UV1・頂点色・基本PBRを実装済み | 対応範囲と制限は[FbxSupport](../FbxSupport.md)。SDK未導入PCは未検証 |
| 照明 | 基本形状はCPUフラット計算。モデルはDxLibの方向・点・スポット光と基本PBR | 分割ビューでも独立した設定を使用。影とライトのアニメーションは未実装 |
| 分割Viewport | D3D11の同一描画先へ左右2ビューを実装。矩形内だけ深度初期化、投影と全画面2D復帰を検証 | [契約と制限](../Rendering/Viewports.md)。重複領域の深度共有は保証しない |
| 複数ウィンドウ | 未実装 | Backend能力調査から開始。今回の分割表示とは別範囲 |
| 透明3D | **基本形状/CPUメッシュのプリミティブ単位ソートを追加**。不透明→透明→Overlay、透明はTestOnly | Windows実画面、実負荷、テクスチャ/MV1。重心ソートは近似で交差面の厳密な解決・OITは未実装 |
| Nativeの任意の外部状態保存・復元 | 全状態の保証なし | ライブラリが管理する状態を列挙し、その範囲だけ保証。未知の外部状態は明示再設定する契約 |
| 物理Snapshot | **`FPhysicsWorld2D/3D::CaptureSnapshot()`を実Worldへ統合**。Debug表示は登録型Watchを廃止し、World採取値から変換（3D表示・履歴、2D表示の入口） | RenderDebugの実画面確認。同一Collider IDのShape更新と改訂番号は別作業 |
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

## 2026-09-23 Physics Snapshot統合・Renderer／Scene統合・累積回帰

- `FPhysicsWorld2D/3D::CaptureSnapshot()`を実Worldへ統合。Debug表示は登録型Watchを廃止し、World採取値からの変換へ移行（3D表示・履歴、2D表示の入口とRenderDebugのF9最小例）。
- ApplicationをFRenderSystemへ統一し、共有JobSystemを接続。root CMakeへRenderSystem.cpp・RenderPass3D.cpp・Application描画統合テスト・DebugTools.cmakeを登録。
- 3D命令のないフレームでは3D実行後の2D状態復元を行わない。これで描画先切替の失敗がフレーム全体の失敗へ変わる回帰を解消した。
- Scope待機分離を維持したままScene寿命統合を接続。関係ないRoot・兄弟ScopeのPrepareを待たずにScene切替が完了することを実Applicationで確認。
- `Toolbox/Log.h`のDXF_LOGマクロを追加（[Logging.md](../Logging.md)）。
- RenderDebugをDebug構成の実DxLib・実画面で確認（3D描画、2D復元、F4・F8・F9、停止・手送り）。
- 未完了: Release構成の実SDK実行ファイルのリンク。公式DxLib 3.25aの`DxLib_vs2015_x64_MT.lib`だけがFBX読込を有効にしてビルドされており、FBX SDKを要求する。
- 次の範囲（モデル）への入力: MV1変換ではなく、FBXからモデルとアニメーションを直接読み込みたいという要望がある。
  FBX SDKの導入（利用許諾・全構成でのDxLib対応）と、フレームワーク側の読込経路のどちらを採るかを、モデル作業の開始時に決める。
