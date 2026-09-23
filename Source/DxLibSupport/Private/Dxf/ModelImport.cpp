// SPDX-License-Identifier: NOASSERTION
// FBXをufbxで読み、DxLibが直接読めるDirectX .x（テキスト形式）へ変換する。
// FBX SDKを使わないため、利用者はSDKのインストールや規約への同意を必要としない。
#include "Dxf/ModelImport.h"
#include "ufbx.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
namespace Dxf
{
namespace
{
/**
 * 1頂点の属性。ufbx_generate_indicesがmemcmpで比較するため、詰め物を含まない並びにする。
 */
struct FVertex_Internal
{
	Toolbox::f32 Position[3];
	Toolbox::f32 Normal[3];
	Toolbox::f32 Uv[2];
	Toolbox::uint32 ControlPoint;
};
static_assert(sizeof(FVertex_Internal) == 36, "FVertex_Internal must not contain padding");

/**
 * .xのテキストを組み立てる。
 */
class FWriter_Internal
{
public:
	explicit FWriter_Internal(Toolbox::TVector<char>& Output) : m_pOutput(&Output)
	{
	}
	void Text(const char* Value)
	{
		Bytes(Value, strlen(Value));
	}
	void Bytes(const char* Value, Toolbox::size_t Length)
	{
		// PushBackの容量拡張に任せ、追記ごとに全文を再確保しない。
		for (Toolbox::size_t Index = 0; Index < Length; ++Index)
		{
			m_pOutput->PushBack(Value[Index]);
		}
	}
	void Float(Toolbox::f64 Value)
	{
		char Buffer[32];
		// 非有限値は.xの構文に書けないため0として扱う。
		if (!(Value == Value) || Value > 3.0e38 || Value < -3.0e38)
		{
			Value = 0.0;
		}
		const Toolbox::int32 Length = snprintf(Buffer, sizeof(Buffer), "%.7g", Value);
		Bytes(Buffer, Length > 0 ? static_cast<Toolbox::size_t>(Length) : 0);
	}
	void Unsigned(Toolbox::uint64 Value)
	{
		char Buffer[32];
		const Toolbox::int32 Length = snprintf(Buffer, sizeof(Buffer), "%llu", static_cast<unsigned long long>(Value));
		Bytes(Buffer, Length > 0 ? static_cast<Toolbox::size_t>(Length) : 0);
	}
	/**
	 * ufbxの行列を、DirectXの行ベクトル形式（平行移動が最終行）の16要素として書く。
	 */
	void Matrix(const ufbx_matrix& Value)
	{
		const ufbx_vec3* Columns[4] = {&Value.cols[0], &Value.cols[1], &Value.cols[2], &Value.cols[3]};
		for (Toolbox::int32 Row = 0; Row < 4; ++Row)
		{
			Float(Columns[Row]->x);
			Text(",");
			Float(Columns[Row]->y);
			Text(",");
			Float(Columns[Row]->z);
			Text(Row == 3 ? ",1" : ",0,");
		}
	}

private:
	Toolbox::TVector<char>* m_pOutput;
};

/**
 * 変換の途中状態。
 */
struct FContext_Internal
{
	const ufbx_scene* pScene = nullptr;
	FImportedModel* pModel = nullptr;
	/**
	 * ノードのelement_idごとの.x上の名前。
	 */
	Toolbox::TVector<Toolbox::FString> NodeNames;
	/**
	 * ufbx_textureのelement_idごとのテクスチャ番号（未登録は-1）。
	 */
	Toolbox::TVector<Toolbox::int32> TextureIndices;
};

Toolbox::FString ToString_Internal(ufbx_string Value)
{
	return Toolbox::FString(Value.data, Value.length);
}

/**
 * .xの識別子に使える名前へ置き換える（英数字と_以外を_にし、重複には番号を付ける）。
 * 元の名前はフレームワークの公開APIで別途扱う。
 */
Toolbox::FString MakeIdentifier_Internal(const char* Prefix, ufbx_string Name, Toolbox::uint32 Id,
                                         const Toolbox::TVector<Toolbox::FString>& Used)
{
	Toolbox::FString Result(Prefix);
	for (Toolbox::size_t Index = 0; Index < Name.length; ++Index)
	{
		const char Character = Name.data[Index];
		const bool bAlnum = (Character >= 'a' && Character <= 'z') || (Character >= 'A' && Character <= 'Z') ||
		                    (Character >= '0' && Character <= '9') || Character == '_';
		Result.PushBack(bAlnum ? Character : '_');
	}
	for (const Toolbox::FString& Other : Used)
	{
		if (Other.Size() == Result.Size() && memcmp(Other.CStr(), Result.CStr(), Result.Size()) == 0)
		{
			Result.PushBack('_');
			Result += Toolbox::ToString(static_cast<Toolbox::uint64>(Id));
			break;
		}
	}
	return Result;
}

/**
 * テクスチャを登録し、.x内の参照名を返す。
 */
const Toolbox::FString* AddTexture_Internal(FContext_Internal& Context, const ufbx_texture* Texture)
{
	if (Texture == nullptr)
	{
		return nullptr;
	}
	Toolbox::int32& Slot = Context.TextureIndices[Texture->element_id];
	if (Slot < 0)
	{
		FImportedModelTexture Entry;
		// DxLibは画像の種類を内容から判別するため、参照名は番号と元の拡張子で一意にする。
		Entry.Name = Toolbox::FString("dxf_texture_");
		Entry.Name += Toolbox::ToString(static_cast<Toolbox::uint64>(Context.pModel->Textures.Size()));
		const ufbx_string Source = Texture->relative_filename.length ? Texture->relative_filename : Texture->filename;
		for (Toolbox::size_t Index = Source.length; Index > 0; --Index)
		{
			const char Character = Source.data[Index - 1];
			if (Character == '/' || Character == '\\')
			{
				break;
			}
			if (Character == '.')
			{
				Entry.Name += Toolbox::FString(Source.data + Index - 1, Source.length - Index + 1);
				break;
			}
		}
		Entry.RelativePath = ToString_Internal(Texture->relative_filename);
		Entry.AbsolutePath = ToString_Internal(Texture->absolute_filename);
		if (Texture->content.size > 0)
		{
			Entry.Embedded.Resize(Texture->content.size);
			memcpy(Entry.Embedded.Data(), Texture->content.data, Texture->content.size);
		}
		Slot = static_cast<Toolbox::int32>(Context.pModel->Textures.Size());
		Context.pModel->Textures.PushBack(Toolbox::Move(Entry));
	}
	return &Context.pModel->Textures[static_cast<Toolbox::size_t>(Slot)].Name;
}

/**
 * 材質を.xのMaterialとして書く。
 */
void WriteMaterial_Internal(FContext_Internal& Context, FWriter_Internal& Writer, const ufbx_material* Material)
{
	Toolbox::f64 Color[4] = {0.8, 0.8, 0.8, 1.0};
	const ufbx_texture* Texture = nullptr;
	if (Material != nullptr)
	{
		const ufbx_material_map& Diffuse = Material->fbx.diffuse_color;
		if (Diffuse.has_value)
		{
			const Toolbox::f64 Factor =
			    Material->fbx.diffuse_factor.has_value ? Material->fbx.diffuse_factor.value_real : 1.0;
			Color[0] = Diffuse.value_vec3.x * Factor;
			Color[1] = Diffuse.value_vec3.y * Factor;
			Color[2] = Diffuse.value_vec3.z * Factor;
		}
		if (Material->fbx.transparency_factor.has_value)
		{
			Color[3] = 1.0 - Material->fbx.transparency_factor.value_real;
		}
		Texture = Diffuse.texture;
		if (Texture == nullptr)
		{
			Texture = Material->pbr.base_color.texture;
		}
	}
	Writer.Text("Material {\n");
	for (Toolbox::int32 Index = 0; Index < 4; ++Index)
	{
		Writer.Float(Color[Index]);
		Writer.Text(Index == 3 ? ";;\n" : ";");
	}
	Writer.Text("0;\n0;0;0;;\n0;0;0;;\n");
	if (const Toolbox::FString* Name = AddTexture_Internal(Context, Texture))
	{
		Writer.Text("TextureFilename {\n\"");
		Writer.Text(Name->CStr());
		Writer.Text("\";\n}\n");
	}
	Writer.Text("}\n");
}

/**
 * メッシュを.xのMeshとして書く。頂点は所属ノードの座標系へ置く。
 */
TResult<void> WriteMesh_Internal(FContext_Internal& Context, FWriter_Internal& Writer, const ufbx_node* Node)
{
	const ufbx_mesh* Mesh = Node->mesh;
	if (Mesh->num_faces == 0)
	{
		return {};
	}
	const ufbx_matrix& GeometryToNode = Node->geometry_to_node;
	const ufbx_matrix NormalMatrix = ufbx_matrix_for_normals(&GeometryToNode);
	// 三角形へ分割し、角ごとの頂点属性を集める。
	Toolbox::TVector<Toolbox::uint32> Scratch(Mesh->max_face_triangles * 3);
	Toolbox::TVector<FVertex_Internal> Corners;
	Toolbox::TVector<Toolbox::uint32> FaceMaterials;
	Corners.Reserve(Mesh->num_triangles * 3);
	FaceMaterials.Reserve(Mesh->num_triangles);
	for (Toolbox::size_t FaceIndex = 0; FaceIndex < Mesh->num_faces; ++FaceIndex)
	{
		const ufbx_face Face = Mesh->faces.data[FaceIndex];
		const Toolbox::uint32 Triangles = ufbx_triangulate_face(Scratch.Data(), Scratch.Size(), Mesh, Face);
		const Toolbox::uint32 Material =
		    Mesh->face_material.count > FaceIndex ? Mesh->face_material.data[FaceIndex] : 0;
		for (Toolbox::uint32 Corner = 0; Corner < Triangles * 3; ++Corner)
		{
			const Toolbox::uint32 Index = Scratch[Corner];
			FVertex_Internal Vertex;
			memset(&Vertex, 0, sizeof(Vertex));
			const ufbx_vec3 Position =
			    ufbx_transform_position(&GeometryToNode, ufbx_get_vertex_vec3(&Mesh->vertex_position, Index));
			Vertex.Position[0] = static_cast<Toolbox::f32>(Position.x);
			Vertex.Position[1] = static_cast<Toolbox::f32>(Position.y);
			Vertex.Position[2] = static_cast<Toolbox::f32>(Position.z);
			if (Mesh->vertex_normal.exists)
			{
				ufbx_vec3 Normal =
				    ufbx_transform_direction(&NormalMatrix, ufbx_get_vertex_vec3(&Mesh->vertex_normal, Index));
				const Toolbox::f64 Length = Normal.x * Normal.x + Normal.y * Normal.y + Normal.z * Normal.z;
				if (Length > 0.0)
				{
					const Toolbox::f64 Scale = 1.0 / Toolbox::Sqrt(Length);
					Normal.x *= Scale;
					Normal.y *= Scale;
					Normal.z *= Scale;
				}
				Vertex.Normal[0] = static_cast<Toolbox::f32>(Normal.x);
				Vertex.Normal[1] = static_cast<Toolbox::f32>(Normal.y);
				Vertex.Normal[2] = static_cast<Toolbox::f32>(Normal.z);
			}
			if (Mesh->vertex_uv.exists)
			{
				const ufbx_vec2 Uv = ufbx_get_vertex_vec2(&Mesh->vertex_uv, Index);
				// FBXのVは下から、DirectXのVは上から数える。
				Vertex.Uv[0] = static_cast<Toolbox::f32>(Uv.x);
				Vertex.Uv[1] = static_cast<Toolbox::f32>(1.0 - Uv.y);
			}
			Vertex.ControlPoint = Mesh->vertex_indices.data[Index];
			Corners.PushBack(Vertex);
		}
		for (Toolbox::uint32 Triangle = 0; Triangle < Triangles; ++Triangle)
		{
			FaceMaterials.PushBack(Material);
		}
	}
	if (Corners.IsEmpty())
	{
		return {};
	}
	// 同じ属性の角を1頂点へまとめる。
	Toolbox::TVector<Toolbox::uint32> Indices(Corners.Size());
	ufbx_vertex_stream Stream = {Corners.Data(), Corners.Size(), sizeof(FVertex_Internal)};
	ufbx_error Error;
	const Toolbox::size_t VertexCount =
	    ufbx_generate_indices(&Stream, 1, Indices.Data(), Indices.Size(), nullptr, &Error);
	if (Error.type != UFBX_ERROR_NONE)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "FBX mesh index generation failed");
	}
	const Toolbox::size_t TriangleCount = Indices.Size() / 3;
	Writer.Text("Mesh ");
	Writer.Text(Context.NodeNames[Node->element_id].CStr());
	Writer.Text("_mesh {\n");
	Writer.Unsigned(VertexCount);
	Writer.Text(";\n");
	for (Toolbox::size_t Index = 0; Index < VertexCount; ++Index)
	{
		for (Toolbox::int32 Axis = 0; Axis < 3; ++Axis)
		{
			Writer.Float(Corners[Index].Position[Axis]);
			Writer.Text(";");
		}
		Writer.Text(Index + 1 == VertexCount ? ";\n" : ",\n");
	}
	const auto WriteFaces = [&]()
	{
		Writer.Unsigned(TriangleCount);
		Writer.Text(";\n");
		for (Toolbox::size_t Triangle = 0; Triangle < TriangleCount; ++Triangle)
		{
			Writer.Text("3;");
			Writer.Unsigned(Indices[Triangle * 3]);
			Writer.Text(",");
			Writer.Unsigned(Indices[Triangle * 3 + 1]);
			Writer.Text(",");
			Writer.Unsigned(Indices[Triangle * 3 + 2]);
			Writer.Text(Triangle + 1 == TriangleCount ? ";;\n" : ";,\n");
		}
	};
	WriteFaces();
	Writer.Text("MeshNormals {\n");
	Writer.Unsigned(VertexCount);
	Writer.Text(";\n");
	for (Toolbox::size_t Index = 0; Index < VertexCount; ++Index)
	{
		for (Toolbox::int32 Axis = 0; Axis < 3; ++Axis)
		{
			Writer.Float(Corners[Index].Normal[Axis]);
			Writer.Text(";");
		}
		Writer.Text(Index + 1 == VertexCount ? ";\n" : ",\n");
	}
	WriteFaces();
	Writer.Text("}\n");
	Writer.Text("MeshTextureCoords {\n");
	Writer.Unsigned(VertexCount);
	Writer.Text(";\n");
	for (Toolbox::size_t Index = 0; Index < VertexCount; ++Index)
	{
		Writer.Float(Corners[Index].Uv[0]);
		Writer.Text(";");
		Writer.Float(Corners[Index].Uv[1]);
		Writer.Text(Index + 1 == VertexCount ? ";;\n" : ";,\n");
	}
	Writer.Text("}\n");
	// 材質。FBXで材質がないメッシュには既定の材質を1つ付ける。
	const Toolbox::size_t MaterialCount = Mesh->materials.count > 0 ? Mesh->materials.count : 1;
	Writer.Text("MeshMaterialList {\n");
	Writer.Unsigned(MaterialCount);
	Writer.Text(";\n");
	Writer.Unsigned(TriangleCount);
	Writer.Text(";\n");
	for (Toolbox::size_t Triangle = 0; Triangle < TriangleCount; ++Triangle)
	{
		const Toolbox::uint32 Material = FaceMaterials[Triangle] < MaterialCount ? FaceMaterials[Triangle] : 0;
		Writer.Unsigned(Material);
		Writer.Text(Triangle + 1 == TriangleCount ? ";\n" : ",\n");
	}
	for (Toolbox::size_t Material = 0; Material < MaterialCount; ++Material)
	{
		WriteMaterial_Internal(Context, Writer, Mesh->materials.count > 0 ? Mesh->materials.data[Material] : nullptr);
	}
	Writer.Text("}\n");
	// スキン。各骨の影響を、まとめた後の頂点番号で書く。
	if (Mesh->skin_deformers.count > 0)
	{
		const ufbx_skin_deformer* Skin = Mesh->skin_deformers.data[0];
		const ufbx_matrix NodeToGeometry = ufbx_matrix_invert(&GeometryToNode);
		Toolbox::TVector<Toolbox::f32> Weights(Mesh->num_vertices);
		Toolbox::TVector<Toolbox::uint32> PerVertex(VertexCount);
		Toolbox::TVector<Toolbox::uint32> Influenced;
		Toolbox::TVector<Toolbox::f32> InfluenceWeights;
		// 先に頂点ごとの影響数を数え、ヘッダーへ書く最大数を求める。
		Toolbox::size_t Bones = 0;
		for (Toolbox::size_t ClusterIndex = 0; ClusterIndex < Skin->clusters.count; ++ClusterIndex)
		{
			const ufbx_skin_cluster* Cluster = Skin->clusters.data[ClusterIndex];
			if (Cluster->bone_node == nullptr || Cluster->num_weights == 0)
			{
				continue;
			}
			++Bones;
			for (Toolbox::size_t Index = 0; Index < Cluster->num_weights; ++Index)
			{
				const Toolbox::uint32 ControlPoint = Cluster->vertices.data[Index];
				for (Toolbox::size_t Vertex = 0; Vertex < VertexCount; ++Vertex)
				{
					if (Corners[Vertex].ControlPoint == ControlPoint)
					{
						++PerVertex[Vertex];
					}
				}
			}
		}
		Toolbox::uint32 MaxPerVertex = 0;
		for (const Toolbox::uint32 Count : PerVertex)
		{
			MaxPerVertex = Count > MaxPerVertex ? Count : MaxPerVertex;
		}
		Writer.Text("XSkinMeshHeader {\n");
		Writer.Unsigned(MaxPerVertex);
		Writer.Text(";\n");
		Writer.Unsigned(MaxPerVertex * 3);
		Writer.Text(";\n");
		Writer.Unsigned(Bones);
		Writer.Text(";\n}\n");
		for (Toolbox::size_t ClusterIndex = 0; ClusterIndex < Skin->clusters.count; ++ClusterIndex)
		{
			const ufbx_skin_cluster* Cluster = Skin->clusters.data[ClusterIndex];
			if (Cluster->bone_node == nullptr || Cluster->num_weights == 0)
			{
				continue;
			}
			for (Toolbox::f32& Weight : Weights)
			{
				Weight = 0.0f;
			}
			for (Toolbox::size_t Index = 0; Index < Cluster->num_weights; ++Index)
			{
				const Toolbox::uint32 ControlPoint = Cluster->vertices.data[Index];
				if (ControlPoint < Weights.Size())
				{
					Weights[ControlPoint] = static_cast<Toolbox::f32>(Cluster->weights.data[Index]);
				}
			}
			Influenced.Clear();
			InfluenceWeights.Clear();
			for (Toolbox::size_t Vertex = 0; Vertex < VertexCount; ++Vertex)
			{
				const Toolbox::f32 Weight = Weights[Corners[Vertex].ControlPoint];
				if (Weight > 0.0f)
				{
					Influenced.PushBack(static_cast<Toolbox::uint32>(Vertex));
					InfluenceWeights.PushBack(Weight);
				}
			}
			Writer.Text("SkinWeights {\n\"");
			Writer.Text(Context.NodeNames[Cluster->bone_node->element_id].CStr());
			Writer.Text("\";\n");
			Writer.Unsigned(Influenced.Size());
			Writer.Text(";\n");
			for (Toolbox::size_t Index = 0; Index < Influenced.Size(); ++Index)
			{
				Writer.Unsigned(Influenced[Index]);
				Writer.Text(Index + 1 == Influenced.Size() ? ";\n" : ",\n");
			}
			for (Toolbox::size_t Index = 0; Index < InfluenceWeights.Size(); ++Index)
			{
				Writer.Float(InfluenceWeights[Index]);
				Writer.Text(Index + 1 == InfluenceWeights.Size() ? ";\n" : ",\n");
			}
			if (Influenced.IsEmpty())
			{
				Writer.Text(";\n;\n");
			}
			// 頂点はノード座標系へ置いたので、骨空間への変換もノード座標系から始める。
			const ufbx_matrix Offset = ufbx_matrix_mul(&Cluster->geometry_to_bone, &NodeToGeometry);
			Writer.Matrix(Offset);
			Writer.Text(";;\n}\n");
		}
	}
	Writer.Text("}\n");
	return {};
}

