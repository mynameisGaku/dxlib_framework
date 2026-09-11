#pragma once
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
namespace Test
{
struct FCase
{
	const char* Name;
	void (*Run)();
};
inline std::vector<FCase>& Cases_Internal()
{
	static std::vector<FCase> Cases;
	return Cases;
}
class FRegistration
{
public:
	FRegistration(const char* Name, void (*Run)())
	{
		Cases_Internal().push_back({Name, Run});
	}
};
inline void Require_Internal(bool bValue, const char* Text, int Line)
{
	if (!bValue)
	{
		throw std::runtime_error(std::string(Text) + " at line " + std::to_string(Line));
	}
}
}
#define DXF_JOIN_IMPL(A, B) A##B
#define DXF_JOIN(A, B) DXF_JOIN_IMPL(A, B)
#define TEST(Name) \
    static void DXF_JOIN(Test_Internal_, __LINE__)(); \
    static Test::FRegistration DXF_JOIN(Registration_, __LINE__)(Name, &DXF_JOIN(Test_Internal_, __LINE__)); \
    static void DXF_JOIN(Test_Internal_, __LINE__)()
#define REQUIRE(...) Test::Require_Internal(static_cast<bool>((__VA_ARGS__)), #__VA_ARGS__, __LINE__)
