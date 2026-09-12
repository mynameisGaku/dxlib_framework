#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/ResourceRegistry.h"
#include "Dxf/AssetService.h"
#include "Dxf/AudioPlayer.h"
#include <functional>
#include <stdexcept>

using namespace Dxf;
using namespace Dxf::Testing;

namespace
{
class FReleaseProbe final : public IResourceRecord
{
public:
	explicit FReleaseProbe(int& Count, std::function<void()> Callback = {})
		: m_pCount(&Count), m_Callback(std::move(Callback))
	{
	}
	void Release_Internal() noexcept override
	{
		++*m_pCount;
		// Limit a regression's recursion, so a failing case reports rather than hangs.
		if (*m_pCount == 1 && m_Callback)
		{
			m_Callback();
		}
	}
private:
	int* m_pCount;
	std::function<void()> m_Callback;
};

class FCallbackSoundBackend final : public ISoundBackend
{
public:
	void SetAfterStart(std::function<void()> Callback)
	{
		m_AfterStart = std::move(Callback);
	}
	void SetOnPoll(std::function<void()> Callback)
	{
		m_OnPoll = std::move(Callback);
	}
	void SetThrowing(bool bThrow)
	{
		m_bThrow = bThrow;
	}
	std::size_t GetLiveSounds() const
	{
		return m_Backend.GetTrace().Sounds.size();
	}
	TResult<int> LoadSound(const std::string& Path, const FSoundLoadOptions& Options) override
	{
		return m_Backend.LoadSound(Path, Options);
	}
	TResult<int> DuplicateSound(int Handle) override
	{
		return m_Backend.DuplicateSound(Handle);
	}
	TResult<void> StartSound(int Handle, bool bLoop) override
	{
		auto Result = m_Backend.StartSound(Handle, bLoop);
		if (m_AfterStart)
		{
			m_AfterStart();
		}
		if (m_bThrow)
		{
			throw std::runtime_error("injected sound start failure");
		}
		return Result;
	}
	void StopSound(int Handle) noexcept override
	{
		m_Backend.StopSound(Handle);
	}
	void DeleteSound(int Handle) noexcept override
	{
		m_Backend.DeleteSound(Handle);
	}
	TResult<void> SetSoundVolume(int Handle, float Volume) override
	{
		return m_Backend.SetSoundVolume(Handle, Volume);
	}
	TResult<bool> IsSoundPlaying(int Handle) override
	{
		auto Result = m_Backend.IsSoundPlaying(Handle);
		if (m_OnPoll)
		{
			m_OnPoll();
		}
		if (m_bThrow)
		{
			throw std::runtime_error("injected sound polling failure");
		}
		return Result;
	}
private:
	FFakeBackend m_Backend;
	std::function<void()> m_AfterStart;
	std::function<void()> m_OnPoll;
	bool m_bThrow = false;
};
}

TEST("resource shutdown reentry releases each registered resource only once")
{
	int Count = 0;
	FResourceRegistry Registry;
	auto Resource = std::make_shared<FReleaseProbe>(Count, [&] { Registry.Shutdown(); });
	REQUIRE(Registry.Register(Resource));
	Registry.Shutdown();
	REQUIRE(Count == 1);
	Registry.Shutdown();
	REQUIRE(Count == 1);
}

TEST("resource cleanup callbacks cannot invalidate the registry shutdown traversal")
{
	int ExpiredCount = 0;
	int FirstCount = 0;
	int LastCount = 0;
	FResourceRegistry Registry;
	{
		auto Expired = std::make_shared<FReleaseProbe>(ExpiredCount);
		REQUIRE(Registry.Register(Expired));
	}
	auto First = std::make_shared<FReleaseProbe>(FirstCount, [&] { Registry.CollectUnused(); });
	auto Last = std::make_shared<FReleaseProbe>(LastCount);
	REQUIRE(Registry.Register(First));
	REQUIRE(Registry.Register(Last));
	Registry.Shutdown();
	REQUIRE(FirstCount == 1 && LastCount == 1);
}

TEST("shutdown during sound start rejects the voice and frees its allocation")
{
	FFakeBackend AssetsBackend;
	FCallbackSoundBackend SoundBackend;
	FAssetService Assets(AssetsBackend, SoundBackend, AssetsBackend);
	FAudioPlayer Audio(SoundBackend);
	auto Sound = Assets.LoadSound("sound.wav");
	REQUIRE(Sound);
	SoundBackend.SetAfterStart([&] { Audio.Shutdown(); });
	auto Playback = Audio.Play(Sound.Value());
	REQUIRE(!Playback && Playback.Error().Code == EErrorCode::InvalidState);
	REQUIRE(SoundBackend.GetLiveSounds() == 1);
}

TEST("sound start exception is contained and the partial voice is freed")
{
	FFakeBackend AssetsBackend;
	FCallbackSoundBackend SoundBackend;
	FAssetService Assets(AssetsBackend, SoundBackend, AssetsBackend);
	FAudioPlayer Audio(SoundBackend);
	auto Sound = Assets.LoadSound("sound.wav");
	REQUIRE(Sound);
	SoundBackend.SetThrowing(true);
	auto Playback = Audio.Play(Sound.Value());
	REQUIRE(!Playback && Playback.Error().Code == EErrorCode::BackendFailure);
	REQUIRE(SoundBackend.GetLiveSounds() == 1);
}

TEST("sound poll exception is contained and the affected voice is stopped")
{
	FFakeBackend AssetsBackend;
	FCallbackSoundBackend SoundBackend;
	FAssetService Assets(AssetsBackend, SoundBackend, AssetsBackend);
	FAudioPlayer Audio(SoundBackend);
	auto Sound = Assets.LoadSound("sound.wav");
	REQUIRE(Sound);
	auto Playback = Audio.Play(Sound.Value());
	REQUIRE(Playback);
	SoundBackend.SetThrowing(true);
	auto Tick = Audio.Tick();
	REQUIRE(!Tick && Tick.Error().Code == EErrorCode::BackendFailure);
	REQUIRE(!Playback.Value());
	REQUIRE(SoundBackend.GetLiveSounds() == 1);
}

TEST("audio shutdown during polling invalidates the rest of the snapshot safely")
{
	FFakeBackend AssetsBackend;
	FCallbackSoundBackend SoundBackend;
	FAssetService Assets(AssetsBackend, SoundBackend, AssetsBackend);
	FAudioPlayer Audio(SoundBackend);
	auto Sound = Assets.LoadSound("sound.wav");
	REQUIRE(Sound);
	auto First = Audio.Play(Sound.Value());
	auto Second = Audio.Play(Sound.Value());
	REQUIRE(First && Second);
	SoundBackend.SetOnPoll([&] { Audio.Shutdown(); });
	REQUIRE(Audio.Tick());
	REQUIRE(!First.Value() && !Second.Value());
	REQUIRE(SoundBackend.GetLiveSounds() == 1);
}
