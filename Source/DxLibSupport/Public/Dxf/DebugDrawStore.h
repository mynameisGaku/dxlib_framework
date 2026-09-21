// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_DEBUG_DRAW_STORE_H
#define DXF_DEBUG_DRAW_STORE_H
#include "Dxf/RenderGeometry3D.h"
#include "Dxf/RenderCommands.h"
#include "Toolbox/JobSystem.h"
namespace Dxf
{
/**
 * 生ポインタを保持しない、World・ObjectまたはScope・世代の識別値。
 */
struct FDebugOwnerKey
{
	/**
	 * Worldまたは所有システムの識別値。
	 */
	Toolbox::uint64 World = 0;
	/**
	 * Object/Scopeの番号。ゼロも有効。
	 */
	Toolbox::uint64 Id = 0;
	/**
	 * 再利用を区別する世代。ゼロは無効。
	 */
	Toolbox::uint64 Generation = 0;
	/**
	 * 同一の所有対象か。
	 */
	bool operator==(const FDebugOwnerKey&) const = default;
};
/**
 * 一フレーム、秒数、明示解除までの保持期間。
 */
enum class EDebugLifetime
{
	Frame, Seconds, Persistent
};
/**
 * 寿命の計算に使う時間。ゲーム停止中もRealは進められる。
 */
enum class EDebugClock
{
	Game, Real
};
/**
 * デバッグ図形の分類と所有・寿命。描画状態は含まない。
 */
struct FDebugDrawTag
{
	/**
	 * 一つ以上のカテゴリビット。
	 */
	Toolbox::uint64 Category = 1;
	/**
	 * 選択フィルターの対象。無効値は匿名図形。
	 */
	FDebugOwnerKey Owner;
	/**
	 * 一括解放するScope。生オブジェクトは参照しない。
	 */
	FDebugOwnerKey Scope;
	/**
	 * 有効期間の方式。
	 */
	EDebugLifetime Lifetime = EDebugLifetime::Frame;
	/**
	 * Secondsの場合の正の寿命。
	 */
	Toolbox::f64 Seconds = 0;
	/**
	 * 秒数を減算する時間系。
	 */
	EDebugClock Clock = EDebugClock::Game;
};
/**
 * ビューごとの重ね表示の選別。Store自体の内容を変更しない。
 */
struct FDebugDrawFilter
{
	/**
	 * 表示対象のカテゴリビット。
	 */
	Toolbox::uint64 Categories = ~Toolbox::uint64{0};
	/**
	 * 一つの選択対象だけを表示するか。
	 */
	bool bSelectedOnly = false;
	/**
	 * 選択したObjectの値ID。
	 */
	FDebugOwnerKey Selected;
};
namespace Detail
{
/**
 * @param Value ワールド形状。Native資源を所有しない。
 */
FORCEINLINE bool ValidDebugPayload_Internal(const FGeometryCommand3D& Value) noexcept
{
	return IsValidGeometry3D(Value);
}
/**
 * @param Value ピクセル座標の補助線。
 */
FORCEINLINE bool ValidDebugPayload_Internal(const FLineCommand2D& Value) noexcept
{
	auto Point = [](FVector2 P)
	{
		return Toolbox::IsFinite(P.X) && Toolbox::IsFinite(P.Y) &&
		Toolbox::Abs(static_cast<Toolbox::f64>(P.X)) <= 2147483000.0 && Toolbox::Abs(static_cast<Toolbox::f64>(P.Y)) <= 2147483000.0;
	};
	return Point(Value.Start) && Point(Value.End) && Toolbox::IsFinite(Value.Options.Opacity) && Value.Options.Opacity >= 0 && Value.Options.Opacity <= 1;
}
/**
 * @param Value 予算に含める面と線。
 */
FORCEINLINE Toolbox::size_t DebugUnits_Internal(const FGeometryCommand3D& Value) noexcept
{
	return Value.Geometry.Lines.Size() + Value.Geometry.Triangles.Size();
}
/**
 * @param Value 一本の線分。
 */
FORCEINLINE Toolbox::size_t DebugUnits_Internal(const FLineCommand2D&) noexcept
{
	return 1;
}
}
/**
 * 所有スレッドで管理する、値だけのデバッグ記録。描画器・Physics Worldに依存しない。
 * Workerは自分専用の値バッファを作り、完了後に所有側がAddする。
 * マスクで無効なカテゴリは、呼出し側もWantsCategoryで収集前に除外する。
 */
template <typename T> class TDebugDrawStore
{
	static_assert(Toolbox::IsSame<T, FGeometryCommand3D> || Toolbox::IsSame<T, FLineCommand2D>,
	              "Debug records must remain resource-free value payloads");
public:
	/**
	 * @param MaximumEntries 最大記録数。
	 * @param MaximumPrimitives 最大面・線数。
	 */
	explicit TDebugDrawStore(Toolbox::size_t MaximumEntries = 4096, Toolbox::size_t MaximumPrimitives = 65536)
	: m_MaximumEntries(MaximumEntries), m_MaximumPrimitives(MaximumPrimitives)
	{
	}
	/**
	 * 所有スレッドの状態を複製しない。
	 */
	TDebugDrawStore(const TDebugDrawStore&) = delete;
	TDebugDrawStore& operator=(const TDebugDrawStore&) = delete;
	/**
	 * @param Category 調べるカテゴリビット。WorkerやJobからはfalse。
	 */
	FORCEINLINE bool WantsCategory(Toolbox::uint64 Category) const noexcept
	{
		return IsOwner_Internal() && (Category & m_EnabledCategories) != 0;
	}
	/**
	 * @param Categories 収集・表示を許可するカテゴリ。既存記録は寿命まで保持する。
	 */
	bool SetEnabledCategories(Toolbox::uint64 Categories) noexcept
	{
		if (!IsOwner_Internal())
		{
			return false;
		}
		m_EnabledCategories = Categories;
		return true;
	}
	/**
	 * 値を記録する。成功値falseはカテゴリ除外または予算超過で記録しなかったことを示す。
	 * @param Value 事前に計算した形状。 @param Tag 所有・分類・寿命。
	 */
	TResult<bool> Add(T Value, const FDebugDrawTag& Tag)
	{
		if (!IsOwner_Internal())
		{
			return TResult<bool>::Failure(EErrorCode::InvalidState, "Debug store owner required");
		}
		if (Tag.Category == 0 || (Tag.Lifetime != EDebugLifetime::Frame && Tag.Lifetime != EDebugLifetime::Seconds && Tag.Lifetime != EDebugLifetime::Persistent) ||
		(Tag.Clock != EDebugClock::Game && Tag.Clock != EDebugClock::Real) || !Toolbox::IsFinite(Tag.Seconds) ||
		(Tag.Lifetime == EDebugLifetime::Seconds && Tag.Seconds <= 0) || !Detail::ValidDebugPayload_Internal(Value))
		{
			return TResult<bool>::Failure(EErrorCode::InvalidArgument, "Invalid debug record");
		}
		if ((Tag.Category & m_EnabledCategories) == 0)
		{
			return TResult<bool>::Success(false);
		}
		const Toolbox::size_t Units = Detail::DebugUnits_Internal(Value);
		if (m_Entries.Size() >= m_MaximumEntries || Units > m_MaximumPrimitives - m_UsedPrimitives)
		{
			if (m_Dropped < Toolbox::TNumericLimits<Toolbox::uint64>::Max())
			{
				++m_Dropped;
			}
			return TResult<bool>::Success(false);
		}
		m_Entries.PushBack({Toolbox::Move(Value), Tag, Tag.Seconds, m_Frame, Units});
		m_UsedPrimitives += Units;
		return TResult<bool>::Success(true);
	}
	/**
	 * 新フレーム開始時に一度だけ呼ぶ。無効引数・重複フレームでは何も変更しない。
	 * @param Frame 単調増加する描画フレーム。 @param GameDelta ゲーム秒数。 @param RealDelta 実秒数。
	 */
	bool Advance(Toolbox::uint64 Frame, Toolbox::f64 GameDelta, Toolbox::f64 RealDelta) noexcept
	{
		if (!IsOwner_Internal() || (m_bHasFrame && Frame <= m_Frame) || !Toolbox::IsFinite(GameDelta) || !Toolbox::IsFinite(RealDelta) || GameDelta < 0 || RealDelta < 0)
		{
			return false;
		}
		Compact_Internal([&](FEntry& Entry)
		{
			if (Entry.Tag.Lifetime == EDebugLifetime::Frame)
			{
				return Entry.BornFrame >= Frame;
			}
			if (Entry.Tag.Lifetime == EDebugLifetime::Persistent)
			{
				return true;
			}
			const Toolbox::f64 Delta = Entry.Tag.Clock == EDebugClock::Game ? GameDelta : RealDelta;
			if (Entry.Remaining <= Delta)
			{
				return false;
			}
			Entry.Remaining -= Delta;
			return true;
		}
		);
		m_Frame = Frame;
		m_bHasFrame = true;
		return true;
	}
	/**
	 * @param Scope 解除するScope。世代も一致させる。
	 */
	Toolbox::size_t ClearScope(FDebugOwnerKey Scope) noexcept
	{
		if (!IsOwner_Internal() || Scope.Generation == 0)
		{
			return 0;
		}
		const Toolbox::size_t Before = m_Entries.Size();
		Compact_Internal([&](FEntry& Entry)
		{
			return !(Entry.Tag.Scope == Scope);
		}
		);
		return Before - m_Entries.Size();
	}
	/**
	 * @param Filter 対象ビューだけの表示条件。戻り値は独立した所有コピー。
	 */
	TResult<Toolbox::TVector<T>> Snapshot(const FDebugDrawFilter& Filter) const
	{
		if (!IsOwner_Internal())
		{
			return TResult<Toolbox::TVector<T>>::Failure(EErrorCode::InvalidState, "Debug snapshot owner required");
		}
		if (Filter.bSelectedOnly && Filter.Selected.Generation == 0)
		{
			return TResult<Toolbox::TVector<T>>::Failure(EErrorCode::InvalidArgument, "Selected owner requires a generation");
		}
		Toolbox::TVector<T> Output;
		for (const auto& Entry : m_Entries)
		{
			if ((Entry.Tag.Category & m_EnabledCategories & Filter.Categories) != 0 && (!Filter.bSelectedOnly || Entry.Tag.Owner == Filter.Selected))
			{
				Output.PushBack(Entry.Value);
			}
		}
		return TResult<Toolbox::TVector<T>>::Success(Toolbox::Move(Output));
	}
	/**
	 * 予算不足で採取を省略した件数。所有者以外には0を返す。
	 */
	FORCEINLINE Toolbox::uint64 GetDroppedCount() const noexcept
	{
		return IsOwner_Internal() ? m_Dropped : 0;
	}
	/**
	 * Scopeなしの永続記録も含め、すべて解除する。フレーム番号と省略件数は維持する。
	 */
	bool Clear() noexcept
	{
		if (!IsOwner_Internal())
		{
			return false;
		}
		m_Entries.Clear();
		m_UsedPrimitives = 0;
		return true;
	}
private:
	/**
	 * 一つの所有記録。
	 */
	struct FEntry
	{
		T Value;
		FDebugDrawTag Tag;
		Toolbox::f64 Remaining;
		Toolbox::uint64 BornFrame;
		Toolbox::size_t Units;
	};
	/**
	 * 所有スレッドとJob区間を検査する。
	 */
	FORCEINLINE bool IsOwner_Internal() const noexcept
	{
		return Toolbox::FThread::CurrentThreadId() == m_OwnerThreadId && !Toolbox::FJobSystem::IsExecutingJob();
	}
	/**
	 * @param Keep 保持する記録だけtrueを返す。割り当てなしで順序を保って詰める。
	 */
	template <typename F> void Compact_Internal(F&& Keep) noexcept
	{
		static_assert(noexcept(m_Entries[0] = Toolbox::Move(m_Entries[0])));
		Toolbox::size_t Write = 0;
		for (Toolbox::size_t Read = 0; Read < m_Entries.Size(); ++Read)
		{
			if (!Keep(m_Entries[Read]))
			{
				m_UsedPrimitives -= m_Entries[Read].Units;
				continue;
			}
			if (Write != Read)
			{
				m_Entries[Write] = Toolbox::Move(m_Entries[Read]);
			}
			++Write;
		}
		while (m_Entries.Size() > Write)
		{
			m_Entries.PopBack();
		}
	}
	/**
	 * このインスタンスの所有スレッド。
	 */
	const Toolbox::uint64 m_OwnerThreadId = Toolbox::FThread::CurrentThreadId();
	/**
	 * 記録数の上限。
	 */
	Toolbox::size_t m_MaximumEntries;
	/**
	 * 面・線数の上限。
	 */
	Toolbox::size_t m_MaximumPrimitives;
	/**
	 * 現在保持する面・線数。
	 */
	Toolbox::size_t m_UsedPrimitives = 0;
	/**
	 * カテゴリの有効集合。
	 */
	Toolbox::uint64 m_EnabledCategories = ~Toolbox::uint64{0};
	/**
	 * 最後に進めた描画フレーム。
	 */
	Toolbox::uint64 m_Frame = 0;
	/**
	 * 初回のフレーム番号が設定されたか。0番から開始できる。
	 */
	bool m_bHasFrame = false;
	/**
	 * 予算超過の累積件数。
	 */
	Toolbox::uint64 m_Dropped = 0;
	/**
	 * 所有する記録の入力順。
	 */
	Toolbox::TVector<FEntry> m_Entries;
};
/**
 * 2D補助線の値ストア。円や回転枠は線分へ展開して記録できる。
 */
using FDebugDrawStore2D = TDebugDrawStore<FLineCommand2D>;
/**
 * 3Dの面と線の値ストア。
 */
using FDebugDrawStore3D = TDebugDrawStore<FGeometryCommand3D>;
}
#endif
