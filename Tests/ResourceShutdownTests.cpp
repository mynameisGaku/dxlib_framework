#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/ResourceRegistry.h"
#include "Dxf/AssetService.h"
#include "Dxf/AudioPlayer.h"
#include "Toolbox/Function.h"
#include "Toolbox/Utility.h"

using namespace Dxf;
using namespace Dxf::Testing;

namespace
{
/**
 * 解放の時期と回数を外部から確認する。
 */
class FReleaseProbe final : public IResourceRecord
{
public:
	/**
	 * 検証に必要な依存先と初期状態を設定する。
	 */
	explicit FReleaseProbe(Toolbox::int32& Count, Toolbox::TFunction<void()> Callback = {})
	    : m_pCount(&Count), m_Callback(Toolbox::Move(Callback))
	{
	}
	/**
	 * 解放を記録し初回のみ再入用の処理を呼ぶ。
	 */
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
	/**
	 * 外部の呼び出し回数への参照。
	 */
	Toolbox::int32* m_pCount;
	/**
	 * 再入を再現するための任意処理。
	 */
	Toolbox::TFunction<void()> m_Callback;
};

/**
 * 音声操作中のコールバックと再入を再現する。
 */
class FCallbackSoundBackend final : public ISoundBackend
{
public:
	/**
	 * 再生開始直後に実行する処理を設定する。
	 */
	void SetAfterStart(Toolbox::TFunction<void()> Callback)
	{
		m_AfterStart = Toolbox::Move(Callback);
	}
	/**
	 * 状態取得中に実行する処理を設定する。
	 */
	void SetOnPoll(Toolbox::TFunction<void()> Callback)
	{
		m_OnPoll = Toolbox::Move(Callback);
	}
	/**
	 * 状態取得で例外を発生させるか設定する。
	 */
	void SetThrowing(bool bThrow)
	{
		m_bThrow = bThrow;
	}
	/**
	 * 未解放の音声資源数を返す。
	 */
	Toolbox::size_t GetLiveSounds() const
	{
		return m_Backend.GetTrace().Sounds.Size();
	}
	/**
	 * 音声資源の読み込みを記録する。
	 */
	TResult<Toolbox::int32> LoadSound(const Toolbox::FString& Path, const FSoundLoadOptions& Options) override
	{
		return m_Backend.LoadSound(Path, Options);
	}
	/**
	 * 複製した音声に別の資源番号を発行する。
	 */
	TResult<Toolbox::int32> DuplicateSound(Toolbox::int32 Handle) override
	{
		return m_Backend.DuplicateSound(Handle);
	}
	/**
	 * 再生開始を記録し指定の失敗条件を適用する。
	 */
	TResult<void> StartSound(Toolbox::int32 Handle, bool bLoop) override
	{
		/**
		 * 検証対象の操作が返した成否と値。
		 */
		auto Result = m_Backend.StartSound(Handle, bLoop);
		if (m_AfterStart)
		{
			m_AfterStart();
		}
		if (m_bThrow)
		{
			throw Toolbox::FException("injected sound start failure");
		}
		return Result;
	}
	/**
	 * 音声の再生状態を停止へ変更する。
	 */
	void StopSound(Toolbox::int32 Handle) noexcept override
	{
		m_Backend.StopSound(Handle);
	}
	/**
	 * 音声資源の解放を記録する。
	 */
	void DeleteSound(Toolbox::int32 Handle) noexcept override
	{
		m_Backend.DeleteSound(Handle);
	}
	/**
	 * 対象音声へ設定された音量を記録する。
	 */
	TResult<void> SetSoundVolume(Toolbox::int32 Handle, Toolbox::f32 Volume) override
	{
		return m_Backend.SetSoundVolume(Handle, Volume);
	}
	/**
	 * 記録された音声の再生状態を返す。
	 */
	TResult<bool> IsSoundPlaying(Toolbox::int32 Handle) override
	{
		/**
		 * 検証対象の操作が返した成否と値。
		 */
		auto Result = m_Backend.IsSoundPlaying(Handle);
		if (m_OnPoll)
		{
			m_OnPoll();
		}
		if (m_bThrow)
		{
			throw Toolbox::FException("injected sound polling failure");
		}
		return Result;
	}

private:
	/**
	 * 検証対象が参照するバックエンド。
	 */
	FFakeBackend m_Backend;
	/**
	 * 再生開始直後に実行する検証用コールバック。
	 */
	Toolbox::TFunction<void()> m_AfterStart;
	/**
	 * 再生状態の取得中に実行する検証用コールバック。
	 */
	Toolbox::TFunction<void()> m_OnPoll;
	/**
	 * フックで例外を発生させるか。
	 */
	bool m_bThrow = false;
};
} // namespace

TEST("resource shutdown reentry releases each registered resource only once")
{
	/**
	 * 観測した処理または要素の数。
	 */
	Toolbox::int32 Count = 0;
	FResourceRegistry Registry;
	/**
	 * 生存期間を検証する資源。
	 */
	auto Resource = Toolbox::MakeShared<FReleaseProbe>(Count,
	                                                   [&]
	                                                   {
		                                                   Registry.Shutdown();
	                                                   });
	REQUIRE(Registry.Register(Resource));
	Registry.Shutdown();
	REQUIRE(Count == 1);
	Registry.Shutdown();
	REQUIRE(Count == 1);
}

