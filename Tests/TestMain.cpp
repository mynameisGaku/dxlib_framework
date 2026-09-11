#include "Support/Test.h"
#include <iostream>
int main()
{
	int Failures = 0;
	for (const auto& TestCase : Test::Cases_Internal())
	{
		try
		{
			TestCase.Run();
			std::cout << "[PASS] " << TestCase.Name << '\n';
		}
		catch (const std::exception& Error)
		{
			++Failures;
			std::cout << "[FAIL] " << TestCase.Name << ": " << Error.what() << '\n';
		}
		catch (...)
		{
			++Failures;
			std::cout << "[FAIL] " << TestCase.Name << ": unknown exception\n";
		}
	}
	std::cout << Test::Cases_Internal().size() - static_cast<std::size_t>(Failures) << "/" << Test::Cases_Internal().size() << " passed\n";
	return Failures == 0 ? 0 : 1;
}
