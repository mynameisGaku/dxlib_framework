# SweepClosestの接触法線 検証記録

開始SHA: `84be2d10b9864bb3722bfdd3bd6a987777e1fbb5`。main / origin/main一致、開始時の作業ツリーはクリーン（未追跡・stage済み変更なし）。前回のSweepClosest（法線なし）の記録は当時の結果として変更していない。対象は本記録の直前のコミットの実装・試験・文書。

## 設計の確定

- 公開API: `FPhysicsWorld2D/3D::SweepClosest`の名前・引数順・既定値・constは変更なし。結果型`FWorldSweepHit2D`／`FWorldSweepHit3D`の末尾へ`Toolbox::TOptional<Toolbox::FVector2/FVector3> Normal`を追加した。既存4メンバー（Collider・Fraction・CenterAtHit・bInitialContact）の名前・型・並び・意味は変えていない。末尾追加でも型のサイズは変わるため、フレームワークとConsumerを再ビルドした（バイナリ互換性は主張しない）。利用箇所はフレームワーク内のWorld実装・試験・配布Consumerだけだった。
- 法線の向き: 接触対象から問い合わせ円／球の中心へ向くWorld座標の単位方向。移動方向の反転・箱中心からの方向・最寄りの座標軸は返さない。接線接触で移動方向と直交しても反転しない。
- 欠落条件: 初期接触（境界接触・重なり・静止を含む）、問い合わせ半径0、最終残差を丸め誤差から区別できない場合は空。空でもヒットと既存4メンバーは返す。非交差は従来どおり外側のOptionalが空。入力・状態の不正は従来どおり例外。
- 既存計算の再利用: 時刻計算は作り直していない。共有カーネル`PointBallEntry_Internal`に、同じ計算の途中で求めていた接触時の相対位置（既存CCDが代表法線に使っていたf64の残差）を返す引数付きの版を加え、既存の3引数版はそれを呼ぶだけにした（CCD用`Toolbox::Sweep`の式・結果・契約は不変）。箱の特徴ごとの計算（Gram行列による射影と有限範囲）は、最初の時刻を選んだ特徴の残差を同時に保持するだけで、別の法線計算を追加していない。f32の`CenterAtHit`から引き直さない。OBBの軸は正規化・直交化しない。
- 丸めの判定: 残差の最大成分が、その計算に使った値（円／球同士は開始の相対位置と半径の和、箱は箱中心からの開始位置・移動量・半幅と半径）の最大成分の2^-30倍以下なら空。f64の相対計算の丸め（規模の数倍の2^-52）に対して方向の誤差をおおむね1e-6以下に抑える比で、固定の絶対閾値ではない。単位化は最大成分で割ってから長さを求める（平方の桁あふれ・消失を避ける）。
- Toolbox側の型: 2D／3D共通の`FShapeSweepHit`（Time・bInitialContact）は残し、それを基底にした次元別の`FShapeSweepHit2D`／`FShapeSweepHit3D`（`Toolbox/ShapeSweepHit2D.h`・`ShapeSweepHit3D.h`）へ`Normal`を追加した。`SweepToCenter`の4関数は同じ引数のまま戻り値を次元別の型にした（`auto`で受ける呼出しと`Time`・`bInitialContact`の意味は不変。`TOptional<FShapeSweepHit>`と明示して受けていた利用者は型名の変更が必要）。同名の代替関数や、World側での法線計算の複製は作っていない。World側は`Result.Normal = Hit->Normal`の1行と、半径0の委譲経路で空のままにすることだけ。
- World側の契約（候補走査・状態ガード・カテゴリ・自己除外・現在姿勢への変換・最小割合とスロット順・割合0でも後続を検査・配列確保なし・O(n)）は変更していない。法線がある候補を、法線のない手前の候補より優先しない。

## 構成と結果（最終コード）

Windows x64、Visual Studio 18 Community / MSVC 19.51、C++20。`Build/FbxContinuation`（Visual Studio 18 2026、x64、`ThirdParty/DxLib-3.25a-source`を自動選択）を、TESTS・NATIVE・STARTER・EXAMPLE・MODEL_VIEWER・RENDER_DEBUG・NATIVE_SMOKE・RUN_DEVICE_TESTSを明示ONにして再生成（終了0）。SDKの再構築は行っていない。

