// SPDX-License-Identifier: NOASSERTION
#include "Dxf/PreparedPrefab3D.h"
#include "Dxf/SceneContentError.h"
#include "Dxf/SceneContentValidation.h"
namespace Dxf
{
FPreparedPrefab3D PreparePrefab(FPrefabDefinition3D Definition, FAssetService& Assets)
{
	ValidatePrefabDefinition(Definition);
	for (auto& Part : Definition.Parts)
	{
		Part.Body.Orientation = Part.Body.Orientation.Normalized();
	}
	if (!Definition.Assets.IsEmpty() && !Definition.ProjectRoot.IsEmpty() && Assets.GetProjectRoot().Normalize().ToUtf8() != Definition.ProjectRoot)
	{
		throw FSceneContentError( {Definition.Path, {}, "$", "AssetService ProjectRoot differs from content preparation", 0, 0});
	}
	auto Resources =
	    Toolbox::MakeShared<FContentResources>(FContentResources::Prepare(Definition.Assets, Assets, Definition.Path));
	auto Immutable = Toolbox::MakeShared<FPrefabDefinition3D>(Toolbox::Move(Definition));
	return {Toolbox::Move(Immutable), Toolbox::Move(Resources)};
}
} // namespace Dxf
