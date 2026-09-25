# World問い合わせの索引（2D／3D）

2D（`FPhysicsWorld2D`）と3D（`FPhysicsWorld3D`）のWorld問い合わせは、Worldが自動で保つ索引（更新可能な軸平行境界の階層、動的AABB木）で候補を絞ってから、形状の詳細判定を行います。**利用者のコードに変更は要りません。** 登録・再構築・同期の呼出しもありません。

対象の問い合わせ: `RaycastClosest`、`SweepClosest`、`SweepClosestIgnoringInitialContacts`、`QueryContacts`、`OverlapAll`。これらを使う`ComputeSlideMove`、`ResolveCharacterOverlap`、`MoveAndSlide`、`ProbeCharacterGround`、`StepCharacter`、キャラクター移動Componentも、そのまま索引を使います。

## 結果と失敗は索引の有無で変わらない

索引は候補を減らすだけで、結果・失敗・世代・順序の契約は、全Colliderをスロット昇順に調べていた従来の実装（総当たり）と同じです。

| 契約 | 内容 |
|---|---|
| 最短の結果 | 割合が小さい方。同じ割合ならColliderスロットの小さい方（候補の訪問順によらない） |
| `OverlapAll` | 完全なID、重複なし、スロット昇順、値所有（一致しなければ確保しない） |
| `QueryContacts` | スロット昇順の先頭32件と全件数`TotalFound`（32件で走査を止めない） |
| 初期接触を除くスイープ | 通常のスイープと別の契約のまま（索引の境界では代用しない） |
| 失敗 | 入力・World状態・除外IDの検査は従来どおり。対象の形状の計算が拒否される場合の例外も従来どおり（下記） |
| 確保 | 無確保の問い合わせは、登録・移動の直後の最初の呼出しを含めて確保しない |

これは、同じWorldで索引と総当たりを切り替えて結果（例外のメッセージを含む）を比べる試験（乱数の操作列・大きな共通オフセット・非常に長い線分）、解析値の試験、変異試験で確かめています（[検証記録](../Development/QueryScale-2026-09-25.md)）。

## 自動で反映される変更

| 操作 | 索引への反映 |
|---|---|
| `CreateBody`・`AttachCollider` | 成功した直後の問い合わせから見える。登録の途中の確保失敗では索引に何も残らない |
| `DetachCollider`・`DestroyBody` | 成功した直後から候補に残らない。スロットを再利用しても世代で区別する |
| `SetBodyTransform` | Stepを待たず、そのBodyのすべてのColliderへ反映する（Staticを含む） |
| `SetColliderQueryCategory` | Stepは不要。カテゴリは候補ごとに判定するため、0化・復帰・変更はすぐ効く |
| 正常な`Step` | 積分・接触補正・連続衝突を含む最終姿勢へ、姿勢が実際に変わったBodyだけを合わせる |
| 途中で失敗した`Step` | 従来どおり問い合わせを拒否する。次の正常な`Step`の完了時に、実際の姿勢へ合わせ直す |
| World・Sceneの破棄 | 索引はWorldの一部として同時に解放する |

## 総当たりへ切り替える条件

次の場合、その問い合わせだけ従来の総当たりで処理します（結果は同じで、速さだけが違う）。

- 現在の姿勢のWorld形状が無効なColliderがあり、今回のカテゴリ・自己除外の対象に含まれる。例: 重心と中心の和がf32で表せない、OBBの格納した軸が単位長・直交でない。これらは従来どおり問い合わせを失敗させるため、索引で除外せず総当たりで同じ例外を出す。対象外（カテゴリ・自己除外）なら索引で成功する。
- 問い合わせ、または登録済みの形状の座標の絶対値が2^100を超える。この範囲では詳細判定のf32の途中計算があふれる可能性があるため、索引では判定しない。

`GetQueryDiagnostics()`の`FallbackRange`・`FallbackUnindexed`で回数を確認できます。

## 候補の判定の保守性

- 形状の境界は、実際に格納した軸（OBBの軸を正規化・直交化しない）、ローカル中心、Bodyの姿勢、半幅0の面・辺・点を含めて求め、丸めで内側へ縮まない余白を足す。
- スイープは半径を含む経路全体、`QueryContacts`は半径＋`Margin`の範囲を覆う。
- 候補の判定には、問い合わせと形状の座標の規模の2^-19倍の余白を足す。3Dの線分とOBBの詳細判定はf32で局所座標へ変換するため、非常に長い線分では丸めが1単位程度になり、この余白がないと候補から漏れる（試験で確認）。
- 形状の境界には移動用の余裕（0.1）を持たせ、そこから出たときだけ入れ直す。入れ直すときは、移動した向きへ移動量の2倍（各軸2以下）だけ広げる。これらは候補の範囲だけで、詳細判定の半径・接触の距離・接触余裕・法線の条件は変わらない。

## 計算量とメモリの目安

- 問い合わせ: 木の訪問はおおむね対数、詳細判定は問い合わせの範囲の近くにあるColliderの数に比例する。**常にO(log n)ではない。** 密集（多数のColliderが重なる）・大きな形状（床など）・長い線分・大きなMarginでは候補が増え、最悪は総当たりと同じ件数になる。総当たりへの切り替えの条件では全件を調べる。
- 更新: 移動したColliderごとに境界の計算と包含の確認。余裕を出たときの入れ直しは木の高さに比例する。StaticのBodyはStepでは調べない。
- 走査はスタックを持たず（親の番号をたどる）、確保しない。木の深さに上限はない（高さはAVLと同じ回転で抑える）。
- メモリ: Colliderスロットあたり、木のノード2個分（葉と内部）と、スロットの派生情報・保存したWorld形状。`IndexMemoryBytes`で確保済みの量を確認できる。

## 診断（任意）

```cpp
World.SetQueryDiagnosticsEnabled(true);          // 既定は無効。有効な間は同じWorldへの問い合わせを並行に行わない
World.ResetQueryDiagnostics();
// ... 問い合わせ・StepCharacterなど ...
const Dxf::FWorldQueryDiagnostics D = World.GetQueryDiagnostics();
// D.Contacts.Queries, D.Contacts.NarrowTests, D.Sweep.NodesVisited, D.IndexHeight, D.IndexMemoryBytes,
// D.IndexReinserts, D.FallbackRange, D.FallbackUnindexed ...
```

集計は物理の状態・問い合わせの結果に影響しません。索引の状態（Collider数・ノード数・高さ・保持領域）は、集計を無効にしていても読めます。`SetQueryIndexEnabled_Internal(false)`は検証用で、問い合わせを総当たりの参照経路で処理します（索引の更新は続けます）。

## 対象外

- Solver・連続衝突の接触の組の生成は、この索引を使いません（従来どおり）。多数のColliderがあるWorldでは`Step`の費用が次の支配項になります（[検証記録の性能測定](../Development/QueryScale-2026-09-25.md#性能測定)）。
- 動く床への追従、剛体との押し合い、カプセル形状は別の機能です。
