// SPDX-License-Identifier: NOASSERTION
// キャラクター移動とWorld問い合わせの負荷測定。CTestには含めない（Releaseで手動実行する）。
// 使い方: dxf_character_benchmark <series> [--reference] [--pilot]
//   legacy
//   前回（2026-09-25）と同じ18条件（2D／3D、障害物16・128・512、キャラクター1・8・32）。床はDynamic（前回どおり）
//   legacy-static 同じ18条件で、床と障害物のBodyをStaticにしたもの
//   heavy   以前中止した障害物1024・キャラクター64（2D／3D、Staticの床）
//   kernels Toolboxの詳細判定の関数の1回あたりの費用（2D／3Dの同じ配置）
//   costs   前回の配置で、キャラクターの位置での問い合わせ種別ごとの費用（索引と総当たり、結果の一致）
//   scaling 移動継続・局所・動的更新・密集・登録の入替・大きな床の系列（索引と総当たり）
//   --reference 索引を使わない総当たりの参照経路で計る（索引のあるビルドだけ）
//   --pilot     所要時間の見積り用に、繰り返しの固定更新を短くする
#include "KernelCosts.h"
#include "LegacyScenarios.h"
#include "Report.h"
#include "ScalingScenarios.h"
#include <stdio.h>
#include <string.h>
using namespace Dxf::Benchmark;
namespace
{
// 慣らしの固定更新の数（前回と同じ）。
constexpr Toolbox::int32 Warmup = 30;
// 1回の繰り返しの固定更新の数（前回と同じ）。
constexpr Toolbox::int32 Measured = 120;
// 見積り用の短い繰り返し。
constexpr Toolbox::int32 PilotMeasured = 12;

// 前回の18条件、または中止した重い条件を計る。
void RunLegacy_Internal(const Toolbox::int32* ObstacleCounts, Toolbox::int32 ObstacleKinds,
                        const Toolbox::int32* CharacterCounts, Toolbox::int32 CharacterKinds, bool bReference,
                        Toolbox::int32 Steps, bool bStaticLevel)
{
	PrintRunHeader(bStaticLevel ? "Character movement, previous 18-condition layout with a Static level body"
	                            : "Character movement, previous 18-condition layout (level body is Dynamic as before)");
	for (Toolbox::int32 Dimension = 0; Dimension < 2; ++Dimension)
	{
		for (Toolbox::int32 O = 0; O < ObstacleKinds; ++O)
		{
			for (Toolbox::int32 C = 0; C < CharacterKinds; ++C)
			{
				const FScenarioFactory Factory = bStaticLevel
				                                     ? (Dimension == 0 ? &MakeLegacyStatic2D : &MakeLegacyStatic3D)
				                                     : (Dimension == 0 ? &MakeLegacy2D : &MakeLegacy3D);
				const FRunResult Result =
				    RunScenario(Factory, ObstacleCounts[O], CharacterCounts[C], Warmup, Steps, bReference);
				PrintRunRow(Dimension == 0 ? "2D" : "3D", ObstacleCounts[O], CharacterCounts[C], bReference, Result);
			}
		}
	}
}
// 前回の配置で、問い合わせ種別ごとの費用を計る。
void RunCosts_Internal()
{
	PrintCostHeader();
	const Toolbox::int32 ObstacleCounts[] = {16, 128, 512};
	for (Toolbox::int32 Dimension = 0; Dimension < 2; ++Dimension)
	{
		for (const Toolbox::int32 Obstacles : ObstacleCounts)
		{
			const FScenarioFactory Factory = Dimension == 0 ? &MakeLegacyStatic2D : &MakeLegacyStatic3D;
			const FQueryCostTable Table = MeasureScenarioQueryCosts(Factory, Obstacles, 8, Warmup, 2000);
			PrintCostRows(Dimension == 0 ? "2D" : "3D", Obstacles, 8, Table);
		}
	}
}
} // namespace

int main(int Count, char** Args)
{
	if (Count < 2)
	{
		printf("usage: dxf_character_benchmark legacy|legacy-static|heavy|kernels|costs|scaling [--reference] "
		       "[--pilot]\n");
		return 2;
	}
	bool bReference = false;
	bool bPilot = false;
	for (Toolbox::int32 Index = 2; Index < Count; ++Index)
	{
		if (strcmp(Args[Index], "--reference") == 0)
		{
			bReference = true;
		}
		else if (strcmp(Args[Index], "--pilot") == 0)
		{
			bPilot = true;
		}
		else
		{
			printf("unknown option: %s\n", Args[Index]);
			return 2;
		}
	}
	const Toolbox::int32 Steps = bPilot ? PilotMeasured : Measured;
	PrintConditions(Warmup, Steps);
	if (strcmp(Args[1], "legacy") == 0)
	{
		const Toolbox::int32 ObstacleCounts[] = {16, 128, 512};
		const Toolbox::int32 CharacterCounts[] = {1, 8, 32};
		RunLegacy_Internal(ObstacleCounts, 3, CharacterCounts, 3, bReference, Steps, false);
		return 0;
	}
	if (strcmp(Args[1], "legacy-static") == 0)
	{
		const Toolbox::int32 ObstacleCounts[] = {16, 128, 512};
		const Toolbox::int32 CharacterCounts[] = {1, 8, 32};
		RunLegacy_Internal(ObstacleCounts, 3, CharacterCounts, 3, bReference, Steps, true);
		return 0;
	}
	if (strcmp(Args[1], "heavy") == 0)
	{
		const Toolbox::int32 ObstacleCounts[] = {1024};
		const Toolbox::int32 CharacterCounts[] = {64};
		RunLegacy_Internal(ObstacleCounts, 1, CharacterCounts, 1, bReference, Steps, true);
		return 0;
	}
	if (strcmp(Args[1], "kernels") == 0)
	{
		RunKernelCosts();
		return 0;
	}
	if (strcmp(Args[1], "costs") == 0)
	{
		RunCosts_Internal();
		return 0;
	}
	if (strcmp(Args[1], "scaling") == 0)
	{
		RunScalingSeries(Warmup, Steps);
		return 0;
	}
	printf("unknown series: %s\n", Args[1]);
	return 2;
}