/**
 * ノードと子を.xのFrameとして書く。
 */
TResult<void> WriteFrame_Internal(FContext_Internal& Context, FWriter_Internal& Writer, const ufbx_node* Node,
                                  Toolbox::uint32 Depth)
{
	if (Depth > 256)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "FBX node hierarchy is too deep");
	}
	Writer.Text("Frame ");
	Writer.Text(Context.NodeNames[Node->element_id].CStr());
	Writer.Text(" {\nFrameTransformMatrix {\n");
	Writer.Matrix(Node->node_to_parent);
	Writer.Text(";;\n}\n");
	if (Node->mesh != nullptr)
	{
		auto Result = WriteMesh_Internal(Context, Writer, Node);
		if (!Result)
		{
			return Result;
		}
	}
	for (Toolbox::size_t Index = 0; Index < Node->children.count; ++Index)
	{
		auto Result = WriteFrame_Internal(Context, Writer, Node->children.data[Index], Depth + 1);
		if (!Result)
		{
			return Result;
		}
	}
	Writer.Text("}\n");
	return {};
}

/**
 * アニメーションスタックを、全ノードの局所行列を一定間隔で標本化したAnimationSetとして書く。
 */
void WriteAnimations_Internal(FContext_Internal& Context, FWriter_Internal& Writer, Toolbox::uint32 SamplesPerSecond)
{
	const ufbx_scene* Scene = Context.pScene;
	for (Toolbox::size_t StackIndex = 0; StackIndex < Scene->anim_stacks.count; ++StackIndex)
	{
		const ufbx_anim_stack* Stack = Scene->anim_stacks.data[StackIndex];
		Toolbox::f64 Duration = Stack->time_end - Stack->time_begin;
		if (!(Duration > 0.0))
		{
			Duration = 0.0;
		}
		FImportedModelClip Clip;
		Clip.Name = ToString_Internal(Stack->name);
		Clip.DurationSeconds = Duration;
		Context.pModel->Clips.PushBack(Toolbox::Move(Clip));
		// 必要な区間数を切り上げ、末尾が標本間隔の途中でも終端を含める。
		const Toolbox::f64 Intervals = Duration * SamplesPerSecond;
		Toolbox::uint64 Segments = static_cast<Toolbox::uint64>(Intervals);
		if (static_cast<Toolbox::f64>(Segments) < Intervals)
		{
			++Segments;
		}
		const Toolbox::uint64 Keys = Segments + 1;
		Writer.Text("AnimationSet clip_");
		Writer.Unsigned(StackIndex);
		Writer.Text(" {\n");
		for (Toolbox::size_t NodeIndex = 0; NodeIndex < Scene->nodes.count; ++NodeIndex)
		{
			const ufbx_node* Node = Scene->nodes.data[NodeIndex];
			if (Node->is_root)
			{
				continue;
			}
			Writer.Text("Animation {\n{");
			Writer.Text(Context.NodeNames[Node->element_id].CStr());
			Writer.Text("}\nAnimationKey {\n4;\n");
			Writer.Unsigned(Keys);
			Writer.Text(";\n");
			for (Toolbox::uint64 Key = 0; Key < Keys; ++Key)
			{
				// ネイティブ再生時間と一致するよう、全区間をクリップ全長へ均等に割り当てる。
				Toolbox::f64 Time = Stack->time_begin + (Segments == 0 ? 0.0
				                                                       : Duration * static_cast<Toolbox::f64>(Key) /
				                                                             static_cast<Toolbox::f64>(Segments));
				Time = Time < Stack->time_end ? Time : Stack->time_end;
				const ufbx_transform Transform = ufbx_evaluate_transform(Stack->anim, Node, Time);
				const ufbx_matrix Local = ufbx_transform_to_matrix(&Transform);
				Writer.Unsigned(Key);
				Writer.Text(";16;");
				Writer.Matrix(Local);
				Writer.Text(Key + 1 == Keys ? ";;;\n" : ";;,\n");
			}
			Writer.Text("}\n}\n");
		}
		Writer.Text("}\n");
	}
}
} // namespace

