#pragma once
#include <string>
#include <utility>
#include <variant>
namespace Dxf
{
enum class EErrorCode
{
	InvalidArgument,
	InvalidState,
	BackendFailure,
	NotFound,
	InitializationFailed,
	UserException
};
struct FError
{
	EErrorCode Code = EErrorCode::BackendFailure;
	std::string Message;
};
/** A value or an error. Accessing the wrong alternative throws std::bad_variant_access. */
template <typename T>
class [[nodiscard]] TResult
{
public:
	static TResult Success(T Value)
	{
		return TResult(std::move(Value));
	}
	static TResult Failure(EErrorCode Code, std::string Message)
	{
		return TResult(FError{Code, std::move(Message)});
	}
	static TResult Failure(FError Error)
	{
		return TResult(std::move(Error));
	}
	explicit operator bool() const noexcept
	{
		return std::holds_alternative<T>(m_Data);
	}
	T& Value() &
	{
		return std::get<T>(m_Data);
	}
	const T& Value() const&
	{
		return std::get<T>(m_Data);
	}
	T&& Value() &&
	{
		return std::get<T>(std::move(m_Data));
	}
	const FError& Error() const
	{
		return std::get<FError>(m_Data);
	}
private:
	explicit TResult(T Value) : m_Data(std::in_place_index<0>, std::move(Value))
	{
	}
	explicit TResult(FError Error) : m_Data(std::in_place_index<1>, std::move(Error))
	{
	}
	std::variant<T, FError> m_Data;
};
template <>
class [[nodiscard]] TResult<void>
{
public:
	TResult() = default;
	static TResult Success()
	{
		return {};
	}
	static TResult Failure(EErrorCode Code, std::string Message)
	{
		return Failure(FError{Code, std::move(Message)});
	}
	static TResult Failure(FError Error)
	{
		TResult Result;
		Result.m_Data = std::move(Error);
		return Result;
	}
	explicit operator bool() const noexcept
	{
		return std::holds_alternative<std::monostate>(m_Data);
	}
	const FError& Error() const
	{
		return std::get<FError>(m_Data);
	}
private:
	std::variant<std::monostate, FError> m_Data;
};
}
