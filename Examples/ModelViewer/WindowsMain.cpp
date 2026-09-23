// SPDX-License-Identifier: NOASSERTION
#include "Dxf/NativeBackends.h"
#include "Dxf/AppRunner.h"
#include "ModelViewerScene.h"
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
	MessageBoxW(nullptr, L"ModelViewerでエラーが発生しました。Visual Studioの出力とDxLibのLog.txtを確認してください。",
		L"dxlib_framework / ModelViewer", MB_OK | MB_ICONERROR);
}
}
Toolbox::int32 WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, Toolbox::int32)
{
	try
	{
		Dxf::FDxLibBackends Backends;
		Dxf::FApplicationSettings Settings;
		Settings.Window.Title = "dxlib_framework - ModelViewer";
		Settings.ProjectRoot = Dxf::ResolveEntryProjectRoot("ModelViewer.dxfpaths").ToUtf8();
		Dxf::FApplication Application(Backends.GetServices(), Settings);
		Dxf::FAppRunner Runner;
		auto Result = Runner.Run(Application, Toolbox::MakeUnique<Dxf::ModelViewer::AModelViewerScene>());
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
		ReportFailure_Internal("Unknown ModelViewer error");
		return 1;
	}
}
