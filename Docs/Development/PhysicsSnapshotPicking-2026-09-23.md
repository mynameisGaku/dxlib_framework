# Physics SnapshotのCollider選択 検証記録（2026-09-23）

開始SHAは`069bca3bbba22b9183a893b7899ea1e52bffb8b3`。main/origin一致、作業開始時の差分・未追跡ファイルなし。対象はこの記録と同じcommitの限定変更。過去のViewport/座標変換記録は変更していない。

## 実装

- Debug層に`PickPhysicsDebugSnapshot3D`と値型`FPhysicsDebugPick3D`。採取済みSnapshotと線分のみを使用し、球/OBBの既存交差処理を再利用する。
- World/世代/スロットの不整合、後続要素の不正や計算不能を失敗として扱う。同距離は先頭側、非交差は成功した空Optional。
- RenderDebugは更新末尾でビューを確定し、表示対象Snapshotと共有して選択。履歴Snapshotと選択IDを別々に持ち、完全なID一致を毎更新確認する。重心の印、選択色、LIVE/HISTORYと採取Step、ID・位置・速度・休止状態を表示。
- 観察OFFからZ/Xで保存履歴を再表示できる経路を閉じ、OFFのクリックで採取・履歴保存・選択を始めない。履歴切替・再生成・Scene終了で選択解除。保存履歴は消さない。
- クリック用の形状登録、追加Step、World再採取、GPU読戻しは追加しない。サンプルの読取getterは実Sceneの検証にも利用し、フレームワークへManagerやInspectorを増やしていない。

[API・操作・計算量](../Rendering/PhysicsSnapshotPicking.md)。最大256件のID検査O(n²)、交差O(n)、追加記憶O(1)。時間性能の保証・計測は行っていない。

## 最終構成と結果

Windows x64、Visual Studio 18 Community、C++20。既存DxLib 3.25a sourceビルド、D3D11、model extension 3、MT/MTdを使用。今回SDKを再構築していない。

| 工程 | 結果 | 終了コード |
|---|---|---|
| CMake生成・対象確認 | RenderDebug/ModelViewer/Starter/Sandbox/NativeSmoke/device有効、ctest -Nで25件 | 0 |
| 全体ビルド | Debug / Release | 各0 |
| root CTest | Debug 25/25、Release 25/25。構成間で直列実行 | 各0 |
| DebugPhysicsCapture内部 | 両構成11/11（既存6＋純粋/実World4＋実RenderDebug1） | 各0 |
| RenderViews / Models | 両構成40/40、30/30 | 各0 |
| NativeViewsTranslation / ApplicationRenderIntegration | 両構成7/7、9/9 | 各0 |
| 実デバイス | 既存3群とNativePhysicsDebugDeviceSmoke、両構成成功 | 各0 |
| No-STL | 305ファイル、違反0 | 0 |
| Python | 19/19 | 0 |
| 配布 | Native OFF / Debug、7段階成功 | すべて0 |
| git diff --check / UTF-8・CRLF | 成功、既存BOM維持 | 0 |

登録は24→25群。新規は開発用の`NativePhysicsDebugSmoke`のみで、通常のソリューションへ追加していない。モデル/描画/Physics本体、ModelViewer、Starter、Sandboxのソースは変更なし。

## 試験が示す範囲

### 手作成Snapshotと実World（Nativeなし）

未採取・非交差、最短/同距離、始点内部・終点・近遠区間外・接触、入力不変性、不正形状/NaN/World/世代/重複ID/BodyCount/上限を確認。最短0の候補があっても後続の不正要素を成功扱いしない。

実Worldに複数Colliderを取り付け、回転Bodyとローカル中心オフセットが一度だけワールド変換されることを確認。Body削除・同じBodyスロット再利用に加え、Bodyを維持したままColliderをdetach/attachし、同じCollider.Index・異なるGenerationを確認。Stepが同じ保存値と最新値を区別し、別WorldおよびWorld破棄後の保存Snapshotも検証。純粋な問い合わせで物理位置やStepを進めない。

### 実RenderDebug＋実Application＋実World（描画境界のみ記録）

`Tests/RenderDebugPickingTests.cpp`は本物のSceneソースをコンパイルし、クリック有無の二経路に同じ40フレームの時刻列を渡す。全フレームのStep、シミュレーション秒、位置・速度・角速度・休止状態、履歴件数、Present回数を比較する。既存のModelViewer試験を今回の接続の根拠へ読み替えていない。

