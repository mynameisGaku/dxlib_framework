// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_SCENECONTENTSOURCE_H
#define DXF_CONTENT_SCENECONTENTSOURCE_H
#include "Dxf/SceneDefinition2D.h"
#include "Dxf/SceneDefinition3D.h"
#include "Dxf/SceneContentParser.h"
#include "Toolbox/Platform.h"
namespace Dxf
{
/**
 * 固定したProjectRootから初期構成を読むCPU側の入口。World・Native資源を変更しない。
 */
class FSceneContentSource
{
public:
	/**
	 * @param ProjectRoot 既存起動処理で決めた絶対パス。CWDへ切り替えない。
	 * @param Limits 一要求全体へ適用する有限上限。
	 */
	explicit FSceneContentSource(Toolbox::FPath ProjectRoot, FSceneContentLimits Limits = {});
	/**
	 * @param Path ProjectRoot相対のPrefabファイル。
	 * @param Overrides インスタンスだけの型付き公開値。
	 */
	FPrefabDefinition2D LoadPrefab2D(Toolbox::FString Path, const Toolbox::TVector<FContentParameterValue>& Overrides = {}) const;
	/**
	 * @param Path ProjectRoot相対の3D Prefabファイル。
	 * @param Overrides インスタンスだけの型付き公開値。
	 */
	FPrefabDefinition3D LoadPrefab3D(Toolbox::FString Path, const Toolbox::TVector<FContentParameterValue>& Overrides = {}) const;
	/**
	 * @param Path ProjectRoot相対のSceneファイル。同一定義を別配置・公開値で展開する。
	 */
	FSceneDefinition2D LoadScene2D(Toolbox::FString Path) const;
	/**
	 * @param Path ProjectRoot相対の3D Sceneファイル。
	 */
	FSceneDefinition3D LoadScene3D(Toolbox::FString Path) const;

private:
	/**
	 * 各CPU要求へコピーできる不変のRoot。
	 */
	Toolbox::FPath m_ProjectRoot;
	/**
	 * 一要求の上限。
	 */
	FSceneContentLimits m_Limits;
};
} // namespace Dxf
#endif
