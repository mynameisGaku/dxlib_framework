#include "Smoke.h"
#include <iostream>
#include <string_view>

int main(int ArgCount, char** Args)
{
	bool bAudio = false;
	for (int Index = 1; Index < ArgCount; ++Index)
	{
		if (std::string_view(Args[Index]) == "--audio")
		{
			bAudio = true;
		}
		else
		{
			std::cerr << "Usage: NativeSmoke [--audio]\nRun from the directory containing Assets.\n";
			return 2;
		}
	}
	auto Result = Dxf::Testing::RunNativeSmoke(std::filesystem::current_path() / "Assets", bAudio);
	if (!Result)
	{
		std::cerr << "REAL_SDK_SMOKE_FAILED: " << Result.Error().Message << '\n';
		return 1;
	}
	std::cout << "REAL_SDK_API_SMOKE_PASSED frames=" << Result.Value().Frames
		<< " audio=" << (Result.Value().bAudioTested ? "tested" : "not-tested")
		<< " visual-output=not-inspected audible-output=not-inspected\n";
	return 0;
}
