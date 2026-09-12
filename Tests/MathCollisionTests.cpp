// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Toolbox/Quaternion.h"
#include "Toolbox/Complex.h"
#include "Toolbox/CollisionWorld.h"
#include "Toolbox/Platform.h"
using namespace Toolbox;
namespace
{
/**
 * 数値誤差を許してスカラーを比較する。
 */
bool Near(f32 A, f32 B, f32 Tolerance = 1e-4f)
{
	return Abs(A - B) <= Tolerance;
}
/**
 * 三軸の誤差をまとめて調べる。
 */
bool Near(FVector3 A, FVector3 B, f32 Tolerance = 1e-4f)
{
	return Near(A.X, B.X, Tolerance) && Near(A.Y, B.Y, Tolerance) && Near(A.Z, B.Z, Tolerance);
}
/**
 * 基準位置の周囲に、判定対象となる全六種類の形状を作る。
 */
TVector<FCollisionShape> ShapesAt(FVector3 Center)
{
	/**
	 * 基準形状の各軸の半分の長さ。
	 */
	const FVector3 Half{1, 1, 1};
	/**
	 * 構築した形状または演算結果。
	 */
	TVector<FCollisionShape> Result;
	Result.PushBack(FAABB{Center - Half, Center + Half});
	Result.PushBack(FOBB{Center, Half});
	Result.PushBack(FSphere{Center, 1});
	Result.PushBack(FCube{Center, 1});
	Result.PushBack(FConvex{{Center + FVector3{-1, -1, -1}, Center + FVector3{1, -1, -1}, Center + FVector3{0, 1, -1},
	                         Center + FVector3{0, 0, 1}}});
	Result.PushBack(
	    FMesh({Center + FVector3{-1, -1, 0}, Center + FVector3{1, -1, 0}, Center + FVector3{0, 1, 0}}, {0, 1, 2}));
	return Result;
}
} // namespace
TEST("SIMD vectors match scalar dot cross and arithmetic")
{
	/**
	 * 再現可能な入力を作る乱数生成器。
	 */
	FRandom Random(1234);
	for (int32 I = 0; I < 1000; ++I)
	{
		/**
		 * 検証対象の左側入力。
		 */
		const FVector3 A{static_cast<f32>(Random() % 100), static_cast<f32>(Random() % 100),
		                 static_cast<f32>(Random() % 100)};
		/**
		 * 検証対象の右側入力。
		 */
		const FVector3 B{static_cast<f32>(Random() % 100), static_cast<f32>(Random() % 100),
		                 static_cast<f32>(Random() % 100)};
		REQUIRE(Near(Dot(A, B), A.X * B.X + A.Y * B.Y + A.Z * B.Z));
		REQUIRE(Near(Cross(A, B), {A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X}));
		REQUIRE(Near(A + B, {A.X + B.X, A.Y + B.Y, A.Z + B.Z}));
		REQUIRE(Near(A * 2, {A.X * 2, A.Y * 2, A.Z * 2}));
	}
	REQUIRE(Normalize({}) == FVector3{});
	REQUIRE(Near(Length(Normalize({2, 3, 4})), 1));
}
TEST("SIMD matrix product matches the scalar reference")
{
	/**
	 * 検証対象の左側入力。
	 */
	FMatrix4 A;
	/**
	 * 検証対象の右側入力。
	 */
	FMatrix4 B;
	for (size_t I = 0; I < 16; ++I)
	{
		A.Values[I] = static_cast<f32>(I) * 0.25f;
		B.Values[I] = static_cast<f32>(16 - I) * 0.5f;
	}
	/**
	 * 検証対象の演算結果。
	 */
	const auto Product = A * B;
	for (size_t Row = 0; Row < 4; ++Row)
	{
		for (size_t Column = 0; Column < 4; ++Column)
		{
			/**
			 * 独立した計算から得た期待値。
			 */
			f32 Expected = 0;
			for (size_t K = 0; K < 4; ++K)
			{
				Expected += A.Values[Row * 4 + K] * B.Values[K * 4 + Column];
			}
			REQUIRE(Near(Product.Values[Row * 4 + Column], Expected));
		}
	}
}
TEST("Matrix inverse reverses translation rotation and scale")
{
	/**
	 * 位置・回転・拡大を合成した行列。
	 */
	const auto Matrix =
	    FMatrix4::Translation({7, 2, -3}) * FMatrix4::Rotation({1, 2, 3}, 1.2f) * FMatrix4::Scale({2, 3, 4});
	/**
	 * 検証する逆行列。
	 */
	FMatrix4 Inverse;
	REQUIRE(Matrix.TryInverse(Inverse));
	REQUIRE(Near(Inverse.TransformPoint(Matrix.TransformPoint({4, -2, 1})), {4, -2, 1}));
	/**
	 * 元の行列と逆行列の積。
	 */
	const auto Identity = Matrix * Inverse;
	for (size_t I = 0; I < 16; ++I)
	{
		REQUIRE(Near(Identity.Values[I], I % 5 == 0 ? 1.0f : 0.0f));
	}
	/**
	 * 失敗時の出力保持を確かめる変更前の値。
	 */
	const auto Before = Inverse;
	REQUIRE(!FMatrix4::Scale({1, 0, 1}).TryInverse(Inverse));
	REQUIRE(Inverse.Values == Before.Values);
}
TEST("Quaternion rotation agrees with matrix and inverse")
{
	/**
	 * 検証する回転。
	 */
	const auto Rotation = FQuaternion::FromAxisAngle({0, 0, 1}, 1.57079632679f);
	REQUIRE(Near(Rotation.Rotate({1, 0, 0}), {0, 1, 0}));
	REQUIRE(Near(Rotation.ToMatrix().TransformDirection({2, 3, 4}), Rotation.Rotate({2, 3, 4})));
	REQUIRE(Near(Rotation.Inverse().Rotate(Rotation.Rotate({2, 3, 4})), {2, 3, 4}));
	for (FVector3 Axis : {FVector3{1, 0, 0}, FVector3{0, 1, 0}, FVector3{0, 0, 1}})
	{
		/**
		 * 軸回りの半回転。
		 */
		const auto HalfTurn = FQuaternion::FromAxisAngle(Axis, 3.14159265359f);
		/**
		 * 逆変換で復元した値。
		 */
		const auto Restored = FQuaternion::FromMatrix(HalfTurn.ToMatrix());
		REQUIRE(Near(Restored.Rotate({1, 2, 3}), HalfTurn.Rotate({1, 2, 3})));
	}
}
TEST("Quaternion slerp uses the shortest rotation and handles opposite signs")
{
	/**
	 * Z軸回りの四分の一回転。
	 */
	const auto Quarter = FQuaternion::FromAxisAngle({0, 0, 1}, 1.57079632679f);
	/**
	 * 基準形状の各軸の半分の長さ。
	 */
	const auto Half = FQuaternion::Slerp({}, Quarter, 0.5f);
	REQUIRE(Near(Half.Rotate({1, 0, 0}), {0.70710678f, 0.70710678f, 0}));
	/**
	 * 符号が逆で同じ回転を表す四元数。
	 */
	const FQuaternion Negative{-Quarter.X, -Quarter.Y, -Quarter.Z, -Quarter.W};
	REQUIRE(Near(FQuaternion::Slerp(Quarter, Negative, 0.5f).Rotate({1, 0, 0}), {0, 1, 0}));
}
TEST("Complex arithmetic supports both precisions and rejects zero division")
{
	/**
	 * 検証対象の左側入力。
	 */
	const FComplex32 A{2, 3};
	/**
	 * 検証対象の右側入力。
	 */
	const FComplex32 B{4, -1};
	REQUIRE(A * B == FComplex32{11, 10});
	/**
	 * 逆変換で復元した値。
	 */
	const auto Restored = (A * B) / B;
	REQUIRE(Near(Restored.Real, A.Real));
	REQUIRE(Near(Restored.Imaginary, A.Imaginary));
	REQUIRE(A.Conjugate() == FComplex32{2, -3});
	REQUIRE(Near(FComplex32{3, 4}.Magnitude(), 5));
	/**
	 * 二乗が倍精度を超える大きな複素数。
	 */
	const FComplex64 Large{1e300, 1e300};
	/**
	 * 巨大な複素数同士の除算結果。
	 */
	const auto Ratio = Large / Large;
	REQUIRE(Abs(Ratio.Real - 1) < 1e-12);
	REQUIRE(Abs(Ratio.Imaginary) < 1e-12);
	/**
	 * 不正入力が例外で拒否されたか。
	 */
	bool Failed = false;
	try
	{
		(void)(A / FComplex32{});
	}
	catch (const FException&)
	{
		Failed = true;
	}
	REQUIRE(Failed);
}
TEST("All collision shape combinations overlap at origin and separate at distance")
{
	/**
	 * 原点を含む全種類の形状。
	 */
	const auto Origin = ShapesAt({});
	/**
	 * 原点から離した全種類の形状。
	 */
	const auto Distant = ShapesAt({10, 10, 10});
	for (const auto& A : Origin)
	{
		for (const auto& B : Origin)
		{
			REQUIRE(Intersects(A, B));
			REQUIRE(Intersects(B, A));
		}
		for (const auto& B : Distant)
		{
			REQUIRE(!Intersects(A, B));
		}
	}
}
TEST("GJK boxes agree with direct AABB overlap")
{
	/**
	 * 再現可能な入力を作る乱数生成器。
	 */
	FRandom Random(442);
	/**
	 * 検証する境界箱。
	 */
	const FAABB Box{{-1, -1, -1}, {1, 1, 1}};
	for (int32 I = 0; I < 400; ++I)
	{
		/**
		 * 比較相手の配置位置。
		 */
		const FVector3 Center{static_cast<f32>(Random() % 1000) / 100 - 5, static_cast<f32>(Random() % 1000) / 100 - 5,
		                      static_cast<f32>(Random() % 1000) / 100 - 5};
		/**
		 * 比較するもう一つの形状。
		 */
		const FAABB Other{Center - FVector3{1, 1, 1}, Center + FVector3{1, 1, 1}};
		REQUIRE(Intersects(FCollisionShape(Box), FCollisionShape(Other)) == Box.Intersects(Other));
	}
	REQUIRE(Intersects(FCube{}, FCube{{1, 0, 0}, 0.5f}));
	REQUIRE(Intersects(FSphere{{}, 1}, FSphere{{2, 0, 0}, 1}));
}
TEST("Rotated box sphere collisions match closest point distance")
{
	/**
	 * 検証する回転。
	 */
	const auto Rotation = FQuaternion::FromAxisAngle({1, 2, 3}, 0.7f);
	/**
	 * 検証する境界箱。
	 */
	FOBB Box;
	Box.HalfExtents = {1, 2, 0.5f};
	Box.Axes = {Rotation.Rotate({1, 0, 0}), Rotation.Rotate({0, 1, 0}), Rotation.Rotate({0, 0, 1})};
	/**
	 * 再現可能な入力を作る乱数生成器。
	 */
	FRandom Random(921);
	for (int32 I = 0; I < 200; ++I)
	{
		/**
		 * 検証するワールド座標。
		 */
		const FVector3 Point{static_cast<f32>(Random() % 600) / 100 - 3, static_cast<f32>(Random() % 600) / 100 - 3,
		                     static_cast<f32>(Random() % 600) / 100 - 3};
		/**
		 * 箱のローカル座標へ戻した球の中心。
		 */
		const FVector3 Local = Rotation.Inverse().Rotate(Point);
		/**
		 * 箱の表面または内部で球中心に最も近い点。
		 */
		const FVector3 Closest{Clamp(Local.X, -1.0f, 1.0f), Clamp(Local.Y, -2.0f, 2.0f), Clamp(Local.Z, -0.5f, 0.5f)};
		/**
		 * 独立した計算から得た期待値。
		 */
		const bool Expected = Length(Local - Closest) <= 0.4f;
		REQUIRE(Intersects(Box, FSphere{Point, 0.4f}) == Expected);
	}
}
TEST("Mesh narrow phase preserves holes and handles triangle contacts")
{
	/**
	 * 穴または分離された三角形を持つメッシュ。
	 */
	const FMesh Mesh({{-3, -1, 0}, {-1, -1, 0}, {-2, 1, 0}, {1, -1, 0}, {3, -1, 0}, {2, 1, 0}}, {0, 1, 2, 3, 4, 5});
	REQUIRE(!Intersects(Mesh, FSphere{{0, 0, 0}, 0.2f}));
	REQUIRE(Intersects(Mesh, FSphere{{2, 0, 0.2f}, 0.2f}));
	REQUIRE(!Intersects(Mesh, FSphere{{2, 0, 1}, 0.2f}));
	REQUIRE(Intersects(FConvex{{{0, 0, 0}}}, FSphere{{0, 0, 0}, 0}));
}
TEST("Mesh triangle tree prunes distant triangles automatically")
{
	/**
	 * 検証メッシュの頂点配列。
	 */
	TVector<FVector3> Vertices;
	/**
	 * 検証メッシュの三角形番号。
	 */
	TVector<uint32> Indices;
	for (uint32 I = 0; I < 100; ++I)
	{
		/**
		 * 格子上のX座標。
		 */
		const f32 X = static_cast<f32>(I % 10) * 10;
		/**
		 * 格子上のY座標。
		 */
		const f32 Y = static_cast<f32>(I / 10) * 10;
		Vertices.PushBack({X, Y, 0});
		Vertices.PushBack({X + 1, Y, 0});
		Vertices.PushBack({X, Y + 1, 0});
		Indices.PushBack(I * 3);
		Indices.PushBack(I * 3 + 1);
		Indices.PushBack(I * 3 + 2);
	}
	/**
	 * 穴または分離された三角形を持つメッシュ。
	 */
	const FMesh Mesh(Move(Vertices), Move(Indices));
	/**
	 * 内部空間分割から取得した三角形候補。
	 */
	const auto Candidates = Mesh.QueryTriangles_Internal({{-1, -1, -1}, {2, 2, 1}});
	REQUIRE(Candidates.Size() == 1);
	REQUIRE(Candidates[0] == 0);
	REQUIRE(Intersects(Mesh, FSphere{{0.1f, 0.1f, 0}, 0.1f}));
}
TEST("Collision world automatically selects quadtree and rebuilds moved shapes")
{
	/**
	 * 自動空間分割を検証するワールド。
	 */
	FCollisionWorld World;
	/**
	 * 更新と削除に使用する登録ID。
	 */
	TVector<FColliderId> Handles;
	for (int32 I = 0; I < 100; ++I)
	{
		/**
		 * 検証するワールド座標。
		 */
		const FVector3 Point{static_cast<f32>(I % 10) * 10, static_cast<f32>(I / 10) * 10, 0};
		Handles.PushBack(World.Add(FAABB{Point, Point + FVector3{1, 1, 0}}));
	}
	REQUIRE(World.FindPairs().IsEmpty());
	REQUIRE(World.GetStats().Index == ESpatialIndex::Quadtree);
	REQUIRE(World.GetStats().NarrowTests < 100);
	REQUIRE(World.Update(Handles[99], FAABB{{0, 0, 0}, {1, 1, 0}}));
	REQUIRE(World.FindPairs().Size() == 1);
	REQUIRE(World.Remove(Handles[99]));
	REQUIRE(!World.Remove(Handles[99]));
	REQUIRE(World.FindPairs().IsEmpty());
	REQUIRE(!World.Update(Handles[99], FSphere{}));
}
TEST("Octree pairs equal brute force after arbitrary insertions")
{
	/**
	 * 自動空間分割を検証するワールド。
	 */
	FCollisionWorld World;
	/**
	 * 総当たり照合用の全形状。
	 */
	TVector<FCollisionShape> Shapes;
	/**
	 * 再現可能な入力を作る乱数生成器。
	 */
	FRandom Random(71);
	for (int32 I = 0; I < 80; ++I)
	{
		/**
		 * 検証するワールド座標。
		 */
		const FVector3 Point{static_cast<f32>(Random() % 200) / 10, static_cast<f32>(Random() % 200) / 10,
		                     static_cast<f32>(Random() % 200) / 10};
		Shapes.PushBack(FSphere{Point, 2});
		World.Add(Shapes.Back());
	}
	/**
	 * 独立した計算から得た期待値。
	 */
	size_t Expected = 0;
	for (size_t I = 0; I < Shapes.Size(); ++I)
	{
		for (size_t J = I + 1; J < Shapes.Size(); ++J)
		{
			if (Intersects(Shapes[I], Shapes[J]))
			{
				++Expected;
			}
		}
	}
	REQUIRE(World.FindPairs().Size() == Expected);
	REQUIRE(World.GetStats().Index == ESpatialIndex::Octree);
	REQUIRE(World.GetStats().NarrowTests < Shapes.Size() * (Shapes.Size() - 1) / 2);
}
TEST("Collision filtering and world identity reject unrelated registrations")
{
	/**
	 * 登録元のワールド。
	 */
	FCollisionWorld First;
	/**
	 * IDを共有しない別ワールド。
	 */
	FCollisionWorld Second;
	/**
	 * 削除対象の登録ID。
	 */
	const auto Id = First.Add(FSphere{}, {1, 1});
	First.Add(FSphere{}, {2, 2});
	REQUIRE(First.FindPairs().IsEmpty());
	REQUIRE(First.Query(FSphere{}, {1, 1}).Size() == 1);
	REQUIRE(!Second.Remove(Id));
	REQUIRE(First.Remove(Id));
	/**
	 * 同じスロットを再使用した登録ID。
	 */
	const auto Replacement = First.Add(FSphere{});
	REQUIRE(!(Replacement == Id));
}
TEST("Invalid collision shapes cannot enter the spatial tree")
{
	/**
	 * 自動空間分割を検証するワールド。
	 */
	FCollisionWorld World;
	/**
	 * 不正入力が例外で拒否されたか。
	 */
	bool Failed = false;
	try
	{
		World.Add(FSphere{{}, -1});
	}
	catch (const FException&)
	{
		Failed = true;
	}
	REQUIRE(Failed);
	REQUIRE(World.FindPairs().IsEmpty());
	REQUIRE(!IsValid(FCollisionShape(FConvex{})));
	REQUIRE(!IsValid(FCollisionShape(FMesh{})));
}
TEST("Complex SIMD batches match scalar products and allow exact in-place output")
{
	/**
	 * 奇数個の入力により、SIMD末尾のスカラー処理も検証する。
	 */
	FComplex32 Left[5]{{2, 3}, {-4, 1}, {0, -2}, {7, 0}, {-0.5f, 1.25f}};
	/**
	 * 虚部の符号とゼロを含む右側入力。
	 */
	FComplex32 Right[5]{{4, -1}, {3, 2}, {-1, 0}, {0, 8}, {2, -4}};
	/**
	 * 独立したスカラー積の期待値。
	 */
	FComplex32 Expected[5];
	/**
	 * 倍精度演算でも同じ入力を使う。
	 */
	FComplex64 DoubleLeft[5];
	/**
	 * 倍精度の右側入力と、その場出力先。
	 */
	FComplex64 DoubleRight[5];
	for (size_t Index = 0; Index < 5; ++Index)
	{
		Expected[Index] = Left[Index] * Right[Index];
		DoubleLeft[Index] = {Left[Index].Real, Left[Index].Imaginary};
		DoubleRight[Index] = {Right[Index].Real, Right[Index].Imaginary};
	}
	MultiplyComplex(Left, Right, Left, 5);
	MultiplyComplex(DoubleLeft, DoubleRight, DoubleRight, 5);
	for (size_t Index = 0; Index < 5; ++Index)
	{
		REQUIRE(Left[Index] == Expected[Index]);
		REQUIRE(DoubleRight[Index].Real == Expected[Index].Real);
		REQUIRE(DoubleRight[Index].Imaginary == Expected[Index].Imaginary);
	}
	MultiplyComplex(Left, Left, Left, 5);
	for (size_t Index = 0; Index < 5; ++Index)
	{
		REQUIRE(Left[Index] == Expected[Index] * Expected[Index]);
	}
	MultiplyComplex(static_cast<const FComplex32*>(nullptr), nullptr, nullptr, 0);
	MultiplyComplex(static_cast<const FComplex64*>(nullptr), nullptr, nullptr, 0);
	/**
	 * 非空配列のnull入力は例外になる。
	 */
	bool Failed = false;
	try
	{
		MultiplyComplex(static_cast<const FComplex32*>(nullptr), Right, Left, 1);
	}
	catch (const FException&)
	{
		Failed = true;
	}
	REQUIRE(Failed);
}
TEST("Vector and quaternion normalization preserve extreme finite scales")
{
	REQUIRE(Near(Normalize({1e30f, -1e30f, 0}), {0.70710678f, -0.70710678f, 0}));
	REQUIRE(Near(Normalize({1e-30f, 0, 0}), {1, 0, 0}));
	REQUIRE(Near(FVector3{1e-39f, 0, 0} / 1e-39f, {1, 0, 0}));
	REQUIRE(Near(Length({1e30f, 0, 0}) / 1e30f, 1));
	REQUIRE(Near(Length({1e-30f, 0, 0}) / 1e-30f, 1));
	for (f32 Scale : {1e30f, 1e-30f})
	{
		/**
		 * 同一の半回転を巨大・微小な非単位四元数で表す。
		 */
		const FQuaternion Rotation{0, 0, Scale, 0};
		REQUIRE(Near(Rotation.Rotate({1, 2, 3}), {-1, -2, 3}));
		/**
		 * 逆元との積は元の四元数の長さに依存しない。
		 */
		const auto Product = Rotation * Rotation.Inverse();
		REQUIRE(Near(Product.W, 1));
		REQUIRE(Near(Product.X, 0));
		REQUIRE(Near(Product.Y, 0));
		REQUIRE(Near(Product.Z, 0));
	}
}
TEST("Matrix inversion pivots rows and rejects invalid tolerances without changing output")
{
	/**
	 * 対角がゼロで、行交換なしでは逆行列を得られない行列。
	 */
	FMatrix4 Matrix;
	Matrix.Values = {0, 2, 0, 0, 3, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 1};
	/**
	 * 逆行列を書き込む先。
	 */
	FMatrix4 Inverse;
	REQUIRE(Matrix.TryInverse(Inverse));
	REQUIRE(Near((Matrix * Inverse).TransformPoint({2, 3, 4}), {2, 3, 4}));
	/**
	 * 無効入力で出力が破壊されないことを確認する。
	 */
	const auto Before = Inverse;
	REQUIRE(!Matrix.TryInverse(Inverse, 0));
	REQUIRE(Inverse.Values == Before.Values);
	REQUIRE(Matrix.Transposed().Transposed().Values == Matrix.Values);
}
TEST("Spatial trees preserve contacts across partitions and after dimension changes")
{
	/**
	 * 初めはXY平面上に置き、後から高さのある登録を加えるワールド。
	 */
	FCollisionWorld World;
	for (int32 Index = 0; Index < 20; ++Index)
	{
		World.Add(FAABB{{f32(Index * 10), 0, 0}, {f32(Index * 10 + 1), 1, 0}});
	}
	REQUIRE(World.FindPairs().IsEmpty());
	REQUIRE(World.GetStats().Index == ESpatialIndex::Quadtree);
	/**
	 * 子領域をまたいで親ノードに保持される長い箱。
	 */
	const auto Spanning = World.Add(FAABB{{-1, -1, -1}, {192, 2, 1}});
	REQUIRE(World.FindPairs().Size() == 20);
	REQUIRE(World.GetStats().Index == ESpatialIndex::Octree);
	REQUIRE(World.Remove(Spanning));
	REQUIRE(World.Query(FAABB{{1.0f + 5e-6f, 0, 0}, {2, 1, 0}}).Size() == 1);
	REQUIRE(World.GetStats().Index == ESpatialIndex::Quadtree);
}
