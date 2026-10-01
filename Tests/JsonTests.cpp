// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Toolbox/JsonDocument.h"
namespace
{
// 失敗が例外として観測されることを確認する。
bool Reject(Toolbox::FStringView Input, Toolbox::FJsonLimits Limits = {})
{
	try
	{
		Toolbox::FJsonDocument::Parse(Input, Limits);
	}
	catch (const Toolbox::FException&)
	{
		return true;
	}
	return false;
}
} // namespace
TEST("JSON values preserve order and decoded Unicode without STL")
{
	const auto Document =
	    Toolbox::FJsonDocument::Parse(" {\"日本語\":[true,false,null,-2.5e+2,\"\\uD83D\\uDE00\",\"a\\u0000b\"]} ");
	const auto Array = Document.Find(0, "日本語");
	REQUIRE(Array >= 0);
	auto Index = Document.Get(Array).FirstChild;
	REQUIRE(Document.Get(Index).Boolean);
	Index = Document.Get(Index).NextSibling;
	REQUIRE(!Document.Get(Index).Boolean);
	Index = Document.Get(Index).NextSibling;
	REQUIRE(Document.Get(Index).Kind == Toolbox::EJsonKind::Null);
	Index = Document.Get(Index).NextSibling;
	REQUIRE(Document.Get(Index).Number == -250);
	Index = Document.Get(Index).NextSibling;
	REQUIRE(Document.Get(Index).Text.Size() == 4);
	Index = Document.Get(Index).NextSibling;
	REQUIRE(Document.Get(Index).Text.Size() == 3);
	REQUIRE(Document.Get(Index).Text.Data()[1] == 0);
}
TEST("JSON rejects syntax Unicode duplicate decoded keys and overflow")
{
	const char* Invalid[] = {"",
	                         "{}{}",
	                         "/*x*/{}",
	                         "[1,]",
	                         "{\"a\":1,}",
	                         "{\"a\":1,\"\\u0061\":2}",
	                         "01",
	                         "+1",
	                         "1.",
	                         "1e",
	                         "1e400",
	                         "NaN",
	                         "\"\\x00\"",
	                         "\"\\uD800\"",
	                         "\"\\uDC00\"",
	                         "\"\\uD800\\u0041\"",
	                         "\"unclosed",
	                         "\"\n\"",
	                         "\"\xc0\xaf\"",
	                         "\"\xed\xa0\x80\"",
	                         "\"\xf4\x90\x80\x80\"",
	                         "\"\x80\""};
	for (const char* Input : Invalid)
	{
		REQUIRE(Reject(Input));
	}
	REQUIRE(Toolbox::FJsonDocument::Parse("\xef\xbb\xbf{}").Size() == 1);
	REQUIRE(Reject("\xef\xbb\xbf\xef\xbb\xbf{}"));
	const char Nul[] = {'[', '0', ']', 0, ' '};
	REQUIRE(Reject({Nul, sizeof(Nul)}));
}
TEST("JSON enforces exact finite byte string node and depth limits")
{
	Toolbox::FJsonLimits Limits;
	Limits.MaxBytes = 2;
	REQUIRE(Toolbox::FJsonDocument::Parse("{}", Limits).Size() == 1);
	REQUIRE(Reject("{} ", Limits));
	Limits.MaxBytes = 100;
	Limits.MaxStringBytes = 4;
	REQUIRE(Toolbox::FJsonDocument::Parse("\"\\uD83D\\uDE00\"", Limits).Get(0).Text.Size() == 4);
	REQUIRE(Reject("\"12345\"", Limits));
	Limits.MaxDepth = 2;
	REQUIRE(Toolbox::FJsonDocument::Parse("[[]]", Limits).Size() == 2);
	REQUIRE(Reject("[[[]]]", Limits));
	Limits.MaxValues = 2;
	REQUIRE(Reject("[0,1]", Limits));
}
