// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_SHA256_H
#define TOOLBOX_SHA256_H
#include "Toolbox/String.h"
namespace Toolbox
{
/**
 * バイト列をSHA-256の指紋へ変換する。署名や秘密鍵による認証ではない。
 */
class FSha256
{
public:
	/**
	 * @param Bytes 長さ付きの任意バイト列。結果は小文字の十六進64文字。長さoverflowは例外。
	 */
	static FString Compute(FStringView Bytes);
};
} // namespace Toolbox
#endif
