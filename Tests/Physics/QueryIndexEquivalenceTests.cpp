// SPDX-License-Identifier: NOASSERTION
// World問い合わせの索引と、全Colliderをスロット昇順に調べる総当たり（参照経路）の結果の一致。
// 種を固定した乱数で、Body・Colliderの生成・着脱・破棄・スロット再利用、Stepを挟まない移動・回転、カテゴリの0化と復帰、
// 休止と起床を含むStepを繰り返し、各手順の後に全種別の問い合わせを両方の経路で行い、ID・割合・中心・法線・接触の順序・
// 例外のメッセージまで一致することを確かめる。失敗時は種・手順・問い合わせの番号を出す。
// 参照経路は同じWorldの同じ状態を使うため、IDは完全に一致する必要がある。同じ幾何計算の誤りは両方に出るため、
// 解析値の試験（WorldQuery・WorldSweep等の既存の試験）と併せて使う。
#include "QueryIndexTestSupport.h"
using namespace Toolbox;
using namespace Dxf;
using namespace PhysicsTest::QueryIndexTest;
namespace
{
// 1つの種で手順を繰り返し、各手順の後に問い合わせを比べる。
template <typename T> void RandomOperations_Internal(uint64 Seed, int32 Steps, int32 QueriesPerStep, f32 Extent)
{
	FRandom Random(Seed);
	typename T::FWorld World;
	// 生存と失効が混ざるBodyとColliderの一覧（失効したIDも除外IDとして使う）。
	TVector<typename T::FBody> Bodies;
	TVector<typename T::FCollider> Colliders;
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		const int32 Operation = Random.Below(10);
		if (Operation == 0 || Bodies.Size() < 4)
		{
			// Bodyの生成（種類は乱数、1～3個のCollider）。
			typename T::FBodyDescription Description;
			const int32 Type = Random.Below(3);
			Description.Type = Type == 0 ? EBodyType::Static : (Type == 1 ? EBodyType::Kinematic : EBodyType::Dynamic);
			Description.Position = T::Point(Random, Extent);
			const auto Body = World.CreateBody(Description);
			Bodies.PushBack(Body);
			const int32 Count = 1 + Random.Below(3);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				auto Collider = T::RandomShape(Random, 1.5f);
				Collider.QueryCategory = Random.Chance(15) ? 0u : static_cast<uint32>(1 + Random.Below(7));
				Colliders.PushBack(World.AttachCollider(Body, Collider));
			}
		}
		else if (Operation == 1)
		{
			// Bodyの破棄（Colliderのスロットは次の追加で再利用される）。
			World.DestroyBody(Bodies[static_cast<size_t>(Random.Below(static_cast<int32>(Bodies.Size())))]);
		}
		else if (Operation == 2)
		{
			// Colliderの追加（生存するBodyへ）。
			const auto Body = Bodies[static_cast<size_t>(Random.Below(static_cast<int32>(Bodies.Size())))];
			if (World.IsAlive(Body))
			{
				Colliders.PushBack(World.AttachCollider(Body, T::RandomShape(Random, 1.5f)));
			}
		}
		else if (Operation == 3 && !Colliders.IsEmpty())
		{
			World.DetachCollider(Colliders[static_cast<size_t>(Random.Below(static_cast<int32>(Colliders.Size())))]);
		}
		else if (Operation <= 5)
		{
			// Stepを挟まない移動・回転（Staticを含む）。
			const auto Body = Bodies[static_cast<size_t>(Random.Below(static_cast<int32>(Bodies.Size())))];
			if (World.IsAlive(Body))
			{
				T::RandomTransform(World, Body, Random, Extent);
			}
		}
		else if (Operation == 6 && !Colliders.IsEmpty())
		{
			// カテゴリの0化・復帰・変更。
			const auto Collider = Colliders[static_cast<size_t>(Random.Below(static_cast<int32>(Colliders.Size())))];
			if (World.IsColliderAlive(Collider))
			{
				World.SetColliderQueryCategory(Collider,
				                               Random.Chance(40) ? 0u : static_cast<uint32>(1 + Random.Below(7)));
			}
		}
		else
		{
			// 積分・接触・休止を含むStep（1～4分割）。
			World.Step(1.0 / 60.0, static_cast<uint32>(1 + Random.Below(4)));
		}
		CompareQueries<T>(World, Random, Bodies, QueriesPerStep, Extent * 1.3f, Seed, Step);
		// 索引の状態の整合（入っている数＋入れられない数＝生存数）。
		const FWorldQueryDiagnostics Diagnostics = World.GetQueryDiagnostics();
		PHYSICS_REQUIRE(Diagnostics.IndexedColliders + Diagnostics.UnindexedColliders == Diagnostics.AliveColliders);
	}
}
// 密集・疎・偏り・大きな床と小さな形状・古い削除スロットを含む、配置ごとの一致。
template <typename T> void Layouts_Internal()
{
	for (uint64 Seed = 1; Seed <= 6; ++Seed)
	{
		RandomOperations_Internal<T>(Seed * 7919, 120, 12, Seed % 2 == 0 ? 6.0f : 40.0f);
	}
	// 大きな床1枚と小さな形状多数。
	FRandom Random(424242);
	typename T::FWorld World;
	typename T::FBodyDescription Fixed;
	Fixed.Type = EBodyType::Static;
	const auto Level = World.CreateBody(Fixed);
	typename T::FColliderDescription Floor;
	if constexpr (sizeof(typename T::FVector) == sizeof(FVector2))
	{
		Floor.Shape = FOrientedBox2D{{0, -1}, {5000, 1}, 0};
	}
	else
	{
		Floor.Shape = FOBB{{0, -1, 0}, {5000, 1, 5000}};
	}
	World.AttachCollider(Level, Floor);
	TVector<typename T::FBody> Bodies;
	for (int32 Index = 0; Index < 300; ++Index)
	{
		const auto Body = World.CreateBody(Fixed);
		World.AttachCollider(Body, T::RandomShape(Random, 0.1f));
		T::Place(World, Body, T::Point(Random, 60));
		Bodies.PushBack(Body);
	}
	CompareQueries<T>(World, Random, Bodies, 400, 70, 424242, 0);
	// 片側への偏り（全員が一点の近くに重なる）。
	for (size_t Index = 0; Index < Bodies.Size(); ++Index)
	{
		T::Place(World, Bodies[Index], T::At(Random.Range(29, 30), Random.Range(0, 0.5f), Random.Range(29, 30)));
	}
	CompareQueries<T>(World, Random, Bodies, 400, 35, 424242, 1);
	// 削除済みスロットが多い状態。
	for (size_t Index = 0; Index < Bodies.Size(); Index += 2)
	{
		World.DestroyBody(Bodies[Index]);
	}
	CompareQueries<T>(World, Random, Bodies, 400, 35, 424242, 2);
}
// 大きな共通オフセット（約3e7）: 詳細判定のf32の途中計算の丸めが大きい座標でも、索引の候補が取りこぼさない。
// 形状の大きさの近くを通る問い合わせを多数比べる（余裕を持たせた境界0.1より丸めが大きい範囲）。
template <typename T> void LargeOffset_Internal()
{
	FRandom Random(1618033);
	typename T::FWorld World;
	typename T::FBodyDescription Fixed;
	Fixed.Type = EBodyType::Static;
	const typename T::FVector Origin = T::At(3.0e7f, -2.0e7f, 2.5e7f);
	TVector<typename T::FBody> Bodies;
	for (int32 Index = 0; Index < 150; ++Index)
	{
		const auto Body = World.CreateBody(Fixed);
		World.AttachCollider(Body, T::RandomShape(Random, 0.5f));
		T::RandomTransform(World, Body, Random, 1);
		T::Place(World, Body, T::Offset(Origin, T::Point(Random, 60)));
		if (Random.Chance(50))
		{
			T::RandomTransform(World, Body, Random, 1);
			T::Place(World, Body, T::Offset(Origin, T::Point(Random, 60)));
		}
		Bodies.PushBack(Body);
	}
	CompareQueries<T>(World, Random, Bodies, 3000, 64, 1618033, 0, Origin);
}
// 非常に長い線分（長さ約2e7）が形状の近くをかすめる: 詳細判定の局所座標の丸め（線分の長さに比例し、1単位程度）が
// 余裕を持たせた境界より大きくなるため、候補の判定の余白がなければ取りこぼす。両方の経路の結果（例外を含む）を比べる。
template <typename T> void LongSegments_Internal()
{
	FRandom Random(2718281);
	typename T::FWorld World;
	typename T::FBodyDescription Fixed;
	Fixed.Type = EBodyType::Static;
	TVector<typename T::FVector> Centers;
	for (int32 Index = 0; Index < 80; ++Index)
	{
		// 生成時の姿勢で置く（移動による索引の予測の広がりがなく、余裕は0.1だけ）。
		typename T::FBodyDescription Description = Fixed;
		T::RandomPose(Description, Random, 20);
		const auto Body = World.CreateBody(Description);
		World.AttachCollider(Body, T::RandomShape(Random, 0.2f));
		Centers.PushBack(T::Position(World, Body));
	}
	for (int32 Query = 0; Query < 4000; ++Query)
	{
		// 形状の近く（±3）の点を通る、ほぼ任意の向きの長い線分。
		const typename T::FVector Near = T::Offset(Centers[static_cast<size_t>(Random.Below(80))], T::Point(Random, 3));
		const typename T::FVector Direction = T::Point(Random, 1);
		const typename T::FVector A = T::Offset(Near, T::Scale(Direction, -1.0e7f));
		const typename T::FVector B = T::Offset(Near, T::Scale(Direction, 1.0e7f));
		auto Call = [&](bool bReference)
		{
			World.SetQueryIndexEnabled_Internal(!bReference);
			auto Result = Capture<decltype(World.RaycastClosest(A, B))>(
			    [&]
			    {
				    return World.RaycastClosest(A, B);
			    });
			World.SetQueryIndexEnabled_Internal(true);
			return Result;
		};
		const bool bSame = SameOutcome(Call(false), Call(true),
		                               [](const auto& X, const auto& Y)
		                               {
			                               return SameSegment(X, Y);
		                               });
		if (!bSame)
		{
			char Message[128];
			snprintf(Message, sizeof(Message), "%s long segment mismatch: query=%d", T::Name, Query);
			throw FException(static_cast<const char*>(Message));
		}
	}
}
const PhysicsTest::FCase Cases_Internal[] = {
    {"2D query index matches the reference over random operation sequences", &Layouts_Internal<F2D>},
    {"3D query index matches the reference over random operation sequences", &Layouts_Internal<F3D>},
    {"2D query index matches the reference at a large common offset", &LargeOffset_Internal<F2D>},
    {"3D query index matches the reference at a large common offset", &LargeOffset_Internal<F3D>},
    {"2D query index matches the reference for very long grazing segments", &LongSegments_Internal<F2D>},
    {"3D query index matches the reference for very long grazing segments", &LongSegments_Internal<F3D>}};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetQueryIndexEquivalenceCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
