// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "SaveCourseProgressCodec.h"
#include "Dxf/SaveError.h"
using namespace Dxf;
using namespace Dxf::GameplaySample;
using namespace Toolbox;
namespace
{
// Writerを通さない、独立して手書きした正常な進行文書。
const char* Fixture = "{\"spawnCounter\":\"3\",\"score\":\"18446744073709551615\",\"revision\":\"9007199254740993\",\"payloadVersion\":1,\"doors\":{\"doorB\":{\"unlocked\":false,\"open\":false},\"doorA\":{\"unlocked\":true,\"open\":true}},\"collectedKeys\":[\"northKey\"],\"checkpoint\":\"north\"}";
// 同じ意味を持つ安定した辞書順の期待値。
const char* Canonical = "{\"checkpoint\":\"north\",\"collectedKeys\":[\"northKey\"],\"doors\":{\"doorA\":{\"open\":true,\"unlocked\":true},\"doorB\":{\"open\":false,\"unlocked\":false}},\"payloadVersion\":1,\"revision\":\"9007199254740993\",\"score\":\"18446744073709551615\",\"spawnCounter\":\"3\"}";
// 不正な手書き値を正常fixtureの一箇所へ入れ、Writerによる事前補正を避ける。
FString Replace(FStringView Text, FStringView From, FStringView To)
{
	FString Result;
	bool Found = false;
	for (size_t I = 0; I < Text.Size(); ++I)
	{
		if (!Found && I + From.Size() <= Text.Size() && FStringView(Text.Data() + I, From.Size()) == From)
		{
			Result += FString(To.Data(), To.Size());
			I += From.Size() - 1;
			Found = true;
		}
		else
		{
			Result += Text[I];
		}
	}
	REQUIRE(Found);
	return Result;
}
// エラー種別と候補を採用しない境界を確認する。
template <typename TAction>
void Reject(TAction&& Action, ESaveError Code = ESaveError::InvalidDocument)
{
	bool Rejected = false;
	try
	{
		Action();
	}
	catch (const FSaveError& Error)
	{
		REQUIRE(Error.GetCode() == Code);
		Rejected = true;
	}
	REQUIRE(Rejected);
}
} // namespace
TEST("Save course progress decodes logical door names and exact large counters without floating point")
{
	// 項目と扉の並びが逆でも、doorAだけの解錠・開扉を維持する。
	const auto Progress = FSaveCourseProgressCodec::Decode(Fixture);
	REQUIRE(Progress.Checkpoint == "north" && Progress.CollectedKeys.Size() == 1);
	REQUIRE(Progress.CollectedKeys[0] == "northKey");
	REQUIRE(Progress.bDoorUnlocked[0] && Progress.bDoorOpen[0]);
	REQUIRE(!Progress.bDoorUnlocked[1] && !Progress.bDoorOpen[1]);
	REQUIRE(Progress.Score == TNumericLimits<uint64>::Max());
	REQUIRE(Progress.Revision == 9007199254740993ULL && Progress.SpawnCounter == 3);
	REQUIRE(FSaveCourseProgressCodec::Encode(Progress) == Canonical);
}
TEST("Save course progress canonicalizes key sets without modifying shared game values")
{
	FSaveCourseProgress Progress;
	Progress.CollectedKeys.PushBack("southKey");
	Progress.CollectedKeys.PushBack("northKey");
	Progress.SpawnCounter = TNumericLimits<uint64>::Max();
	// 二つのcodec呼出しもゲーム更新・音声要求・通し番号変更を行わない。
	const auto Bytes = FSaveCourseProgressCodec::Encode(Progress);
	REQUIRE(Bytes == FSaveCourseProgressCodec::Encode(Progress));
	REQUIRE(Progress.CollectedKeys[0] == "southKey" && Progress.Revision == 0);
	const auto Decoded = FSaveCourseProgressCodec::Decode(Bytes);
	REQUIRE(Decoded.CollectedKeys[0] == "northKey" && Decoded.CollectedKeys[1] == "southKey");
	REQUIRE(Decoded.Checkpoint == "start" && Decoded.SpawnCounter == TNumericLimits<uint64>::Max());
	Progress.bDoorOpen[0] = true;
	Reject([&]()
	       {
		       FSaveCourseProgressCodec::Encode(Progress);
	       });
}
TEST("Save course progress rejects unknown versions fields keys and damaged exact counters")
{
	Reject([&]()
	       {
		       FSaveCourseProgressCodec::Decode(Replace(Fixture, "\"payloadVersion\":1", "\"payloadVersion\":2"));
	       },
	       ESaveError::UnsupportedVersion);
	const char* Before[] = {"\"payloadVersion\":1", "\"checkpoint\":\"north\"", "\"collectedKeys\":[\"northKey\"]", "\"collectedKeys\":[\"northKey\"]", "\"revision\":\"9007199254740993\"", "\"score\":\"18446744073709551615\"", "\"spawnCounter\":\"3\"", "\"doorB\":{\"unlocked\":false,\"open\":false}"};
	const char* After[] = {"\"payloadVersion\":1.0", "\"checkpoint\":\"missing\"", "\"collectedKeys\":[\"missing\"]", "\"collectedKeys\":[\"northKey\",\"northKey\"]", "\"revision\":\"09007199254740993\"", "\"score\":\"18446744073709551616\"", "\"spawnCounter\":\"18446744073709551616\"", "\"doorB\":{\"unlocked\":false,\"open\":true}"};
	for (size_t I = 0; I < 8; ++I)
	{
		Reject([&]()
		       {
			       FSaveCourseProgressCodec::Decode(Replace(Fixture, Before[I], After[I]));
		       });
	}
	Reject([&]()
	       {
		       FSaveCourseProgressCodec::Decode(Replace(Fixture, "\"payloadVersion\":1", "\"payloadVersion\":1,\"future\":0"));
	       });
	Reject([&]()
	       {
		       FSaveCourseProgressCodec::Decode(Replace(Fixture, "\"checkpoint\":\"north\"", "\"checkpoint\":\"north\",\"checkpoint\":\"start\""));
	       });
	Reject([&]()
	       {
		       FSaveCourseProgressCodec::Decode(FString(Fixture) + "{}");
	       });
}
TEST("Save course progress preserves caller state when a loaded candidate is rejected")
{
	auto Current = FSaveCourseProgressCodec::Decode(Fixture);
	// 候補の解析・全検証が成功するまで、生存中ゲーム値への代入は起きない。
	Reject([&]()
	       {
		       Current = FSaveCourseProgressCodec::Decode(Replace(Fixture, "\"score\":\"18446744073709551615\"", "\"score\":-1"));
	       });
	REQUIRE(Current.Score == TNumericLimits<uint64>::Max());
	REQUIRE(Current.Checkpoint == "north" && Current.bDoorUnlocked[0]);
}
