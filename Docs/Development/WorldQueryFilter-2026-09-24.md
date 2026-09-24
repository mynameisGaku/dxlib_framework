# 線分問い合わせの対象フィルター 検証記録

開始SHA: `0193442ee5998a8045c80715bb8f7c2b4199a667`。main / origin/main一致、開始時の作業ツリーはクリーン（未追跡・stage済み変更なし）。過去の記録（[2Dの記録](WorldSegmentQuery2D-2026-09-24.md)、[3Dの記録](WorldSegmentQuery-2026-09-24.md)、Native診断記録）は変更していない。対象は本記録と同じコミットの実装・試験・文書。

## 変更と責務

- `FColliderDescription2D` / `FColliderDescription3D` の末尾へ `Toolbox::uint32 QueryCategory = 1u` を追加。内部のCollider登録へ保持し、外部のMapや別のCollisionWorldへ二重登録しない。スロット再利用時は新しいDescriptionの値で登録全体を作り直すため、以前のカテゴリは残らない。
- 2D／3D共通の値型 `FWorldQueryFilter`（`Dxf/WorldQueryFilter.h`、`IncludeCategories = 0xffffffffu`）を追加。整数型だけに依存し、2D／3Dの形状ヘッダーを取り込まない。
- 各Worldへ `RaycastClosest(Start, End, ExcludedBody, const FWorldQueryFilter& Filter) const` を追加（第3・第4引数に既定引数なし）。既存の3引数版は全ビットのフィルターへ委譲し、走査と交点計算は4引数版だけに置いた。判定は `(QueryCategory & IncludeCategories) != 0`。対象外のColliderは所有Bodyの参照・形状変換・交差計算の前に飛ばし、最短候補の選定前に絞る。
- 各Worldへ `SetColliderQueryCategory` / `GetColliderQueryCategory` を追加。完全な世代付きIDとStep状態（Snapshotと共有する既存ガード）を検査し、失敗時は値を変えない。Setterはカテゴリだけを書き換える。
- 状態ガードは各Worldの内部関数 `RequireQueryState_Internal` にまとめ、問い合わせとカテゴリ操作で共有した。これに伴い3Dのガードの例外文言が「World query requires…」から「3D world query requires…」へ変わった（文言に依存する試験・文書はなし）。
- 既存の `Toolbox::FCollisionFilter`（双方向の衝突フィルター）、接触・CCD・休止・積分、Snapshotのスキーマ、Debugの保存Snapshot選択、2D/3Dの幾何計算、モデル・描画、サンプルは変更していない。新しいMutex・Job・索引・キャッシュは追加していない。
- 通常経路はO(保持Colliderスロット数)、追加領域O(1)のまま。ビット判定の追加による速度・割当の測定は行っていない。

