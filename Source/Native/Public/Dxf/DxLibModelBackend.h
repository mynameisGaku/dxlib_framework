// SPDX-License-Identifier: NOASSERTION
#pragma once
#include "Dxf/ModelBackend.h"
namespace Dxf
{
/**
 * DxLibのモデル機能（MV1）でモデルを扱う型。変換済みのデータをメモリから読み込む。
 */
class FDxLibModelBackend final : public IModelBackend
{
public:
	/**
	 * リンクしたDxLibでモデルを扱えるかを調べる。公式VCパッケージだけの構成ではfalse。
	 */
	static bool IsAvailable() noexcept;
	/**
	 * 変換済みのモデルデータからネイティブモデルを作る。
	 * @param Model 変換済みのモデルデータ。
	 * @param Directory 外部テクスチャを探すディレクトリ（UTF-8の絶対パス）。
	 */
	TResult<FModelAllocation> LoadModel(const FImportedModel& Model, const Toolbox::FString& Directory) override;
	/**
	 * 同じモデルデータを共有するネイティブモデルを作る。
	 * @param Handle 複製元のハンドル。
	 */
	TResult<Toolbox::int32> DuplicateModel(Toolbox::int32 Handle) override;
	/**
	 * ネイティブモデルを解放する。
	 * @param Handle ハンドル。
	 */
	void DeleteModel(Toolbox::int32 Handle) noexcept override;
};
}
