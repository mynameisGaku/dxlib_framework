// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_COLLISION_SHAPES_H
#define TOOLBOX_COLLISION_SHAPES_H
#include "Toolbox/Vector3.h"
#include "Toolbox/Array.h"
#include "Toolbox/Variant.h"
#include "Toolbox/Vector.h"
#include "Toolbox/SharedPtr.h"
namespace Toolbox
{
/**
 * 座標軸に平行な境界箱。MinとMaxが一致する軸は厚さゼロとして扱う。
 */
struct FAABB
{
	/**
	 * 各軸の最小座標。
	 */
	FVector3 Min;
	/**
	 * 各軸の最大座標。
	 */
	FVector3 Max;
	/**
	 * 最小値と最大値が有限で、順序が正しいか調べる。
	 */
	bool IsValid() const noexcept;
	/**
	 * 箱の中心を返す。
	 */
	FORCEINLINE FVector3 Center() const noexcept
	{
		return Min * 0.5f + Max * 0.5f;
	}
	/**
	 * 他の箱を境界も含めて完全に収めるか調べる。
	 * @param Other 演算または比較の相手。
	 */
	bool Contains(const FAABB& Other) const noexcept;
	/**
	 * 境界への接触を含めて重なるか調べる。
	 * @param Other 演算または比較の相手。
	 */
	bool Intersects(const FAABB& Other) const noexcept;
	/**
	 * 二つの箱を収める最小の境界箱を作る。
	 * @param Other 演算または比較の相手。
	 */
	FORCEINLINE FAABB Merged(const FAABB& Other) const noexcept
	{
		return {Toolbox::Min(Min, Other.Min), Toolbox::Max(Max, Other.Max)};
	}
};
/**
 * 任意の向きを持つ箱。Axesには互いに直交する単位軸を指定する。
 */
struct FOBB
{
	/**
	 * ワールド座標での中心。
	 */
	FVector3 Center;
	/**
	 * 各ローカル軸に沿う半分の長さ。
	 */
	FVector3 HalfExtents{0.5f, 0.5f, 0.5f};
	/**
	 * ローカルX・Y・Z軸のワールド方向。
	 */
	TArray<FVector3, 3> Axes{FVector3{1, 0, 0}, FVector3{0, 1, 0}, FVector3{0, 0, 1}};
};
/**
 * 中心から一定距離以内を占める球。
 */
struct FSphere
{
	/**
	 * ワールド座標での中心。
	 */
	FVector3 Center;
	/**
	 * 非負の半径。ゼロは点として扱う。
	 */
	f32 Radius = 0.5f;
};
/**
 * 三辺が同じ長さの、任意回転可能な立方体。
 */
struct FCube
{
	/**
	 * ワールド座標での中心。
	 */
	FVector3 Center;
	/**
	 * 各辺の半分の長さ。
	 */
	f32 HalfExtent = 0.5f;
	/**
	 * 互いに直交する単位軸。
	 */
	TArray<FVector3, 3> Axes{FVector3{1, 0, 0}, FVector3{0, 1, 0}, FVector3{0, 0, 1}};
};
/**
 * 頂点集合の凸包。頂点はワールド座標で、順序は問わない。
 */
struct FConvex
{
	/**
	 * 凸包を定義する一つ以上の有限な頂点。
	 */
	TVector<FVector3> Vertices;
};
/**
 * 三角形の表面集合。凹形状を扱えるが、閉じた内部を自動で充填しない。
 */
class FMesh
{
public:
	/**
	 * 空の無効形状を作る。
	 */
	FMesh() = default;
	/**
	 * 頂点と三角形を所有し、内部の四分木または八分木を自動構築する。
	 * @param Vertices 所有する頂点座標。
	 * @param Indices 三角形を構成する頂点番号。
	 */
	FMesh(TVector<FVector3> Vertices, TVector<uint32> Indices);
	/**
	 * 判定可能なデータがあるか調べる。
	 */
	bool IsValid() const noexcept;
	/**
	 * 変更できない頂点配列を返す。更新は新しいFMeshを作って行う。
	 */
	const TVector<FVector3>& GetVertices() const;
	/**
	 * 変更できない三角形番号を返す。
	 */
	const TVector<uint32>& GetIndices() const;
	/**
	 * 構築時に計算した境界箱を返す。
	 */
	FAABB GetBounds() const;
	/**
	 * 境界箱と重なる三角形だけを空間分割から取得する。
	 * @param Box 比較または検索する境界箱。
	 */
	TVector<size_t> QueryTriangles_Internal(const FAABB& Box) const;

private:
	/**
	 * 三角形と空間分割をまとめた不変データ。
	 */
	struct FData;
	/**
	 * コピー間で共有する不変データの所有権。
	 */
	TSharedPtr<FData> m_pData;
};
/**
 * 登録可能な衝突形状。保持する形状はコピーまたは移動で所有する。
 */
using FCollisionShape = TVariant<FAABB, FOBB, FSphere, FCube, FConvex, FMesh>;
/**
 * 形状の座標・サイズ・頂点番号が判定可能か調べる。
 * @param Shape 対象の衝突形状。
 */
bool IsValid(const FCollisionShape& Shape) noexcept;
/**
 * 形状全体を囲む境界箱を求める。不正な形状は例外で通知する。
 * @param Shape 対象の衝突形状。
 */
FAABB Bounds(const FCollisionShape& Shape);
/**
 * 実形状の重なりを判定する。接触を含み、Toleranceはワールド単位。
 * @param A 左側の入力値。
 * @param B 右側の入力値。
 * @param Tolerance 許容する数値誤差。
 */
bool Intersects(const FCollisionShape& A, const FCollisionShape& B, f32 Tolerance = 1e-5f);
} // namespace Toolbox
#endif
