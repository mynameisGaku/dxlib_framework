// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_ASSET_DECODE_H
#define DXF_ASSET_DECODE_H
#include "Dxf/Result.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * 非同期読み込みでCPU準備する資産ファイルの上限バイト数。
 */
static constexpr Toolbox::size_t MaxAssetFileBytes = 67108864;
/**
 * CPU準備に対応する非圧縮形式。
 */
enum class EAssetDecodeKind
{
	/**
	 * 無圧縮のBMP画像。
	 */
	Bitmap,
	/**
	 * PCM16のWAVE音声。
	 */
	Wave
};
/**
 * 検証済みファイルの内容と形式情報。Native取込は原本バイト列を使う。
 */
struct FDecodedAsset
{
	/**
	 * 検証済みのファイルバイト列。
	 */
	Toolbox::TVector<Toolbox::uint8> Bytes;
	/**
	 * 判別した形式。
	 */
	EAssetDecodeKind Kind = EAssetDecodeKind::Bitmap;
	/**
	 * 画像の幅。単位は画素。
	 */
	Toolbox::int32 Width = 0;
	/**
	 * 画像の高さ。単位は画素。
	 */
	Toolbox::int32 Height = 0;
	/**
	 * 音声の標本化周波数。単位はHz。
	 */
	Toolbox::uint32 SampleRate = 0;
	/**
	 * 音声のチャンネル数。
	 */
	Toolbox::uint16 Channels = 0;
	/**
	 * 音声の標本あたりビット数。
	 */
	Toolbox::uint16 BitsPerSample = 0;
};
/**
 * ファイルの種類を判別して検証する。DxLibを使わない。
 * 非圧縮BMPとPCM16 WAVE以外は明示的に拒否する。
 * @param Data ファイルのバイト列。
 * @param Size バイト列の長さ。
 */
TResult<FDecodedAsset> DecodeAssetBytes(const void* Data, Toolbox::size_t Size);
} // namespace Dxf
#endif
