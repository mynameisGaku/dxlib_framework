// SPDX-License-Identifier: NOASSERTION
#include "Dxf/NativeBackends.h"
#include "Dxf/AppRunner.h"
#include "RenderDebugScene.h"
#include "../Shared/ProjectRootEntry.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
namespace
{
// 失敗内容はデバッガへ出力し、利用者へ起動失敗を通知する。
void ReportFailure_Internal(const char* Message)
{
	OutputDebugStringA(Message);
	MessageBoxW(nullptr, L"RenderDebugでエラーが発生しました。Visual Studioの出力とDxLibのLog.txtを確認してください。",
		L"dxlib_framework / RenderDebug", MB_OK | MB_ICONERROR);
}
}
Toolbox::int32 WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, Toolbox::int32)
{
	try
	{
		Dxf::FDxLibBackends Backends;
		Dxf::FApplicationSettings Settings;
		Settings.Window.Title = "dxlib_framework - RenderDebug";
		Settings.ProjectRoot = Dxf::ResolveEntryProjectRoot("RenderDebug.dxfpaths").ToUtf8();
		Dxf::FApplication Application(Backends.GetServices(), Settings);
		Dxf::FAppRunner Runner;
		auto Result = Runner.Run(Application, Toolbox::MakeUnique<Dxf::RenderDebug::ARenderDebugScene>(Application.GetExecutionJobs()));
		if (!Result)
		{
			ReportFailure_Internal(Result.Error().Message.CStr());
			return 1;
		}
		return 0;
	}
	catch (const Toolbox::FException& Error)
	{
		ReportFailure_Internal(Error.What());
		return 1;
	}
	catch (...)
	{
		ReportFailure_Internal("Unknown RenderDebug error");
		return 1;
	}
}
