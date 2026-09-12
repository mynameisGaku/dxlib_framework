// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/GameObject.h"
#include "Dxf/GameObjectComponent.h"
#include "Dxf/GameObjectCollection.h"
#include "Dxf/AssetService.h"
#include "Toolbox/Function.h"
using namespace Dxf;
using namespace Dxf::Testing;
namespace
{
// 固定更新の呼び出しを数える。
struct FFixedCounters
{
	// 固定更新フックを呼んだ回数。
	Toolbox::int32 FixedTick = 0;
	// 実際に記録された呼び出し順序。
	Toolbox::TVector<Toolbox::FString> Order;
};
// 固定更新フックを持つ検証用コンポーネント。
class DFixedComponent final : public DGameObjectComponent
{
public:
	// 検証に必要な依存先と初期状態を設定する。
	explicit DFixedComponent(FFixedCounters& Counters, Toolbox::TFunction<void()> Fixed = {})
	    : m_pCounters(&Counters), m_Fixed(Toolbox::Move(Fixed))
	{
	}

protected:
	// 固定更新の呼び出しを観測し、指定された処理を実行する。
	void OnFixedTick(const FFixedTickContext&) override
	{
		++m_pCounters->FixedTick;
		m_pCounters->Order.PushBack(Toolbox::FString("component"));
		if (m_Fixed)
		{
			m_Fixed();
		}
	}

private:
	// 外部の集計への参照。
	FFixedCounters* m_pCounters;
	// 固定更新中の処理を再現するコールバック。
	Toolbox::TFunction<void()> m_Fixed;
};
// 固定更新フックを持つ検証用オブジェクト。
class DFixedObject : public DGameObject
{
public:
	// 検証に必要な依存先と初期状態を設定する。
	explicit DFixedObject(FFixedCounters& Counters, Toolbox::TFunction<void()> Fixed = {})
	    : m_pCounters(&Counters), m_Fixed(Toolbox::Move(Fixed))
	{
	}

protected:
	// 固定更新の呼び出しを観測し、指定された処理を実行する。
	void OnFixedTick(const FFixedTickContext&) override
	{
		++m_pCounters->FixedTick;
		m_pCounters->Order.PushBack(Toolbox::FString("object"));
		if (m_Fixed)
		{
			m_Fixed();
		}
	}

private:
	// 外部の集計への参照。
	FFixedCounters* m_pCounters;
	// 固定更新中の処理を再現するコールバック。
	Toolbox::TFunction<void()> m_Fixed;
};
// 固定更新の実行環境を作る。
struct FFixedFixture
{
	// 検証対象が参照するバックエンド。
	FFakeBackend Backend;
	// 検証用の資源管理。
	FAssetService Assets{Backend, Backend, Backend};
	// 検証用の入力情報。
	FInputSnapshot Input;
	// 未配達の押下を保持する入力列。
	Toolbox::TVector<FInputSnapshot> Pending;
	// 初期化の実行環境を返す。
	FInitContext GetInit()
	{
		return {Assets};
	}
	// 固定更新の実行環境を返す。
	FFixedTickContext GetFixed()
	{
		return {Input, Pending};
	}
};
} // namespace
TEST("FixedTick dispatches to object before components")
{
	FFixedFixture Fixture;
	FFixedCounters Counts;
	FGameObjectCollection Objects;
	// 検証対象のオブジェクト。
	auto Object = Objects.Spawn<DFixedObject>(Counts).Value();
	REQUIRE(Object.Get()->AddComponent<DFixedComponent>(Counts));
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(Fixture.GetInit()));
	REQUIRE(Objects.FixedTick_Internal(Fixture.GetFixed()));
	REQUIRE(Counts.FixedTick == 2);
	REQUIRE(Counts.Order.Size() == 2);
	REQUIRE(Counts.Order[0] == Toolbox::FString("object"));
	REQUIRE(Counts.Order[1] == Toolbox::FString("component"));
}
TEST("FixedTick exception becomes failure result")
{
	FFixedFixture Fixture;
	FFixedCounters Counts;
	FGameObjectCollection Objects;
	auto Object = Objects.Spawn<DFixedObject>(Counts).Value();
	REQUIRE(Object.Get()->AddComponent<DFixedComponent>(Counts, [] { throw Toolbox::FException("fixed update"); }));
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(Fixture.GetInit()));
	REQUIRE(!Objects.FixedTick_Internal(Fixture.GetFixed()));
}
TEST("FixedTick respects pause unless allowed")
{
	FFixedFixture Fixture;
	FFixedCounters Counts;
	FGameObjectCollection Objects;
	auto Object = Objects.Spawn<DFixedObject>(Counts).Value();
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(Fixture.GetInit()));
	FFixedTickContext Paused = Fixture.GetFixed();
	Paused.bPaused = true;
	REQUIRE(Objects.FixedTick_Internal(Paused));
	REQUIRE(Counts.FixedTick == 0);
	Object.Get()->SetTickWhenPaused(true);
	REQUIRE(Objects.FixedTick_Internal(Paused));
	REQUIRE(Counts.FixedTick == 1);
}
TEST("FixedTick skips destroy requested objects")
{
	FFixedFixture Fixture;
	FFixedCounters Counts;
	FGameObjectCollection Objects;
	auto Object = Objects.Spawn<DFixedObject>(Counts).Value();
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(Fixture.GetInit()));
	Object.Get()->Destroy();
	REQUIRE(Objects.FixedTick_Internal(Fixture.GetFixed()));
	REQUIRE(Counts.FixedTick == 0);
}
TEST("FixedTick rejects reentrant dispatch")
{
	FFixedFixture Fixture;
	FFixedCounters Counts;
	FGameObjectCollection Objects;
	TObjectHandle<DFixedObject> Self;
	TResult<void> Inner;
	auto Object = Objects.Spawn<DFixedObject>(Counts, [&] { Inner = Self.Get()->FixedTick_Internal(Fixture.GetFixed()); }).Value();
	Self = Object;
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(Fixture.GetInit()));
	REQUIRE(Objects.FixedTick_Internal(Fixture.GetFixed()));
	REQUIRE(!Inner);
	REQUIRE(Counts.FixedTick == 1);
}
