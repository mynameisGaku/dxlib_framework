// SPDX-License-Identifier: NOASSERTION
#include "Dxf/SceneContentParser.h"
#include "Dxf/ContentExpansion.h"
#include "Dxf/ContentSchema.inl"
#include "Dxf/ContentSchemaExtras.inl"
#include "Dxf/ContentSceneConnections.inl"
#include "Toolbox/JsonError.h"
namespace Dxf::ContentPrivate
{
Toolbox::TVector<FContentAssetDefinition> ReadSceneAssets(FSchemaReader& Reader, Toolbox::int32 Index, FSceneContentLimits Limits)
{
	FPrefabDefinition2D Values;
	ReadAssets(Reader, Index, Values, Limits);
	return Toolbox::Move(Values.Assets);
}

FPrefabDefinition2D ParseExpanded2D(Toolbox::FStringView Text, const Toolbox::FString& Path, const Toolbox::TVector<FContentParameterValue>& Overrides, FSceneContentLimits Limits, const FChildExpansion2D& Expand, const Toolbox::TVector<FContentAssetDefinition>& SharedAssets)
{
	try
	{
		const auto Document = Toolbox::FJsonDocument::Parse(Text, Limits.Json);
		FSchemaReader Reader(Document, Path, Limits);
		return Parse<FSchema2D>(Reader, Path, Overrides, Limits, Expand, SharedAssets);
	}
	catch (const Toolbox::FJsonError& E)
	{
		throw FSceneContentError({Path, {}, "$", E.What(), E.GetLine(), E.GetColumn()});
	}
}
FPrefabDefinition3D ParseExpanded3D(Toolbox::FStringView Text, const Toolbox::FString& Path, const Toolbox::TVector<FContentParameterValue>& Overrides, FSceneContentLimits Limits, const FChildExpansion3D& Expand, const Toolbox::TVector<FContentAssetDefinition>& SharedAssets)
{
	try
	{
		const auto Document = Toolbox::FJsonDocument::Parse(Text, Limits.Json);
		FSchemaReader Reader(Document, Path, Limits);
		return Parse<FSchema3D>(Reader, Path, Overrides, Limits, Expand, SharedAssets);
	}
	catch (const Toolbox::FJsonError& E)
	{
		throw FSceneContentError({Path, {}, "$", E.What(), E.GetLine(), E.GetColumn()});
	}
}
} // namespace Dxf::ContentPrivate
namespace Dxf
{
FPrefabDefinition2D ParsePrefab2D(Toolbox::FStringView Text, Toolbox::FString Path, const Toolbox::TVector<FContentParameterValue>& Overrides, FSceneContentLimits Limits)
{
	return ContentPrivate::ParseExpanded2D(Text, Path, Overrides, Limits, {});
}
FPrefabDefinition3D ParsePrefab3D(Toolbox::FStringView Text, Toolbox::FString Path, const Toolbox::TVector<FContentParameterValue>& Overrides, FSceneContentLimits Limits)
{
	return ContentPrivate::ParseExpanded3D(Text, Path, Overrides, Limits, {});
}
} // namespace Dxf
