#pragma once
/**
 * ネイティブ実装では利用側のUNICODE設定に依存せずUTF-8の文字列を使用する。
 */
#ifndef DX_NON_USING_NAMESPACE_DXLIB
#define DX_NON_USING_NAMESPACE_DXLIB
#endif
#include "DxLib.h"
/**
 * Windowsの文字種選択マクロが、この接続部の同名メンバー関数を書き換えるのを防ぐ。
 */
#ifdef CreateFont
#undef CreateFont
#endif
#ifdef DrawText
#undef DrawText
#endif
#include "Dxf/Result.h"
namespace Dxf::Detail
{
/**
 * DxLibの戻り値をフレームワークの結果へ変換する。
 * @param Code DxLibが返した処理結果の整数。
 * @param Operation 失敗時に返す処理内容の説明。
 */
inline TResult<void> CheckNative_Internal(Toolbox::int32 Code, const char* Operation)
{
	return Code < 0 ? TResult<void>::Failure(EErrorCode::BackendFailure, Operation) : TResult<void>{};
}
} // namespace Dxf::Detail
