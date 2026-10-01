// SPDX-License-Identifier: NOASSERTION
#include "Dxf/PreparedPrefab2D.h"
#include "Dxf/SceneContentError.h"
#include "Dxf/SceneContentValidation.h"
namespace Dxf
{
FPreparedPrefab2D PreparePrefab(FPrefabDefinition2D Definition, FAssetService& Assets)
{
	ValidatePrefabDefinition(Definition);
	if (!Definition.Assets.IsEmpty() && !Definition.ProjectRoot.IsEmpty() && Assets.GetProjectRoot().Normalize().ToUtf8() != Definition.ProjectRoot)
	{
		throw FSceneContentError( {Definition.Path, {}, "$", "AssetService ProjectRoot differs from content preparation", 0, 0});
	}
	auto Resources =
	    Toolbox::MakeShared<FContentResources>(FContentResources::Prepare(Definition.Assets, Assets, Definition.Path));
	auto Immutable = Toolbox::MakeShared<FPrefabDefinition2D>(Toolbox::Move(Definition));
	return {Toolbox::Move(Immutable), Toolbox::Move(Resources)};
}
} // namespace Dxf
