// SPDX-License-Identifier: NOASSERTION
#include "ProjectRootEntry.h"
#include "Toolbox/Platform.h"

// exe横の設定で解決したProjectRootを標準出力へ出す起動試験器。
// 使い方: dxf_asset_probe <SettingsFileName>
Toolbox::int32 main(Toolbox::int32 ArgCount, char** Args)
{
	if (ArgCount != 2)
	{
		Toolbox::Err << "Usage: dxf_asset_probe <SettingsFileName>\n";
		return 2;
	}
	try
	{
		Toolbox::Out << Dxf::ResolveEntryProjectRoot(Args[1]).ToUtf8() << '\n';
		return 0;
	}
	catch (const Toolbox::FException& Error)
	{
		Toolbox::Err << Error.What() << '\n';
		return 1;
	}
	catch (...)
	{
		Toolbox::Err << "Unknown asset root error\n";
		return 1;
	}
}
