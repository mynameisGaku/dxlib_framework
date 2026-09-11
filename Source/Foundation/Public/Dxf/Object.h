#pragma once
#include <type_traits>
namespace Dxf
{
/** Lightweight RTTI root. It does not own or globally register derived instances. */
class DObject
{
public:
	virtual ~DObject() = default;
	template <typename T> bool IsA() const noexcept
	{
		return TryCast<T>() != nullptr;
	}
	template <typename T> T* TryCast() noexcept
	{
		static_assert(std::is_base_of_v<DObject, T>);
		return dynamic_cast<T*>(this);
	}
	template <typename T> const T* TryCast() const noexcept
	{
		static_assert(std::is_base_of_v<DObject, T>);
		return dynamic_cast<const T*>(this);
	}
	virtual bool IsHandleAccessible_Internal() const noexcept
	{
		return true;
	}
protected:
	DObject() = default;
	DObject(const DObject&) = delete;
	DObject& operator=(const DObject&) = delete;
};
}