namespace
{
// 現在の変換で失われる機能を記録し、姿勢を正しく保持できないスキンは拒否する。
// @param Scene ufbxによる解析結果。
// @param Warnings 部分読み込みの理由を追記する先。
TResult<void> CheckFeatures_Internal(const ufbx_scene& Scene, Toolbox::TVector<Toolbox::FString>& Warnings)
{
	// 複数メッシュに同じ未対応属性があっても、理由ごとに1件へまとめる。
	bool bExtraUv = false;
	bool bVertexColor = false;
	for (const ufbx_mesh* Mesh : Scene.meshes)
	{
		bExtraUv = bExtraUv || Mesh->uv_sets.count > 1;
		bVertexColor = bVertexColor || Mesh->vertex_color.exists;
		if (Mesh->skin_deformers.count > 1)
		{
			return TResult<void>::Failure(EErrorCode::InvalidArgument, "FBX meshes with multiple skins are unsupported");
		}
	}
	for (const ufbx_skin_deformer* Skin : Scene.skin_deformers)
	{
		if (Skin->skinning_method != UFBX_SKINNING_METHOD_LINEAR && Skin->skinning_method != UFBX_SKINNING_METHOD_RIGID)
		{
			return TResult<void>::Failure(EErrorCode::InvalidArgument, "FBX dual-quaternion skinning is unsupported");
		}
	}
	if (Scene.blend_deformers.count > 0)
	{
		Warnings.PushBack("Morph targets are ignored; only the base mesh and skeletal animation are imported");
	}
	if (bExtraUv)
	{
		Warnings.PushBack("Additional UV sets are ignored; only the first UV set is imported");
	}
	if (bVertexColor)
	{
		Warnings.PushBack("Vertex colors are ignored");
	}
	// 基本拡散色以外の材質属性を読み込めたものとして扱わない。
	bool bAdvancedMaterial = false;
	bool bPhong = false;
	for (const ufbx_material* Material : Scene.materials)
	{
		bAdvancedMaterial = bAdvancedMaterial || (Material->shader_type != UFBX_SHADER_FBX_LAMBERT && Material->shader_type != UFBX_SHADER_FBX_PHONG);
		bPhong = bPhong || Material->shader_type == UFBX_SHADER_FBX_PHONG;
	}
	if (bAdvancedMaterial)
	{
		Warnings.PushBack("PBR or unknown material shading is unsupported; only the FBX diffuse color and base texture fallback are imported");
	}
	if (bPhong)
	{
		Warnings.PushBack("Phong specular, shininess and emission are ignored; only diffuse color and texture are imported");
	}
	if (Scene.cameras.count > 0 || Scene.lights.count > 0)
	{
		Warnings.PushBack("File cameras and lights are ignored; use the render view settings");
	}
	if (Scene.constraints.count > 0)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "FBX constraints must be baked into node animation before import");
	}
	// ufbxが補正して読み進めた内容も、呼出し側が確認できるように残す。
	for (const ufbx_warning& Warning : Scene.metadata.warnings)
	{
		Warnings.PushBack(Toolbox::FString("ufbx: ") + Toolbox::FString(Warning.description.data, Warning.description.length));
	}
	return {};
}
} // namespace

