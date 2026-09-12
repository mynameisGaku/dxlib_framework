// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_COMPILER_H
#define TOOLBOX_COMPILER_H

/**
 * 小さな関数を呼び出し元へ展開するようコンパイラへ指定する。
 * 既存のプラットフォーム定義がある場合はそれを使用する。展開の可否はコンパイラが決める。
 */
#ifndef FORCEINLINE
#if defined(_MSC_VER)
#define FORCEINLINE __forceinline
#elif defined(__clang__) || defined(__GNUC__)
#define FORCEINLINE inline __attribute__((always_inline))
#else
#define FORCEINLINE inline
#endif
#endif

#endif
