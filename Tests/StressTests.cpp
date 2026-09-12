#include "Toolbox/UniquePtr.h"
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/SlotMap.h"
#include "Dxf/AssetService.h"
#include "Dxf/Utf8.h"
#include "Toolbox/Platform.h"
#include "Toolbox/Vector.h"
using namespace Dxf;
using namespace Dxf::Testing;
namespace
{
/**
 * 登録順序を識別するタグ付きオブジェクト。
 */
class DTaggedObject final : public DObject
{
public:
	/**
	 * 検証に必要な依存先と初期状態を設定する。
	 */
	explicit DTaggedObject(Toolbox::uint64 Tag) : m_Tag(Tag)
	{
	}
	/**
	 * 対象を識別する検証用タグを返す。
	 */
	Toolbox::uint64 GetTag() const noexcept
	{
		return m_Tag;
	}

private:
	/**
	 * 順序を識別する対象のタグ。
	 */
	Toolbox::uint64 m_Tag;
};
/**
 * ストレス検証で実装結果と照合する期待状態。
 */
struct FExpectedObject
{
	/**
	 * 生存期間や世代を検証する登録ハンドル。
	 */
	TObjectHandle<DTaggedObject> Handle;
	/**
	 * 対象の順序を識別する値。
	 */
	Toolbox::uint64 Tag = 0;
};
/**
 * Unicodeスカラー値を検証用のUTF-8列へ変換する。
 */
Toolbox::FString EncodeScalar_Internal(Toolbox::uint32 Scalar)
{
	/**
	 * 検証結果の出力先。
	 */
	Toolbox::FString Output;
	if (Scalar < 0x80)
	{
		Output += static_cast<char>(Scalar);
	}
	else if (Scalar < 0x800)
	{
		Output += static_cast<char>(0xc0 | (Scalar >> 6));
		Output += static_cast<char>(0x80 | (Scalar & 63));
	}
	else if (Scalar < 0x10000)
	{
		Output += static_cast<char>(0xe0 | (Scalar >> 12));
		Output += static_cast<char>(0x80 | ((Scalar >> 6) & 63));
		Output += static_cast<char>(0x80 | (Scalar & 63));
	}
	else
	{
		Output += static_cast<char>(0xf0 | (Scalar >> 18));
		Output += static_cast<char>(0x80 | ((Scalar >> 12) & 63));
		Output += static_cast<char>(0x80 | ((Scalar >> 6) & 63));
		Output += static_cast<char>(0x80 | (Scalar & 63));
	}
	return Output;
}
} // namespace
TEST("Slot generations survive 20000 deterministic create remove and reuse operations")
{
	/**
	 * 再現可能なストレス入力を作る乱数。
	 */
	Toolbox::FRandom Random(0x445846U);
	/**
	 * 検証対象を所有する世代付き格納先。
	 */
	TSlotMap<DObject> Storage;
	/**
	 * ハンドルの所属先検証に使う別の格納先。
	 */
	TSlotMap<DObject> OtherStorage;
	Toolbox::TVector<FExpectedObject> Live;
	/**
	 * 削除済みの対象を指すハンドル。
	 */
	Toolbox::TVector<TObjectHandle<DTaggedObject>> Dead;
	for (Toolbox::uint64 Step = 1; Step <= 20000; ++Step)
	{
		if (Live.IsEmpty() || (Live.Size() < 256 && Random() % 2 == 0))
		{
			/**
			 * 生存期間や世代を検証する登録ハンドル。
			 */
			auto Handle = Storage.Insert(Toolbox::MakeUnique<DTaggedObject>(Step)).Cast<DTaggedObject>();
			REQUIRE(Handle && Handle.Get()->GetTag() == Step);
			REQUIRE(!OtherStorage.Remove(Handle));
			Live.PushBack({Handle, Step});
		}
		else
		{
			const auto Index = static_cast<Toolbox::size_t>(Random()) % Live.Size();
			/**
			 * 生存期間や世代を検証する登録ハンドル。
			 */
			auto Handle = Live[Index].Handle;
			REQUIRE(Storage.Remove(Handle));
			REQUIRE(!Handle && !Storage.Remove(Handle));
			Dead.PushBack(Handle);
			Live[Index] = Live.Back();
			Live.PopBack();
		}
		REQUIRE(Storage.Size() == Live.Size());
		for (const auto& Entry : Live)
		{
			REQUIRE(Entry.Handle && Entry.Handle.Get()->GetTag() == Entry.Tag);
		}
		if (!Dead.IsEmpty())
		{
			REQUIRE(!Dead[static_cast<Toolbox::size_t>(Random()) % Dead.Size()]);
		}
	}
	for (const auto& Handle : Dead)
	{
		REQUIRE(!Handle);
	}
}
TEST("Assets survive 1000 load release cache-collection cycles without live handles")
{
	/**
	 * 検証用のバックエンド。
	 */
	FFakeBackend Backend;
	/**
	 * 検証に使用する資源管理。
	 */
	FAssetService Assets(Backend, Backend, Backend);
	for (Toolbox::int32 Cycle = 0; Cycle < 1000; ++Cycle)
	{
		{
			/**
			 * 検証で使用する画像資源。
			 */
			auto Texture = Assets.LoadTexture("repeated.bmp").Value();
			auto SameTexture = Assets.LoadTexture("./repeated.bmp").Value();
			REQUIRE(Texture.GetResource_Internal() == SameTexture.GetResource_Internal());
			/**
			 * 検証で使用する音声資源。
			 */
			auto Sound = Assets.LoadSound("repeated.wav").Value();
			/**
			 * 検証で使用するフォント資源。
			 */
			auto Font = Assets.LoadFont().Value();
			REQUIRE(Sound.IsValid() && Font.IsValid());
		}
		Assets.CollectUnused();
		REQUIRE(Backend.GetTrace().Textures.IsEmpty());
		REQUIRE(Backend.GetTrace().Sounds.IsEmpty());
		REQUIRE(Backend.GetTrace().Fonts.IsEmpty());
	}
	REQUIRE(Backend.GetTrace().TextureLoads == 1000);
}
TEST("UTF8 validation accepts every Unicode scalar and rejects every encoded surrogate")
{
	for (Toolbox::uint32 Scalar = 0; Scalar <= 0x10ffff; ++Scalar)
	{
		const bool bSurrogate = Scalar >= 0xd800 && Scalar <= 0xdfff;
		REQUIRE(IsValidUtf8(EncodeScalar_Internal(Scalar)) == !bSurrogate);
	}
}
