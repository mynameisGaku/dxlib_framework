#pragma once
#include "Dxf/Sound.h"
namespace Dxf
{
/**
 * 音声の読み込み器を管理する型。
 */
class FSoundLoader
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Backend ネイティブ処理の呼び出し先。
	 * @param Registry リソースの登録先。
	 */
	FSoundLoader(ISoundBackend& Backend, FResourceRegistry& Registry) : m_pBackend(&Backend), m_pRegistry(&Registry)
	{
	}
	/**
	 * 対象のリソースを読み込む。
	 * @param Path 読み込むファイルのパス。
	 * @param Options 処理に適用する設定。
	 */
	TResult<FSound> Load(const Toolbox::FString& Path, const FSoundLoadOptions& Options);
	/**
	 * 準備済みデータからリソースを読み込む。ファイルを読まない。
	 * @param Data 音声ファイルのバイト列。呼び出し中だけ有効。
	 * @param Size バイト列の長さ。
	 * @param Options 処理に適用する設定。
	 * @param Path 登録に残すファイルのパス。
	 */
	TResult<FSound> LoadMemory(const void* Data, Toolbox::size_t Size, const FSoundLoadOptions& Options,
	                          const Toolbox::FString& Path);

private:
	/**
	 * ネイティブ処理の呼び出し先。
	 */
	ISoundBackend* m_pBackend;
	/**
	 * リソースの登録先。
	 */
	FResourceRegistry* m_pRegistry;
};
} // namespace Dxf
