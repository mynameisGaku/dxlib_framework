// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Support/TestFs.h"
#include "Dxf/AssetService.h"
#include "Dxf/AssetDecode.h"
#include "Dxf/TaskDispatcher.h"
#include "Toolbox/Thread.h"
using namespace Dxf;
using namespace Dxf::Testing;
namespace
{
// 検証用バイト列へ値を追加する。
void AppendU16_Internal(Toolbox::FString& Out, Toolbox::uint16 Value)
{
	Out.PushBack(static_cast<char>(Value & 0xFF));
	Out.PushBack(static_cast<char>((Value >> 8) & 0xFF));
}
// 検証用バイト列へ値を追加する。
void AppendU32_Internal(Toolbox::FString& Out, Toolbox::uint32 Value)
{
	AppendU16_Internal(Out, static_cast<Toolbox::uint16>(Value & 0xFFFF));
	AppendU16_Internal(Out, static_cast<Toolbox::uint16>((Value >> 16) & 0xFFFF));
}
// 検証用バイト列へ値を追加する。
void AppendI32_Internal(Toolbox::FString& Out, Toolbox::int32 Value)
{
	AppendU32_Internal(Out, static_cast<Toolbox::uint32>(Value));
}
// 検証用のBMPを作る。引数はすべてヘッダーへそのまま書く。
Toolbox::FString MakeBmp_Internal(Toolbox::int32 Width, Toolbox::int32 Height, Toolbox::uint16 Bits,
                                  Toolbox::uint32 Compression, Toolbox::uint32 OffBits, Toolbox::size_t PixelBytes)
{
	Toolbox::FString Out("BM", 2);
	AppendU32_Internal(Out, 0);
	AppendU16_Internal(Out, 0);
	AppendU16_Internal(Out, 0);
	AppendU32_Internal(Out, OffBits);
	AppendU32_Internal(Out, 40);
	AppendI32_Internal(Out, Width);
	AppendI32_Internal(Out, Height);
	AppendU16_Internal(Out, 1);
	AppendU16_Internal(Out, Bits);
	AppendU32_Internal(Out, Compression);
	AppendU32_Internal(Out, 0);
	AppendU32_Internal(Out, 2835);
	AppendU32_Internal(Out, 2835);
	AppendU32_Internal(Out, 0);
	AppendU32_Internal(Out, 0);
	while (Out.Size() < OffBits)
	{
		Out.PushBack('\0');
	}
	for (Toolbox::size_t Index = 0; Index < PixelBytes; ++Index)
	{
		Out.PushBack(static_cast<char>(Index & 0xFF));
	}
	return Out;
}
// 検証用のWAVEを作る。引数は形式区画へそのまま書く。
Toolbox::FString MakeWave_Internal(Toolbox::uint16 Format, Toolbox::uint16 Channels, Toolbox::uint32 SampleRate,
                                   Toolbox::uint16 Bits, Toolbox::size_t DataBytes, bool bHasFormat, bool bHasData)
{
	Toolbox::FString Out("RIFF", 4);
	AppendU32_Internal(Out, 0);
	Out.PushBack('W');
	Out.PushBack('A');
	Out.PushBack('V');
	Out.PushBack('E');
	if (bHasFormat)
	{
		Out.PushBack('f');
		Out.PushBack('m');
		Out.PushBack('t');
		Out.PushBack(' ');
		AppendU32_Internal(Out, 16);
		AppendU16_Internal(Out, Format);
		AppendU16_Internal(Out, Channels);
		AppendU32_Internal(Out, SampleRate);
		AppendU32_Internal(Out, SampleRate * Channels * Bits / 8);
		AppendU16_Internal(Out, static_cast<Toolbox::uint16>(Channels * Bits / 8));
		AppendU16_Internal(Out, Bits);
	}
	if (bHasData)
	{
		Out.PushBack('d');
		Out.PushBack('a');
		Out.PushBack('t');
		Out.PushBack('a');
		AppendU32_Internal(Out, static_cast<Toolbox::uint32>(DataBytes));
		for (Toolbox::size_t Index = 0; Index < DataBytes; ++Index)
		{
			Out.PushBack(static_cast<char>(Index & 0xFF));
		}
	}
	return Out;
}
// 検証用の非同期環境をまとめる。
struct FAsyncFixture
{
	FFakeBackend Backend;
	Toolbox::FJobSystem Jobs{4};
	FTaskDispatcher Tasks{Jobs};
	FAssetService Assets{Backend, Backend, Backend};
};
// 要求が確定するまで待って一度だけ反映する。
void DrainUntilReady_Internal(FTaskDispatcher& Tasks)
{
	Tasks.WaitForPrepares();
	Tasks.PumpCommits();
}
// バイト列の検査値を求める。
Toolbox::uint64 ChecksumOf_Internal(const Toolbox::FString& Content)
{
	return Checksum_Internal(Content.Data(), Content.Size());
}
} // namespace
TEST("bitmap decode accepts a minimal image and reports facts")
{
	auto Decoded = DecodeAssetBytes(MakeBmp_Internal(2, 2, 24, 0, 54, 16).Data(), 70);
	REQUIRE(Decoded);
	REQUIRE(Decoded.Value().Kind == EAssetDecodeKind::Bitmap);
	REQUIRE(Decoded.Value().Width == 2);
	REQUIRE(Decoded.Value().Height == 2);
}
TEST("bitmap decode rejects broken inputs")
{
	REQUIRE(!DecodeAssetBytes(MakeBmp_Internal(2, 2, 24, 0, 54, 16).Data(), 20));
	Toolbox::FString BadMagic = MakeBmp_Internal(2, 2, 24, 0, 54, 16);
	BadMagic[0] = 'X';
	REQUIRE(!DecodeAssetBytes(BadMagic.Data(), BadMagic.Size()));
	REQUIRE(!DecodeAssetBytes(MakeBmp_Internal(0, 2, 24, 0, 54, 16).Data(), 70));
	REQUIRE(!DecodeAssetBytes(MakeBmp_Internal(2, 0, 24, 0, 54, 16).Data(), 70));
	REQUIRE(!DecodeAssetBytes(MakeBmp_Internal(2, 2, 8, 0, 54, 16).Data(), 70));
	REQUIRE(!DecodeAssetBytes(MakeBmp_Internal(2, 2, 24, 1, 54, 16).Data(), 70));
	REQUIRE(!DecodeAssetBytes(MakeBmp_Internal(100000, 100000, 24, 0, 54, 16).Data(), 70));
	REQUIRE(!DecodeAssetBytes(MakeBmp_Internal(2, 2, 24, 0, 1000, 4).Data(), 1004));
	REQUIRE(!DecodeAssetBytes(MakeBmp_Internal(2, 2, 24, 0, 54, 4).Data(), 58));
}
TEST("wave decode accepts PCM16 and reports facts")
{
	auto Decoded = DecodeAssetBytes(MakeWave_Internal(1, 1, 22050, 16, 8, true, true).Data(), 52);
	REQUIRE(Decoded);
	REQUIRE(Decoded.Value().Kind == EAssetDecodeKind::Wave);
	REQUIRE(Decoded.Value().SampleRate == 22050);
	REQUIRE(Decoded.Value().Channels == 1);
	REQUIRE(Decoded.Value().BitsPerSample == 16);
}
TEST("wave decode rejects broken inputs")
{
	REQUIRE(!DecodeAssetBytes(MakeWave_Internal(1, 1, 22050, 16, 8, false, true).Data(), 28));
	REQUIRE(!DecodeAssetBytes(MakeWave_Internal(1, 1, 22050, 16, 8, true, false).Data(), 36));
	REQUIRE(!DecodeAssetBytes(MakeWave_Internal(3, 1, 22050, 16, 8, true, true).Data(), 52));
	REQUIRE(!DecodeAssetBytes(MakeWave_Internal(1, 0, 22050, 16, 8, true, true).Data(), 52));
	REQUIRE(!DecodeAssetBytes(MakeWave_Internal(1, 3, 22050, 16, 8, true, true).Data(), 52));
	REQUIRE(!DecodeAssetBytes(MakeWave_Internal(1, 1, 0, 16, 8, true, true).Data(), 52));
	REQUIRE(!DecodeAssetBytes(MakeWave_Internal(1, 1, 22050, 8, 8, true, true).Data(), 52));
	REQUIRE(!DecodeAssetBytes(MakeWave_Internal(1, 1, 22050, 16, 7, true, true).Data(), 51));
	REQUIRE(!DecodeAssetBytes("plain text", 10));
	REQUIRE(!DecodeAssetBytes(nullptr, 0));
}
TEST("async texture loads prepared bytes exactly once")
{
	FAsyncFixture Fixture;
	// 検証用の作業場所。
	const Toolbox::FPath Scratch = Test::PrepareScratchDirectory("AsyncTexture");
	// 読み込む画像の内容。
	const Toolbox::FString Content = MakeBmp_Internal(2, 2, 24, 0, 54, 16);
	Test::WriteScratchFile(Scratch / "image.bmp", Content);
	auto Ticket = Fixture.Assets.LoadTextureAsync(Fixture.Tasks, FTaskScope{}, (Scratch / "image.bmp").ToUtf8());
	REQUIRE(Ticket);
	DrainUntilReady_Internal(Fixture.Tasks);
	REQUIRE(Ticket.Value().IsReady());
	auto Result = Ticket.Value().Take();
	REQUIRE(Result);
	REQUIRE(Fixture.Backend.GetTrace().TextureMemoryLoads == 1);
	REQUIRE(Fixture.Backend.GetTrace().TextureLoads == 0);
	REQUIRE(Fixture.Backend.GetTrace().TextureMemoryChecksum == ChecksumOf_Internal(Content));
	// 同期読み込みは非同期の登録を再利用する。
	REQUIRE(Fixture.Assets.LoadTexture((Scratch / "image.bmp").ToUtf8()));
	REQUIRE(Fixture.Backend.GetTrace().TextureLoads == 0);
}
TEST("async sound loads prepared bytes exactly once")
{
	FAsyncFixture Fixture;
	// 検証用の作業場所。
	const Toolbox::FPath Scratch = Test::PrepareScratchDirectory("AsyncSound");
	// 読み込む音声の内容。
	const Toolbox::FString Content = MakeWave_Internal(1, 1, 22050, 16, 8, true, true);
	Test::WriteScratchFile(Scratch / "sound.wav", Content);
	auto Ticket = Fixture.Assets.LoadSoundAsync(Fixture.Tasks, FTaskScope{}, (Scratch / "sound.wav").ToUtf8());
	REQUIRE(Ticket);
	DrainUntilReady_Internal(Fixture.Tasks);
	REQUIRE(Ticket.Value().IsReady());
	auto Result = Ticket.Value().Take();
	REQUIRE(Result);
	REQUIRE(Fixture.Backend.GetTrace().SoundMemoryLoads == 1);
	REQUIRE(Fixture.Backend.GetTrace().SoundLoads == 0);
	REQUIRE(Fixture.Backend.GetTrace().SoundMemoryChecksum == ChecksumOf_Internal(Content));
}
TEST("async load reports missing and broken files")
{
	FAsyncFixture Fixture;
	// 検証用の作業場所。
	const Toolbox::FPath Scratch = Test::PrepareScratchDirectory("AsyncMissing");
	auto Missing = Fixture.Assets.LoadTextureAsync(Fixture.Tasks, FTaskScope{}, (Scratch / "missing.bmp").ToUtf8());
	REQUIRE(Missing);
	DrainUntilReady_Internal(Fixture.Tasks);
	auto MissingResult = Missing.Value().Take();
	REQUIRE(!MissingResult && MissingResult.Error().Code == EErrorCode::NotFound);
	Test::WriteScratchFile(Scratch / "broken.bmp", "not an image");
	auto Broken = Fixture.Assets.LoadTextureAsync(Fixture.Tasks, FTaskScope{}, (Scratch / "broken.bmp").ToUtf8());
	REQUIRE(Broken);
	DrainUntilReady_Internal(Fixture.Tasks);
	auto BrokenResult = Broken.Value().Take();
	REQUIRE(!BrokenResult && BrokenResult.Error().Code == EErrorCode::InvalidArgument);
	auto WrongKind = Fixture.Assets.LoadSoundAsync(
	    Fixture.Tasks, FTaskScope{}, (Scratch / "broken.bmp").ToUtf8(), FSoundLoadOptions{ESoundStorage::Memory});
	REQUIRE(WrongKind);
	DrainUntilReady_Internal(Fixture.Tasks);
	auto WrongResult = WrongKind.Value().Take();
	REQUIRE(!WrongResult && WrongResult.Error().Code == EErrorCode::InvalidArgument);
}
TEST("async sound rejects stream storage without backend access")
{
	FAsyncFixture Fixture;
	auto Result = Fixture.Assets.LoadSoundAsync(Fixture.Tasks, FTaskScope{}, "sound.wav",
	                                            FSoundLoadOptions{ESoundStorage::Stream});
	REQUIRE(!Result && Result.Error().Code == EErrorCode::InvalidArgument);
	REQUIRE(Fixture.Backend.GetTrace().SoundMemoryLoads == 0);
	REQUIRE(Fixture.Backend.GetTrace().SoundLoads == 0);
}
TEST("async load expires when the scope dies")
{
	FAsyncFixture Fixture;
	// 検証用の作業場所。
	const Toolbox::FPath Scratch = Test::PrepareScratchDirectory("AsyncExpire");
	Test::WriteScratchFile(Scratch / "image.bmp", MakeBmp_Internal(2, 2, 24, 0, 54, 16));
	FTaskScope Scope = Fixture.Tasks.CreateScope();
	auto Ticket = Fixture.Assets.LoadTextureAsync(Fixture.Tasks, Scope, (Scratch / "image.bmp").ToUtf8());
	REQUIRE(Ticket);
	Fixture.Tasks.DestroyScope(Scope);
	DrainUntilReady_Internal(Fixture.Tasks);
	auto Result = Ticket.Value().Take();
	REQUIRE(!Result && Result.Error().Code == EErrorCode::InvalidState);
	REQUIRE(Fixture.Backend.GetTrace().TextureMemoryLoads == 0);
}
TEST("async load shares the synchronous cache")
{
	FAsyncFixture Fixture;
	// 検証用の作業場所。
	const Toolbox::FPath Scratch = Test::PrepareScratchDirectory("AsyncCache");
	Test::WriteScratchFile(Scratch / "image.bmp", MakeBmp_Internal(2, 2, 24, 0, 54, 16));
	// 同期読み込みの実体。弱参照キャッシュの再利用に必要。
	auto Sync = Fixture.Assets.LoadTexture((Scratch / "image.bmp").ToUtf8());
	REQUIRE(Sync);
	auto Ticket = Fixture.Assets.LoadTextureAsync(Fixture.Tasks, FTaskScope{}, (Scratch / "image.bmp").ToUtf8());
	REQUIRE(Ticket);
	REQUIRE(Ticket.Value().IsReady());
	REQUIRE(Ticket.Value().Take());
	REQUIRE(Fixture.Backend.GetTrace().TextureMemoryLoads == 0);
}
TEST("async load fails closed services and backends")
{
	FAsyncFixture Fixture;
	// 検証用の作業場所。
	const Toolbox::FPath Scratch = Test::PrepareScratchDirectory("AsyncClosed");
	Test::WriteScratchFile(Scratch / "image.bmp", MakeBmp_Internal(2, 2, 24, 0, 54, 16));
	Fixture.Assets.Shutdown();
	auto Stopped =
	    Fixture.Assets.LoadTextureAsync(Fixture.Tasks, FTaskScope{}, (Scratch / "image.bmp").ToUtf8());
	REQUIRE(!Stopped && Stopped.Error().Code == EErrorCode::InvalidState);
	FAsyncFixture Failing;
	Test::WriteScratchFile(Scratch / "fail.bmp", MakeBmp_Internal(2, 2, 24, 0, 54, 16));
	Failing.Backend.GetTrace().bFailTexture = true;
	auto Failed =
	    Failing.Assets.LoadTextureAsync(Failing.Tasks, FTaskScope{}, (Scratch / "fail.bmp").ToUtf8());
	REQUIRE(Failed);
	DrainUntilReady_Internal(Failing.Tasks);
	auto FailedResult = Failed.Value().Take();
	REQUIRE(!FailedResult && FailedResult.Error().Code == EErrorCode::BackendFailure);
}
TEST("async load resolves the project root once")
{
	FAsyncFixture Fixture;
	// 検証用の作業場所。
	const Toolbox::FPath Scratch = Test::PrepareScratchDirectory("AsyncRoot");
	REQUIRE(Fixture.Assets.SetProjectRoot(Scratch));
	REQUIRE(Toolbox::CreateDirectory(Scratch / "Assets"));
	Test::WriteScratchFile(Scratch / "Assets" / "image.bmp", MakeBmp_Internal(2, 2, 24, 0, 54, 16));
	auto Ticket = Fixture.Assets.LoadTextureAsync(Fixture.Tasks, FTaskScope{}, "Assets/image.bmp");
	REQUIRE(Ticket);
	DrainUntilReady_Internal(Fixture.Tasks);
	REQUIRE(Ticket.Value().Take());
	REQUIRE(Fixture.Backend.GetTrace().TextureMemoryLoads == 1);
}
TEST("empty tickets and discarded dispatchers never resolve")
{
	FAsyncTexture Empty;
	REQUIRE(!Empty.IsReady());
	REQUIRE(!Empty.Take());
	FAsyncFixture Fixture;
	// 検証用の作業場所。
	const Toolbox::FPath Scratch = Test::PrepareScratchDirectory("AsyncDiscard");
	Test::WriteScratchFile(Scratch / "image.bmp", MakeBmp_Internal(2, 2, 24, 0, 54, 16));
	auto Ticket = Fixture.Assets.LoadTextureAsync(Fixture.Tasks, FTaskScope{}, (Scratch / "image.bmp").ToUtf8());
	REQUIRE(Ticket);
	Fixture.Tasks.Shutdown();
	REQUIRE(!Ticket.Value().Take());
}
