#include "Dxf/NativeBackends.h"
#include "Dxf/AppRunner.h"
#include "SandboxGame.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <filesystem>
#include <limits>
#include <vector>
namespace
{
std::string GetAssetRoot_Internal()
{
	std::vector<wchar_t> Buffer(512);
	for (;;)
	{
		const DWORD Size = GetModuleFileNameW(nullptr, Buffer.data(), static_cast<DWORD>(Buffer.size()));
		if (Size == 0)
		{
			throw std::runtime_error("GetModuleFileNameW failed");
		}
		if (Size < Buffer.size())
		{
			const auto Root = (std::filesystem::path(std::wstring(Buffer.data(), Size)).parent_path() / L"Assets").generic_u8string();
			return {reinterpret_cast<const char*>(Root.data()), Root.size()};
		}
		if (Buffer.size() >= 32768)
		{
			throw std::runtime_error("Executable path is too long");
		}
		Buffer.resize(Buffer.size() * 2);
	}
}
void ShowError_Internal(const std::string& Message)
{
	if (Message.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
	{
		return;
	}
	const int Length = static_cast<int>(Message.size());
	const int Count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Message.data(), Length, nullptr, 0);
	std::wstring Text = L"Application failed. Inspect the DxLib Log.txt file.";
	if (Count > 0)
	{
		Text.resize(static_cast<std::size_t>(Count));
		MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Message.data(), Length, Text.data(), Count);
	}
	MessageBoxW(nullptr, Text.c_str(), L"dxlib_framework", MB_OK | MB_ICONERROR);
}
}
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	try
	{
		Dxf::FDxLibBackends Backends;
		Dxf::FApplicationSettings Settings;
		Settings.Window.Title = "dxlib_framework - Sandbox";
		Dxf::FApplication Application(Backends.GetServices(), Settings, std::make_unique<Dxf::Sandbox::DSandboxGameInstance>());
		Dxf::FAppRunner Runner;
		auto Result = Runner.Run(Application, std::make_unique<Dxf::Sandbox::DSandboxScene>(GetAssetRoot_Internal()));
		if (!Result)
		{
			ShowError_Internal(Result.Error().Message);
			return 1;
		}
		return 0;
	}
	catch (const std::exception& Error)
	{
		ShowError_Internal(Error.what());
		return 1;
	}
	catch (...)
	{
		ShowError_Internal("Unknown application error");
		return 1;
	}
}
