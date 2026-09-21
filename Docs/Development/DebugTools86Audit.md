# 86a37b9 継続実装の監査記録

## 起点と実際の変更

mainの確認済み起点は86a37b9cdb7e3641e41f7838a8c5694de801679c。
GitHubへのcommit/pushはしていない。過去の起動成功はこの起点のWindows側の実績であり、今回の変更後の実機成功とは別。

- 旧FRenderSystem2Dのソースと型参照を廃止し、既存FRenderSystemへApplication/root CMakeを接続する移行器。
- Applicationの全メンバー構築後に共有JobSystemをRendererへ接続。
- DebugCamera3D、DebugStepController、登録ColliderからのPhysicsDebugSnapshot3D、値履歴、表示アダプター。
- 実Worldを使うRenderDebug独立サンプルと、rootの実World統合テスト2件を作成。
- 全制約をRenderingDebugRoadmap.mdへ引き継ぐ。

## TDDで再現したもの

1. 新Debug API不在：01-api-red.log。実行時の仕様失敗ではなくAPI未存在のコンパイルRed。
2. カメラが大きな注視点・短距離・極点でf32の基底を失う：04-audit-red.log。SetPose成功でもMakeView失敗になっていた。丸め後の実ビューを検査して拒否。
3. 全overlay無効時に不正な設定値が成功になる：同ログ。入力検証を無条件に先行。
4. 起点の実旧Rendererが3D命令を実行しない：05-renderer-red.logの0/2。旧ソースとヘッダーはGit blob SHA照合済み。
   新Rendererへの切替後は3D→2D→Presentの順が記録される。
5. 時計のリセット契約：08-reset-red.logのReset未実装コンパイルRed。再生成時に残余時間/手送りを消し、pause/scaleは保持。
6. 移行器の失敗復元が同時編集を上書きする：11-applier-concurrency-red.log。自身の変更後に別編集がある場合はその編集を保護して退避先を報告する。

実装後に追加した通常系・ランダムではない境界回帰を、すべて先書きのRedとしては扱わない。
基本時間計画は、独自の重複実装を監査時に廃止して既存FFixedStepSchedulerへ集約した。

## 最終実行結果

| 構成 | 部分検証CTest |
|---|---:|
| GCC Debug | 9/9 |
| GCC Release | 9/9 |
| GCC ASan+UBSan、リーク検査 | 9/9 |
| GCC ThreadSanitizer | 9/9 |
| Clang Release | 9/9 |

内訳：新Debug/描画所有者テスト23件、既存の描画継続20件、既存のRenderViews+DebugStore30件、Native変換4件、Job故障注入5系統。
CTestの登録数とは別の数え方。前のプロジェクト全体228/228などを今回の結果へ転記しない。
ReleaseのDebug/描画継続/Viewsの実行ファイル各100回、計300実行通過。
新公開ヘッダー5個をGCC/Clangで各単独構文検査し10/10。
移行器の13件の単体検査が通過。追加CMakeモジュールを実ソースで登録・リンクする専用fixtureでも23/23。
このfixtureはroot全体ではなく、CMakeのソース登録とモジュール依存に範囲を限定する。

## 検証の境界

実Renderer・RenderContext・Job・幾何・既存描画テストを利用した部分チェックアウト。
Captureのローカルテストはgetterの返す姿勢/位置を制御する明示的テストダブルであり、Physicsソルバーを検証していない。
新しいTests/DebugPhysicsIntegrationTests.cppは実FPhysicsWorld3Dへリンクするための2ケースを用意したが、この環境では未実行。
RenderDebugScene.cpp / WindowsMain.cppの、現在の完全なRuntime/Gameplay/Physicsへのコンパイル・リンクは未実行。
全repoのDebug/Release、Windows/MSVC、実DxLib SDK、実画面/入力/音声、実機のP1〜P8検証は未確認。
Native変換検査は手書きDxLibヘッダーによる翻訳検査であり、SDK ABIの検証ではない。
ASan/UBSan/TSanは実行した経路だけで指摘なし。任意ユーザーコードのThreadSafe保証や全リーク不在の証明ではない。
GitHubの最新ソース全体をcloneできていないので、完全なrootへの移行器適用・全体再buildの成功は主張しない。

## 機能の境界

物理の採取は登録済み3D Colliderだけ。World全列挙、2D、接触点、Impulse、Island、CCDは未採取。
Shape定義/Body種別は作成時のWatch、姿勢・速度・休止は採取時の実World読取値。
履歴は観察であり巻戻しではない。最大240件/256Collider、サンプルは120件/3Collider。
表示は現行CPUフラット照明と基本形状のワイヤー。MV1・テクスチャモデル・スキニング・影・実ライト管理は未実装。
分割Viewport、複数ウィンドウ、透明自動ソート、任意Native状態復元、GPU計測は未実装。
デバッグUIはこのサンプルのキー操作と説明パネル。汎用エディター、ピッキング、検索、時間軸UIの完成版ではない。
資源保持・World所有スレッド・生成中の入力不変の契約は維持する。

## 移行器の注意

元のC++を丸ごと過去ZIPへ戻す処理ではない。旧Renderer型/includeとContext旧呼出しを特定し、Applicationの空のコンストラクタ本体へ接続を追加。
アンカー、blob SHA、HEAD、CMake登録、プロジェクト内includeを検査する。未知のroot SubmitGeneratedなどは推測せず拒否。
失敗時は自分が変更したファイルを復元する。ただし後発の他者編集があるファイルは上書きしない。
複数ファイル/プロセスにまたがるOSレベルの完全な原子トランザクションではない。適用中は他の編集を止める。
ユーザー作業をgit reset/clean/stashしたり、SDKやAssetsを置き換えたりしない。

## 次の必須確認

Developmentソリューションの生成→root全体Debug/Release→新旧全CTest→実RenderDebug起動。
実機確認表はDocs/Rendering/DebugTools.mdを参照。Windowsでの新しい結果を記録した後にcommit/pushする。

## 配布候補の再展開検査

配布ZIPを別フォルダーへ展開し、ValidationSourceの新規Releaseビルドで9/9 CTest、移行器13/13を再確認した。
これは部分検証用ソースの再構築であり、全リポジトリへの適用・実World・Windowsサンプルの成功ではない。
Source/Examples/Testsの部分取得79ファイル、追加検証用Toolsのcpp2ファイルを検査し、STL違反0。
