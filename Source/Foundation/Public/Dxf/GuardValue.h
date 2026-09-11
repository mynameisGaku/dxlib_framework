#pragma once
#include <utility>
namespace Dxf
{
template <typename T>
class TGuardValue
{
public:
	TGuardValue(T& Value, T Temporary) : m_pValue(&Value), m_Previous(std::exchange(Value, std::move(Temporary)))
	{
	}
	~TGuardValue() noexcept
	{
		*m_pValue = std::move(m_Previous);
	}
	TGuardValue(const TGuardValue&) = delete;
	TGuardValue& operator=(const TGuardValue&) = delete;
private:
	T* m_pValue;
	T m_Previous;
};
}
