# 現在のPhysics Worldへの最短線分問い合わせ 検証記録

開始SHA: `51372138db7c3097f351ee566f9ee7cbf9deb83a`。main / origin/main一致、開始時の作業ツリーはクリーン。過去の完了記録は変更していない。対象は本記録と同じコミットの実装・試験・文書。

## 変更と責務

- `FPhysicsWorld3D::RaycastClosest` と値型 `FWorldSegmentHit3D` を追加。現在の登録スロットを直接走査し、既存の `ToWorld_Internal` と `Toolbox::IntersectSegment` を再利用する。
- 任意の自己Body一つを完全なIDで除外。現在の球/OBBの最短一件を返し、正常な非交差と例外を区別する。
- 公開結果型との循環includeを避けるため、既存の `FBodyId3D` / `FColliderId3D` を同名ヘッダーへ移動した。メンバー・型・並び・既存RigidBody3D.hからの利用は保持した。
- Snapshot採取・Step・起床・追加形状登録をしない。既存SnapshotのStep状態ガードを参照し、処理中と途中失敗後を拒否する。積分・接触・CCD・休止計算は変更していない。
- DebugのSnapshot選択、RenderDebug、ModelViewer、Starter、Sandboxのコードは変更していない。
- 通常経路はO(保持Colliderスロット数)、追加領域O(1)。配列確保の不在は実装確認であり、速度や割当回数の計測結果ではない。

[API・失敗契約・画面由来線分の例](../Physics/WorldSegmentQuery.md)。

## 構成と結果

Windows x64、Visual Studio 18 Community / MSVC 19.51、C++20。既存 `ThirdParty/DxLib-3.25a-source` のD3D11、model extension 3、MT/MTd構成を使用。SDKの再構築は行っていない。

| 工程 | 実行結果 | 終了コード |
|---|---|---|
| root CMake生成 | Development側の既存Native/ModelViewer/RenderDebug/device試験を有効化 | 0 |
| rootビルド | Debug / Release成功 | 各0 |
| root CTest | 初回Debug 25/25、Release 25/25。最終Debug 24/25、失敗1群の単独再試験1/1成功。構成間直列 | 初回各0 / 最終Debug 8 / 単独再試験0 |
| PhysicsContinuation内部 | 各構成159/159、うち新規World問い合わせ7/7 | 各0 |
| 既存描画・Scene・Snapshot・Starter・Sandbox | 上記root全群に含む。実デバイス4群も各構成成功 | 各0 |
| No-STL | 309ファイル、違反0 | 0 |
| Python | 19/19 | 0 |
| 配布 | Native OFF / Debug、8段階成功 | 全工程0 |
| 追加の単独DebugValidation | coreライブラリのビルド成功。全ターゲットのビルドは別項の理由で失敗、CTest未実施 | core 0 / 全体1 |
| 差分・文字コード | git diff --check、UTF-8・CRLF・既存BOM維持を確認 | 0 |

CTest群は25のまま。新規7ケースは既存PhysicsContinuationへ追加した。CTest群と内部ケースを足し合わせていない。専用Native実行ファイルや通常ソリューションのプロジェクトは追加していない。既存画素判定・時間上限・失敗条件は緩和していない。

配布は `Build/PackageValidation/run-d_61_m9l/Relocated package` へ移動したインストールを利用。`Consumer/Physics.cpp` を `dxf::physics` だけにリンクした `ConsumerBuild/PhysicsOnly.exe` が、World生成→登録→問い合わせ→自己除外→削除→保存ID失効を実行した。framework/debug_toolsを使用する従来Consumerとsupport単独Consumerも維持。新しいPhysicsOnly実行により従来7段階から8段階となった。SDK未導入PCを使用した検証ではない。

## 新規実World回帰の範囲

`Tests/Physics/WorldQueryTests.cpp` は実 `FPhysicsWorld3D` と既存Solverにリンクする。Debug・Renderer・表示用Snapshotを問い合わせに使用しない。試験の更新回数の照合だけにPhysicsのCaptureSnapshotを使用する。

1. 空World、ColliderなしBody、Static/Kinematic/Dynamic、球の解析値、回転OBBのローカル中心と軸、同Bodyの球オフセット、最短、接線、終点、始点内部、線分外。
2. Stepなしの移動・回転・Detach/Attach・Destroy/Create、同距離のスロット順、再利用の世代、全自己Collider除外、旧世代/別World/無効ID拒否。
3. 301番目のColliderも検索し、Debug表示件数の上限に依存しない。
4. 空WorldでNaN/Inf・ゼロ長・変位の桁あふれを拒否。先行ヒットが割合0でも後続のワールド形状変換の桁あふれを拒否。
5. 停止した実JobSystemを渡してStep内部を失敗させ、問い合わせ拒否・正常Stepによる回復を確認。Step引数の拒否だけでは問い合わせを禁止しない。
6. 一方だけ100回問い合わせた二つの実Worldへ同じ外力・トルクを蓄積し、その後20Stepの位置・速度・角速度・姿勢・休止状態と更新回数を照合。
7. 接地して600Step後に休止したBodyへ100回問い合わせ、選択成功・位置不変・非起床・Step数不変を確認。

