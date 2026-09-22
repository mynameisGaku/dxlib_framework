#include "Smoke.h"
#include "Dxf/NativeBackends.h"
#include "Dxf/DxLibSession.h"
#include "Dxf/AssetService.h"
#include "Dxf/AudioPlayer.h"
#include "Dxf/RenderSystem.h"
#include "Dxf/InputSystem.h"
#include "Toolbox/Platform.h"
#include "Toolbox/Utility.h"

namespace Dxf::Testing
{
namespace
{
// 失敗結果を例外へ変えてスモーク検証を中断する。
void RequireSuccess_Internal(TResult<void> Result)
{
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
}
template <typename T> T TakeOrThrow_Internal(TResult<T> Result)
{
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
	return Toolbox::Move(Result).Value();
}
// ネイティブパスをUTF-8の文字列へ変換する。
Toolbox::FString ToUtf8_Internal(const Toolbox::FPath& Path)
{
	// 文字コード変換後のバイト列。
	const auto Encoded = Path.ToUtf8();
	return {Encoded.Begin(), Encoded.End()};
}
// ファイル検証用の一時ディレクトリを所有する。
class FTemporaryDirectory
{
public:
	// 検証に必要な依存先と初期状態を設定する。
	FTemporaryDirectory()
	{
		const auto Stamp = Toolbox::MonotonicNanoseconds();
		for (Toolbox::int32 Attempt = 0; Attempt < 100; ++Attempt)
		{
			m_Path = Toolbox::TemporaryDirectory() /
			         ("dxf-smoke-" + Toolbox::ToString(Stamp) + "-" + Toolbox::ToString(Attempt));
			if (Toolbox::CreateDirectory(m_Path))
			{
				return;
			}
		}
		throw Toolbox::FException("Cannot allocate a unique smoke-test directory");
	}
	// 所有データを解放し、必要な破棄の観測を行う。
	~FTemporaryDirectory()
	{
		// 失敗から取り出したエラー。
		Toolbox::int32 Error;
		Toolbox::RemoveTree(m_Path, Error);
	}
	// 検証に必要な依存先と初期状態を設定する。
	FTemporaryDirectory(const FTemporaryDirectory&) = delete;
	FTemporaryDirectory& operator=(const FTemporaryDirectory&) = delete;
	const Toolbox::FPath& GetPath() const noexcept
	{
		return m_Path;
	}

private:
	// 一時ファイルの所有先。
	Toolbox::FPath m_Path;
};
} // namespace
// 実バックエンドの画像・文字・入力・任意の音声を検証する。
TResult<FNativeSmokeReport> RunNativeSmoke(const Toolbox::FPath& AssetsDirectory, bool bTestAudio)
{
	try
	{
		// 検証用画像のファイル名。
		const auto BitmapPath = AssetsDirectory / "player.bmp";
		// 検証用音声のファイル名。
		const auto SoundPath = AssetsDirectory / "confirm.wav";
		if (!Toolbox::IsRegularFile(BitmapPath) || (bTestAudio && !Toolbox::IsRegularFile(SoundPath)))
		{
			return TResult<FNativeSmokeReport>::Failure(EErrorCode::NotFound, "Smoke-test assets are missing");
		}
		// 検証終了時に解放する一時資源。
		FTemporaryDirectory Temporary;
		// 文字コード処理を検証する日本語パス。
		const auto JapanesePath = Temporary.GetPath() / Toolbox::FPath(u8"日本語の画像.bmp");
		Toolbox::CopyFile(BitmapPath, JapanesePath);

		// 複数の機能を提供する検証用バックエンド群。
		FDxLibBackends Backends;
		// 各機能の依存先を束ねた参照。
		auto Services = Backends.GetServices();
		// 実行中のセッション。
		FDxLibSession Session(Services.Platform);
		// 起動または描画の設定。
		FWindowSettings Settings;
		Settings.Title = "dxlib_framework 日本語・実SDKスモークテスト";
		Settings.Width = 320;
		Settings.Height = 240;
		Settings.bVSync = false;
		// 失敗結果を例外へ変えてスモーク検証を中断する。
		RequireSuccess_Internal(Session.Initialize(Settings));
		// 検証に使用する資源管理。
		FAssetService Assets(Services.Textures, Services.Sounds, Services.Fonts);
		// 再生状態を管理する音声サービス。
		FAudioPlayer Audio(Services.Sounds);
		// 描画を実行する検証用レンダラー。
		FRenderSystem Renderer(Services.Renderer);
		// 検証で配信する入力状態。
		FInputSystem Input(Services.Input);
		// 検証で使用する画像資源。
		auto Texture = TakeOrThrow_Internal(Assets.LoadTexture(ToUtf8_Internal(JapanesePath)));
		// 検証で使用するフォント資源。
		auto Font = TakeOrThrow_Internal(Assets.LoadFont());
		// 描画先または遷移先。
		auto Target = TakeOrThrow_Internal(Assets.CreateRenderTarget(96, 96));
		if (Texture.GetWidth() != 64 || Texture.GetHeight() != 64)
		{
			throw Toolbox::FException("Loaded sample texture has unexpected dimensions");
		}
		// 検証で使用する音声資源。
		FSound Sound;
		if (bTestAudio)
		{
			Sound = TakeOrThrow_Internal(Assets.LoadSound(ToUtf8_Internal(SoundPath)));
			// 最初に生成または登録した対象。
			auto First = TakeOrThrow_Internal(Audio.Play(Sound));
			// 二番目に生成または登録した対象。
			auto Second = TakeOrThrow_Internal(Audio.Play(Sound));
			// 失敗結果を例外へ変えてスモーク検証を中断する。
			RequireSuccess_Internal(Audio.SetVolume(Second, 0.25f));
			if (!Audio.Stop(First) || !Second)
			{
				throw Toolbox::FException("Playback instances were not independent");
			}
		}
		// スモーク検証で確認した項目。
		FNativeSmokeReport Report;
		for (Toolbox::int32 Frame = 0; Frame < 12; ++Frame)
		{
			if (!TakeOrThrow_Internal(Services.Platform.PumpEvents()))
			{
				throw Toolbox::FException("The smoke-test window was closed before completion");
			}
			// 失敗結果を例外へ変えてスモーク検証を中断する。
			RequireSuccess_Internal(Input.Update());
			// 失敗結果を例外へ変えてスモーク検証を中断する。
			RequireSuccess_Internal(Renderer.BeginFrame(320, 240, {12, 12, 12, 255}));
			// フックへ渡す描画環境。
			auto& Render = Renderer.GetContext();
			// 失敗結果を例外へ変えてスモーク検証を中断する。
			RequireSuccess_Internal(Render.SetRenderTarget(Target));
			// 失敗結果を例外へ変えてスモーク検証を中断する。
			RequireSuccess_Internal(Render.ClearTarget({0, 0, 0, 255}));
			// 失敗結果を例外へ変えてスモーク検証を中断する。
			RequireSuccess_Internal(Render.Get2D().DrawSprite(Texture, {8, 8}));
			// 失敗結果を例外へ変えてスモーク検証を中断する。
			RequireSuccess_Internal(Render.SetBackBuffer());
			// 失敗結果を例外へ変えてスモーク検証を中断する。
			RequireSuccess_Internal(Render.Get2D().DrawSprite(Target.AsTexture(), {16, 16}));
			// 失敗結果を例外へ変えてスモーク検証を中断する。
			RequireSuccess_Internal(Render.Get2D().FillRectangle({128, 24, 240, 80}));
			// 失敗結果を例外へ変えてスモーク検証を中断する。
			RequireSuccess_Internal(Render.Get2D().DrawText(Font, "日本語 UTF-8", {16, 140}));
			// 失敗結果を例外へ変えてスモーク検証を中断する。
			RequireSuccess_Internal(Render.Native(
			    []
			    {
				    return TResult<void>::Success();
			    }));
			// 失敗結果を例外へ変えてスモーク検証を中断する。
			RequireSuccess_Internal(Renderer.EndFrame());
			// 失敗結果を例外へ変えてスモーク検証を中断する。
			RequireSuccess_Internal(Audio.Tick());
			++Report.Frames;
		}
		Audio.Shutdown();
		Renderer.CancelFrame();
		Assets.Shutdown();
		Report.bResourcesInvalidated = !Texture.IsValid() && !Font.IsValid() && !Target.IsValid() && !Sound.IsValid();
		if (!Report.bResourcesInvalidated)
		{
			throw Toolbox::FException("Assets survived backend shutdown");
		}
		Session.Shutdown();
		Report.bJapanesePathTested = true;
		Report.bAudioTested = bTestAudio;
		return TResult<FNativeSmokeReport>::Success(Report);
	}
	catch (const Toolbox::FException& Error)
	{
		return TResult<FNativeSmokeReport>::Failure(EErrorCode::BackendFailure, Error.What());
	}
	catch (...)
	{
		return TResult<FNativeSmokeReport>::Failure(EErrorCode::UserException,
		                                            "Unknown exception in native smoke test");
	}
}
} // namespace Dxf::Testing
