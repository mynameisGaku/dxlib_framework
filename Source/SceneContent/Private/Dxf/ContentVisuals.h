// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_VISUALS_H
#define DXF_CONTENT_VISUALS_H
#include "Dxf/ContentView2D.h"
#include "Dxf/ContentVisualDefinition.h"
#include "Dxf/ContentResources.h"
#include "Dxf/RenderContext.h"
#include "Dxf/AssetService.h"
#include "Toolbox/Vector2.h"
#include "Toolbox/Quaternion.h"
namespace Dxf::ContentPrivate
{
/**
 * 個体別モデルと共有ラベルだけを所有する描画アダプター。物理を更新しない。
 */
class FContentVisuals
{
public:
	/**
	 * @param Count 部品数。初期化前に容量を確保する。
	 */
	void Reserve(Toolbox::size_t Count);
	/**
	 * @param Visual 型検証済みの表示条件。
	 * @param Resources 既存準備経路の読み取り専用資源。
	 * @param Assets 個体モデルの作成を担当する所有側サービス。
	 */
	void Add(const FContentVisualDefinition& Visual, const FContentResources& Resources, FAssetService& Assets);
	/**
	 * @param Seconds この一回の更新の秒数。View数に依存しない。
	 */
	void Advance(Toolbox::f64 Seconds);
	/**
	 * @param Index 部品の検証済みindex。
	 * @param Visual 不変表示条件。
	 * @param Resources 準備資源。
	 * @param Position 補間または表示専用のWorld位置。
	 * @param Angle 補間したラジアン角。
	 * @param View 画素への写し方とクリップ。
	 * @param Render 現在の描画窓口。
	 */
	void Draw2D(Toolbox::size_t Index, const FContentVisualDefinition& Visual, const FContentResources& Resources, Toolbox::FVector2 Position, Toolbox::f32 Angle, const FContentView2D& View, FRenderContext& Render) const;
	/**
	 * @param Index 部品の検証済みindex。
	 * @param Visual 不変表示条件。
	 * @param Resources 準備資源。
	 * @param Position 補間または表示専用のWorld位置。
	 * @param Rotation 補間した向き。
	 * @param Render 指定ビューを持つ描画窓口。
	 */
	void Draw3D(Toolbox::size_t Index, const FContentVisualDefinition& Visual, const FContentResources& Resources, Toolbox::FVector3 Position, Toolbox::FQuaternion Rotation, FRenderContext& Render) const;
	/**
	 * 個体モデルを所有側で解放する。共有資源を強制失効させない。
	 */
	void Clear() noexcept;

private:
	/**
	 * 一つの部品の個体表示値。
	 */
	struct FEntry
	{
		/**
		 * Transformの受付保存だけを描画時に行う。Advanceは更新側だけ。
		 */
		mutable FModelInstance Model;
		/**
		 * ラベル文字列は初期化時に一度だけ共有化する。
		 */
		FSharedText Label;
	};
	/**
	 * 部品indexに対応する個体値。
	 */
	Toolbox::TVector<FEntry> m_Entries;
};
} // namespace Dxf::ContentPrivate
#endif
