# Toolbox

ToolboxはSTLに依存しない基盤モジュールです。CMakeでは`dxf::toolbox`、Visual Studioでは`dxf_toolbox`から利用できます。`dxf::framework`をリンクするとToolboxも含まれます。ヘッダーは`Toolbox/...`、名前空間は`Toolbox`です。通常のルートソリューションは7プロジェクトで、フレームワーク全体の公開84ヘッダーを各プロジェクトから参照できます。

## 基盤APIと移行

| 目的 | 使用するAPI |
|---|---|
| 整数・浮動小数点 | `int8`〜`int64`、`uint8`〜`uint64`、`f32`、`f64` |
| 可変長／固定長配列 | `TVector<T>`、`TArray<T, N>` |
| 文字列 | `FString`、`FWideString`、`FStringView` |
| 単独所有 | `TUniquePtr<T>`、`MakeUnique<T>()` |
| 共有所有／弱参照 | `TSharedPtr<T>`、`TWeakPtr<T>`、`MakeShared<T>()` |
| 連想配列／集合 | `TMap<K, V>`、`TSet<T>` |
| 任意値／選択型 | `TOptional<T>`、`TVariant<T...>` |
| コールバック | `TFunction<Signature>` |
| 移動・転送・基本演算 | `Move`、`Forward`、`Min`、`Max`、`Clamp` |
| パス・時刻 | `FPath`、`MonotonicNanoseconds()` |

今回の変更はソース互換性を壊します。公開APIの文字列・所有ポインタ・コールバック・パス等もToolbox型へ置き換えています。STL型との暗黙変換は提供しません。呼び出し側の型と生成処理も更新してください。

```cpp
#include "Toolbox/UniquePtr.h"
#include "Toolbox/Vector.h"
#include "Dxf/SlotMap.h"

/**
 * 所有領域へ登録する最小のオブジェクト。
 */
struct DProbe final : Dxf::DObject
{
};

/**
 * ゲームオブジェクトの所有領域。
 */
Dxf::TSlotMap<Dxf::DObject> Objects;
/**
 * 登録したオブジェクトの非所有ハンドル。
 */
auto Object = Objects.Insert(Toolbox::MakeUnique<DProbe>());
```

ToolboxのAPIはPascalCaseです。例えば配列の`push_back`・`size`・`empty`は`PushBack`・`Size`・`IsEmpty`、所有ポインタの`get`・`reset`は`Get`・`Reset`、弱参照の`lock`は`Lock`へ移行します。STL全体との互換APIではなく、フレームワークが必要とする機能を実装しています。

`TMap`・`TSet`は小規模集合向けの線形探索です。`TVector`の再確保は要素への参照・ポインタを無効化します。共有ポインタの参照カウントと、参照先オブジェクトのスレッド安全性は別です。参照先の変更やコンテナの並行アクセスには外側で同期が必要です。

## ベクトル・行列・複素数・Quaternion

`FVector3`は3次元ベクトル、`FMatrix4`は行優先の4×4行列です。行列は列ベクトルに作用し、`A * B`はBを先に適用します。位置には`TransformPoint`、方向には`TransformDirection`を使います。`TryInverse`は特異行列や非有限値でfalseを返し、出力を変更しません。

`FQuaternion`はX・Y・Zが虚部、Wが実部で、初期値は無回転です。`FromAxisAngle`の角度はラジアン、`Slerp`は最短経路の球面補間です。`ToMatrix`／`FromMatrix`で変換でき、`FromMatrix`は回転行列を要求します。

`FComplex32`／`FComplex64`は実部・虚部を持ち、四則演算・共役・絶対値・偏角・極形式に対応します。配列の積には両精度に対応する`MultiplyComplex(A, B, Output, Count)`を使えます。SSE2経路はf32の複素数を2個ずつ、f64は実部と虚部を2レーンで処理します。f32の奇数末尾はスカラーで計算します。出力を入力と完全に同じ配列にすることはできますが、部分的に重なる配列は渡さないでください。`Count`が0ならnullptrを渡せます。0より大きい場合のnullptrは例外で通知します。

ベクトル演算、行列積、複素数配列の積はSSE2対応環境でSIMDを使用します。非対応環境にはスカラー経路があります。全ての数学処理がSIMD化されるわけではなく、分岐や精度を優先する処理はスカラーです。最適化の有効性はデータ量と利用環境によって変わります。

## 衝突形状と自動空間分割

`FAABB`、`FOBB`、`FSphere`、`FCube`、`FConvex`、`FMesh`を`FCollisionShape`で保持します。座標はワールド座標、OBB／Cubeの軸は互いに直交する単位ベクトルで指定します。球の半径や箱の半径は非負にします。`FConvex`は頂点集合の凸包で、`FMesh`は三角形の表面集合です。

```cpp
#include "Toolbox/CollisionWorld.h"

/**
 * 登録形状と探索用の空間分割を所有する。
 */
Toolbox::FCollisionWorld World;
/**
 * 原点の球を更新・削除するための世代付きID。
 */
const auto Ball = World.Add(Toolbox::FSphere{{0, 0, 0}, 1.0f});
/**
 * 問い合わせ球と交差した登録。
 */
const auto Hits = World.Query(Toolbox::FSphere{{1, 0, 0}, 0.5f});
World.Update(Ball, Toolbox::FSphere{{3, 0, 0}, 1.0f});
/**
 * 現在重なっている全ペア。順序違いの重複はない。
 */
const auto Pairs = World.FindPairs();
World.Remove(Ball);
```

`Add`・`Update`・`Remove`の変更は次の問い合わせに自動反映します。登録が少ない場合は直接比較、XY平面に近い分布では四分木、立体的な分布では八分木を内部で選択します。大きく分割境界をまたぐ形状は親ノードに保持します。ユーザーが木を選択・再構築する必要はありません。`GetStats()`で選択方式・ノード数・候補数・詳細判定数を確認できます。形状が集中・巨大化した場合など、全用途で一定の高速化を保証するものではありません。

`FMesh`も構築時に三角形用の四分木／八分木を構築し、問い合わせ境界に近い三角形へ候補を絞ります。メッシュデータは不変として共有し、変更するときは新しい`FMesh`を作って`World.Update`へ渡します。`Intersects(A, B)`による形状同士の直接判定も可能です。

判定は接触を含みます。メッシュは閉じた内部を自動で充填しないため、表面に触れず完全に内部にある形状はメッシュと交差しません。ワールドは単一スレッドで使用します。連続衝突判定、貫通量・接触点の算出、剛体の押し戻し・物理応答は提供しません。
