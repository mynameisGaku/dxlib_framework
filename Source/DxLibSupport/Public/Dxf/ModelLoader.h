// SPDX-License-Identifier: NOASSERTION
#pragma once
#include "Dxf/Model.h"
#include "Dxf/ModelBackend.h"
#include "Toolbox/Mutex.h"
namespace Dxf
{
/**
 * .fbxの読み込みとインスタンスの生成、ネイティブモデルの解放順序を管理する型。
 * ネイティブ処理は構築したスレッド（所有スレッド）だけで行う。
 * 他のスレッドで最後の参照が外れたモデルは解放を保留し、所有スレッドのCollectDeferredで解放する。
 */
class FModelLoader
{
public:
	/**
	 * 必要な依存関係を受け取り、構築したスレッドを所有スレッドとして記録する。
	 * @param Backend ネイティブ処理。nullptrならモデルを扱えない構成として読み込みを失敗させる。
	 * @param Registry 解放順序を管理する登録先。
	 */
	FModelLoader(IModelBackend* Backend, FResourceRegistry& Registry);
	/**
	 * 保留中の解放を所有スレッドで済ませる。
	 */
	~FModelLoader();
	FModelLoader(const FModelLoader&) = delete;
	FModelLoader& operator=(const FModelLoader&) = delete;
	/**
	 * .fbxを読み込む。所有スレッドから呼ぶ。
	 * @param Path 解決済みのファイルパス（UTF-8）。
	 * @param Options 読み込み設定。
	 */
	TResult<FModel> Load(const Toolbox::FString& Path, const FModelLoadOptions& Options);
	/**
	 * モデルデータを共有するインスタンスを作る。所有スレッドから呼ぶ。
	 * @param Model 複製元のモデル。
	 */
	TResult<FModelInstance> CreateInstance(const FModel& Model);
	/**
	 * 他のスレッドで保留した解放を行う。所有スレッド以外からは何もしない。
	 */
	void CollectDeferred() noexcept;
	/**
	 * 保留中の解放の数を取得する。
	 */
	Toolbox::size_t GetDeferredCount() const noexcept;
	/**
	 * 所有スレッドから呼ばれているかを調べる。
	 */
	bool IsOwnerThread() const noexcept;

private:
	/**
	 * ネイティブモデルの解放を受け付ける。所有スレッド以外では保留する。
	 */
	static void Release_Internal(void* Context, Toolbox::int32 Handle) noexcept;
	/**
	 * ネイティブ処理。
	 */
	IModelBackend* m_pBackend;
	/**
	 * 解放順序を管理する登録先。
	 */
	FResourceRegistry* m_pRegistry;
	/**
	 * 所有スレッドの識別値。
	 */
	Toolbox::uint64 m_OwnerThread;
	/**
	 * 保留中の解放を守る排他。
	 */
	mutable Toolbox::FMutex m_DeferredMutex;
	/**
	 * 解放を保留したハンドル。
	 */
	Toolbox::TVector<Toolbox::int32> m_Deferred;
};
}
