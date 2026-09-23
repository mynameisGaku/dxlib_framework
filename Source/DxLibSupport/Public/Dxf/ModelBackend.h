// SPDX-License-Identifier: NOASSERTION
#pragma once
#include "Dxf/ModelImport.h"
namespace Dxf
{
/**
 * ネイティブモデルの確保結果を管理する型。
 */
struct FModelAllocation
{
	/**
	 * ネイティブAPIのリソース識別値。
	 */
	Toolbox::int32 NativeHandle = -1;
	/**
	 * クリップごとのネイティブ側の長さ（ネイティブの時間単位）。変換結果のクリップと同じ順序。
	 */
	Toolbox::TVector<double> NativeClipDurations;
};
/**
 * モデルのネイティブ処理を管理する型。すべて所有スレッドから呼ぶ。
 */
class IModelBackend
{
public:
	/**
	 * 派生型を含むオブジェクトを安全に終了する。
	 */
	virtual ~IModelBackend() = default;
	/**
	 * 変換済みのモデルデータからネイティブモデルを作る。
	 * @param Model 変換済みのモデルデータ。呼び出し中だけ有効。
	 * @param Directory 外部テクスチャを探すディレクトリ（UTF-8の絶対パス）。
	 */
	virtual TResult<FModelAllocation> LoadModel(const FImportedModel& Model, const Toolbox::FString& Directory) = 0;
	/**
	 * 同じモデルデータを共有し、変換と再生状態を独立に持つネイティブモデルを作る。
	 * @param Handle 複製元のハンドル。
	 */
	virtual TResult<Toolbox::int32> DuplicateModel(Toolbox::int32 Handle) = 0;
	/**
	 * ネイティブモデルを解放する。
	 * @param Handle ハンドル。
	 */
	virtual void DeleteModel(Toolbox::int32 Handle) noexcept = 0;
};
}
