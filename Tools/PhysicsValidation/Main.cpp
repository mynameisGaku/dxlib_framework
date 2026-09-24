// SPDX-License-Identifier: NOASSERTION
// Cランタイムへの出力は検証ツールのこの入口だけで扱う。
#include "TestCases.h"
#include <stdio.h>
using namespace Toolbox;
int32 RunGroup_Internal(const PhysicsTest::FCase* Cases, size_t Count)
{
	size_t Failed = 0;
	for (size_t Index = 0; Index < Count; ++Index)
	{
		try
		{
			Cases[Index].Run();
			printf("PASS %s\n", Cases[Index].Name);
		}
		catch (const FException& Error)
		{
			++Failed;
			printf("FAIL %s: %s\n", Cases[Index].Name, Error.What());
		}
		catch (...)
		{
			++Failed;
			printf("FAIL %s: unexpected exception\n", Cases[Index].Name);
		}
	}
	printf("RESULT %zu/%zu passed\n", Count - Failed, Count);
	return Failed == 0 ? 0 : 1;
}

int32 main()
{
	size_t Count = 0;
	const PhysicsTest::FCase* Cases = PhysicsTest::GetCollisionCases(Count);
	int32 Failed = RunGroup_Internal(Cases, Count);
	Cases = PhysicsTest::GetSweepCases(Count);
	Failed += RunGroup_Internal(Cases, Count);
	Cases = PhysicsTest::GetFixedStepCases(Count);
	Failed += RunGroup_Internal(Cases, Count);
	Cases = PhysicsTest::GetRigidBodyCases(Count);
	Failed += RunGroup_Internal(Cases, Count);
	Cases = PhysicsTest::GetContactCases(Count);
	Failed += RunGroup_Internal(Cases, Count);
	Cases = PhysicsTest::GetSolverCases(Count);
	Failed += RunGroup_Internal(Cases, Count);
	Cases = PhysicsTest::GetContinuousCases(Count);
	Failed += RunGroup_Internal(Cases, Count);
	Cases = PhysicsTest::GetStabilityCases(Count);
	Failed += RunGroup_Internal(Cases, Count);
	Cases = PhysicsTest::GetWorldQueryCases(Count);
	Failed += RunGroup_Internal(Cases, Count);
	Cases = PhysicsTest::GetWorldQuery2DCases(Count);
	Failed += RunGroup_Internal(Cases, Count);
	Cases = PhysicsTest::GetWorldQueryFilterCases(Count);
	Failed += RunGroup_Internal(Cases, Count);
	Cases = PhysicsTest::GetWorldOverlapCases(Count);
	Failed += RunGroup_Internal(Cases, Count);
	Cases = PhysicsTest::GetWorldSweepCases(Count);
	Failed += RunGroup_Internal(Cases, Count);
	Cases = PhysicsTest::GetWorldSweepNormalCases(Count);
	Failed += RunGroup_Internal(Cases, Count);
	Cases = PhysicsTest::GetParallelCases(Count);
	Failed += RunGroup_Internal(Cases, Count);
	return Failed == 0 ? 0 : 1;
}