| 工程 | 実行結果 | 終了コード |
|---|---|---|
| rootビルド | Debug / Release成功。追加・変更した行に警告なし（既存箇所のC4324等は変更前から。`new-code-warnings.log`） | 各0 |
| root CTest Debug（`-j 1 --no-tests=error --output-on-failure`） | 26/26（実デバイス群を含む） | 0 |
| root CTest Release（同上、Debugの後に直列） | **25/26**。`NativeModelDeviceSmoke`が失敗（下記）。他の25群は成功 | 8 |
| PhysicsContinuation内部（root・単独とも） | 各構成237/237。うち新規の接触法線9、既存のスイープ20 | 各0 |
| PhysicsOverlapFault（root・単独とも） | 各構成18/18（法線の有無の確保なし確認2を含む） | 各0 |
| 単独 `python Tools/ValidateDebug.py --logs Build/WorldSweepNormals-StandaloneLogs` | Debug 22/22、Release 22/22、8工程PASS。実DxLib SDK不使用 | 0 |
| 公開ヘッダーの単独コンパイル（cl /W4 /WX /permissive-、プロジェクトと同じく/wd4324） | `ShapeSweepHit.h`・`ShapeSweepHit2D.h`・`ShapeSweepHit3D.h`・`ShapeSweep2D.h`・`ShapeSweep3D.h`・`WorldSweepHit2D.h`・`WorldSweepHit3D.h`・`RigidBody2D.h`・`RigidBody3D.h`の単独、2D＋3Dの併用（各種）、`Dxf/ViewCoordinates.h`＋Worldの17通り | 0 |
| No-STL | 328ファイル、違反0 | 0 |
| Python `unittest discover -s Tools/Tests` | 24/24 | 0 |
| 配布 `python Tools/ValidatePackage.py --logs Build/WorldSweepNormals-PackageLogs`（x64開発者環境） | Native OFF・Debug、8段階PASS | 0 |
| `git diff --check` | 問題なし。変更・追加したテキストはPythonでバイト検査し、UTF-8・CRLF・BOMなし（変更前もBOMなし） | 0 |

CTest群の数はroot 26・単独22のまま（新規ケースは既存のPhysicsContinuationと隔離済みのPhysicsOverlapFaultへ正規登録で追加）。群数と内部ケースを足し合わせていない。

コード指紋（`git diff -- . ':!Docs'`と、Docs以外の未追跡ファイルの内容のSHA-256。対象は製品コード・試験・CMake・検証ツール）: 最終実行の前後とも`cfcfaed56b9fdd9a0694f848079277a013f6cd3d291499b659de900550dde6f5`で一致（`fingerprint-before.txt`／`fingerprint-after.txt`）。上表はすべてこのコードでの結果。これより前に、`7c3a68e027d3a2710789c662b999a552aea6e0cda821a8208692be0b0c992e46`のコードでroot Debug／Releaseを一度実行し、両構成26/26・237/237・18/18で成功したが（`run1-superseded/`）、その後`RigidBody2D.h`／`RigidBody3D.h`の`SweepClosest`の説明コメントへ法線の1行を追加したため、ヘッダー確認・root・単独・No-STL・Python・配布・差分検査を最終コードで全部やり直した。最初の実行の成功を最終結果へ数えていない。本記録と`Docs/`の文書は最終実行の後に追加・更新した（指紋の対象外）。

配布は移動したインストールを使い、`dxf::physics`だけにリンクした`PhysicsOnly`が、既存の手順に続けて次を実行した。2D: 壁の角に当たるスイープの法線が(-0.6,-0.8)（許容1e-5）、自己Bodyとの初期接触と静止の初期接触では法線が空、半径0の移動は`RaycastClosest`と同じ割合で法線が空。3D: 回転したOBBの壁への法線が有限・単位長（長さの二乗の誤差1e-5以内）で移動と逆向きのX成分を持ち、静止の初期接触では空。段階数は8のまま。SDK未導入PCでの検証ではない。

## 新規回帰の範囲

`Tests/Physics/WorldSweepNormalTests.cpp`（9ケース、既存PhysicsContinuationへ`CMake/PhysicsTests.cmake`で正規登録）。期待値は手計算の解析値。許容差は、f32への出力丸め（各成分2^-25以下）の約3倍の1e-7、単位長は2e-7。World経由で回転を合成する配置と、f32で格納した回転軸を使う配置は、姿勢・軸のf32演算（数ulp）を含めて1e-6。f32で格納した入力（0.9、1.6、6.3、1e-3など）は、その格納値から期待値を求めた。

