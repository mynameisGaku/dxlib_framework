// SPDX-License-Identifier: NOASSERTION
#include "Dxf/ContentResources.h"
#include "Dxf/SceneContentError.h"
namespace Dxf
{
FContentResources FContentResources::Prepare(const Toolbox::TVector<FContentAssetDefinition>& Definitions, FAssetService& Assets, const Toolbox::FString& Path)
{
	FContentResources Result;
	Result.m_Entries.Reserve(Definitions.Size());
	for (const auto& Definition : Definitions)
	{
		FEntry Entry;
		Entry.Kind = Definition.Kind;
		try
		{
			if (Definition.Kind == EContentAssetKind::Texture)
			{
				auto Loaded = Assets.LoadTexture(Definition.Path, Definition.Texture);
				if (!Loaded)
				{
					throw Toolbox::FException(Loaded.Error().Message);
				}
				Entry.Texture = Toolbox::Move(Loaded.Value());
			}
			else if (Definition.Kind == EContentAssetKind::Model)
			{
				auto Loaded = Assets.LoadModel(Definition.Path, Definition.Model);
				if (!Loaded)
				{
					throw Toolbox::FException(Loaded.Error().Message);
				}
				Entry.Model = Toolbox::Move(Loaded.Value());
			}
			else if (Definition.Kind == EContentAssetKind::Sound)
			{
				auto Loaded = Assets.LoadSound(Definition.Path, Definition.Sound);
				if (!Loaded)
				{
					throw Toolbox::FException(Loaded.Error().Message);
				}
				Entry.Sound = Toolbox::Move(Loaded.Value());
			}
			else if (Definition.Kind == EContentAssetKind::Font)
			{
				auto Loaded = Assets.LoadFont(Definition.Font);
				if (!Loaded)
				{
					throw Toolbox::FException(Loaded.Error().Message);
				}
				Entry.Font = Toolbox::Move(Loaded.Value());
			}
			else
			{
				throw Toolbox::FException("Unknown Content resource kind");
			}
		}
		catch (const Toolbox::FException& Error)
		{
			throw FSceneContentError({Path, Definition.Id, "assets/" + Definition.Id, Error.What(), 0, 0});
		}
		Result.m_Entries.PushBack(Toolbox::Move(Entry));
	}
	return Result;
}
const FContentResources::FEntry& FContentResources::Require(Toolbox::uint32 Index, EContentAssetKind Kind) const
{
	if (Index >= m_Entries.Size() || m_Entries[Index].Kind != Kind)
	{
		throw Toolbox::FException("Content asset index or type mismatch");
	}
	return m_Entries[Index];
}
const FTexture& FContentResources::GetTexture(Toolbox::uint32 Index) const
{
	const auto& Value = Require(Index, EContentAssetKind::Texture).Texture;
	if (!Value.IsValid())
	{
		throw Toolbox::FException("Content Texture expired");
	}
	return Value;
}
const FModel& FContentResources::GetModel(Toolbox::uint32 Index) const
{
	const auto& Value = Require(Index, EContentAssetKind::Model).Model;
	if (!Value.IsValid())
	{
		throw Toolbox::FException("Content Model expired");
	}
	return Value;
}
const FSound& FContentResources::GetSound(Toolbox::uint32 Index) const
{
	const auto& Value = Require(Index, EContentAssetKind::Sound).Sound;
	if (!Value.IsValid())
	{
		throw Toolbox::FException("Content Sound expired");
	}
	return Value;
}
const FFont& FContentResources::GetFont(Toolbox::uint32 Index) const
{
	const auto& Value = Require(Index, EContentAssetKind::Font).Font;
	if (!Value.IsValid())
	{
		throw Toolbox::FException("Content Font expired");
	}
	return Value;
}
} // namespace Dxf
