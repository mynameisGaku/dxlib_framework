// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_CONTENT_COURSE_3D_SCENE_H
#define DXF_SAMPLE_CONTENT_COURSE_3D_SCENE_H
#include "Dxf/ContentScene3D.h"
#include "ContentCourseControls.h"
#include "ContentCoursePortal.h"
#include "InteractionUi.h"
namespace Dxf::GameplaySample
{
/**
 * ファイル由来の装置を公開APIから操作する3DのScene。生成と遷移を取りまとめる。
 */
class DContentCourse3DScene final : public DContentScene3D
{
public:
	/**
	 * 次元別の型付きPrefab参照。
	 */
	using FPrefabHandle = TObjectHandle<DPrefabInstance3D>;
	/**
	 * 次元別の既存Sensor通知参照。
	 */
	using FListenerHandle = TObjectHandle<DContactListener3DComponent>;
	/**
	 * @param Prepared 所有側の必須資源準備を終えた値。
	 */
	explicit DContentCourse3DScene(FPreparedScene3D Prepared);
	/**
	 * @param Id 明示公開した個体名。不在・失効は空。
	 */
	FPrefabHandle FindCoursePrefab(Toolbox::FStringView Id) const noexcept;
	/**
	 * 最初の定義を別位置へ再配置する。データ定義は変更しない。
	 */
	void SpawnExtra();
	/**
	 * @param Split 表示数だけの指定。更新を追加しない。
	 */
	void ConfigureViews(bool Split);
	/**
	 * 自動入力試験と同じ操作値への入口。UIもこの値だけを変更する。
	 */
	FContentCourseControls& GetControls() noexcept;

protected:
	/**
	 * @param Context 所有側の資源と既存初期化トランザクション。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
	/**
	 * @param Render Contentの表示後に全画面UIを重ねる。
	 */
	void OnDraw(FRenderContext& Render) const override;

private:
	/**
	 * 新しい明示生成用の不変の準備値。実体はCollectionとWorldが所有する。
	 */
	/**
	 * 追加生成の所有側資源準備へ借りる。ApplicationがSceneより長く生存する。
	 */
	FAssetService* m_pAssets = nullptr;
	FPreparedScene3D m_Content;
	/**
	 * UIとゲーム処理が共有するScene内の操作値。
	 */
	FContentCourseControls m_Controls;
	/**
	 * 入力消費とPauseを既存のUIへ委譲する。
	 */
	mutable FInteractionUi m_Ui;
	/**
	 * 再読み込みの要求を既存Collectionへ載せる。
	 */
	TObjectHandle<DContentCoursePortal> m_Portal;
	/**
	 * 明示生成名の通し番号。物理Step番号ではない。
	 */
	Toolbox::uint32 m_SpawnCount = 0;
	/**
	 * 表示条件が変わったときだけ更新する寸法と表示数。
	 */
	Toolbox::int32 m_Width = 0;
	Toolbox::int32 m_Height = 0;
	bool m_bSplit = false;
};
} // namespace Dxf::GameplaySample
#endif
