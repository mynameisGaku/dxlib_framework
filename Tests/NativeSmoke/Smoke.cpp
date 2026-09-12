#include "Smoke.h"
#include "Dxf/NativeBackends.h"
#include "Dxf/DxLibSession.h"
#include "Dxf/AssetService.h"
#include "Dxf/AudioPlayer.h"
#include "Dxf/RenderSystem2D.h"
#include "Dxf/InputSystem.h"
#include <chrono>
#include <stdexcept>

namespace Dxf::Testing
{
namespace
{
void RequireSuccess_Internal(TResult<void> Result)
{
	if (!Result)
	{
		throw std::runtime_error(Result.Error().Message);
	}
}
template <typename T>
T TakeOrThrow_Internal(TResult<T> Result)
{
	if (!Result)
	{
		throw std::runtime_error(Result.Error().Message);
	}
	return std::move(Result).Value();
}
std::string ToUtf8_Internal(const std::filesystem::path& Path)
{
	const auto Encoded = Path.u8string();
	return {Encoded.begin(), Encoded.end()};
}
class FTemporaryDirectory
{
public:
	FTemporaryDirectory()
	{
		const auto Stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		for (int Attempt = 0; Attempt < 100; ++Attempt)
		{
			m_Path = std::filesystem::temp_directory_path() /
				("dxf-smoke-" + std::to_string(Stamp) + "-" + std::to_string(Attempt));
			if (std::filesystem::create_directory(m_Path))
			{
				return;
			}
		}
		throw std::runtime_error("Cannot allocate a unique smoke-test directory");
	}
	~FTemporaryDirectory()
	{
		std::error_code Error;
		std::filesystem::remove_all(m_Path, Error);
	}
	FTemporaryDirectory(const FTemporaryDirectory&) = delete;
	FTemporaryDirectory& operator=(const FTemporaryDirectory&) = delete;
	const std::filesystem::path& GetPath() const noexcept
	{
		return m_Path;
	}
private:
	std::filesystem::path m_Path;
};
}
TResult<FNativeSmokeReport> RunNativeSmoke(const std::filesystem::path& AssetsDirectory, bool bTestAudio)
{
	try
	{
		const auto BitmapPath = AssetsDirectory / "player.bmp";
		const auto SoundPath = AssetsDirectory / "confirm.wav";
		if (!std::filesystem::is_regular_file(BitmapPath) ||
			(bTestAudio && !std::filesystem::is_regular_file(SoundPath)))
		{
			return TResult<FNativeSmokeReport>::Failure(EErrorCode::NotFound, "Smoke-test assets are missing");
		}
		FTemporaryDirectory Temporary;
		const auto JapanesePath = Temporary.GetPath() / std::filesystem::path(u8"日本語の画像.bmp");
		std::filesystem::copy_file(BitmapPath, JapanesePath);

		FDxLibBackends Backends;
		auto Services = Backends.GetServices();
		FDxLibSession Session(Services.Platform);
		FWindowSettings Settings;
		Settings.Title = "dxlib_framework 日本語・実SDKスモークテスト";
		Settings.Width = 320;
		Settings.Height = 240;
		Settings.bVSync = false;
		RequireSuccess_Internal(Session.Initialize(Settings));
		FAssetService Assets(Services.Textures, Services.Sounds, Services.Fonts);
		FAudioPlayer Audio(Services.Sounds);
		FRenderSystem2D Renderer(Services.Renderer);
		FInputSystem Input(Services.Input);
		auto Texture = TakeOrThrow_Internal(Assets.LoadTexture(ToUtf8_Internal(JapanesePath)));
		auto Font = TakeOrThrow_Internal(Assets.LoadFont());
		auto Target = TakeOrThrow_Internal(Assets.CreateRenderTarget(96, 96));
		if (Texture.GetWidth() != 64 || Texture.GetHeight() != 64)
		{
			throw std::runtime_error("Loaded sample texture has unexpected dimensions");
		}
		FSound Sound;
		if (bTestAudio)
		{
			Sound = TakeOrThrow_Internal(Assets.LoadSound(ToUtf8_Internal(SoundPath)));
			auto First = TakeOrThrow_Internal(Audio.Play(Sound));
			auto Second = TakeOrThrow_Internal(Audio.Play(Sound));
			RequireSuccess_Internal(Audio.SetVolume(Second, 0.25f));
			if (!Audio.Stop(First) || !Second)
			{
				throw std::runtime_error("Playback instances were not independent");
			}
		}
		FNativeSmokeReport Report;
		for (int Frame = 0; Frame < 12; ++Frame)
		{
			if (!TakeOrThrow_Internal(Services.Platform.PumpEvents()))
			{
				throw std::runtime_error("The smoke-test window was closed before completion");
			}
			RequireSuccess_Internal(Input.Update());
			RequireSuccess_Internal(Renderer.BeginFrame(320, 240, {12, 12, 12, 255}));
			auto& Render = Renderer.GetContext();
			RequireSuccess_Internal(Render.SetRenderTarget(Target));
			RequireSuccess_Internal(Render.ClearTarget({0, 0, 0, 255}));
			RequireSuccess_Internal(Render.Draw(Texture, {8, 8}));
			RequireSuccess_Internal(Render.SetBackBuffer());
			RequireSuccess_Internal(Render.Draw(Target.AsTexture(), {16, 16}));
			RequireSuccess_Internal(Render.FillRectangle({128, 24, 240, 80}));
			RequireSuccess_Internal(Render.DrawText(Font, "日本語 UTF-8", {16, 140}));
			RequireSuccess_Internal(Render.Native([] { return TResult<void>::Success(); }));
			RequireSuccess_Internal(Renderer.EndFrame());
			RequireSuccess_Internal(Audio.Tick());
			++Report.Frames;
		}
		Audio.Shutdown();
		Renderer.CancelFrame();
		Assets.Shutdown();
		Report.bResourcesInvalidated = !Texture.IsValid() && !Font.IsValid() && !Target.IsValid() && !Sound.IsValid();
		if (!Report.bResourcesInvalidated)
		{
			throw std::runtime_error("Assets survived backend shutdown");
		}
		Session.Shutdown();
		Report.bJapanesePathTested = true;
		Report.bAudioTested = bTestAudio;
		return TResult<FNativeSmokeReport>::Success(Report);
	}
	catch (const std::exception& Error)
	{
		return TResult<FNativeSmokeReport>::Failure(EErrorCode::BackendFailure, Error.what());
	}
	catch (...)
	{
		return TResult<FNativeSmokeReport>::Failure(EErrorCode::UserException, "Unknown exception in native smoke test");
	}
}
}