TEST("resource cleanup callbacks cannot invalidate the registry shutdown traversal")
{
	Toolbox::int32 ExpiredCount = 0;
	Toolbox::int32 FirstCount = 0;
	Toolbox::int32 LastCount = 0;
	FResourceRegistry Registry;
	{
		/**
		 * 生存期間が終了した対象。
		 */
		auto Expired = Toolbox::MakeShared<FReleaseProbe>(ExpiredCount);
		REQUIRE(Registry.Register(Expired));
	}
	/**
	 * 最初に生成または登録した対象。
	 */
	auto First = Toolbox::MakeShared<FReleaseProbe>(FirstCount,
	                                                [&]
	                                                {
		                                                Registry.CollectUnused();
	                                                });
	/**
	 * 最後に処理した対象。
	 */
	auto Last = Toolbox::MakeShared<FReleaseProbe>(LastCount);
	REQUIRE(Registry.Register(First));
	REQUIRE(Registry.Register(Last));
	Registry.Shutdown();
	REQUIRE(FirstCount == 1 && LastCount == 1);
}

TEST("shutdown during sound start rejects the voice and frees its allocation")
{
	/**
	 * 画像・音声資源を提供するバックエンド。
	 */
	FFakeBackend AssetsBackend;
	/**
	 * 音声操作を記録するバックエンド。
	 */
	FCallbackSoundBackend SoundBackend;
	/**
	 * 検証に使用する資源管理。
	 */
	FAssetService Assets(AssetsBackend, SoundBackend, AssetsBackend);
	/**
	 * 再生状態を管理する音声サービス。
	 */
	FAudioPlayer Audio(SoundBackend);
	/**
	 * 検証で使用する音声資源。
	 */
	auto Sound = Assets.LoadSound("sound.wav");
	REQUIRE(Sound);
	SoundBackend.SetAfterStart(
	    [&]
	    {
		    Audio.Shutdown();
	    });
	/**
	 * 開始した再生の識別情報。
	 */
	auto Playback = Audio.Play(Sound.Value());
	REQUIRE(!Playback && Playback.Error().Code == EErrorCode::InvalidState);
	REQUIRE(SoundBackend.GetLiveSounds() == 1);
}

TEST("sound start exception is contained and the partial voice is freed")
{
	/**
	 * 画像・音声資源を提供するバックエンド。
	 */
	FFakeBackend AssetsBackend;
	/**
	 * 音声操作を記録するバックエンド。
	 */
	FCallbackSoundBackend SoundBackend;
	/**
	 * 検証に使用する資源管理。
	 */
	FAssetService Assets(AssetsBackend, SoundBackend, AssetsBackend);
	/**
	 * 再生状態を管理する音声サービス。
	 */
	FAudioPlayer Audio(SoundBackend);
	/**
	 * 検証で使用する音声資源。
	 */
	auto Sound = Assets.LoadSound("sound.wav");
	REQUIRE(Sound);
	SoundBackend.SetThrowing(true);
	/**
	 * 開始した再生の識別情報。
	 */
	auto Playback = Audio.Play(Sound.Value());
	REQUIRE(!Playback && Playback.Error().Code == EErrorCode::BackendFailure);
	REQUIRE(SoundBackend.GetLiveSounds() == 1);
}

TEST("sound poll exception is contained and the affected voice is stopped")
{
	/**
	 * 画像・音声資源を提供するバックエンド。
	 */
	FFakeBackend AssetsBackend;
	/**
	 * 音声操作を記録するバックエンド。
	 */
	FCallbackSoundBackend SoundBackend;
	/**
	 * 検証に使用する資源管理。
	 */
	FAssetService Assets(AssetsBackend, SoundBackend, AssetsBackend);
	/**
	 * 再生状態を管理する音声サービス。
	 */
	FAudioPlayer Audio(SoundBackend);
	/**
	 * 検証で使用する音声資源。
	 */
	auto Sound = Assets.LoadSound("sound.wav");
	REQUIRE(Sound);
	/**
	 * 開始した再生の識別情報。
	 */
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
	/**
	 * 画像・音声資源を提供するバックエンド。
	 */
	FFakeBackend AssetsBackend;
	/**
	 * 音声操作を記録するバックエンド。
	 */
	FCallbackSoundBackend SoundBackend;
	/**
	 * 検証に使用する資源管理。
	 */
	FAssetService Assets(AssetsBackend, SoundBackend, AssetsBackend);
	/**
	 * 再生状態を管理する音声サービス。
	 */
	FAudioPlayer Audio(SoundBackend);
	/**
	 * 検証で使用する音声資源。
	 */
	auto Sound = Assets.LoadSound("sound.wav");
	REQUIRE(Sound);
	/**
	 * 最初に生成または登録した対象。
	 */
	auto First = Audio.Play(Sound.Value());
	/**
	 * 二番目に生成または登録した対象。
	 */
	auto Second = Audio.Play(Sound.Value());
	REQUIRE(First && Second);
	SoundBackend.SetOnPoll(
	    [&]
	    {
		    Audio.Shutdown();
	    });
	REQUIRE(Audio.Tick());
	REQUIRE(!First.Value() && !Second.Value());
	REQUIRE(SoundBackend.GetLiveSounds() == 1);
}
