#pragma once
#include "Toolbox/String.h"
#include "Toolbox/Utility.h"
#include "Toolbox/Variant.h"
namespace Dxf
{
/**
 * 処理に失敗した原因を分類する。
 */
enum class EErrorCode
{
	/**
	 * 引数が不正。
	 */
	InvalidArgument,
	/**
	 * 現在の状態では実行できない。
	 */
	InvalidState,
	/**
	 * バックエンド処理に失敗。
	 */
	BackendFailure,
	/**
	 * 対象が見つからない。
	 */
	NotFound,
	/**
	 * 初期化に失敗。
	 */
	InitializationFailed,
	/**
	 * 利用者の処理が例外を送出。
	 */
	UserException
};
/**
 * エラーの分類と説明を保持する。
 */
struct FError
{
	/**
	 * エラーの分類。
	 */
	EErrorCode Code = EErrorCode::BackendFailure;
	/**
	 * エラーの説明。
	 */
	Toolbox::FString Message;
};
/**
 * 成功値またはエラーを保持する。異なる状態の値へアクセスするとToolbox::FBadAccessを送出する。
 */
template <typename T> class [[nodiscard]] TResult
{
public:
	/**
	 * 成功を表す処理結果を生成する。
	 * @param Value 処理対象の値。
	 */
	static TResult Success(T Value)
	{
		return TResult(Toolbox::InPlaceIndex<0>, Toolbox::Move(Value));
	}
	/**
	 * エラーを表す処理結果を生成する。
	 * @param Code エラーの分類。
	 * @param Message エラーの説明。
	 */
	static TResult Failure(EErrorCode Code, Toolbox::FString Message)
	{
		return TResult(Toolbox::InPlaceIndex<1>, FError{Code, Toolbox::Move(Message)});
	}
	/**
	 * エラーを表す処理結果を生成する。
	 * @param Error エラー情報。
	 */
	static TResult Failure(FError Error)
	{
		return TResult(Toolbox::InPlaceIndex<1>, Toolbox::Move(Error));
	}
	/**
	 * 処理または参照が有効かを返す。
	 */
	explicit operator bool() const noexcept
	{
		return Data.Index() == 0;
	}
	/**
	 * 成功時に格納された値を取得する。
	 */
	FORCEINLINE T& Value() &
	{
		return Toolbox::Get<0>(Data);
	}
	/**
	 * 成功時に格納された値を取得する。
	 */
	FORCEINLINE const T& Value() const&
	{
		return Toolbox::Get<0>(Data);
	}
	/**
	 * 成功時に格納された値を取得する。
	 */
	FORCEINLINE T&& Value() &&
	{
		return Toolbox::Get<0>(Toolbox::Move(Data));
	}
	/**
	 * 失敗時に格納されたエラーを取得する。
	 */
	FORCEINLINE const FError& Error() const
	{
		return Toolbox::Get<1>(Data);
	}

private:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	explicit TResult(Toolbox::TInPlaceIndex<0>, T Value) : Data(Toolbox::InPlaceIndex<0>, Toolbox::Move(Value))
	{
	}
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	explicit TResult(Toolbox::TInPlaceIndex<1>, FError Error) : Data(Toolbox::InPlaceIndex<1>, Toolbox::Move(Error))
	{
	}
	/**
	 * 保持するデータ。
	 */
	Toolbox::TVariant<T, FError> Data;
};
/**
 * 成功値またはエラーを保持する。
 */
template <> class [[nodiscard]] TResult<void>
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	TResult() = default;
	/**
	 * 成功を表す処理結果を生成する。
	 */
	FORCEINLINE static TResult Success()
	{
		return {};
	}
	/**
	 * エラーを表す処理結果を生成する。
	 * @param Code エラーの分類。
	 * @param Message エラーの説明。
	 */
	static TResult Failure(EErrorCode Code, Toolbox::FString Message)
	{
		return Failure(FError{Code, Toolbox::Move(Message)});
	}
	/**
	 * エラーを表す処理結果を生成する。
	 * @param Error エラー情報。
	 */
	static TResult Failure(FError Error)
	{
		// 処理結果。
		TResult Result;
		Result.Data = Toolbox::Move(Error);
		return Result;
	}
	/**
	 * 処理または参照が有効かを返す。
	 */
	explicit operator bool() const noexcept
	{
		return Toolbox::HoldsAlternative<Toolbox::FEmpty>(Data);
	}
	/**
	 * 失敗時に格納されたエラーを取得する。
	 */
	FORCEINLINE const FError& Error() const
	{
		return Toolbox::Get<FError>(Data);
	}

private:
	/**
	 * 保持するデータ。
	 */
	Toolbox::TVariant<Toolbox::FEmpty, FError> Data;
};
} // namespace Dxf
