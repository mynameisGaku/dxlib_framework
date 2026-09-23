// SPDX-License-Identifier: NOASSERTION
// コピー側Starterへだけ追加する検証器。ゲームのソースは変更しない。
#include "StarterCopyProbe.h"
#include "SandboxMenuScene.h"
#include "SandboxGame.h"
#include "Toolbox/Platform.h"
#include "Dxf/NativeHandle.h"
#include "DxLib.h"
#include <stdio.h>
#ifdef CreateDirectory
#undef CreateDirectory
#endif
#ifdef CopyFile
#undef CopyFile
#endif
namespace
{
using namespace Dxf;
using namespace Dxf::Sandbox;
// 次のPollで届けるキー。InputSystemの押下・解放判定は本物を使う。
FRawInput GInput;
// OS境界だけを固定する。Applicationとゲーム本体には代用品を使わない。
class FCopyInput final : public IInputSource
{
public:
	TResult<FRawInput> Poll() override
	{
		return TResult<FRawInput>::Success(GInput);
	}
};
// 一つの条件が成立しなければ検証を中断する。
void Check(bool Ok, const char* Message)
{
	if (!Ok)
	{
		throw Toolbox::FException(Message);
	}
}
// GUI実行ファイルでも成功・失敗の理由を保存する。
void Report(const Toolbox::FString& Root, const Toolbox::FString& Text)
{
	FILE* File = _wfopen(Toolbox::ToWide((Toolbox::FPath(Root) / "probe-result.txt").ToUtf8()).CStr(), L"wb");
	if (File)
	{
		fwrite(Text.Data(), 1, Text.Size(), File);
		fclose(File);
	}
}
// 一括読み戻し用の画像を解放する。
void ReleaseImage(void*, Toolbox::int32 Handle) noexcept
{
	DxLib::DeleteSoftImage(Handle);
}
// 表示の差を領域ごとに確認するための観察値。
struct FImageCheck
{
	// 結果時間の表示領域。
	Toolbox::uint64 ScoreHash = 1469598103934665603ull;
	Toolbox::int32 ScorePixels = 0;
	// 失敗理由の表示領域。
	Toolbox::int32 ErrorPixels = 0;
};
// NativeSandboxSmokeと同じゴール色判定に、文字領域の一括読み戻しを加える。
FImageCheck Capture(const Toolbox::FPath& File, bool Play)
{
	Check(DxLib::SetDrawScreen(DX_SCREEN_FRONT) == 0, "front buffer selection failed");
	FNativeHandle Image(DxLib::MakeARGB8ColorSoftImage(1280, 720), nullptr, &ReleaseImage);
	Check(Image.Get() >= 0, "image allocation failed");
	const bool Read = DxLib::GetDrawScreenSoftImage(0, 0, 1280, 720, Image.Get()) == 0;
	const bool Saved = DxLib::SaveDrawScreenToPNG(0, 0, 1280, 720, File.ToUtf8().CStr()) == 0;
	Check(DxLib::SetDrawScreen(DX_SCREEN_BACK) == 0, "back buffer restoration failed");
	Check(Read && Saved, "image readback or save failed");
	FImageCheck Result;
	// DxLibの出力引数に合わせた色成分。
	int R = 0;
	int G = 0;
	int B = 0;
	int A = 0;
	Check(DxLib::GetPixelSoftImage(Image.Get(), 1150, 400, &R, &G, &B, &A) == 0, "goal pixel read failed");
	Check(Play ? R == 40 && G == 160 && B == 80 : R == 0 && G == 0 && B == 0, "goal or menu pixels differ");
	for (Toolbox::int32 Y = 210; Y < 315; Y += 2)
	{
		for (Toolbox::int32 X = 60; X < 1200; X += 2)
		{
			Check(DxLib::GetPixelSoftImage(Image.Get(), X, Y, &R, &G, &B, &A) == 0, "text pixel read failed");
			if (Y < 255)
			{
				Result.ScoreHash =
				    (Result.ScoreHash ^ static_cast<Toolbox::uint64>((R << 16) | (G << 8) | B)) * 1099511628211ull;
				Result.ScorePixels += R + G + B > 100 ? 1 : 0;
			}
			if (Y >= 270)
			{
				Result.ErrorPixels += R + G + B > 100 ? 1 : 0;
			}
		}
	}
	return Result;
}
} // namespace
Dxf::IInputSource& StarterCopyInput()
{
	// WindowsMainのApplicationより長く生存する検証入力。
	static FCopyInput Input;
	return Input;
}
Dxf::TResult<void> RunStarterCopyProbe(Dxf::FApplication& App, const Toolbox::FString& Root,
                                       Toolbox::TUniquePtr<Dxf::DScene> Scene)
{
	try
	{
		const Toolbox::FPath Output = Toolbox::ExecutableDirectory() / "probe";
		Check(Toolbox::IsDirectory(Output) || Toolbox::CreateDirectory(Output), "output directory failed");
		const auto Started = App.Start(Toolbox::Move(Scene));
		Check(static_cast<bool>(Started), "Starter initial scene failed");
		// 2進数で正確に表せる刻みを使い、ポーズの有無だけを比較する。
		Toolbox::f64 Time = 0;
		auto Step = [&]
		{
			const auto Result = App.Step(Time += 0.125);
			Check(Result && Result.Value(), "Application step failed");
		};
		auto Press = [&](EKey Key)
		{
			GInput.Keys[static_cast<Toolbox::size_t>(Key)] = true;
			Step();
			GInput.Keys[static_cast<Toolbox::size_t>(Key)] = false;
			Step();
		};
		Step();
		const auto Title = Capture(Output / "title.png", false);
		Check(Title.ErrorPixels == 0, "unexpected error in initial title");
		// 呼出しスクリプトがコピー側の画像だけを退避している。
		auto* OriginalTitle = App.GetScenes().GetCurrent();
		Press(EKey::Enter);
		Check(App.GetScenes().GetCurrent() == OriginalTitle && App.GetScenes().GetLastTransitionError(),
		      "failed loading did not preserve title and reason");
		const auto Failure = Capture(Output / "load-failure.png", false);
		Check(Failure.ErrorPixels > 30, "failure reason is not visible");
		Toolbox::CopyFile(Toolbox::FPath(Root) / "Assets/player.bmp.saved", Toolbox::FPath(Root) / "Assets/player.bmp");
		// 同じApplicationで再試行。結果時間は保持資源ではなく実表示で比較する。
		Toolbox::uint64 FirstScore = 0;
		for (Toolbox::int32 Attempt = 0; Attempt < 2; ++Attempt)
		{
			Press(EKey::Enter);
			auto* Play = App.GetScenes().GetCurrent()->TryCast<DSandboxScene>();
			Check(Play && !App.GetScenes().GetLastTransitionError(), "retry failed");
			const auto Player = Play->GetPlayer();
			Check(Player.Get() && Player.Get()->GetPosition().X == 320, "retry did not reset player");
			Capture(Output / (Attempt == 0 ? "play.png" : "retry.png"), true);
			Press(EKey::Space);
			Press(EKey::P);
			Check(Play->GetClock().IsPaused(), "pause failed");
			GInput.Keys[static_cast<Toolbox::size_t>(EKey::D)] = true;
			// 2周目だけ2秒のポーズを挟む。同じ結果時間になる必要がある。
			for (Toolbox::int32 Frame = 0; Frame < Attempt * 16; ++Frame)
			{
				Step();
				Check(Player.Get()->GetPosition().X == 320, "player moved during pause");
			}
			Press(EKey::P);
			for (Toolbox::int32 Frame = 0; Frame < 32; ++Frame)
			{
				Step();
			}
			GInput.Keys[static_cast<Toolbox::size_t>(EKey::D)] = false;
			Check(App.GetScenes().GetCurrent()->TryCast<ASandboxMenuScene>() && !Player.Get(),
			      "goal did not retire player");
			const auto Result = Capture(Output / (Attempt == 0 ? "result.png" : "paused-result.png"), false);
			Check(Result.ScorePixels > 30, "result time is not visible");
			if (Attempt == 0)
			{
				FirstScore = Result.ScoreHash;
			}
			else
			{
				Check(Result.ScoreHash == FirstScore, "pause or retry changed the result time");
			}
		}
		Press(EKey::Space);
		const auto Back = Capture(Output / "title-again.png", false);
		Check(Back.ErrorPixels == 0 && Back.ScorePixels == 0, "title retained old result or error");
		GInput.Keys[static_cast<Toolbox::size_t>(EKey::Escape)] = true;
		const auto Quit = App.Step(Time += 0.125);
		Check(Quit && !Quit.Value() && !App.IsRunning(), "quit failed");
		Report(Root, "PASS: copied Starter; missing texture retry; pause score pixels; retry; exit\nRoot=" + Root);
		return TResult<void>::Success();
	}
	catch (const Toolbox::FException& Error)
	{
		Report(Root, "FAIL: " + Toolbox::FString(Error.What()));
		return TResult<void>::Failure(EErrorCode::BackendFailure, Error.What());
	}
}