- 円／球: 正面（-X）、斜め（f32の0.9からの(-0.8,-0.6)相当）、接線（(0,-1)、移動方向と直交）、終点ちょうど、対象半径0の点に問い合わせ半径が正で当たる場合。
- 箱: 面（x=5の壁、中心x=4.5と法線-Xを別々に確認）、2Dの頂点（左下頂点(5,0.5)、中心x=5-√3/2、法線(-√3/2,-0.5)）、3Dの頂点（(5,0.5,0.5)、法線(-√0.5,-0.5,-0.5)）、3Dの辺、斜めの移動での角。
- 軸: 2Dの+30度と-30度で異なる面の法線、X軸回り30度の格納軸のOBB、せん断した格納軸（法線(1,-ε)/√(1+ε²)。直交化すると(1,0,0)）、長さ1+3e-5の軸（法線は単位長）、半幅0の線分・点・板、線分の端点。
- 欠落: 初期の境界接触・深い重なり・同中心・静止（2D／3D、円・箱）、問い合わせ半径0（円・球・箱、移動あり／なし）、非交差は外側が空。
- 数値範囲: 2^20の共通座標（2D円・3D OBB）、小さな半径の長い移動（1e-3で±1e5、法線を返す）、小さい形状の短い移動（1e-6、法線を返す）、接線付近（対象y=1.5-2^-20）、判定境界（相対位置2^20と半径の和2^-10ちょうどは空、2^-10+2^-16は-X）、長い移動に対して小さすぎる半径（1e-6で1e7。ヒット・割合は保持し法線は空。円・球・箱）。
- 実World（2D／3D）: 実登録の壁で既存4メンバーと法線、2DのBody角度30度＋Colliderローカル角度15度＋ローカル中心（45度の正方形の面、法線(-√2/2,√2/2)）、3DのX軸回り30度＋ローカル中心（法線(0,-0.5,√3/2)）、同じ割合のスロット順、割合0の初期接触（法線なし）を後ろの法線ありの候補より優先、カテゴリによる除外、半径0とRaycastClosestの一致（法線なし）、Stepなしの姿勢・カテゴリ変更の反映。
- 利用: 2Dで問い合わせ結果だけから床・壁・法線なし（初期接触）に分類する例（壁の角をかすめると面法線ではなく(-0.8,0.6)方向）、3Dのカメラ候補で遮蔽物の面法線と移動方向の内積が負。分類の閾値はこの例の方針で、フレームワークの規則ではない。
- 非変更・所有: 既存の`WorldSweepTests`の読み取り専用（問い合わせあり／なしの数値経過・休止・蓄積力の一致）・休止・後続エラー・失敗後の再利用の各ケースは、同じ問い合わせ経路（法線の計算を含む）で引き続き成功している。結果はWorldや内部配列を借用しない値型。

`Tests/Physics/OverlapAllocationFaultTests.cpp`（隔離した`PhysicsOverlapFault`）へ、2D／3Dで「法線ありのヒット・初期接触で法線なし・非交差・半径0で法線なし」の成功経路が確保しないことの確認を各1項目追加（16→18項目）。

## 試験先行・挙動Red・変異試験

- コンパイルRed（`tdd-red-compile.log`、終了1）: 試験と登録を先に追加してビルドし、`Normal`が`FWorldSweepHit2D`（11件）・`FWorldSweepHit3D`（8件）・`FShapeSweepHit`（36件）のメンバーでないエラー（C2039）で失敗。ほかに、存在しないメンバーを引数にした`Toolbox::Abs`の多重定義解決の連鎖エラー（C2672）が1件。
- 挙動Red（`behavior-red.log`・`behavior-red-fault.log`）: 型とフィールドだけを追加し、法線を常に空にした一時的な実装（`stub-compile.log`、終了0）で実行。新規9ケース中8ケースが失敗（欠落条件だけを確認するケースは空の法線で成功）、既存228ケースは成功。故障注入の新規2項目も失敗（法線ありを期待）。この一時的な実装は保存しておいた変更前のファイルへ戻してから本実装を行った。
- 本実装後（`green-debug.log`）: PhysicsContinuation 237/237、PhysicsOverlapFault 18/18。途中の期待値修正はなし。clang-format適用後の再ビルドでも同じ（`green-debug-2.log`）。
- 変異試験（`mutate.py`・`mutation.log`）: 製品コードへ一つずつ欠陥を入れてビルド・実行し、元へ戻した。実施した10件すべてを検出、各復元はSHA-256で一致、復元後のビルド終了0。検出した契約: 法線の符号反転（向き）／辺・頂点でも面法線を返す（特徴ごとの方向）／箱の初期接触へ任意の有効法線を入れる／円・球の初期接触へ開始方向を入れる（初期接触は空）／OBBの格納軸を直交化してから計算（実際の形状）／3D Worldで欠落した法線を上向きで埋める（代替方向を作らない）／丸めの判定を除去（再現可能な欠落条件）／判定を2^-20へ厳しくする（正常な方向を不要に欠落させない）／円・球で移動方向の反転を返す（接線での直交）／2D Worldで法線がある後ろの候補を優先（最小割合とスロット順）。

