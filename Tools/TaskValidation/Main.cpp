// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include <stdio.h>
#include <string.h>

int main(int ArgumentCount, char** Arguments)
{
	// 任意の名前による絞り込みは、失敗ケースの独立した再現に使用する。
	Toolbox::size_t Passed = 0;
	Toolbox::size_t Total = 0;
	for (const auto& Entry : Test::Cases_Internal())
	{
		if (ArgumentCount > 1 && strcmp(Entry.Name, Arguments[1]) != 0)
		{
			continue;
		}
		++Total;
		try
		{
			Entry.Run();
			++Passed;
			printf("PASS %s\n", Entry.Name);
		}
		catch (const Toolbox::FException& Error)
		{
			printf("FAIL %s: %s\n", Entry.Name, Error.What());
		}
		catch (...)
		{
			printf("FAIL %s: unknown exception\n", Entry.Name);
		}
	}
	printf("%llu/%llu passed\n", static_cast<unsigned long long>(Passed), static_cast<unsigned long long>(Total));
	return Total != 0 && Passed == Total ? 0 : 1;
}
