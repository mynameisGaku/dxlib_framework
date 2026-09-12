#pragma once
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * 型確認を提供する軽量な基底型。派生インスタンスの所有や全体登録は行わない。
 */
class DObject
{
public:
	/**
	 * 派生型を含むオブジェクトを安全に終了する。
	 */
	virtual ~DObject() = default;
	/**
	 * 指定した型として扱えるかを調べる。
	 */
	template <typename T> bool IsA() const noexcept
	{
		return TryCast<T>() != nullptr;
	}
	/**
	 * 指定した型へ安全に変換する。
	 */
	template <typename T> T* TryCast() noexcept
	{
		static_assert(Toolbox::IsBaseOf<DObject, T>);
		return dynamic_cast<T*>(this);
	}
	/**
	 * 指定した型へ安全に変換する。
	 */
	template <typename T> const T* TryCast() const noexcept
	{
		static_assert(Toolbox::IsBaseOf<DObject, T>);
		return dynamic_cast<const T*>(this);
	}
	/**
	 * ハンドルを通して参照できる状態かを調べる。
	 */
	virtual bool IsHandleAccessible_Internal() const noexcept
	{
		return true;
	}

protected:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	DObject() = default;
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	DObject(const DObject&) = delete;
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	DObject& operator=(const DObject&) = delete;
};
} // namespace Dxf