## モデル実描画の偶発終了

最終コードのroot全群の実行で、Releaseの`NativeModelDeviceSmoke`が失敗した（Debugの全群では成功）。失敗時の出力と初回結果は`release-model-failure-LastTest.log`・`release-model-failure-ctest.log`に保存した。bc4cfc8の診断により、シナリオの8番目の起動で、DxLibの再初期化（`DxLib_Init result=0`）とモデルの読込みの後、最初のフレームで`ProcessMessage`が`-1`を返し、Runtimeが`platform-false`で停止し、Stepが`result=false`（描画0、注入なし）で終わったため、シナリオが「想定外のStep結果」として中断したことまでは追える。7番目の起動（注入によるエラー）は想定どおりに終了していた。

今回の変更はCPUの形状問い合わせ（Toolbox・Physics）と試験・文書だけで、Native・描画・モデル・DxLibの初期化／メッセージ処理には触れていない。ただし、それで今回の失敗の原因を説明したことにはならず、`ProcessMessage`が-1を返した起源（SDK内部やOS通知など）は引き続き特定していない。

事前に回数を決めた限定再試験: Releaseの`NativeModelDeviceSmoke`だけを、結果にかかわらず3回実行し、3回とも`REAL_SDK_MODEL_SMOKE_PASSED failures=0`（終了0。`model-retest.log`、`model-retest-*.log`）。この再試験の成功を全群の結果へ足していない。最終コードのroot Releaseの全群の結果は25/26のまま。画素条件・時間上限・起動失敗の条件は変更していない。偶発終了は再現頻度の低い未解決の事象として残り、今回の結果で解決済みにはしていない。

## 再実行とログ

```powershell
cmake -S . -B Build/FbxContinuation -DDXF_BUILD_TESTS=ON -DDXF_BUILD_NATIVE=ON -DDXF_BUILD_STARTER=ON -DDXF_BUILD_EXAMPLE=ON -DDXF_BUILD_MODEL_VIEWER=ON -DDXF_BUILD_RENDER_DEBUG=ON -DDXF_BUILD_NATIVE_SMOKE=ON -DDXF_RUN_DEVICE_TESTS=ON
cmake --build Build/FbxContinuation --config Debug --parallel 6
ctest --test-dir Build/FbxContinuation -C Debug -j 1 --no-tests=error --output-on-failure
cmake --build Build/FbxContinuation --config Release --parallel 6
ctest --test-dir Build/FbxContinuation -C Release -j 1 --no-tests=error --output-on-failure
python Tools/ValidateDebug.py --logs Build/WorldSweepNormals-StandaloneLogs
python Tools/CheckNoStl.py
python -m unittest discover -s Tools/Tests -v
# Visual Studioのx64開発者環境で実行する。
python Tools/ValidatePackage.py --logs Build/WorldSweepNormals-PackageLogs
git diff --check
```

rootのログは`Build/WorldSweepNormals-RootLogs/`（configure・build・ctest・`*-LastTest.log`・`*-ctest-list.log`、指紋、Red・挙動Red・変異・ヘッダー確認・No-STL・Python・配布のコンソール出力、変更を適用したスクリプト）、単独は`Build/WorldSweepNormals-StandaloneLogs/`、配布は`Build/WorldSweepNormals-PackageLogs/`。生成物・ログ・SDKはコミットに含めない。

## 未実施・対象外

SDK未導入PC、開発ツール未導入PC、実Direct3D9、物理入力・聴感、全D3D/COM資源のリーク列挙、非WindowsのSanitizer、速度の測定は未実施。配布はNative無効・Debugであり、Native有効配布・Release配布・別PCの成功へ読み替えない。接触点・侵入量の公開、初期接触の分離方向・押し出し、滑り移動、キャラクターコントローラー、Raycastの法線、SweepAny、全ヒット、移動する箱・カプセル、BVH、並列化には進めていない。
