// SPDX-License-Identifier: NOASSERTION
#include "Dxf/AssetDecode.h"
namespace Dxf
{
namespace
{
// 画像・音声の寸法上限。積のoverflow検査と併用する。
constexpr Toolbox::uint64 MaxBitmapExtent = 16384;
constexpr Toolbox::uint32 MaxWaveSampleRate = 192000;
// 範囲を確認して2バイト符号なし整数を読む。
// @param Data 読み取るバイト列。
// @param Size バイト列の長さ。
// @param Offset 読み取る位置。
// @param Value 読み取った値の格納先。
bool ReadU16_Internal(const Toolbox::uint8* Data, Toolbox::size_t Size, Toolbox::size_t Offset, Toolbox::uint16& Value)
{
	if (Offset + 2 > Size)
	{
		return false;
	}
	Value = static_cast<Toolbox::uint16>(Data[Offset] | (static_cast<Toolbox::uint16>(Data[Offset + 1]) << 8));
	return true;
}
// 範囲を確認して4バイト符号なし整数を読む。
// @param Data 読み取るバイト列。
// @param Size バイト列の長さ。
// @param Offset 読み取る位置。
// @param Value 読み取った値の格納先。
bool ReadU32_Internal(const Toolbox::uint8* Data, Toolbox::size_t Size, Toolbox::size_t Offset, Toolbox::uint32& Value)
{
	if (Offset + 4 > Size)
	{
		return false;
	}
	Value = static_cast<Toolbox::uint32>(Data[Offset]) | (static_cast<Toolbox::uint32>(Data[Offset + 1]) << 8) |
	        (static_cast<Toolbox::uint32>(Data[Offset + 2]) << 16) | (static_cast<Toolbox::uint32>(Data[Offset + 3]) << 24);
	return true;
}
// 範囲を確認して4バイト符号付き整数を読む。
// @param Data 読み取るバイト列。
// @param Size バイト列の長さ。
// @param Offset 読み取る位置。
// @param Value 読み取った値の格納先。
bool ReadI32_Internal(const Toolbox::uint8* Data, Toolbox::size_t Size, Toolbox::size_t Offset, Toolbox::int32& Value)
{
	Toolbox::uint32 Raw = 0;
	if (!ReadU32_Internal(Data, Size, Offset, Raw))
	{
		return false;
	}
	Value = static_cast<Toolbox::int32>(Raw);
	return true;
}
// 無圧縮BMPを検証する。画素列はNative取込が原本から読む。
// @param Data ファイルのバイト列。
// @param Size バイト列の長さ。
TResult<FDecodedAsset> DecodeBitmap_Internal(const Toolbox::uint8* Data, Toolbox::size_t Size)
{
	if (Size < 54 || Data[0] != 'B' || Data[1] != 'M')
	{
		return TResult<FDecodedAsset>::Failure(EErrorCode::InvalidArgument, "Not a bitmap file");
	}
	// ファイル先頭から画素列までのバイト数。
	Toolbox::uint32 OffBits = 0;
	// 情報部のバイト数。
	Toolbox::uint32 InfoSize = 0;
	// 画像の幅と高さ。負の高さは上から下への格納。
	Toolbox::int32 Width = 0;
	Toolbox::int32 Height = 0;
	// 一画素あたりのビット数。
	Toolbox::uint16 Bits = 0;
	// 圧縮方式。無圧縮のみ対応する。
	Toolbox::uint32 Compression = 0;
	Toolbox::uint16 Planes = 0;
	if (!ReadU32_Internal(Data, Size, 10, OffBits) || !ReadU32_Internal(Data, Size, 14, InfoSize) ||
	    !ReadI32_Internal(Data, Size, 18, Width) || !ReadI32_Internal(Data, Size, 22, Height) ||
	    !ReadU16_Internal(Data, Size, 26, Planes) || !ReadU16_Internal(Data, Size, 28, Bits) ||
	    !ReadU32_Internal(Data, Size, 30, Compression))
	{
		return TResult<FDecodedAsset>::Failure(EErrorCode::InvalidArgument, "Truncated bitmap header");
	}
	if (InfoSize != 40 || Planes != 1 || (Bits != 24 && Bits != 32) || Compression != 0)
	{
		return TResult<FDecodedAsset>::Failure(EErrorCode::InvalidArgument, "Unsupported bitmap format");
	}
	// 高さの絶対値。64bitへ広げてから反転するため最小値も安全に扱える。
	Toolbox::uint64 AbsHeight =
	    Height < 0 ? static_cast<Toolbox::uint64>(-(Toolbox::int64)Height) : static_cast<Toolbox::uint64>(Height);
	if (Width <= 0 || Height == 0 || static_cast<Toolbox::uint64>(Width) > MaxBitmapExtent || AbsHeight > MaxBitmapExtent)
	{
		return TResult<FDecodedAsset>::Failure(EErrorCode::InvalidArgument, "Invalid bitmap dimensions");
	}
	// 一行あたりのバイト数。4バイト境界へ切り上げる。
	const Toolbox::uint64 Stride =
	    (static_cast<Toolbox::uint64>(Width) * Bits + 31) / 32 * 4;
	// 画素列全体のバイト数。
	const Toolbox::uint64 Pixels = Stride * AbsHeight;
	if (Pixels == 0 || static_cast<Toolbox::uint64>(OffBits) + Pixels > Size)
	{
		return TResult<FDecodedAsset>::Failure(EErrorCode::InvalidArgument, "Bitmap pixels exceed file contents");
	}
	// 検証済みの内容。
	FDecodedAsset Decoded;
	Decoded.Kind = EAssetDecodeKind::Bitmap;
	Decoded.Width = Width;
	Decoded.Height = static_cast<Toolbox::int32>(AbsHeight);
	return TResult<FDecodedAsset>::Success(Toolbox::Move(Decoded));
}
// PCM16 WAVEを検証する。標本列はNative取込が原本から読む。
// @param Data ファイルのバイト列。
// @param Size バイト列の長さ。
TResult<FDecodedAsset> DecodeWave_Internal(const Toolbox::uint8* Data, Toolbox::size_t Size)
{
	if (Size < 12 || Data[0] != 'R' || Data[1] != 'I' || Data[2] != 'F' || Data[3] != 'F' || Data[8] != 'W' ||
	    Data[9] != 'A' || Data[10] != 'V' || Data[11] != 'E')
	{
		return TResult<FDecodedAsset>::Failure(EErrorCode::InvalidArgument, "Not a wave file");
	}
	// 形式区画を見つけたか。
	bool bFormat = false;
	// 形式区画の音声形式。
	Toolbox::uint16 Format = 0;
	// 形式区画のチャンネル数・標本化周波数・ビット数。
	Toolbox::uint16 Channels = 0;
	Toolbox::uint32 SampleRate = 0;
	Toolbox::uint16 Bits = 0;
	// 形式区画の1標本あたりバイト数と1秒あたりバイト数。
	Toolbox::uint16 BlockAlign = 0;
	Toolbox::uint32 ByteRate = 0;
	// 音声区画の開始位置と長さ。
	Toolbox::size_t DataOffset = 0;
	Toolbox::uint64 DataSize = 0;
	// 区画の走査位置。
	Toolbox::size_t Offset = 12;
	while (Offset + 8 <= Size)
	{
		// 区画の長さ。
		Toolbox::uint32 ChunkSize = 0;
		if (!ReadU32_Internal(Data, Size, Offset + 4, ChunkSize))
		{
			break;
		}
		// 区画本体の終了位置。奇数長の詰め物を越える。
		const Toolbox::uint64 ChunkEnd =
		    static_cast<Toolbox::uint64>(Offset) + 8 + ChunkSize + (ChunkSize % 2);
		if (ChunkEnd > Size)
		{
			break;
		}
		if (Data[Offset] == 'f' && Data[Offset + 1] == 'm' && Data[Offset + 2] == 't' && Data[Offset + 3] == ' ')
		{
			if (ChunkSize < 16 || !ReadU16_Internal(Data, Size, Offset + 8, Format) ||
			    !ReadU16_Internal(Data, Size, Offset + 10, Channels) ||
			    !ReadU32_Internal(Data, Size, Offset + 12, SampleRate) ||
			    !ReadU32_Internal(Data, Size, Offset + 16, ByteRate) ||
			    !ReadU16_Internal(Data, Size, Offset + 20, BlockAlign) ||
			    !ReadU16_Internal(Data, Size, Offset + 22, Bits))
			{
				return TResult<FDecodedAsset>::Failure(EErrorCode::InvalidArgument, "Truncated wave format");
			}
			bFormat = true;
		}
		else if (Data[Offset] == 'd' && Data[Offset + 1] == 'a' && Data[Offset + 2] == 't' && Data[Offset + 3] == 'a')
		{
			DataOffset = Offset + 8;
			DataSize = ChunkSize;
		}
		Offset = static_cast<Toolbox::size_t>(ChunkEnd);
	}
	if (!bFormat)
	{
		return TResult<FDecodedAsset>::Failure(EErrorCode::InvalidArgument, "Wave format chunk is missing");
	}
	if (DataSize == 0)
	{
		return TResult<FDecodedAsset>::Failure(EErrorCode::InvalidArgument, "Wave data chunk is missing");
	}
	if (Format != 1 || (Channels != 1 && Channels != 2) || SampleRate < 8000 || SampleRate > MaxWaveSampleRate ||
	    Bits != 16)
	{
		return TResult<FDecodedAsset>::Failure(EErrorCode::InvalidArgument, "Unsupported wave format");
	}
	if (BlockAlign != Channels * 2 || ByteRate != SampleRate * BlockAlign || DataSize % BlockAlign != 0)
	{
		return TResult<FDecodedAsset>::Failure(EErrorCode::InvalidArgument, "Inconsistent wave data size");
	}
	// 検証済みの内容。
	FDecodedAsset Decoded;
	Decoded.Kind = EAssetDecodeKind::Wave;
	Decoded.SampleRate = SampleRate;
	Decoded.Channels = Channels;
	Decoded.BitsPerSample = Bits;
	return TResult<FDecodedAsset>::Success(Toolbox::Move(Decoded));
}
} // namespace
// ファイルの種類を判別して検証する。DxLibを使わない。
// @param Data ファイルのバイト列。
// @param Size バイト列の長さ。
TResult<FDecodedAsset> DecodeAssetBytes(const void* Data, Toolbox::size_t Size)
{
	if (Data == nullptr || Size == 0)
	{
		return TResult<FDecodedAsset>::Failure(EErrorCode::InvalidArgument, "Empty asset contents");
	}
	// 読み取るバイト列。
	const Toolbox::uint8* Bytes = static_cast<const Toolbox::uint8*>(Data);
	if (Size >= 2 && Bytes[0] == 'B' && Bytes[1] == 'M')
	{
		return DecodeBitmap_Internal(Bytes, Size);
	}
	if (Size >= 12 && Bytes[0] == 'R' && Bytes[1] == 'I' && Bytes[2] == 'F' && Bytes[3] == 'F')
	{
		return DecodeWave_Internal(Bytes, Size);
	}
	return TResult<FDecodedAsset>::Failure(EErrorCode::InvalidArgument, "Unsupported asset format");
}
} // namespace Dxf
