# Viewport座標変換・簡易選択の検証（2026-09-23）

## 対象と環境

開始main/originは`73e8c9a1e3191a3312475a301fb5d529c009fc4c`、作業開始時はクリーン。対象はこの記録と同じcommitの変更。過去のViewport/Starter/FBX検証記録は変更しない。

Windows x64、Visual Studio 18 Community、C++20、実DxLib 3.25a D3D11。既存`ThirdParty/DxLib-3.25a-source`（FBX SDKなしのDxLibビルド、model extension 3、MT/MTd）を使用。SDK再構築・Autodesk方式への切替はしていない。

## 実装範囲

- support: `ProjectWorldToScreen`、`MakeViewPickSegment`、結果型`FProjectedPoint3D`。指定ビューと描画先寸法だけを使う値計算。Native状態を変更しない。
- Toolbox: 球/OBBの有限線分交差。球は既存Sweepを再利用、箱は既存FMatrix4逆行列と区間交差。既存の数学規約や衝突処理は変更しない。
- ModelViewer: Pで球/箱の選択、Vで分割、Oで正射影、左クリック。入力と描画が同じ準備済みビューと形状を参照。時間更新は従来のOnTick一回。
- 公開API/失敗条件/座標規約は[仕様書](../Rendering/ViewCoordinates.md)。Manager、World Raycastサービス、新しい常設サンプルは追加しない。

## 最終実行結果

| 工程 | 構成・結果 | 終了コード |
|---|---|---|
| CMake生成 | VS x64、Native/Starter/Sandbox/ModelViewer/NativeSmoke/device有効 | 0 |
| ビルド | Debug、Releaseとも成功 | 各0 |
| root CTest | Debug 24/24、26.28秒 | 0 |
| root CTest | Release直列再実行 24/24、16.91秒 | 0 |
| RenderViews | 両構成40/40（従来32＋今回8） | 各0 |
| Models / NativeViewsTranslation | 両構成30/30、7/7 | 各0 |
| ApplicationRenderIntegration | 両構成9/9。既存試験を4条件へ拡張 | 各0 |
| CheckNoStl | 299ファイル、違反0 | 0 |
| Python unittest | 19/19 | 0 |
| 配布 | Native OFF / Debug、install後の別場所への移動、外部framework/support consumer含む7段階 | すべて0 |
| git diff --check | 成功 | 0 |

root登録数は24のまま。追加テストを既存RenderViewsとNativeModelDeviceSmokeへ登録し、通常ソリューションへ検証専用プロジェクトを増やしていない。元のStarter/Sandboxに変更なし。両構成でStarter/Sandboxをビルドし、既存Sandbox実Application・固定入力試験も通過。

最初のRelease全体試験は23/24、呼出し終了1（NativeSandboxDeviceSmokeの`DxLib_Init failed`）。Debug側の実デバイス試験と時間帯が重なっていた。原因を断定せず、両構成の実デバイス実行が重ならないよう直列でRelease全体を再実行し24/24を確認。失敗ログは`Build/coordinates-tests-release.log`、成功ログは`Build/coordinates-tests-release-retry.log`。

開発途中には新規試験の配列初期化、ModelViewerヘッダーのinclude不足、固定入力用サービスのモデル窓口渡し忘れを修正。最初の「Debugビルド成功」は当時の有効ターゲットであり、その時点ではModelViewer実行まで確認した結果ではなかった。最終ではModelViewerを明示的に有効化し、その本体をNativeModelSmokeへ組み込んだ。新規画素判定は照明の暗部と小数位置の文字アンチエイリアスを考慮して設定した。既存画素判定は変更していない。

## 根拠を区別する

### CPU・手作成データ（Nativeなし）

透視の既知の画面位置/非線形深度、正射影、移動・回転カメラ、非単位Up、近遠面0/1、視野外/眼平面/背後、641×481の異なる矩形・半開境界・画素中心、往復、NaN/Inf/不正カメラ・寸法・矩形・表現不能を確認。照明値を変更しても投影に依存しない。

球/OBBの非交差・接触・始点内部・両端・範囲外、接線の内外、回転箱、最短・同距離の安定順序、不正形状を検証。手作成の解析値を使い、往復一致だけを根拠にしていない。

### 実DxLib・実描画

