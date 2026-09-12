#pragma once
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * スコープ終了時に値を元の状態へ戻す。
 */
template <typename T> class TGuardValue
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Value 処理対象の値。
	 * @param Temporary スコープ内で一時的に設定する値。
	 */
	TGuardValue(T& Value, T Temporary)
	    : m_pValue(&Value), m_Previous(Toolbox::Exchange(Value, Toolbox::Move(Temporary)))
	{
	}
	/**
	 * 変更した値を構築前の状態へ戻す。
	 */
	~TGuardValue() noexcept
	{
		*m_pValue = Toolbox::Move(m_Previous);
	}
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	TGuardValue(const TGuardValue&) = delete;
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	TGuardValue& operator=(const TGuardValue&) = delete;

private:
	/**
	 * 処理対象の値。
	 */
	T* m_pValue;
	/**
	 * 前回の状態。
	 */
	T m_Previous;
};
} // namespace Dxf