### 実RenderDebug＋実DxLib D3D11

`NativePhysicsDebugSmoke`は実Sceneを実Applicationで起動し、入力境界のみFRawInputで固定。球/箱選択、空白クリック解除、カメラ移動、停止中のStep不変、位置の異なる履歴、F8 OFF/ON、World再生成、Scene再入場・終了を検証。説明パネルとF9領域のクリックも3D選択へ通らない。

過去のStep 0にある球を、Liveで落下後に履歴表示へ戻して選択。保存位置と保存IDを検査し、HISTORY/Step/COM/速度の表示画像を保存した。観察OFF時は選択IDなし・表示Items空・履歴件数と停止中のシミュレーション秒が不変で、詳細欄の文字も画素上で消える。

画素試験ではF2でUnlitにし、重心周囲の黄色を100画素超、白い印を15画素超、下部詳細文字を100画素超と照合。画面一括読戻し後のSoftImage上で調べ、画素ごとのGPUアクセスを行わない。既存の画素判定は変更していない。保存した`physics-history.png`も目視し、履歴の強調形状と重心ラベル・採取値を確認した。

## 途中の失敗と制約

最初は新しいRenderDebug試験を既存NativeModelSmokeへ組み込み、累積実行が120秒でtimeoutした（`Build/snapshot-target-debug.log`、呼出し終了1）。既存モデル試験の上限/判定を緩めず、新しい試験を別の開発用ターゲットへ分離した。負荷変動を含む実行時間を性能改善の証拠とはしない。

新規テストのVariant取得指定と、記録用Backendの2D図形対応フラグの不足も修正した。観察OFFの履歴抑止は新規回帰で検証したが、変更前mainでのRed、別コピーでの故意の誤実装による検出試験は実施していない。前回Release初回のDxLib_Init失敗の原因特定・修正を今回の成果として扱わない。

## 再実行・成果物

リポジトリルートから各終了コード0を確認し、順に実行する。

```powershell
cmake -S . -B Build/FbxContinuation -A x64 -DDXF_BUILD_NATIVE=ON -DDXF_BUILD_STARTER=ON -DDXF_BUILD_EXAMPLE=ON -DDXF_BUILD_MODEL_VIEWER=ON -DDXF_BUILD_RENDER_DEBUG=ON -DDXF_BUILD_NATIVE_SMOKE=ON -DDXF_RUN_DEVICE_TESTS=ON
cmake --build Build/FbxContinuation --config Debug --parallel 6
ctest --test-dir Build/FbxContinuation -C Debug -N
ctest --test-dir Build/FbxContinuation -C Debug --no-tests=error --output-on-failure
cmake --build Build/FbxContinuation --config Release --parallel 6
ctest --test-dir Build/FbxContinuation -C Release --no-tests=error --output-on-failure
python Tools/CheckNoStl.py
python -m unittest discover -s Tools/Tests -v
git diff --check
```

配布はVS x64開発環境から`python Tools/ValidatePackage.py --logs Build/SnapshotPickingPackageLogs`。Native無効の外部Consumerでdebug_toolsをリンクし、実World→Capture→新問い合わせを実行する。support単独Consumerの座標変換試験も維持。run_idは`69fcd38718ec41deb056d031db23e67a`。install後に配布物を別位置へ移して再生成・リンク・実行した結果。

ログは`Build/snapshot-*.log`、最終全体は`snapshot-tests-debug-final.log`と`snapshot-tests-release-final.log`。実行ファイルは`Build/FbxContinuation/{Debug,Release}/NativePhysicsDebugSmoke.exe`。画像は`Build/FbxContinuation/model-smoke/physics-*.png`（最後の構成で上書き）。これらとSDK・Build生成物はcommit対象外。

## 未実施・今回の外

物理マウス/キーの人手操作、音声聴感、SDK未導入PC、開発ツール/シェーダーコンパイラー未導入PC、実D3D9、D3D/COM全資源のリーク列挙は未実施。Native OFF配布をNative有効の独立環境確認へ読み替えない。

Live World全体のRaycast、BVH/GPU Picking、精密モデル三角形選択、2D選択、World編集、汎用Inspector等は追加していない。この限定単位で完了とする。
