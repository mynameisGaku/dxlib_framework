# 89ec1f0からの物理・当たり判定の継続作業

起点: `89ec1f07883684609b12f79470d2d4ebfc96b360`（main）。この変更は既存のStarter、Sandbox、FORCEINLINE、Visual Studioフィルタ設定を置き換えません。

## 今回の到達点

**幾何学的な交差判定、直線移動中の最初の接触、固定時間更新の計画を扱う変更です。完成した剛体シミュレーターではありません。**

- 既存の球対AABB・OBB・Cubeを、汎用GJKから専用の距離判定へ振り分けました。球と球も同じ入口で処理します。差を取る前からf64にし、f32の広域境界の丸めが詳細判定を拒否しない順序にしています。無効な形状・Boundsのオーバーフローを通知する既存契約は維持します。
- OBB・Cubeの距離は、軸の近似的な直交性を完全な直交として扱わず、保持されている軸が作る平行六面体に対して計算します。三座標の自由・下限・上限の組み合わせ（27候補）の境界付き最小二乗です。GJKの全形状の精度を改善した、という変更ではありません。
- 2Dの`FVector2`、`FCircle2D`、`FAABB2D`と交差判定を追加しました。隙間の比較はユークリッド距離です。
- 球同士、球と平行移動するAABB、円同士、円と平行移動する矩形の`Sweep()`を追加しました。反対順序の箱・円／球も受け付けます。
- `FFixedStepScheduler`は経過時間から固定更新回数を作ります。シーン、GameObject、ソルバーへ勝手に接続する処理はありません。
- GCC 14.2で再現したVariant/Optionalの構築制約のコンパイル問題を、Toolboxの`IsConstructible`で修正しました。STLへの置き換えではありません。

## 公開API

### 連続衝突判定

```cpp
#include "Toolbox/ContinuousCollision.h"

Toolbox::FSphere Ball{{-10, 0, 0}, 0.5f};
Toolbox::FAABB Wall{{0, -2, -2}, {0, 2, 2}};
Toolbox::FSweepHit3D Hit = Toolbox::Sweep(Ball, {20, 0, 0}, Wall, {});
// Hit.bHitはtrue、Hit.Timeは0.475、法線は(-1, 0, 0)。
```

変位は速度ではなく、その区間全体で移動する距離です。`Time`は0〜1の区間割合であって秒数ではありません。非交差の`Time`も1なので、必ず`bHit`を確認してください。法線は二つ目の形状から一つ目へ向きます。

初期接触は`Time=0`、`bInitialContact=true`です。初期貫通・同一点・半径ゼロの場合、幾何学的な法線が一意ではないので決定的な代表方向を返します。貫通量や接触多様体を返すAPIではありません。

```cpp
Toolbox::FCircle2D Circle{{-5, 0}, 1};
Toolbox::FAABB2D Rectangle{{0, -2}, {0, 2}};
Toolbox::FSweepHit2D Hit2D = Toolbox::Sweep(Circle, {10, 0}, Rectangle, {});
// 2Dでは法線もFVector2として返します。
```

球対箱は、最近点の式が変わる面通過時刻で区間を分け、各区間の距離式を解きます。箱を半径分だけ拡張したAABBへのレイ判定ではありません。このため、角を斜めにかすめるだけの軌跡を安易に接触としません。

入力は有限で、半径・Toleranceは非負とします。不正入力は`FException`です。内部の距離計算はf64ですが、公開座標がf32である以上、入力時点で失われた細部を復元することはできません。極端なスケールまで数学的に厳密、あるいは全CPUでビット一致と保証するものではありません。

### 固定ステップの計画

```cpp
#include "Toolbox/FixedStepScheduler.h"

Toolbox::FFixedStepScheduler Scheduler;
Toolbox::FFixedStepPlan Plan = Scheduler.Advance(1.0 / 30.0);
// 既定の1/60秒ではStepCount=2。
// 呼び出し元がStepCount回、Plan.StepSecondsを使ってシミュレーションを進めます。
```

既定は1/60秒、1フレーム最大8ステップ、受け付ける実経過時間は最大0.25秒です。上限で捨てた時間は`DroppedSeconds`、1ステップ未満の残りは`InterpolationAlpha`で確認できます。無制限に過去の更新を溜め込む方式ではありません。

`Advance()`は計画を返した時点でその時間を消費します。実際の更新が失敗した場合のロールバックは呼び出し元の責任です。無効な経過時間・桁あふれで`Advance()`自体が失敗した場合は、それまでの残余時間を変更しません。

物理次元は持たないため2D／3Dで使えますが、このクラス単独で重力・反発・摩擦が動き始めることはありません。

## 責務とビルド

- `Collision2D`：2D形状の検証と距離判定。
- `ContinuousCollision`：直線移動の接触時刻。
- `FixedStepScheduler`：実経過時間から固定更新計画への変換。

実装は`dxf::toolbox`へ追加します。通常ソリューションへテスト専用プロジェクトを常時表示しないよう、`dxf_physics_tests`は`DXF_BUILD_TESTS=ON`のときだけ作成します。既存の`GenerateProjectFiles.bat -Development`の運用を維持します。

今回の部分だけをDxLibなしで確認する入口:

```powershell
# CMakeとNinjaを実行できるDeveloper PowerShell for VSで実行します。
cmake -S Tools/PhysicsValidation -B Build/physics -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build Build/physics
ctest --test-dir Build/physics --output-on-failure
```

LinuxのGCC／Clangがある環境では次で4構成・ヘッダー単独検査・STL監査をまとめて実行できます。

```sh
python Tools/ValidatePhysics.py
```

## TDDの区別

1. 実際の起点ソースで、面接触の見逃しと引数順の不一致を再現しました。1,734配置のうち651配置で少なくとも片方が失敗しました。
2. 10個の接触・構築制約ケースを先に追加し、5ケース失敗を確認。距離判定の修正後に10ケースが通過しました。GCCのコンパイル失敗も別に記録しました。
3. Sweepと固定ステップは、存在しない公開APIを使うテストのコンパイル失敗を先に記録しました。これは実行時のアサーション失敗とは区別します。
4. 2万軌跡の独立した距離探索との照合などは追加回帰検査です。初回から通った検査をRed確認済みとは呼びません。

## 残っている商用品質の判定項目

このパッチで「商用の物理シミュレーションが完成」とは判断しません。以下はこの変更に含まれません。

- 剛体の質量・慣性テンソル、重力、角速度、反発・摩擦を解くソルバー。
- 接触多様体の維持、ウォームスタート、積み重なり、休止・起床、ジョイント。
- 回転する箱・凸形状・三角形メッシュに対するCCD、一般凸形状GJKの全面的な数値保証。
- GameObject／Componentとの自動連携、全形状の動的World、並列更新とクロスプラットフォーム決定性。
- 対象ゲームのスケール・最大物体数を定めた長時間試験と性能測定。

## 検証範囲の重要な制約

作成環境でGitHubから取得したのは、起点の対象C++13ファイルとビルド・監査設定3ファイルです。それぞれGit blob SHAが一致することを確認しています。リポジトリ全体を取得・ビルドした結果ではありません。

新規・変更したコードを実際の対象ソースと組み合わせた専用テストを実行しました。既存のフレームワーク全テスト、Root CMake全体のビルド、Windows／MSVC／実DxLib SDK／デバイス、GitHub Actionsは未実行です。GitHubへのcommit／pushも実施していません。

今回の検証結果と生ログは継続作業パッケージの`Validation`に収録します。過去の`Docs/ValidationReport.md`を今回の全体検証結果として書き換えることはしません。
