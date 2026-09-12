#pragma once
#include "Dxf/AssetBackend.h"
#include "Dxf/ResourceRegistry.h"
namespace Dxf
{
/**
 * 音声の読み込み設定と付随情報を管理する型。
 */
struct FSoundMetadata
{
	/**
	 * 読み込むファイルのパス。
	 */
	Toolbox::FString Path;
	/**
	 * 処理に適用する設定。
	 */
	FSoundLoadOptions Options;
};
/**
 * ネイティブ音声と設定を保持するレコード。
 */
using FSoundResource = TResourceRecord<FSoundMetadata>;
/**
 * 再生する音声を管理する型。
 */
class FSound
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	FSound() = default;
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	explicit FSound(Toolbox::TSharedPtr<FSoundResource> Resource) : m_pResource(Toolbox::Move(Resource))
	{
	}
	/**
	 * 検証に成功したかを調べる。
	 */
	bool IsValid() const noexcept
	{
		return GetNativeHandle_Internal() >= 0;
	}
	/**
	 * ネイティブAPIのリソース識別値を取得する。
	 */
	Toolbox::int32 GetNativeHandle_Internal() const noexcept
	{
		return m_pResource ? m_pResource->GetHandle_Internal() : -1;
	}
	/**
	 * 共有するリソースを取得する。
	 */
	FORCEINLINE const Toolbox::TSharedPtr<FSoundResource>& GetResource_Internal() const noexcept
	{
		return m_pResource;
	}

private:
	/**
	 * 共有するリソース。
	 */
	Toolbox::TSharedPtr<FSoundResource> m_pResource;
};
} // namespace Dxf
