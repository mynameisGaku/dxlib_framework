#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/SlotMap.h"
#include "Dxf/AssetService.h"
#include "Dxf/Utf8.h"
#include <random>
#include <vector>
using namespace Dxf;
using namespace Dxf::Testing;
namespace
{
class DTaggedObject final : public DObject
{
public:
	explicit DTaggedObject(std::uint64_t Tag) : m_Tag(Tag)
	{
	}
	std::uint64_t GetTag() const noexcept
	{
		return m_Tag;
	}
private:
	std::uint64_t m_Tag;
};
struct FExpectedObject
{
	TObjectHandle<DTaggedObject> Handle;
	std::uint64_t Tag = 0;
};
std::string EncodeScalar_Internal(std::uint32_t Scalar)
{
	std::string Output;
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
}
TEST("Slot generations survive 20000 deterministic create remove and reuse operations")
{
	std::mt19937 Random(0x445846U);
	TSlotMap<DObject> Storage;
	TSlotMap<DObject> OtherStorage;
	std::vector<FExpectedObject> Live;
	std::vector<TObjectHandle<DTaggedObject>> Dead;
	for (std::uint64_t Step = 1; Step <= 20000; ++Step)
	{
		if (Live.empty() || (Live.size() < 256 && Random() % 2 == 0))
		{
			auto Handle = Storage.Insert(std::make_unique<DTaggedObject>(Step)).Cast<DTaggedObject>();
			REQUIRE(Handle && Handle.Get()->GetTag() == Step);
			REQUIRE(!OtherStorage.Remove(Handle));
			Live.push_back({Handle, Step});
		}
		else
		{
			const auto Index = static_cast<std::size_t>(Random()) % Live.size();
			auto Handle = Live[Index].Handle;
			REQUIRE(Storage.Remove(Handle));
			REQUIRE(!Handle && !Storage.Remove(Handle));
			Dead.push_back(Handle);
			Live[Index] = Live.back();
			Live.pop_back();
		}
		REQUIRE(Storage.Size() == Live.size());
		for (const auto& Entry : Live)
		{
			REQUIRE(Entry.Handle && Entry.Handle.Get()->GetTag() == Entry.Tag);
		}
		if (!Dead.empty())
		{
			REQUIRE(!Dead[static_cast<std::size_t>(Random()) % Dead.size()]);
		}
	}
	for (const auto& Handle : Dead)
	{
		REQUIRE(!Handle);
	}
}
TEST("Assets survive 1000 load release cache-collection cycles without live handles")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	for (int Cycle = 0; Cycle < 1000; ++Cycle)
	{
		{
			auto Texture = Assets.LoadTexture("repeated.bmp").Value();
			auto SameTexture = Assets.LoadTexture("./repeated.bmp").Value();
			REQUIRE(Texture.GetResource_Internal() == SameTexture.GetResource_Internal());
			auto Sound = Assets.LoadSound("repeated.wav").Value();
			auto Font = Assets.LoadFont().Value();
			REQUIRE(Sound.IsValid() && Font.IsValid());
		}
		Assets.CollectUnused();
		REQUIRE(Backend.GetTrace().Textures.empty());
		REQUIRE(Backend.GetTrace().Sounds.empty());
		REQUIRE(Backend.GetTrace().Fonts.empty());
	}
	REQUIRE(Backend.GetTrace().TextureLoads == 1000);
}
TEST("UTF8 validation accepts every Unicode scalar and rejects every encoded surrogate")
{
	for (std::uint32_t Scalar = 0; Scalar <= 0x10ffff; ++Scalar)
	{
		const bool bSurrogate = Scalar >= 0xd800 && Scalar <= 0xdfff;
		REQUIRE(IsValidUtf8(EncodeScalar_Internal(Scalar)) == !bSurrogate);
	}
}
