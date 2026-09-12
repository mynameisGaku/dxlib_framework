#pragma once
#include "Toolbox/Algorithm.h"
#include "Dxf/NativeHandle.h"
#include "Toolbox/SharedPtr.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * ネイティブリソースの所有情報を管理する型。
 */
class IResourceRecord
{
public:
	/**
	 * 派生型を含むオブジェクトを安全に終了する。
	 */
	virtual ~IResourceRecord() = default;
	/**
	 * 所有中のネイティブリソースを解放する。
	 */
	virtual void Release_Internal() noexcept = 0;
};
/**
 * ネイティブリソースの所有情報を管理する型。
 */
template <typename TMetadata> class TResourceRecord final : public IResourceRecord
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Handle ハンドル。
	 * @param Metadata リソースの付随情報。
	 */
	TResourceRecord(FNativeHandle Handle, TMetadata Metadata)
	    : m_Handle(Toolbox::Move(Handle)), m_Metadata(Toolbox::Move(Metadata))
	{
	}
	/**
	 * 所有中のネイティブリソースを解放する。
	 */
	void Release_Internal() noexcept override
	{
		m_Handle.Reset();
	}
	/**
	 * ハンドルを取得する。
	 */
	Toolbox::int32 GetHandle_Internal() const noexcept
	{
		return m_Handle.Get();
	}
	/**
	 * バックエンドの識別ポインターを取得する。
	 */
	const void* GetBackendIdentity_Internal() const noexcept
	{
		return m_Handle.GetBackendIdentity_Internal();
	}
	/**
	 * リソースの付随情報を取得する。
	 */
	const TMetadata& GetMetadata() const noexcept
	{
		return m_Metadata;
	}

private:
	/**
	 * ハンドル。
	 */
	FNativeHandle m_Handle;
	/**
	 * リソースの付随情報。
	 */
	TMetadata m_Metadata;
};
/**
 * リソースの一括解放と登録を管理する型。
 */
class FResourceRegistry
{
public:
	/**
	 * 所有する状態を終了し、必要なリソースを解放する。
	 */
	~FResourceRegistry()
	{
		Shutdown();
	}
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	FResourceRegistry() = default;
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	FResourceRegistry(const FResourceRegistry&) = delete;
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	FResourceRegistry& operator=(const FResourceRegistry&) = delete;
	/**
	 * 管理するリソースを登録する。
	 * @param Resource 共有するリソース。
	 */
	bool Register(const Toolbox::TSharedPtr<IResourceRecord>& Resource)
	{
		if (!Resource)
		{
			return false;
		}
		if (m_bShutdown)
		{
			Resource->Release_Internal();
			return false;
		}
		m_Records.EmplaceBack(Resource);
		return true;
	}
	/**
	 * 参照されていないキャッシュ項目を除去する。
	 */
	void CollectUnused()
	{
		Toolbox::EraseIf(m_Records,
		                 [](const auto& Record)
		                 {
			                 return Record.IsExpired();
		                 });
	}
	/**
	 * 管理する処理とリソースを順序どおり終了する。
	 */
	void Shutdown() noexcept
	{
		if (m_bShutdown)
		{
			return;
		}
		m_bShutdown = true;
		/**
		 * 解放コールバックの再入で走査が壊れないよう、外部処理の前に登録一覧を取り外す。
		 *
		 * 登録済みリソースの一覧。
		 */
		auto Records = Toolbox::Move(m_Records);
		/**
		 * 登録されたリソース情報を順に処理する。
		 */
		for (auto& Record : Records)
		{
			/**
			 * 共有するリソースを取得して有効性を確認する。
			 */
			if (auto Resource = Record.Lock())
			{
				Resource->Release_Internal();
			}
		}
	}
	/**
	 * 終了処理が完了しているかを調べる。
	 */
	bool IsShutdown() const noexcept
	{
		return m_bShutdown;
	}

private:
	/**
	 * 登録済みリソースの一覧。
	 */
	Toolbox::TVector<Toolbox::TWeakPtr<IResourceRecord>> m_Records;
	/**
	 * 終了処理が完了しているか。
	 */
	bool m_bShutdown = false;
};
} // namespace Dxf
