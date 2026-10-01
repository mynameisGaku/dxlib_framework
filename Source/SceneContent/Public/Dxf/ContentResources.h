// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_RESOURCES_H
#define DXF_CONTENT_RESOURCES_H
#include "Dxf/AssetService.h"
#include "Dxf/ContentAssetDefinition.h"
namespace Dxf
{
/**
 * 所有側で準備した型付き資源参照。Nativeの唯一所有は既存Resourceに残る。
 */
class FContentResources
{
public:
	/**
	 * 全必須資源を既存AssetServiceで取得する。一つの失敗で今回の参照だけ回収する。
	 * @param Definitions 検証済みの資源表。
	 * @param Assets 所有スレッドで借用するサービス。
	 * @param Path 診断用定義パス。
	 */
	static FContentResources Prepare(const Toolbox::TVector<FContentAssetDefinition>& Definitions, FAssetService& Assets, const Toolbox::FString& Path);
	/**
	 * 検証済みindexの画像を返す。型違い・範囲外・失効は例外。
	 * @param Index 定義の資源index。
	 */
	const FTexture& GetTexture(Toolbox::uint32 Index) const;
	/**
	 * @param Index 検証済みモデルのindex。型違い・範囲外・失効は例外。
	 */
	const FModel& GetModel(Toolbox::uint32 Index) const;
	/**
	 * @param Index 検証済み音のindex。型違い・範囲外・失効は例外。
	 */
	const FSound& GetSound(Toolbox::uint32 Index) const;
	/**
	 * @param Index 検証済みFontのindex。型違い・範囲外・失効は例外。
	 */
	const FFont& GetFont(Toolbox::uint32 Index) const;

private:
	/**
	 * 型付き資源参照を同じ定義indexに保持する。未採用型は空。
	 */
	struct FEntry
	{
		EContentAssetKind Kind = EContentAssetKind::Texture;
		FTexture Texture;
		FModel Model;
		FSound Sound;
		FFont Font;
	};
	const FEntry& Require(Toolbox::uint32 Index, EContentAssetKind Kind) const;
	/**
	 * 所有側で取得・解放する参照集合。
	 */
	Toolbox::TVector<FEntry> m_Entries;
};
} // namespace Dxf
#endif
