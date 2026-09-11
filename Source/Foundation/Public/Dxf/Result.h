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
		return TResult(std::in_place_index<0>, std::move(Value));
	}
	static TResult Failure(EErrorCode Code, std::string Message)
	{
		return TResult(std::in_place_index<1>, FError{Code, std::move(Message)});
	}
	static TResult Failure(FError Error)
	{
		return TResult(std::in_place_index<1>, std::move(Error));
	}
	explicit operator bool() const noexcept
	{
		return Data.index() == 0;
	}
	T& Value() &
	{
		return std::get<0>(Data);
	}
	const T& Value() const&
	{
		return std::get<0>(Data);
	}
	T&& Value() &&
	{
		return std::get<0>(std::move(Data));
	}
	const FError& Error() const
	{
		return std::get<1>(Data);
	}
private:
	explicit TResult(std::in_place_index_t<0>, T Value) : Data(std::in_place_index<0>, std::move(Value))
	{
	}
	explicit TResult(std::in_place_index_t<1>, FError Error) : Data(std::in_place_index<1>, std::move(Error))
	{
	}
	std::variant<T, FError> Data;
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
		Result.Data = std::move(Error);
		return Result;
	}
	explicit operator bool() const noexcept
	{
		return std::holds_alternative<std::monostate>(Data);
	}
	const FError& Error() const
	{
		return std::get<FError>(Data);
	}
private:
	std::variant<std::monostate, FError> Data;
};
}
