// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/Sha256.h"
#include "Toolbox/Array.h"
namespace Toolbox
{
namespace
{
// FIPS 180-4の32bit回転。シフト幅は1〜31だけを渡す。
uint32 Rotate(uint32 Value, uint32 Count) noexcept
{
	return (Value >> Count) | (Value << (32 - Count));
}
// 64バイトの一ブロックを、符号なし32bitの剰余演算で取り込む。
void Compress(TArray<uint32, 8>& State, const uint8* Bytes) noexcept
{
	// 規格で定められた64回の加算定数。
	constexpr uint32 Constants[] = {0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};
	// 展開するメッセージ語。確保は行わない。
	TArray<uint32, 64> Words{};
	for (uint32 I = 0; I < 16; ++I)
	{
		Words[I] = (static_cast<uint32>(Bytes[I * 4]) << 24) | (static_cast<uint32>(Bytes[I * 4 + 1]) << 16) | (static_cast<uint32>(Bytes[I * 4 + 2]) << 8) | Bytes[I * 4 + 3];
	}
	for (uint32 I = 16; I < 64; ++I)
	{
		// 短い回転・シフトで求める二つの展開項。
		const uint32 First = Rotate(Words[I - 15], 7) ^ Rotate(Words[I - 15], 18) ^ (Words[I - 15] >> 3);
		const uint32 Second = Rotate(Words[I - 2], 17) ^ Rotate(Words[I - 2], 19) ^ (Words[I - 2] >> 10);
		Words[I] = Words[I - 16] + First + Words[I - 7] + Second;
	}
	// このブロック内の8語。開始状態は後で加算するため保持する。
	TArray<uint32, 8> Work = State;
	for (uint32 I = 0; I < 64; ++I)
	{
		// 選択関数と多数決関数をそれぞれの回転項へ足す。
		const uint32 First = Work[7] + (Rotate(Work[4], 6) ^ Rotate(Work[4], 11) ^ Rotate(Work[4], 25)) + ((Work[4] & Work[5]) ^ (~Work[4] & Work[6])) + Constants[I] + Words[I];
		const uint32 Second = (Rotate(Work[0], 2) ^ Rotate(Work[0], 13) ^ Rotate(Work[0], 22)) + ((Work[0] & Work[1]) ^ (Work[0] & Work[2]) ^ (Work[1] & Work[2]));
		for (uint32 J = 7; J > 0; --J)
		{
			Work[J] = Work[J - 1];
		}
		Work[4] += First;
		Work[0] = First + Second;
	}
	for (uint32 I = 0; I < 8; ++I)
	{
		State[I] += Work[I];
	}
}
} // namespace
FString FSha256::Compute(FStringView Bytes)
{
	if (Bytes.Size() > TNumericLimits<uint64>::Max() / 8)
	{
		throw FException("SHA-256 input length overflow");
	}
	// 規格が定める初期8語。
	TArray<uint32, 8> State{{0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU, 0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U}};
	// 完了した入力ブロックのバイト位置。
	size_t Offset = 0;
	while (Bytes.Size() - Offset >= 64)
	{
		Compress(State, reinterpret_cast<const uint8*>(Bytes.Data()) + Offset);
		Offset += 64;
	}
	// 最終ブロック。0x80と64bit長さで余白を埋める。
	TArray<uint8, 64> Tail{};
	const size_t Remainder = Bytes.Size() - Offset;
	for (size_t I = 0; I < Remainder; ++I)
	{
		Tail[I] = static_cast<uint8>(Bytes[Offset + I]);
	}
	Tail[Remainder] = 0x80;
	if (Remainder >= 56)
	{
		Compress(State, Tail.Data());
		Tail = {};
	}
	// 型やpaddingではなく、規格のbig-endian長さだけを書き込む。
	const uint64 Bits = static_cast<uint64>(Bytes.Size()) * 8;
	for (uint32 I = 0; I < 8; ++I)
	{
		Tail[63 - I] = static_cast<uint8>(Bits >> (I * 8));
	}
	Compress(State, Tail.Data());
	// 指紋の安定した小文字表現。
	constexpr char Digits[] = "0123456789abcdef";
	FString Result;
	Result.Resize(64);
	for (uint32 I = 0; I < 8; ++I)
	{
		for (uint32 J = 0; J < 8; ++J)
		{
			Result[I * 8 + J] = Digits[(State[I] >> (28 - J * 4)) & 15];
		}
	}
	return Result;
}
} // namespace Toolbox
