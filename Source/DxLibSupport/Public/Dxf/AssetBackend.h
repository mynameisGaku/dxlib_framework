#pragma once
#include "Dxf/Result.h"
namespace Dxf
{
/**
 * テクスチャの読み込み設定を管理する型。
 */
struct FTextureLoadOptions
{
	/**
	 * 3D描画に使える形式で読み込むか。
	 */
	bool bUse3D = true;
	/**
	 * 読込時に乗算済みアルファ（色へアルファを掛けた形式）へ変換するか。乗算済みの合成（UIの透明なパネル等）で使う。
	 */
	bool bPremultipliedAlpha = false;
};
/**
 * テクスチャの確保結果と寸法を管理する型。
 */
struct FTextureAllocation
{
	/**
	 * ネイティブAPIのリソース識別値。
	 */
	Toolbox::int32 NativeHandle = -1;
	/**
	 * 幅。
	 */
	Toolbox::int32 Width = 0;
	/**
	 * 高さ。
	 */
	Toolbox::int32 Height = 0;
};
/**
 * 音声データの保持方式を管理する型。
 */
enum class ESoundStorage
{
	/**
	 * 音声データをメモリへ読み込む。
	 */
	Memory,
	/**
	 * ファイルから音声を逐次読み込む。
	 */
	Stream
};
/**
 * 音声の読み込み設定を管理する型。
 */
struct FSoundLoadOptions
{
	/**
	 * 音声データを保持する方式。
	 */
	ESoundStorage Storage = ESoundStorage::Memory;
};
/**
 * フォントの読み込み設定を管理する型。
 */
struct FFontOptions
{
	/**
	 * フォントの書体名。
	 */
	Toolbox::FString Family = "Meiryo";
	/**
	 * ピクセル単位の文字サイズ。
	 */
	Toolbox::int32 Size = 20;
	/**
	 * 文字の太さ。
	 */
	Toolbox::int32 Thickness = 4;
	/**
	 * 文字の縁を滑らかにするか。
	 */
	bool bAntialias = true;
	/**
	 * 文字の画像を乗算済みアルファで作るか。乗算済みの合成（UIの透明なパネル等）で使う。寸法は変わらない。
	 */
	bool bPremultipliedAlpha = false;
};
/**
 * テクスチャのネイティブ処理を管理する型。
 */
class ITextureBackend
{
public:
	/**
	 * 派生型を含むオブジェクトを安全に終了する。
	 */
	virtual ~ITextureBackend() = default;
	/**
	 * 画像を読み込みテクスチャを取得する。
	 * @param Path 読み込むファイルのパス。
	 * @param Options 処理に適用する設定。
	 */
	virtual TResult<FTextureAllocation> LoadTexture(const Toolbox::FString& Path,
	                                                const FTextureLoadOptions& Options) = 0;
	/**
	 * 準備済み画像データからテクスチャを取得する。ファイルを読まない。
	 * @param Data 画像ファイルのバイト列。呼び出し中だけ有効。
	 * @param Size バイト列の長さ。
	 * @param Options 処理に適用する設定。
	 */
	virtual TResult<FTextureAllocation> LoadTextureMemory(const void* Data, Toolbox::size_t Size,
	                                                     const FTextureLoadOptions& Options) = 0;
	/**
	 * 描画先として使うテクスチャを生成する。
	 * @param Width 幅。
	 * @param Height 高さ。
	 * @param bAlpha 透過を扱う描画先を生成するか。
	 */
	virtual TResult<FTextureAllocation> CreateRenderTarget(Toolbox::int32 Width, Toolbox::int32 Height,
	                                                       bool bAlpha) = 0;
	/**
	 * ネイティブテクスチャを解放する。
	 * @param Handle ハンドル。
	 */
	virtual void DeleteTexture(Toolbox::int32 Handle) noexcept = 0;
};
/**
 * フォントのネイティブ処理を管理する型。
 */
class IFontBackend
{
public:
	/**
	 * 派生型を含むオブジェクトを安全に終了する。
	 */
	virtual ~IFontBackend() = default;
	/**
	 * ネイティブフォントを生成する。
	 * @param Options 処理に適用する設定。
	 */
	virtual TResult<Toolbox::int32> CreateFont(const FFontOptions& Options) = 0;
	/**
	 * ネイティブフォントを解放する。
	 * @param Handle ハンドル。
	 */
	virtual void DeleteFont(Toolbox::int32 Handle) noexcept = 0;
	/**
	 * 一行の文字列の描画幅（画素）を計測する。描画と同じフォント・同じ文字コードで測る。
	 * 計測に対応しないBackendは失敗を返す（既定）。
	 * @param Handle フォントのハンドル。
	 * @param Text UTF-8の文字列（改行を含まない）。
	 */
	virtual TResult<Toolbox::int32> MeasureTextWidth(Toolbox::int32 Handle, const Toolbox::FString& Text)
	{
		(void)Handle;
		(void)Text;
		return TResult<Toolbox::int32>::Failure(EErrorCode::BackendFailure, "Text measurement is not supported");
	}
	/**
	 * 複数行を並べるときの行の送り（画素）。計測に対応しないBackendは失敗を返す（既定）。
	 * @param Handle フォントのハンドル。
	 */
	virtual TResult<Toolbox::int32> GetFontLineHeight(Toolbox::int32 Handle)
	{
		(void)Handle;
		return TResult<Toolbox::int32>::Failure(EErrorCode::BackendFailure, "Text measurement is not supported");
	}
};
/**
 * 音声のネイティブ処理を管理する型。
 */
class ISoundBackend
{
public:
	/**
	 * 派生型を含むオブジェクトを安全に終了する。
	 */
	virtual ~ISoundBackend() = default;
	/**
	 * 音声ファイルを読み込む。
	 * @param Path 読み込むファイルのパス。
	 * @param Options 処理に適用する設定。
	 */
	virtual TResult<Toolbox::int32> LoadSound(const Toolbox::FString& Path, const FSoundLoadOptions& Options) = 0;
	/**
	 * 準備済み音声データから音声を取得する。ファイルを読まない。
	 * @param Data 音声ファイルのバイト列。呼び出し中だけ有効。
	 * @param Size バイト列の長さ。
	 * @param Options 処理に適用する設定。
	 */
	virtual TResult<Toolbox::int32> LoadSoundMemory(const void* Data, Toolbox::size_t Size,
	                                               const FSoundLoadOptions& Options) = 0;
	/**
	 * 独立して再生できる音声ハンドルを複製する。
	 * @param Handle ハンドル。
	 */
	virtual TResult<Toolbox::int32> DuplicateSound(Toolbox::int32 Handle) = 0;
	/**
	 * ネイティブ音声の再生を開始する。
	 * @param Handle ハンドル。
	 * @param bLoop 繰り返し再生するか。
	 */
	virtual TResult<void> StartSound(Toolbox::int32 Handle, bool bLoop) = 0;
	/**
	 * ネイティブ音声の再生を停止する。
	 * @param Handle ハンドル。
	 */
	virtual void StopSound(Toolbox::int32 Handle) noexcept = 0;
	/**
	 * ネイティブ音声を解放する。
	 * @param Handle ハンドル。
	 */
	virtual void DeleteSound(Toolbox::int32 Handle) noexcept = 0;
	/**
	 * ネイティブ音声の音量を設定する。
	 * @param Handle ハンドル。
	 * @param Volume 再生音量。
	 */
	virtual TResult<void> SetSoundVolume(Toolbox::int32 Handle, Toolbox::f32 Volume) = 0;
	/**
	 * ネイティブ音声が再生中かを調べる。
	 * @param Handle ハンドル。
	 */
	virtual TResult<bool> IsSoundPlaying(Toolbox::int32 Handle) = 0;
};
} // namespace Dxf
