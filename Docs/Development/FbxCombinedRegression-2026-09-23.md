# ufbxモデルの組み合わせ回帰検証（2026-09-23）

## 対象と結論

作業開始時のローカルmainとorigin/mainはともに`7359ba95935835f8e8edfcd1a06c87c228dba8db`、作業ツリーはクリーンでした。ufbx、拡張3のソース版DxLib、基本PBR等の現行実装を保持し、実装本体への変更はありません。現行説明の矛盾を取り除き、既存のNativeModelDeviceSmokeへ不足する組み合わせを追加しました。過去のValidation記録は変更していません。

## 既存試験との照合

| 観点 | 既存の根拠／今回の補強 |
|---|---|
| 個体別再生・停止・速度・再読込 | ModelTestsとNativeModelSmokeのSkinnedColumn試験を維持 |
| 複数モーフ＋動く骨＋複製 | 手作成CombinedModel.fbxを追加。1本の骨の移動、Widenのアニメーション、Lowerの手動操作を併用。片側の形状変化と他個体の色ハッシュ不変を実画面で確認 |
| PBR＋UV1＋頂点色＋通常材質 | 同じCombinedModelをPBR／通常材質の2個体として混在。UV0/1変更でPBR側のみ変化。同形状・同材質・同UVの無頂点色PbrUvとの比較で輪郭一致、頂点色による輝度低下を確認 |
| 受付後の変更 | 材質・両モーフ・時刻を受付後に変更。受付済み描画は変更前の画素ハッシュと一致し、次の描画だけ変化 |
| 静止カメラ・ライト・2D | 既存の点光源位置／スポット方向／正射影試験を維持。追加で透視／正射影と3種のライトを同一フレーム内で切替え、各単独ビューの左右の画素と一致。2Dの橙色を完全一致で確認 |
| 失敗と復元 | 既存のNativeModelTestsでライト作成・設定・描画失敗時の解放と外部ライト／Z／照明の復元を確認。実DxLibでは受付後に資源を失効させ、描画失敗後の次フレームの2D画素復元を追加 |
| Scene切替・再読み込み・終了 | 実Application→実DGameScene→実DGameObjectの経路を追加。空Sceneへの切替・CollectUnused後のネイティブモデル／基礎モデル数0、戻ったSceneで同一画像、終了時のオブジェクト終了通知を確認 |
| Application失敗終了 | モデル受付後、実Assets.Shutdownで資源を失効させる。Applicationの失敗・終了、Sceneの後始末、後続画像取得が走らないことを確認。その後新しいApplicationで再び同じ試験が成功 |
| 機能不足 | 既存ModelTestsのモデル読込／描画バックエンド不在とNativeModelTestsの旧拡張・PBR拒否、失敗時のライト復元を維持。これらは境界のテスト実装による試験で、実Direct3D9機での試験ではない |
| シェーダー寿命 | 再読み込みでシェーダーハンドル数が増えないこと、次のセッション開始時のモデル／シェーダーハンドル数0を確認。DxfReleasePbrが定数バッファも終了前に解放するコードを確認。D3Dの全COM資源を列挙したリーク検査までは未実施 |

このプロジェクトの描画オブジェクトの所有経路はDGameSceneとそのオブジェクト集合です。「実World」の証拠をこの名称へ勝手に読み替えません。独立した汎用World型のモデル試験は追加していません。既存の物理World試験は全体CTestに含まれますが、今回のモデルとの物理連携の証拠にはしません。

低レベルFRenderSystem試験、実Application／Scene試験はいずれも今回のネイティブ試験では実DxLibを使います。CombinedModelは手作成の小さいFBXであり、任意の制作ツールの出力互換性を確認したものではありません。既存の画素閾値とGGX参照色（各成分誤差3以下）は変更していません。

試験調整中、右側へ移したモデルがスポットの照射範囲外となりました。画素閾値を下げず、右ビューのライト位置をモデル位置へ合わせました。またRender.Nativeはその場でキューを実行するため、失効試験はNative呼出しの前に資源を失効させています。製品不具合を修正したという記録ではありません。

## 今回実行した結果

ログは生成物として`Build/FbxContinuation`に保存し、Gitへ追加していません。

| 実行 | 結果 | 記録 |
|---|---|---|
| Debug全体ビルド／CTest | 23/23成功、13.61秒 | combined-debug-build.log / combined-debug-tests.log |
| Release全体ビルド／CTest | 23/23成功、9.23秒 | combined-release-build.log / combined-release-tests.log |
| モデル単体試験 | 上記両構成に含まれる既存30件成功 | Model CTest |
| 実DxLib組み合わせ描画 | 両構成成功、正常→Application失敗→再開始も実行 | NativeModelDeviceSmoke、model-smoke/*.png |
| No-STL | 287ファイル、違反0 | Tools/CheckNoStl.py |
| Pythonツール試験 | 19件成功 | combined-python-tests.log |
| 配布検証 | Native無効のDebugで7段階成功 | combined-package.log、package-logs/ |
| 実Native外部プロジェクト | インストール済みライブラリを使いDebug／Releaseの両方で実モデル描画成功 | combined-native-consumer-*-run.log |
| 実行専用配置 | 外部利用Release実行ファイルとAssets/Models、Tests/Assetsのみのコピーで成功。PATHはWindowsとSystem32のみ | combined-runtime-only/runtime.log |

外部プロジェクトはNativeModelSmokeのソースをコピーして`find_package(dxlib_framework)`と`dxf::native`でビルドし、フレームワークのソースを組み込んでいません。SDKは今回のPCで既にビルドされた`ThirdParty/DxLib-3.25a-source`を明示指定しました。実行専用配置にSDK、Tools、fxc、生成シェーダーヘッダーはありません。ただしPC上に開発ツールが存在するため「コンパイラーを導入していないPCで成功」とは扱いません。

## 未実施と再実行

- Autodesk FBX SDKを一度も導入していない別Windowsで、ソースZIPからSetup・両構成・外部利用を行う試験は未実施です。このPCには比較用SDKが存在します。
- Visual Studio／Windows SDK／fxcのない別Windowsでの配布実行も未実施です。Setup時のfxc事前コンパイルと実行時の組み込みバイトコード利用は別工程です。
- 実Direct3D9機でのPBR拒否、D3D全資源のリーク列挙、任意の制作ツールの互換性は今回の確認範囲外です。

必要な環境・コマンド・外部プロジェクト・判定条件は[独立環境での再実行手順](FbxCleanEnvironment.md)へ整理しました。GPUモーフ、追加材質マップ、影、カメラ／ライトのアニメーションは追加していません。