TResult<FImportedModel> ImportFbxModel(const void* Data, Toolbox::size_t Size, const FModelImportOptions& Options)
{
	if (Data == nullptr || Size == 0)
	{
		return TResult<FImportedModel>::Failure(EErrorCode::InvalidArgument, "FBX data is empty");
	}
	if (Options.SamplesPerSecond == 0 || Options.SamplesPerSecond > 1000)
	{
		return TResult<FImportedModel>::Failure(EErrorCode::InvalidArgument,
		                                        "FBX animation sample rate must be 1..1000");
	}
	if (!(Options.TargetUnitMeters >= 0.0) || Options.TargetUnitMeters > 1.0e6)
	{
		return TResult<FImportedModel>::Failure(EErrorCode::InvalidArgument,
		                                        "FBX target unit must be 0 or a positive length in meters");
	}
	ufbx_load_opts LoadOptions;
	memset(&LoadOptions, 0, sizeof(LoadOptions));
	// 座標系と単位の変換はここだけで行う。DxLibは左手系・Y軸上向き。単位は指定があるときだけ変える。
	LoadOptions.target_axes = ufbx_axes_left_handed_y_up;
	LoadOptions.target_unit_meters = Options.TargetUnitMeters;
	LoadOptions.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;
	LoadOptions.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
	LoadOptions.generate_missing_normals = true;
	// OBJ等の別形式を拡張子だけFBXへ変えて受け付けない。
	LoadOptions.file_format = UFBX_FILE_FORMAT_FBX;
	ufbx_error Error;
	// ufbx自身の所有型で、変換中に例外が発生した場合も解析結果を解放する。
	ufbx_unique_ptr<ufbx_scene> SceneOwner(ufbx_load_memory(Data, Size, &LoadOptions, &Error));
	// 所有者の有効期間だけ参照する解析結果。
	ufbx_scene* Scene = SceneOwner.get();
	if (Scene == nullptr)
	{
		char Message[512];
		ufbx_format_error(Message, sizeof(Message), &Error);
		return TResult<FImportedModel>::Failure(EErrorCode::InvalidArgument,
		                                        Toolbox::FString("FBX load failed: ") + Toolbox::FString(Message));
	}
	// 変換するキー数を制限し、整数変換の範囲外と過大な出力を拒否する。
	constexpr Toolbox::f64 MaxAnimationKeys = 1000000.0;
	Toolbox::f64 TotalAnimationKeys = 0.0;
	for (Toolbox::size_t Index = 0; Index < Scene->anim_stacks.count; ++Index)
	{
		// 全ノードを各時刻で標本化するため、ノード数も上限に含める。
		const ufbx_anim_stack* Stack = Scene->anim_stacks.data[Index];
		const Toolbox::f64 Duration = Stack->time_end - Stack->time_begin;
		TotalAnimationKeys +=
		    (Duration * Options.SamplesPerSecond + 2.0) * static_cast<Toolbox::f64>(Scene->nodes.count);
		if (!Toolbox::IsFinite(Stack->time_begin) || !Toolbox::IsFinite(Stack->time_end) || Duration < 0.0 ||
		    !Toolbox::IsFinite(TotalAnimationKeys) || TotalAnimationKeys > MaxAnimationKeys)
		{
			return TResult<FImportedModel>::Failure(EErrorCode::InvalidArgument,
			                                        "FBX animation duration is invalid or exceeds the key limit");
		}
	}
	FImportedModel Model;
	// 省略と失敗を分類し、失敗した入力をネイティブへ渡さない。
	auto Features = CheckFeatures_Internal(*Scene, Model.Warnings);
	if (!Features)
	{
		return TResult<FImportedModel>::Failure(Features.Error());
	}
	Model.SamplesPerSecond = Options.SamplesPerSecond;
	FContext_Internal Context;
	Context.pScene = Scene;
	Context.pModel = &Model;
	Context.NodeNames.Resize(Scene->elements.count);
	Context.TextureIndices.Resize(Scene->elements.count);
	for (Toolbox::int32& Index : Context.TextureIndices)
	{
		Index = -1;
	}
	Toolbox::TVector<Toolbox::FString> Used;
	for (Toolbox::size_t Index = 0; Index < Scene->nodes.count; ++Index)
	{
		const ufbx_node* Node = Scene->nodes.data[Index];
		Toolbox::FString Name = MakeIdentifier_Internal("", Node->name, Node->element_id, Used);
		if (Name.IsEmpty())
		{
			Name = Toolbox::FString("node_");
			Name += Toolbox::ToString(static_cast<Toolbox::uint64>(Node->element_id));
		}
		Used.PushBack(Name);
		Context.NodeNames[Node->element_id] = Toolbox::Move(Name);
	}
	FWriter_Internal Writer(Model.ModelData);
	Writer.Text("xof 0303txt 0032\n");
	Writer.Text("AnimTicksPerSecond {\n");
	Writer.Unsigned(Options.SamplesPerSecond);
	Writer.Text(";\n}\n");
	// 根ノードは変換済みの空の座標系なので、子だけを最上位のFrameとして書く。
	for (Toolbox::size_t Index = 0; Index < Scene->root_node->children.count; ++Index)
	{
		auto Result = WriteFrame_Internal(Context, Writer, Scene->root_node->children.data[Index], 0);
		if (!Result)
		{
			return TResult<FImportedModel>::Failure(Result.Error());
		}
	}
	WriteAnimations_Internal(Context, Writer, Options.SamplesPerSecond);
	Model.ModelData.PushBack('\0');
	return TResult<FImportedModel>::Success(Toolbox::Move(Model));
}
} // namespace Dxf
