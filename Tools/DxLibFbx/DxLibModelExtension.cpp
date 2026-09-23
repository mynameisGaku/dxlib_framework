// SPDX-License-Identifier: NOASSERTION
// FBX SDKを使わず、ufbxで取り出した追加属性をDxLibのモデル確定前へ渡す。
#define DX_MAKE
#include "Dxf/ImportedModel.h"
#pragma warning(push)
#pragma warning(disable: 4828)
#include "DxModelRead.h"
#include "DxMemory.h"
#pragma warning(pop)
#include <string.h>
namespace
{
// 同期読み込みの呼出し中だけ参照する。別スレッドや次の読込へ持ち越さない。
thread_local const Dxf::FImportedModel* CurrentModel = nullptr;
}
extern "C" const Dxf::FImportedModel* DxfSetModelExtension(const Dxf::FImportedModel* Model)
{
	const Dxf::FImportedModel* Previous = CurrentModel;
	CurrentModel = Model;
	return Previous;
}
extern "C" int DxfApplyModelExtensions(void* Data)
{
	if (CurrentModel == nullptr)
	{
		return 0;
	}
	auto* Model = static_cast<DxLib::MV1_MODEL_R*>(Data);
	for (const auto& Extension : CurrentModel->MeshExtensions)
	{
		DxLib::MV1_MESH_R* Target = nullptr;
		for (auto* Mesh = Model->MeshFirst; Mesh != nullptr; Mesh = Mesh->DataNext)
		{
			if (Mesh->Container->NameA != nullptr && strcmp(Mesh->Container->NameA, Extension.FrameName.CStr()) == 0)
			{
				if (Target != nullptr)
				{
					return -1;
				}
				Target = Mesh;
			}
		}
		if (Target == nullptr || Extension.AdditionalUvs.Size() > 1)
		{
			return -1;
		}
		for (Toolbox::size_t Index = 0; Index < Extension.AdditionalUvs.Size(); ++Index)
		{
			const auto& Uvs = Extension.AdditionalUvs[Index];
			if (Uvs.Size() != Target->PositionNum)
			{
				return -1;
			}
			const Toolbox::size_t Set = Index + 1;
			Target->UVNum[Set] = Target->PositionNum;
			Target->UVs[Set] = static_cast<DxLib::FLOAT4*>(DxLib::ADDMEMAREA(sizeof(DxLib::FLOAT4) * Uvs.Size(), &Model->Mem));
			if (Target->UVs[Set] == nullptr)
			{
				return -1;
			}
			for (Toolbox::size_t Vertex = 0; Vertex < Uvs.Size(); ++Vertex)
			{
				Target->UVs[Set][Vertex] = {Uvs[Vertex].X, Uvs[Vertex].Y, 0, 0};
			}
			for (Toolbox::uint32 Face = 0; Face < Target->FaceNum; ++Face)
			{
				for (Toolbox::uint32 Corner = 0; Corner < Target->Faces[Face].IndexNum; ++Corner)
				{
					Target->Faces[Face].UVIndex[Set][Corner] = Target->Faces[Face].VertexIndex[Corner];
				}
			}
		}
	}
	return 0;
}
