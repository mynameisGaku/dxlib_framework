#include "Smoke.h"
#include "Toolbox/Platform.h"
#include "Toolbox/String.h"

// 登録された検証を実行して成否を終了コードへ変える。
Toolbox::int32 main(Toolbox::int32 ArgCount, char** Args)
{
	bool bAudio = false;
	for (Toolbox::int32 Index = 1; Index < ArgCount; ++Index)
	{
		if (Toolbox::FStringView(Args[Index]) == "--audio")
		{
			bAudio = true;
		}
		else
		{
			Toolbox::Err << "Usage: NativeSmoke [--audio]\nRun from the directory containing Assets.\n";
			return 2;
		}
	}
	// 検証対象の操作が返した成否と値。
	auto Result = Dxf::Testing::RunNativeSmoke(Toolbox::CurrentDirectory() / "Assets", bAudio);
	if (!Result)
	{
		Toolbox::Err << "REAL_SDK_SMOKE_FAILED: " << Result.Error().Message << '\n';
		return 1;
	}
	Toolbox::Out << "REAL_SDK_API_SMOKE_PASSED frames=" << Result.Value().Frames
	             << " audio=" << (Result.Value().bAudioTested ? "tested" : "not-tested")
	             << " visual-output=not-inspected audible-output=not-inspected\n";
	return 0;
}