`NativeModelDeviceSmoke`の追加試験は本物の`Examples/ModelViewer/ModelViewerScene.cpp`と`PickingExample.cpp`をコンパイルして起動する。入力だけをFRawInputで固定し、実InputSystem/Application/DGameSceneを通す。球・箱それぞれを十分内側で選び、全画面/左右の異なるカメラ/正射影で色・印・文字を画素照合。領域内の空白と領域外では非選択。R再読込・Enter Scene切替・終了後の再起動でも選択が残らない。

画面は一括でSoftImageへ読戻し、以降はCPU上で調べる。中心周囲に選択色が100画素超、白い印が10画素超、ラベルが10画素超を要求。非選択は元の色100画素超、選択色/印/ラベル0。黄色は照明で暗くなってもRGB比を保つ条件、文字はアンチエイリアスを含む明るい灰色を数える。投影座標の整数化は画素走査のためだけで、APIに半画素補正を追加していない。

既存の641×481別描画先の非等サイズViewport試験では、描画した正方形の四隅を今回APIの投影値と±1画素で比較（連続座標と塗りつぶし境界の整数画素差）。従来の寸法/中心の解析値判定も保持。ModelViewerでは右側の描画・2D復帰・Present後に左ビューを渡して投影し、その位置の画像を検査。

### 実Application・実World・モデル時刻

PhysicsSceneは実WorldのStepIndexを1/2ビュー×計算有効/無効の4条件で確認。4描画フレーム・3固定Stepで変化なし（Backendは記録用）。実DxLib側は実GameScene/GameObject・CombinedModelで同じ4条件を通し、Tick回数とアニメーション時刻が描画前後で変わらず、1/60秒だけ進むことを確認。

既存のモデル組合せ、領域外深度の保持、故障時のPresent抑止、資源失効、Scene再入場、モデル/シェーダーの解放・再利用検証も同じNativeModelDeviceSmokeで成功。新機能追加のために既存契約を弱めていない。

## 再実行

リポジトリルートから（PowerShell、DebugとReleaseの実デバイス試験は直列）：

```powershell
cmake -S . -B Build/FbxContinuation -A x64 -DDXF_BUILD_NATIVE=ON -DDXF_BUILD_STARTER=ON -DDXF_BUILD_EXAMPLE=ON -DDXF_BUILD_MODEL_VIEWER=ON -DDXF_BUILD_NATIVE_SMOKE=ON -DDXF_RUN_DEVICE_TESTS=ON
cmake --build Build/FbxContinuation --config Debug --parallel 6
ctest --test-dir Build/FbxContinuation -C Debug --no-tests=error --output-on-failure
cmake --build Build/FbxContinuation --config Release --parallel 6
ctest --test-dir Build/FbxContinuation -C Release --no-tests=error --output-on-failure
python Tools/CheckNoStl.py
python -m unittest discover -s Tools/Tests -v
git diff --check
```

各工程の終了コード0を確認して次へ進む。配布はVisual Studio x64開発コマンドプロンプトで`python Tools/ValidatePackage.py --logs Build/CoordinatesPackageLogsFinal`。Generatorの異なるBuildへ混ぜない。今回最終配布run_idは`4f107fa684b944e2a62934b8f91ac481`。外部supportだけのConsumerで新ヘッダーをincludeし、投影位置・線分と球/OBB交差を実行した。

生成/ビルド/試験ログは`Build/coordinates-*.log`、配布ログは上記ディレクトリ。実行ファイルは`Build/FbxContinuation/{Debug,Release}/NativeModelSmoke.exe`と`ModelViewer.exe`。画像は`Build/FbxContinuation/model-smoke/picking-*.png`および既存画像（最後の構成で上書き）。SDK/Build/画像/ログはcommit対象外。変更テキストはUTF-8/CRLF、既存BOM保持。変更範囲へclang-formatの既存設定を適用し、括弧内一行の規約に合わせColumnLimitだけ0とした。

## 未実施・対象外

人の物理マウス/キー操作、音声聴感、SDK未導入PC、開発ツール/シェーダーコンパイラー未導入PC、実D3D9、D3D/COM全資源のリーク列挙は未実施。Native OFF配布成功を実SDK配布の独立環境成功へ読み替えない。今回のAPIはシェーダーやコンパイラーを追加しない。

精密な変形モデル三角形Picking、GPUモーフ、追加材質マップ、影、カメラ/ライトアニメーション、ギズモ/Inspectorは範囲外。今回の限定到達点で区切る。