[API・利用例・契約](../Physics/WorldSegmentQuery.md#問い合わせ対象の絞り込み2d3d共通)。

## 構成と結果（最終コード）

Windows x64、Visual Studio 18 Community / MSVC 19.51、C++20。`Build/FbxContinuation`（Visual Studio 18 2026、x64、`ThirdParty/DxLib-3.25a-source` を自動選択）を、TESTS・NATIVE・STARTER・EXAMPLE・MODEL_VIEWER・RENDER_DEBUG・NATIVE_SMOKE・RUN_DEVICE_TESTSを明示ONにして再生成（終了0）。SDKの再構築は行っていない。最終コード（Docsを除く差分と新規ファイル）の指紋は `Build/WorldQueryFilter-RootLogs/code-fingerprint.txt`（`6da2c056…b931`）で、下表の全工程の前後で一致を確認した。

| 工程 | 実行結果 | 終了コード |
|---|---|---|
| rootビルド | Debug / Release成功。新規ファイル由来の警告なし | 各0 |
| root CTest（`-j 1 --no-tests=error --output-on-failure`、Debug→Releaseの直列） | Debug 25/25、Release 25/25。実デバイス4群を含む | 各0 |
| PhysicsContinuation内部（root・単独とも） | 各構成188/188。うち新規フィルター20、2D問い合わせ9、3D問い合わせ7 | 各0 |
| NativeModelDeviceSmoke | Debug・Releaseとも `REAL_SDK_MODEL_SMOKE_PASSED failures=0`（全群内の各1回。単独の反復なし） | 各0 |
| 単独 `python Tools/ValidateDebug.py --logs Build/WorldQueryFilter-StandaloneLogs` | Debug 21/21、Release 21/21、8工程PASS。実DxLib SDK不使用 | 0 |
| No-STL | 317ファイル、違反0 | 0 |
| Python `unittest discover -s Tools/Tests` | 24/24 | 0 |
| 配布 `python Tools/ValidatePackage.py --logs Build/WorldQueryFilter-PackageLogs`（x64開発者環境） | Native OFF・Debug、8段階PASS | 0 |
| `git diff --check` | 問題なし。変更ファイルはUTF-8・CRLF（BOMなしのまま） | 0 |

CTest群はroot 25、単独21のまま。新規20ケースは既存PhysicsContinuationへ正規登録（`CMake/PhysicsTests.cmake`）で追加し、単独入口に本体cppを手列挙していない。群数と内部ケースを足し合わせていない。

配布は移動したインストールを使い、`dxf::physics` だけにリンクした `PhysicsOnly` が、既存の2D／3D手順に続けて、2D: カテゴリ付き登録→手前の不一致を飛ばし自己除外と併用して奥の一致を取得→Setterでカテゴリ0へ変更しGetterで確認→Stepなしで対象外、3D: カテゴリ付き登録→割合0.55の一致→一致Bodyの自己除外で空→カテゴリ変更→Body削除後のID失効、を実行した。段階数は8のまま。SDK未導入PCでの検証ではない。

## 新規回帰の範囲（`Tests/Physics/WorldQueryFilterTests.cpp`）

同じ10の契約を、次元ごとの型と登録操作（`F2D` / `F3D`）で実`FPhysicsWorld2D`と実`FPhysicsWorld3D`の両方に実行する（計20ケース）。共通のビット判定だけをWorld統合の根拠にしていない。

1. 互換性: 既定カテゴリ1とマスク全ビットの既定値、従来の2引数・`{}`・自己除外の3引数呼出しと明示全ビットの4引数呼出しの結果（Collider・割合・交点）が一致。
2. 選択: x=3拾得物・x=5壁・x=8キャラクター。壁とキャラクターを対象にすると手前の壁（割合0.45）で止まる、壁の先からはキャラクター、キャラクターだけなら奥のキャラクター（0.75）。
3. ビット集合: カテゴリ0は全ビット・従来入口でも対象外、複数ビット所属と各ビット検索、最上位ビット、複数ビットのマスク、一致なし・マスク0は空。
4. Collider単位: 同一Bodyの異なるカテゴリを個別に判定し、自己Body指定は一致カテゴリのColliderもまとめて除外。
5. 即時変更: Setter後の同じStepIndex（0）で結果が変わり、Getterで値を確認、カテゴリ0→復元で対象へ戻る、IDは生存のまま。
6. IDと再利用: 別World（同じスロット番号）・明示無効・Detach済みのSetter／Getterを拒否し既存値を変えない、再登録は新しいDescriptionの値、Body削除後の旧世代Collider・旧世代Body除外を拒否（マスク0でも）。
7. 検証の先行: マスク0でもゼロ長・NaN・表現不能な変位・明示無効IDを拒否、全Colliderがカテゴリ0でも正常な空、停止済みJobSystemの故障注入でStepを途中失敗させると問い合わせ・Setter・Getterを拒否し、正常Step後に回復。
8. 候補外の計算: 変換不能な形状（f32最大値のBody位置＋ローカル中心）が対象外なら失敗せず割合0の候補を返す、マスクに含めると失敗、カテゴリを一致へ変えても失敗（先行する割合0でも隠さない）、そのBodyを除外すれば成功。
9. 非変更: 問い合わせ・カテゴリ変更（0→別カテゴリ）を行うWorldと行わない対照Worldへ同じ力・トルクを与え、20Stepの位置・角度（3Dは四元数）・速度・角速度・休止・Step数が一致。World識別子の一致は要求しない。
10. 接触と観察の分離: 床と箱をカテゴリ0にしたWorldと既定カテゴリのWorldで600Step後の状態が一致して休止、Snapshotに2 Collider、問い合わせでは見つからず、Setter後に見つかり、休止・位置・Step数は不変。

Step実行中に別スレッドから問い合わせる試験は行っていない（外側で直列化する契約。データ競合をガードの試験に使わない）。非変更の観察にはテスト側でCaptureSnapshotのStepIndexを使うが、製品の問い合わせ処理はSnapshotを作らない。

## 試験先行・変異試験

- 試験と登録を先に追加し、実装前にビルド: `dxf_physics_tests` がAPI・型・フィールド不在のコンパイルエラー52件で失敗（終了1、`Build/WorldQueryFilter-RootLogs/tdd-red-compile.log`）。
- 実装後の最初の実行で188/188成功。実装済みAPIでの挙動のRedや、修正前の不具合再現は経ていない。
- 変異試験（新規コードへ一つずつ故意の欠陥を入れ、ビルド・実行し、元へ戻す）: 実施した6件すべて検出（`Build/WorldQueryFilter-RootLogs/mutation.log`）。2Dでマスクを無視（8ケース失敗）、3Dでマスクを無視（8ケース失敗）、2Dで最短を選んでから絞る（5ケース失敗）、3Dでスロット再利用時に以前のカテゴリを引き継ぐ（IDと再利用のケース）、2Dで形状計算の後に絞る（候補外の計算のケース）、3DでSetter／GetterのStep状態ガードを外す（検証の先行のケース）。

## 途中の失敗

変異試験スクリプトで、再利用の変異の置換対象（`Slot = Record;`）がBody登録側にも一致して2回見つかり、変更を書き込む前に停止した。Collider登録側の文脈へ絞って再実行した。製品コード・試験の失敗ではない。ビルド・CTestの失敗はなかった。

## モデル実描画の偶発終了

今回のroot全群（Debug・Release各1回）ではNativeModelDeviceSmokeは成功し、早期終了は発生しなかった。限定回数の結果であり、以前の偶発終了の原因は引き続き特定していない。bc4cfc8の診断・厳格な判定は保持した。

## 再実行とログ

```powershell
cmake -S . -B Build/FbxContinuation -DDXF_BUILD_TESTS=ON -DDXF_BUILD_NATIVE=ON -DDXF_BUILD_STARTER=ON -DDXF_BUILD_EXAMPLE=ON -DDXF_BUILD_MODEL_VIEWER=ON -DDXF_BUILD_RENDER_DEBUG=ON -DDXF_BUILD_NATIVE_SMOKE=ON -DDXF_RUN_DEVICE_TESTS=ON
cmake --build Build/FbxContinuation --config Debug --parallel 6
ctest --test-dir Build/FbxContinuation -C Debug -j 1 --no-tests=error --output-on-failure
cmake --build Build/FbxContinuation --config Release --parallel 6
ctest --test-dir Build/FbxContinuation -C Release -j 1 --no-tests=error --output-on-failure
python Tools/ValidateDebug.py --logs Build/WorldQueryFilter-StandaloneLogs
python Tools/CheckNoStl.py
python -m unittest discover -s Tools/Tests -v
# Visual Studioのx64開発者環境で実行する。
python Tools/ValidatePackage.py --logs Build/WorldQueryFilter-PackageLogs
git diff --check
```

rootのログは `Build/WorldQueryFilter-RootLogs/`（`configure.log`、`*-build.log`、`*-ctest.log`、`*-LastTest.log`、`*-ctest-list.log`、No-STL・Python・配布のコンソール出力）、単独は `Build/WorldQueryFilter-StandaloneLogs/`（Summary.json、`physics-cases-*.log`）、配布は `Build/WorldQueryFilter-PackageLogs/`。生成物・ログ・SDKはコミットに含めない。

## 未実施・対象外

SDK未導入PC、開発ツール未導入PC、実Direct3D9、物理入力・聴感、全D3D/COM資源のリーク列挙、非WindowsのSanitizer、速度・割当の専用測定は未実施。配布はNative無効・Debugであり、Native有効配布・Release配布・別PCの成功へ読み替えない。衝突応答用のカテゴリ、センサー、接触イベント、全Hit一覧、任意コールバック・除外ID配列、索引・キャッシュ、Snapshotへのカテゴリ保存、画面選択には進めていない。
