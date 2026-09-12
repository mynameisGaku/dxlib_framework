#include "Support/Test.h"
#include "Toolbox/Platform.h"
// 登録された検証を実行して成否を終了コードへ変える。
Toolbox::int32 main()
{
	// 失敗したテストの件数。
	Toolbox::int32 Failures = 0;
	for (const auto& TestCase : Test::Cases_Internal())
	{
		try
		{
			TestCase.Run();
			Toolbox::Out << "[PASS] " << TestCase.Name << '\n';
		}
		catch (const Toolbox::FException& Error)
		{
			++Failures;
			Toolbox::Out << "[FAIL] " << TestCase.Name << ": " << Error.What() << '\n';
		}
		catch (...)
		{
			++Failures;
			Toolbox::Out << "[FAIL] " << TestCase.Name << ": unknown exception\n";
		}
	}
	Toolbox::Out << Test::Cases_Internal().Size() - static_cast<Toolbox::size_t>(Failures) << "/"
	             << Test::Cases_Internal().Size() << " passed\n";
	return Failures == 0 ? 0 : 1;
}