Step実行途中に別スレッドから問い合わせる試験は行っていない。公開契約が外側での直列化を要求するため、データ競合でガードを試すことはしない。処理中フラグの参照は実装確認、途中失敗と回復は上記の実World試験で確認した。

## 途中の失敗と追加調査

新規試験の初回ビルドは、存在しない `TNumericLimits::Infinity` と四元数の等値演算子を使用して失敗（終了1、`world-query-physics-build.log`）。既存の数値演算と四成分比較へ修正し、再ビルド・実行成功。実装後に追加した試験であり、変更前mainでの挙動Redや変異試験を実施したとは扱わない。初回のDebug/Release全群と配布は成功したが、最終Debug全群の再実行は24/25（終了8）。NativeModelDeviceSmokeで複数モーフ・スキンの読み込み成功後に `FAIL exception The window was closed` が発生した。原因は断定せず、ログを保持し、対象の実デバイス試験を単独再実行し、1/1成功（終了0、12.92秒）を確認した。コードや画素条件は変更していない。タイムアウトは発生していない。

`Tools/DebugValidation` はPhysicsソースを手列挙しているため、今回必要となる `SegmentIntersection.cpp` を登録した。追加確認で全体をビルドしたところ、モデルのGetMorphWeight/GetNativeTime_Internalとファイル処理の未解決参照で失敗した。Model.cpp / Platform.cppを一時追加した調査では、別のView系ターゲットに既存のアラインメント警告C4324のエラー扱いも確認した。調査用追加は残さず、最終変更は今回の交差ソースの登録一行に限定した。この単独構成の全体整備は残課題とし、古い実行ファイルを成功の根拠に使用していない。現行rootの全群成功とは分けて報告する。

## 再実行とログ

リポジトリルートから順に実行し、各終了コードを確認する。

```powershell
cmake -S . -B Build/FbxContinuation -A x64 -DDXF_BUILD_NATIVE=ON -DDXF_BUILD_STARTER=ON -DDXF_BUILD_EXAMPLE=ON -DDXF_BUILD_MODEL_VIEWER=ON -DDXF_BUILD_RENDER_DEBUG=ON -DDXF_BUILD_NATIVE_SMOKE=ON -DDXF_RUN_DEVICE_TESTS=ON
cmake --build Build/FbxContinuation --config Debug --parallel 6
ctest --test-dir Build/FbxContinuation -C Debug --no-tests=error --output-on-failure
cmake --build Build/FbxContinuation --config Release --parallel 6
ctest --test-dir Build/FbxContinuation -C Release --no-tests=error --output-on-failure
python Tools/CheckNoStl.py
python -m unittest discover -s Tools/Tests -v
# Visual Studioのx64開発者環境で実行する。
python Tools/ValidatePackage.py --logs Build/WorldQueryPackageLogs
```

同じGeneratorのBuildディレクトリを使用する。今回のローカルログは `Build/world-query-*.log`、配布各工程のコマンド・終了コードとSummary.jsonは `Build/WorldQueryPackageLogs/`。最終Debugは `world-query-build-debug-final2.log` / `world-query-ctest-debug-final.log`、Releaseは `world-query-build-release.log` / `world-query-ctest-release.log`。最初のDebug回帰後にコメントと診断文を整え、最終Debugを再実行した。単独再試験ログは `world-query-native-model-retry.log`。生成物・ログ・SDKはコミットに含めない。

追加単独構成の再現は `cmake -S Tools/DebugValidation -B Build/WorldQueryDebugValidation -A x64` と `cmake --build Build/WorldQueryDebugValidation --config Debug --parallel 4`。失敗ログは `world-query-standalone-build.log` と調査時の `world-query-standalone-build2.log`、最終coreのみの成功は `world-query-standalone-core.log`。

## 未実施・対象外

SDK未導入PC、開発ツール未導入PC、実Direct3D9、物理入力、音声の聴感、全D3D/COM資源のリーク列挙、速度/割当の専用測定は未実施。配布はNative無効・Debugであり、Native有効配布やRelease配布へ読み替えない。今回追加したCPU API専用の実描画試験は作っていない。2D、全Hit一覧、空間索引、精密モデル選択、描画機能の拡張には進めていない。
