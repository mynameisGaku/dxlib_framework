// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Toolbox/SharedPtr.h"
#include "Toolbox/UniquePtr.h"
#include "Toolbox/Optional.h"
#include "Toolbox/Variant.h"
#include "Toolbox/Map.h"
#include "Toolbox/Platform.h"
#include "Dxf/Result.h"
using namespace Toolbox;
namespace
{
/**
 * 破棄回数を観測し、所有権の二重解放を検出するテスト対象。
 */
struct FOwnedProbe
{
	/**
	 * 観測する整数または外部カウンターを保持する。
	 */
	explicit FOwnedProbe(int32& Destroyed) : Count(Destroyed)
	{
	}
	virtual ~FOwnedProbe()
	{
		++Count;
	}
	/**
	 * 外部で観測する呼び出しまたは破棄の回数。
	 */
	int32& Count;
};
/**
 * 再確保中のコピーを意図的に失敗させる値。
 */
struct FThrowingValue
{
	/**
	 * 次にコピーを失敗させるまでの残り回数。
	 */
	inline static int32 Remaining = 100;
	/**
	 * 現在構築済みで生存している値の数。
	 */
	inline static int32 Alive = 0;
	/**
	 * 観測する整数または外部カウンターを保持する。
	 */
	explicit FThrowingValue(int32 Number = 0) : Value(Number)
	{
		++Alive;
	}
	/**
	 * 観測する整数または外部カウンターを保持する。
	 */
	FThrowingValue(const FThrowingValue& Other) : Value(Other.Value)
	{
		if (--Remaining == 0)
		{
			throw FException("copy failure");
		}
		++Alive;
	}
	/**
	 * 観測する整数または外部カウンターを保持する。
	 */
	FThrowingValue(FThrowingValue&& Other) noexcept(false) : Value(Other.Value)
	{
		++Alive;
	}
	FThrowingValue& operator=(const FThrowingValue&) = default;
	/**
	 * 破棄された回数または生存数を更新する。
	 */
	~FThrowingValue()
	{
		--Alive;
	}
	/**
	 * 保持して検証に使用する値。
	 */
	int32 Value;
};
/**
 * コピー構築だけが可能で、移動と代入を明示的に禁止した値。
 */
struct FCopyOnlyValue
{
	/**
	 * 指定した整数を保持する。
	 */
	explicit FCopyOnlyValue(int32 Number) : Value(Number)
	{
	}
	/**
	 * 元の値から同じ不変値を作る。
	 */
	FCopyOnlyValue(const FCopyOnlyValue&) = default;
	/**
	 * 移動構築を必要とする実装を検出する。
	 */
	FCopyOnlyValue(FCopyOnlyValue&&) = delete;
	/**
	 * コピー代入がなくても再構築できることを検証する。
	 */
	FCopyOnlyValue& operator=(const FCopyOnlyValue&) = delete;
	/**
	 * 移動代入を使用できないことを明示する。
	 */
	FCopyOnlyValue& operator=(FCopyOnlyValue&&) = delete;
	/**
	 * コピー後も保たれる検証値。
	 */
	const int32 Value;
};
/**
 * 移動構築はできるが、コピーと代入はできない所有値。
 */
struct FMoveConstructOnlyValue
{
	/**
	 * 整数の所有領域を作る。
	 */
	explicit FMoveConstructOnlyValue(int32 Number) : Value(MakeUnique<int32>(Number))
	{
	}
	/**
	 * 所有領域をコピーしない。
	 */
	FMoveConstructOnlyValue(const FMoveConstructOnlyValue&) = delete;
	/**
	 * 構築時に限り所有権を移す。
	 */
	FMoveConstructOnlyValue(FMoveConstructOnlyValue&&) noexcept = default;
	/**
	 * コピー代入を禁止する。
	 */
	FMoveConstructOnlyValue& operator=(const FMoveConstructOnlyValue&) = delete;
	/**
	 * 移動代入ではなく再構築が必要なことを検証する。
	 */
	FMoveConstructOnlyValue& operator=(FMoveConstructOnlyValue&&) = delete;
	/**
	 * 再構築を通じて移動する整数の所有領域。
	 */
	TUniquePtr<int32> Value;
};
/**
 * 再構築時の例外に備えた解放処理を検証する非代入型。
 */
struct FThrowingConstructOnlyValue
{
	/**
	 * 現在存在するインスタンス数。
	 */
	inline static int32 Alive = 0;
	/**
	 * コピー構築の失敗を発生させるか。
	 */
	inline static bool Fail = false;
	/**
	 * 生存数を増やして検証値を保持する。
	 */
	explicit FThrowingConstructOnlyValue(int32 Number) : Value(Number)
	{
		++Alive;
	}
	/**
	 * 指定された場合は構築前に例外を送出する。
	 */
	FThrowingConstructOnlyValue(const FThrowingConstructOnlyValue& Other) : Value(Other.Value)
	{
		if (Fail)
		{
			throw FException("reconstruction failure");
		}
		++Alive;
	}
	/**
	 * 代入を禁止し、Optionalに再構築させる。
	 */
	FThrowingConstructOnlyValue& operator=(const FThrowingConstructOnlyValue&) = delete;
	/**
	 * 実際に構築された値だけを生存数から引く。
	 */
	~FThrowingConstructOnlyValue()
	{
		--Alive;
	}
	/**
	 * 生存中に保持する検証値。
	 */
	int32 Value;
};
} // namespace
TEST("Toolbox vector preserves values after a failed reserve")
{
	{
		/**
		 * コンテナーの動作を検証する値の一覧。
		 */
		TVector<FThrowingValue> Values;
		Values.EmplaceBack(10);
		Values.EmplaceBack(20);
		FThrowingValue::Remaining = 2;
		/**
		 * 期待した例外が送出されたか。
		 */
		bool Failed = false;
		try
		{
			Values.Reserve(100);
		}
		catch (const FException&)
		{
			Failed = true;
		}
		REQUIRE(Failed);
		REQUIRE(Values.Size() == 2);
		REQUIRE(Values[0].Value == 10);
		REQUIRE(Values[1].Value == 20);
		REQUIRE(FThrowingValue::Alive == 2);
		FThrowingValue::Remaining = 100;
	}
	REQUIRE(FThrowingValue::Alive == 0);
}
TEST("Toolbox vector supports self append and over-aligned values")
{
	/**
	 * 自分の要素を追加する文字列一覧。
	 */
	TVector<FString> Texts{"a", "b", "c", "d"};
	Texts.PushBack(Texts[0]);
	REQUIRE(Texts.Back() == "a");
	REQUIRE(Texts[0] == "a");
	/**
	 * 通常より大きいアラインメントで連続領域の確保を検証する。
	 */
	struct alignas(64) FAligned
	{
		/**
		 * 保持して検証に使用する値。
		 */
		int32 Value = 7;
		/**
		 * 64バイト配置を検証するための余白。
		 */
		unsigned char Padding[60]{};
	};
	/**
	 * コンテナーの動作を検証する値の一覧。
	 */
	TVector<FAligned> Values(5);
	REQUIRE(reinterpret_cast<uintptr_t>(Values.Data()) % 64 == 0);
	REQUIRE(Values[4].Value == 7);
}
TEST("Toolbox unique ownership transfers without double destruction")
{
	/**
	 * 所有対象が破棄された回数。
	 */
	int32 Destroyed = 0;
	/**
	 * 最初に作成した対象。
	 */
	auto First = MakeUnique<FOwnedProbe>(Destroyed);
	/**
	 * コピーまたは移動先の対象。
	 */
	auto Second = Move(First);
	REQUIRE(!First);
	REQUIRE(Second);
	Second.Reset();
	REQUIRE(Destroyed == 1);
}
TEST("Toolbox shared and weak ownership expire exactly once")
{
	/**
	 * 所有対象が破棄された回数。
	 */
	int32 Destroyed = 0;
	/**
	 * 所有権を持たず対象の生存を確認する参照。
	 */
	TWeakPtr<FOwnedProbe> Weak;
	{
		/**
		 * 最初に作成した対象。
		 */
		auto First = MakeShared<FOwnedProbe>(Destroyed);
		Weak = First;
		/**
		 * コピーまたは移動先の対象。
		 */
		auto Second = First;
		REQUIRE(First.UseCount() == 2);
		First.Reset();
		REQUIRE(!Weak.IsExpired());
		/**
		 * 弱参照から取得した一時的な所有権。
		 */
		auto Locked = Weak.Lock();
		REQUIRE(Locked);
		Second.Reset();
		REQUIRE(Destroyed == 0);
	}
	REQUIRE(Destroyed == 1);
	REQUIRE(Weak.IsExpired());
	REQUIRE(!Weak.Lock());
}
TEST("Toolbox shared conversion retains the original destructor")
{
	/**
	 * 非仮想デストラクターを持つ基底型。
	 */
	struct FBase
	{
		/**
		 * 保持して検証に使用する値。
		 */
		int32 Value = 1;
	};
	/**
	 * 共有所有権が派生型の破棄方法を記憶するか検証する。
	 */
	struct FDerived : FBase
	{
		/**
		 * 観測する整数または外部カウンターを保持する。
		 */
		explicit FDerived(int32& Count) : Destroyed(Count)
		{
		}
		/**
		 * 破棄された回数または生存数を更新する。
		 */
		~FDerived()
		{
			++Destroyed;
		}
		/**
		 * 所有対象が破棄された回数。
		 */
		int32& Destroyed;
	};
	/**
	 * 所有対象が破棄された回数。
	 */
	int32 Destroyed = 0;
	{
		/**
		 * 基底型を通じて保持する派生オブジェクト。
		 */
		TSharedPtr<FBase> Base = MakeShared<FDerived>(Destroyed);
		REQUIRE(Base->Value == 1);
	}
	REQUIRE(Destroyed == 1);
}
TEST("Toolbox optional and variant reject invalid access")
{
	/**
	 * 保持して検証に使用する値。
	 */
	TOptional<TUniquePtr<int32>> Value;
	/**
	 * 期待した例外が送出されたか。
	 */
	bool Failed = false;
	try
	{
		Value.Value();
	}
	catch (const FBadAccess&)
	{
		Failed = true;
	}
	REQUIRE(Failed);
	Value.Emplace(MakeUnique<int32>(42));
	REQUIRE(**Value == 42);
	/**
	 * 異なる型の値を切り替える検証対象。
	 */
	TVariant<TUniquePtr<int32>, FString> Variant(InPlaceIndex<0>, MakeUnique<int32>(17));
	/**
	 * 所有値を移動した先。
	 */
	auto Moved = Move(Variant);
	REQUIRE(*Get<0>(Moved) == 17);
	Moved = FString("error");
	REQUIRE(Get<1>(Moved) == "error");
	Failed = false;
	try
	{
		Get<0>(Moved);
	}
	catch (const FBadAccess&)
	{
		Failed = true;
	}
	REQUIRE(Failed);
}
TEST("Toolbox function copies captures independently")
{
	/**
	 * 最初に作成した対象。
	 */
	TFunction<int32()> First = [Count = 0]() mutable
	{
		return ++Count;
	};
	/**
	 * コピーまたは移動先の対象。
	 */
	auto Second = First;
	REQUIRE(First() == 1);
	REQUIRE(First() == 2);
	REQUIRE(Second() == 1);
	/**
	 * 何も呼び出す対象を持たないコールバック。
	 */
	TFunction<void()> Empty;
	/**
	 * 期待した例外が送出されたか。
	 */
	bool Failed = false;
	try
	{
		Empty();
	}
	catch (const FBadAccess&)
	{
		Failed = true;
	}
	REQUIRE(Failed);
}
TEST("Toolbox strings retain embedded NUL and support self concatenation")
{
	/**
	 * 埋め込みNULと自己連結を検証する文字列。
	 */
	FString Text("a\0b", 3);
	REQUIRE(Text.Size() == 3);
	REQUIRE(FStringView(Text).Find('\0') == 1);
	Text += Text;
	REQUIRE(Text.Size() == 6);
	REQUIRE(Text[5] == 'b');
	REQUIRE(Text.CStr()[6] == 0);
	/**
	 * 所有値を移動した先。
	 */
	auto Moved = Move(Text);
	REQUIRE(Text.IsEmpty());
	Text += 'x';
	REQUIRE(Text == "x");
	REQUIRE(Moved.Size() == 6);
	REQUIRE(ToString(int64(-9223372036854775807LL - 1)) == "-9223372036854775808");
}
TEST("Toolbox maps erase expired entries without losing live values")
{
	/**
	 * コンテナーの動作を検証する値の一覧。
	 */
	TMap<FString, int32> Values;
	Values["a"] = 1;
	Values["b"] = 2;
	Values["c"] = 3;
	REQUIRE(EraseIf(Values,
	                [](const auto& Item)
	                {
		                return Item.Second != 2;
	                }) == 2);
	REQUIRE(Values.Size() == 1);
	REQUIRE(Values.At("b") == 2);
}
TEST("Toolbox stable sort preserves equal key order")
{
	/**
	 * コンテナーの動作を検証する値の一覧。
	 */
	TVector<TPair<int32, int32>> Values{{2, 0}, {1, 1}, {2, 2}, {1, 3}};
	StableSort(Values.Begin(), Values.End(),
	           [](const auto& A, const auto& B)
	           {
		           return A.First < B.First;
	           });
	REQUIRE(Values[0].Second == 1);
	REQUIRE(Values[1].Second == 3);
	REQUIRE(Values[2].Second == 0);
	REQUIRE(Values[3].Second == 2);
}
TEST("Toolbox paths normalize without filesystem access")
{
	REQUIRE(FPath("assets/./image/../player.bmp").Normalize().ToUtf8() == "assets/player.bmp");
	REQUIRE(FPath("../assets/../../file").Normalize().ToUtf8() == "../../file");
	REQUIRE(FPath("/a/../../file").Normalize().ToUtf8() == "/file");
	REQUIRE(FPath("a/..").Normalize().ToUtf8() == ".");
	REQUIRE((FPath("a") / "b").ToUtf8() == "a/b");
	REQUIRE(FromWide(ToWide("日本語の画像.bmp")) == "日本語の画像.bmp");
}
TEST("Toolbox atomic counter and monotonic time advance")
{
	/**
	 * 原子操作の戻り値を検証するカウンター。
	 */
	FAtomicCounter Counter(7);
	REQUIRE(Counter.FetchAdd(2) == 7);
	REQUIRE(Counter.Load() == 9);
	/**
	 * 比較交換が要求する現在値。
	 */
	uint64 Expected = 8;
	REQUIRE(!Counter.CompareExchange(Expected, 10));
	REQUIRE(Expected == 9);
	REQUIRE(Counter.CompareExchange(Expected, 10));
	REQUIRE(Counter.Load() == 10);
	/**
	 * 最初に作成した対象。
	 */
	const auto First = MonotonicNanoseconds();
	REQUIRE(MonotonicNanoseconds() >= First);
	static_assert(sizeof(int32) == 4 && sizeof(int64) == 8 && sizeof(f32) == 4 && sizeof(f64) == 8);
}
TEST("Toolbox vector grows and self-appends values with deleted move constructors")
{
	/**
	 * 最初の確保と拡張の両方でコピー専用型を使用する。
	 */
	TVector<FCopyOnlyValue> Values;
	for (int32 Index = 0; Index < 12; ++Index)
	{
		Values.EmplaceBack(Index);
	}
	Values.Reserve(16);
	while (Values.Size() < 16)
	{
		Values.PushBack(Values[0]);
	}
	Values.PushBack(Values[3]);
	REQUIRE(Values.Size() == 17);
	REQUIRE(Values.Back().Value == 3);
	for (int32 Index = 0; Index < 12; ++Index)
	{
		REQUIRE(Values[static_cast<size_t>(Index)].Value == Index);
	}
}
TEST("Toolbox variant preserves duplicate alternative indices through copying and moving")
{
	/**
	 * 同じ型の二番目を明示して構築する。
	 */
	TVariant<int32, int32> Original(InPlaceIndex<1>, 73);
	/**
	 * コピーでも選択番号を保つ。
	 */
	const auto Copy = Original;
	REQUIRE(Copy.Index() == 1);
	REQUIRE(Get<1>(Copy) == 73);
	/**
	 * 移動でも選択番号を保つ。
	 */
	auto Moved = Move(Original);
	REQUIRE(Moved.Index() == 1);
	REQUIRE(Get<1>(Moved) == 73);
	/**
	 * 別の選択肢からコピー代入する。
	 */
	TVariant<int32, int32> Assigned(InPlaceIndex<0>, 11);
	Assigned = Copy;
	REQUIRE(Assigned.Index() == 1);
	Assigned = TVariant<int32, int32>(InPlaceIndex<0>, 29);
	REQUIRE(Assigned.Index() == 0);
	Assigned = Move(Moved);
	REQUIRE(Assigned.Index() == 1);
	REQUIRE(Get<1>(Assigned) == 73);
}
TEST("Toolbox variant keeps TResult error payloads distinct from failure state")
{
	/**
	 * 成功値自体がFErrorでも、失敗状態とは区別する。
	 */
	auto Success = Dxf::TResult<Dxf::FError>::Success({Dxf::EErrorCode::NotFound, "payload"});
	/**
	 * エラー側も同じFError型を持つ。
	 */
	auto Failure = Dxf::TResult<Dxf::FError>::Failure(Dxf::EErrorCode::InvalidArgument, "failure");
	/**
	 * 複製で失敗が成功に変わらないことを検証する。
	 */
	auto Copy = Failure;
	REQUIRE(!Copy);
	REQUIRE(Copy.Error().Message == "failure");
	/**
	 * 移動代入で失敗側の選択を保つ。
	 */
	Success = Move(Copy);
	REQUIRE(!Success);
	REQUIRE(Success.Error().Code == Dxf::EErrorCode::InvalidArgument);
}
TEST("Toolbox void callbacks discard callable results and preserve captures")
{
	/**
	 * 呼び出した回数を外部から確認する。
	 */
	int32 Count = 0;
	/**
	 * 値を返すラムダをvoidのコールバックとして保持する。
	 */
	TFunction<void(int32)> Function = [&Count](int32 Amount)
	{
		Count += Amount;
		return Count;
	};
	Function(4);
	/**
	 * コピーも同じ外部カウンターを参照する。
	 */
	auto Copy = Function;
	Copy(7);
	REQUIRE(Count == 11);
}
TEST("Toolbox optional reconstructs nonassignable copy-only and move-only values")
{
	/**
	 * コピー専用値を持つ二つのOptional。
	 */
	TOptional<FCopyOnlyValue> First;
	/**
	 * 既存値を破棄して再構築する対象。
	 */
	TOptional<FCopyOnlyValue> Second;
	First.Emplace(21);
	Second.Emplace(9);
	Second = First;
	REQUIRE(Second->Value == 21);
	Second = *Second;
	REQUIRE(Second->Value == 21);
	Second.Reset();
	Second = First;
	REQUIRE(Second->Value == 21);
	/**
	 * 移動構築だけで所有権を受け取れることを検証する。
	 */
	TOptional<FMoveConstructOnlyValue> Owner;
	/**
	 * 非代入型の所有権を再構築で受け取る先。
	 */
	TOptional<FMoveConstructOnlyValue> Receiver;
	Owner.Emplace(42);
	Receiver.Emplace(3);
	Receiver = Move(Owner);
	REQUIRE(*Receiver->Value == 42);
	REQUIRE(!Owner->Value);
	Receiver = Move(*Receiver);
	REQUIRE(*Receiver->Value == 42);
	Receiver = FMoveConstructOnlyValue(84);
	REQUIRE(*Receiver->Value == 84);
}
TEST("Toolbox optional stays empty after failed reconstruction without leaking")
{
	{
		/**
		 * 再構築でコピーする元の値。
		 */
		TOptional<FThrowingConstructOnlyValue> Source;
		/**
		 * コピーに失敗すると空になる先。
		 */
		TOptional<FThrowingConstructOnlyValue> Target;
		Source.Emplace(5);
		Target.Emplace(8);
		FThrowingConstructOnlyValue::Fail = true;
		/**
		 * 再構築の例外を観測したか。
		 */
		bool Failed = false;
		try
		{
			Target = Source;
		}
		catch (const FException&)
		{
			Failed = true;
		}
		FThrowingConstructOnlyValue::Fail = false;
		REQUIRE(Failed);
		REQUIRE(!Target);
		REQUIRE(Source->Value == 5);
		REQUIRE(FThrowingConstructOnlyValue::Alive == 1);
	}
	REQUIRE(FThrowingConstructOnlyValue::Alive == 0);
}
