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
	// 変換後の法線を単位長にする倍率。モーフ法線にも同じ倍率を使う。
	Toolbox::f32 NormalScale;
	Toolbox::f32 Uv[2];
	// 材質の拡散色を掛けた頂点色。色の境界も頂点の共有判定に含める。
	Toolbox::f32 Color[4];
	Toolbox::uint32 ControlPoint;
};
static_assert(sizeof(FVertex_Internal) == 56, "FVertex_Internal must not contain padding");

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
	// 出力モーフに対応する元チャンネル。解析結果の生存中だけ使う。
	Toolbox::TVector<const ufbx_blend_channel*> MorphChannels;
	// モーフの展開量と標本数の累計。
	Toolbox::size_t MorphVertices = 0;
	Toolbox::uint64 MorphSamples = 0;
	FImportedModel* pModel = nullptr;
	/**
	 * ノードのelement_idごとの.x上の名前。
	 */
	Toolbox::TVector<Toolbox::FString> NodeNames;
	// ノード名と衝突しないネイティブのメッシュフレーム名。
	Toolbox::TVector<Toolbox::FString> MeshNames;
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
	bool Duplicate = true;
	while (Duplicate)
	{
		Duplicate = false;
		for (const Toolbox::FString& Other : Used)
		{
			if (Other == Result)
			{
				Result.PushBack('_');
				Result += Toolbox::ToString(static_cast<Toolbox::uint64>(Id));
				Duplicate = true;
				break;
			}
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

// ufbxが意味を解釈できた金属度・粗さ系だけをPBRとして扱う。
bool IsPbr_Internal(const ufbx_material* Material)
{
	return Material != nullptr && Material->features.pbr.enabled &&
	       Material->shader_type != UFBX_SHADER_3DS_MAX_PBR_SPEC_GLOSS && Material->shader_type != UFBX_SHADER_UNKNOWN;
}
// テクスチャの参照も、採用した材質方式のベース色と一致させる。
const ufbx_texture* BaseTexture_Internal(const ufbx_material* Material)
{
	if (Material == nullptr)
		return nullptr;
	if (IsPbr_Internal(Material) && Material->pbr.base_color.texture != nullptr)
		return Material->pbr.base_color.texture;
	return Material->fbx.diffuse_color.texture != nullptr ? Material->fbx.diffuse_color.texture
	                                                      : Material->pbr.base_color.texture;
}
// 材質の拡散色と不透明度を求める。未指定の成分は呼出し側の既定値を保つ。
void DiffuseColor_Internal(const ufbx_material* Material, Toolbox::f64 (&Color)[4])
{
	if (Material != nullptr)
	{
		const bool Pbr = IsPbr_Internal(Material);
		const ufbx_material_map& Diffuse = Pbr ? Material->pbr.base_color : Material->fbx.diffuse_color;
		if (Diffuse.has_value)
		{
			const Toolbox::f64 Factor =
			    (Pbr ? (Material->pbr.base_factor.has_value ? Material->pbr.base_factor.value_real : 1.0)
			         : (Material->fbx.diffuse_factor.has_value ? Material->fbx.diffuse_factor.value_real : 1.0));
			Color[0] = Diffuse.value_vec3.x * Factor;
			Color[1] = Diffuse.value_vec3.y * Factor;
			Color[2] = Diffuse.value_vec3.z * Factor;
		}
		if (!Pbr && Material->fbx.transparency_factor.has_value)
		{
			Color[3] = 1.0 - Material->fbx.transparency_factor.value_real;
		}
	}
}

// 材質の色とテクスチャ参照を出力する。
void WriteMaterial_Internal(FContext_Internal& Context, FWriter_Internal& Writer, const ufbx_material* Material,
                            const ufbx_mesh& Mesh)
{
	// 材質がない場合に使う拡散色。
	Toolbox::f64 Color[4] = {0.8, 0.8, 0.8, 1.0};
	DiffuseColor_Internal(Material, Color);
	const ufbx_texture* Texture = BaseTexture_Internal(Material);
	Writer.Text("Material {\n");
	for (Toolbox::int32 Index = 0; Index < 4; ++Index)
	{
		Writer.Float(Color[Index]);
		Writer.Text(Index == 3 ? ";;\n" : ";");
	}
	if (IsPbr_Internal(Material))
	{
		// .xの鏡面欄を内部PBR経路の伝達に使う。既存照明の鏡面光は常に無効。
		Context.pModel->bHasPbrMaterials = true;
		Writer.Float(Material->pbr.roughness.has_value ? Material->pbr.roughness.value_real : 0.5);
		Writer.Text(";\n");
		Writer.Float(Material->pbr.metalness.has_value ? Material->pbr.metalness.value_real : 0);
		Writer.Text(";1;");
		const bool Uv1 = Texture != nullptr && Texture->uv_set.length > 0 && Mesh.uv_sets.count > 1 &&
		                 ToString_Internal(Texture->uv_set) == ToString_Internal(Mesh.uv_sets.data[1].name);
		Writer.Text(Uv1 ? "1;;\n0;0;0;;\n" : "0;;\n0;0;0;;\n");
	}
	else
	{
		Writer.Text("0;\n0;0;0;;\n0;0;0;;\n");
	}
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
	Context.pModel->bAllMeshesHaveUv1 = Context.pModel->bAllMeshesHaveUv1 && Mesh->uv_sets.count >= 2;
	const ufbx_matrix& GeometryToNode = Node->geometry_to_node;
	const ufbx_matrix NormalMatrix = ufbx_matrix_for_normals(&GeometryToNode);
	// 三角形へ分割し、角ごとの頂点属性を集める。
	Toolbox::TVector<Toolbox::uint32> Scratch(Mesh->max_face_triangles * 3);
	Toolbox::TVector<FVertex_Internal> Corners;
	Toolbox::TVector<Toolbox::uint32> FaceMaterials;
	Corners.Reserve(Mesh->num_triangles * 3);
	// 追加UVは存在する組だけ確保する。通常のモデルの頂点サイズを増やさない。
	Toolbox::TVector<Toolbox::TVector<FVector2>> ExtraCorners;
	ExtraCorners.Resize(Mesh->uv_sets.count > 0 ? Mesh->uv_sets.count - 1 : 0);
	for (auto& Set : ExtraCorners)
	{
		Set.Reserve(Mesh->num_triangles * 3);
	}
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
					Vertex.NormalScale = static_cast<Toolbox::f32>(Scale);
					Normal.x *= Scale;
					Normal.y *= Scale;
					Normal.z *= Scale;
				}
				Vertex.Normal[0] = static_cast<Toolbox::f32>(Normal.x);
				Vertex.Normal[1] = static_cast<Toolbox::f32>(Normal.y);
				Vertex.Normal[2] = static_cast<Toolbox::f32>(Normal.z);
			}
			for (Toolbox::size_t Set = 0; Set < Mesh->uv_sets.count; ++Set)
			{
				// 追加UVの継ぎ目も頂点共有の判定へ含める。
				const ufbx_vec2 Uv = ufbx_get_vertex_vec2(&Mesh->uv_sets.data[Set].vertex_uv, Index);
				if (!Toolbox::IsFinite(Uv.x) || !Toolbox::IsFinite(Uv.y) || Toolbox::Abs(Uv.x) > 3.0e38 ||
				    Toolbox::Abs(Uv.y) > 3.0e38)
				{
					return TResult<void>::Failure(EErrorCode::InvalidArgument,
					                              "FBX UV coordinates must be finite float values");
				}
				if (Set == 0)
				{
					Vertex.Uv[0] = static_cast<Toolbox::f32>(Uv.x);
					Vertex.Uv[1] = static_cast<Toolbox::f32>(1.0 - Uv.y);
				}
				else
				{
					ExtraCorners[Set - 1].PushBack(
					    {static_cast<Toolbox::f32>(Uv.x), static_cast<Toolbox::f32>(1.0 - Uv.y)});
				}
			}
			if (Mesh->vertex_color.exists)
			{
				// 頂点色は材質の拡散色に乗算する。面ごとに材質が異なる場合も共有しない。
				const ufbx_vec4 Color = ufbx_get_vertex_vec4(&Mesh->vertex_color, Index);
				Toolbox::f64 Diffuse[4] = {0.8, 0.8, 0.8, 1.0};
				DiffuseColor_Internal(Material < Mesh->materials.count ? Mesh->materials.data[Material] : nullptr,
				                      Diffuse);
				for (Toolbox::size_t Component = 0; Component < 4; ++Component)
				{
					// DxLibの頂点色は8ビット。範囲外や非有限値を黙って補正しない。
					const Toolbox::f64 Value = Color.v[Component] * Diffuse[Component];
					if (!(Value >= 0.0 && Value <= 1.0))
					{
						return TResult<void>::Failure(
						    EErrorCode::InvalidArgument,
						    "FBX vertex color multiplied by diffuse must be finite and in 0..1");
					}
					Vertex.Color[Component] = static_cast<Toolbox::f32>(Value);
				}
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
	ufbx_vertex_stream Streams[2] = {};
	Streams[0] = {Corners.Data(), Corners.Size(), sizeof(FVertex_Internal)};
	for (Toolbox::size_t Index = 0; Index < ExtraCorners.Size(); ++Index)
	{
		Streams[Index + 1] = {ExtraCorners[Index].Data(), Corners.Size(), sizeof(FVector2)};
	}
	ufbx_error Error;
	const Toolbox::size_t VertexCount =
	    ufbx_generate_indices(Streams, ExtraCorners.Size() + 1, Indices.Data(), Indices.Size(), nullptr, &Error);
	if (Error.type != UFBX_ERROR_NONE)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "FBX mesh index generation failed");
	}
	if (Mesh->uv_sets.count > 1)
	{
		// 標準.xに入らないUVは、最終頂点順でネイティブへ引き渡す。
		FImportedModelMesh Extension;
		Extension.FrameName = Context.MeshNames[Node->element_id];
		for (Toolbox::size_t Set = 1; Set < Mesh->uv_sets.count; ++Set)
		{
			Toolbox::TVector<FVector2> Uvs;
			Uvs.Reserve(VertexCount);
			for (Toolbox::size_t Index = 0; Index < VertexCount; ++Index)
			{
				Uvs.PushBack(ExtraCorners[Set - 1][Index]);
			}
			Extension.AdditionalUvs.PushBack(Toolbox::Move(Uvs));
		}
		Context.pModel->MeshExtensions.PushBack(Toolbox::Move(Extension));
	}
	// 単一ターゲットのモーフを、重複除去後の頂点へ展開する。
	for (const ufbx_blend_deformer* Deformer : Mesh->blend_deformers)
	{
		for (const ufbx_blend_channel* Channel : Deformer->channels)
		{
			if (Channel->keyframes.count == 0)
			{
				continue;
			}
			const ufbx_blend_shape* Shape = Channel->keyframes.data[0].shape;
			if (VertexCount > 4000000u - Context.MorphVertices)
			{
				return TResult<void>::Failure(EErrorCode::InvalidArgument, "FBX morph vertex limit exceeded");
			}
			Context.MorphVertices += VertexCount;
			if (Shape->offset_weights.count > 0)
			{
				return TResult<void>::Failure(EErrorCode::InvalidArgument,
				                              "FBX weighted morph offsets are unsupported");
			}
			FImportedModelMorph Morph;
			Morph.Name = ToString_Internal(Node->name) + ":" + ToString_Internal(Channel->name);
			Morph.FrameName = Context.MeshNames[Node->element_id];
			Morph.DefaultWeight = static_cast<Toolbox::f32>(Channel->weight / Channel->keyframes.data[0].target_weight);
			if (!(Morph.DefaultWeight >= 0 && Morph.DefaultWeight <= 1))
			{
				return TResult<void>::Failure(EErrorCode::InvalidArgument,
				                              "FBX morph weight must be finite and in 0..1");
			}
			for (Toolbox::uint32 Point : Shape->offset_vertices)
			{
				if (Point >= Mesh->num_vertices)
				{
					return TResult<void>::Failure(EErrorCode::InvalidArgument,
					                              "FBX morph references a missing control point");
				}
			}
			Morph.PositionOffsets.Reserve(VertexCount);
			if (Shape->normal_offsets.count > 0)
			{
				Morph.NormalOffsets.Reserve(VertexCount);
			}
			else
			{
				Context.pModel->Warnings.PushBack("Morph target has no normal offsets; base normals are retained");
			}
			for (Toolbox::size_t Vertex = 0; Vertex < VertexCount; ++Vertex)
			{
				const Toolbox::uint32 Point = Corners[Vertex].ControlPoint;
				const ufbx_vec3 Offset =
				    ufbx_transform_direction(&GeometryToNode, ufbx_get_blend_shape_vertex_offset(Shape, Point));
				if (!Toolbox::IsFinite(Offset.x) || !Toolbox::IsFinite(Offset.y) || !Toolbox::IsFinite(Offset.z) ||
				    Toolbox::Abs(Offset.x) > 3.0e38 || Toolbox::Abs(Offset.y) > 3.0e38 ||
				    Toolbox::Abs(Offset.z) > 3.0e38)
				{
					return TResult<void>::Failure(EErrorCode::InvalidArgument,
					                              "FBX morph offsets must be finite float values");
				}
				Morph.PositionOffsets.PushBack({static_cast<Toolbox::f32>(Offset.x),
				                                static_cast<Toolbox::f32>(Offset.y),
				                                static_cast<Toolbox::f32>(Offset.z)});
				if (Shape->normal_offsets.count > 0)
				{
					const Toolbox::uint32 OffsetIndex = ufbx_get_blend_shape_offset_index(Shape, Point);
					const ufbx_vec3 Delta =
					    OffsetIndex < Shape->normal_offsets.count
					        ? ufbx_transform_direction(&NormalMatrix, Shape->normal_offsets.data[OffsetIndex])
					        : ufbx_vec3{};
					const Toolbox::f64 X = Corners[Vertex].Normal[0] + Delta.x * Corners[Vertex].NormalScale;
					const Toolbox::f64 Y = Corners[Vertex].Normal[1] + Delta.y * Corners[Vertex].NormalScale;
					const Toolbox::f64 Z = Corners[Vertex].Normal[2] + Delta.z * Corners[Vertex].NormalScale;
					const Toolbox::f64 Length = Toolbox::Sqrt(X * X + Y * Y + Z * Z);
					if (!(Length > 0) || !Toolbox::IsFinite(Length))
					{
						return TResult<void>::Failure(EErrorCode::InvalidArgument, "FBX morph normal is invalid");
					}
					Morph.NormalOffsets.PushBack({static_cast<Toolbox::f32>(X / Length - Corners[Vertex].Normal[0]),
					                              static_cast<Toolbox::f32>(Y / Length - Corners[Vertex].Normal[1]),
					                              static_cast<Toolbox::f32>(Z / Length - Corners[Vertex].Normal[2])});
				}
			}
			Context.pModel->Morphs.PushBack(Toolbox::Move(Morph));
			Context.MorphChannels.PushBack(Channel);
		}
	}
	const Toolbox::size_t TriangleCount = Indices.Size() / 3;
	Writer.Text("Mesh ");
	Writer.Text(Context.MeshNames[Node->element_id].CStr());
	Writer.Text(" {\n");
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
	if (Mesh->vertex_color.exists)
	{
		Context.pModel->VertexColorFrames.PushBack(Context.MeshNames[Node->element_id]);
		Writer.Text("MeshVertexColors {\n");
		Writer.Unsigned(VertexCount);
		Writer.Text(";\n");
		for (Toolbox::size_t Index = 0; Index < VertexCount; ++Index)
		{
			Writer.Unsigned(Index);
			Writer.Text(";");
			for (Toolbox::size_t Component = 0; Component < 4; ++Component)
			{
				// DxLibの区切り付きIndexedColor読込は0..255を期待する。
				Writer.Float(Corners[Index].Color[Component] * 255.0);
				Writer.Text(";");
			}
			Writer.Text(Index + 1 == VertexCount ? ";;\n" : ";,\n");
		}
		Writer.Text("}\n");
	}
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
		WriteMaterial_Internal(Context, Writer, Mesh->materials.count > 0 ? Mesh->materials.data[Material] : nullptr,
		                       *Mesh);
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
TResult<void> WriteAnimations_Internal(FContext_Internal& Context, FWriter_Internal& Writer,
                                       Toolbox::uint32 SamplesPerSecond)
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
		// 必要な区間数を切り上げ、末尾が標本間隔の途中でも終端を含める。
		const Toolbox::f64 Intervals = Duration * SamplesPerSecond;
		Toolbox::uint64 Segments = static_cast<Toolbox::uint64>(Intervals);
		if (static_cast<Toolbox::f64>(Segments) < Intervals)
		{
			++Segments;
		}
		const Toolbox::uint64 Keys = Segments + 1;
		Context.MorphSamples += Keys * Context.MorphChannels.Size();
		if (Context.MorphSamples > 1000000)
		{
			return TResult<void>::Failure(EErrorCode::InvalidArgument, "FBX morph animation key limit exceeded");
		}
		for (const ufbx_blend_channel* Channel : Context.MorphChannels)
		{
			Toolbox::TVector<Toolbox::f32> Weights;
			Weights.Reserve(static_cast<Toolbox::size_t>(Keys));
			for (Toolbox::uint64 Key = 0; Key < Keys; ++Key)
			{
				const Toolbox::f64 Time =
				    Stack->time_begin +
				    (Segments > 0 ? Duration * static_cast<Toolbox::f64>(Key) / static_cast<Toolbox::f64>(Segments)
				                  : 0.0);
				const Toolbox::f64 Weight =
				    ufbx_evaluate_blend_weight(Stack->anim, Channel, Time) / Channel->keyframes.data[0].target_weight;
				if (!(Weight >= 0.0 && Weight <= 1.0))
				{
					return TResult<void>::Failure(EErrorCode::InvalidArgument,
					                              "FBX animated morph weight must be finite and in 0..1");
				}
				Weights.PushBack(static_cast<Toolbox::f32>(Weight));
			}
			Clip.MorphWeights.PushBack(Toolbox::Move(Weights));
		}
		Context.pModel->Clips.PushBack(Toolbox::Move(Clip));
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
	return {};
}
} // namespace

namespace
{
// ufbxの軸指定をカメラのローカル方向へ変換する。
ufbx_vec3 AxisVector_Internal(ufbx_coordinate_axis Axis)
{
	switch (Axis)
	{
	case UFBX_COORDINATE_AXIS_POSITIVE_X:
		return {1, 0, 0};
	case UFBX_COORDINATE_AXIS_NEGATIVE_X:
		return {-1, 0, 0};
	case UFBX_COORDINATE_AXIS_POSITIVE_Y:
		return {0, 1, 0};
	case UFBX_COORDINATE_AXIS_NEGATIVE_Y:
		return {0, -1, 0};
	case UFBX_COORDINATE_AXIS_POSITIVE_Z:
		return {0, 0, 1};
	case UFBX_COORDINATE_AXIS_NEGATIVE_Z:
		return {0, 0, -1};
	default:
		return {};
	}
}
// 浮動小数点の範囲検査はApplyToで行う。
Toolbox::FVector3 SceneVector_Internal(ufbx_vec3 Value)
{
	return {static_cast<Toolbox::f32>(Value.x), static_cast<Toolbox::f32>(Value.y), static_cast<Toolbox::f32>(Value.z)};
}
// ノードに接続されたカメラ・ライトを静止時のワールド座標で保持する。
TResult<void> ImportSceneObjects_Internal(const ufbx_scene& Scene, FImportedModel& Model)
{
	// ufbxの角度は度、公開APIはラジアン。
	constexpr Toolbox::f64 Radians = 0.017453292519943295;
	for (const ufbx_node* Node : Scene.nodes)
	{
		if (Node->camera != nullptr)
		{
			const ufbx_camera& Camera = *Node->camera;
			FModelCameraInfo Info;
			Info.Name = ToString_Internal(Node->name);
			Info.Eye = SceneVector_Internal(Node->node_to_world.cols[3]);
			// ufbx 0.23はtarget_camera_axes未指定時にprojection_axesを初期化しない。
			const ufbx_coordinate_axes Axes =
			    ufbx_coordinate_axes_valid(Camera.projection_axes)
			        ? Camera.projection_axes
			        : ufbx_coordinate_axes{UFBX_COORDINATE_AXIS_POSITIVE_Z, UFBX_COORDINATE_AXIS_POSITIVE_Y,
			                               UFBX_COORDINATE_AXIS_NEGATIVE_X};
			Info.Forward = -Toolbox::Normalize(
			    SceneVector_Internal(ufbx_transform_direction(&Node->node_to_world, AxisVector_Internal(Axes.front))));
			Info.Up = Toolbox::Normalize(
			    SceneVector_Internal(ufbx_transform_direction(&Node->node_to_world, AxisVector_Internal(Axes.up))));
			Info.bOrthographic = Camera.projection_mode == UFBX_PROJECTION_MODE_ORTHOGRAPHIC;
			Info.VerticalFov =
			    Info.bOrthographic ? 1.0f : static_cast<Toolbox::f32>(Camera.field_of_view_deg.y * Radians);
			Info.NearPlane = static_cast<Toolbox::f32>(Camera.near_plane);
			Info.FarPlane = static_cast<Toolbox::f32>(Camera.far_plane);
			Info.AspectRatio = static_cast<Toolbox::f32>(Camera.aspect_ratio);
			Info.OrthographicHeight = Info.bOrthographic ? static_cast<Toolbox::f32>(Camera.orthographic_size.y) : 1.0f;
			FRenderView3D Probe;
			if (!Info.ApplyTo(Probe) || !Toolbox::IsFinite(Info.AspectRatio) || Info.AspectRatio <= 0)
			{
				char Details[512];
				snprintf(Details, sizeof(Details),
				         "FBX camera has invalid projection or transform: %s eye=%g,%g,%g forward=%g,%g,%g up=%g,%g,%g "
				         "fov=%g near=%g far=%g height=%g aspect=%g",
				         Info.Name.CStr(), Info.Eye.X, Info.Eye.Y, Info.Eye.Z, Info.Forward.X, Info.Forward.Y,
				         Info.Forward.Z, Info.Up.X, Info.Up.Y, Info.Up.Z, Info.VerticalFov, Info.NearPlane,
				         Info.FarPlane, Info.OrthographicHeight, Info.AspectRatio);
				return TResult<void>::Failure(EErrorCode::InvalidArgument, Toolbox::FString(Details));
			}
			Model.Cameras.PushBack(Toolbox::Move(Info));
		}
		if (Node->light != nullptr)
		{
			const ufbx_light& Light = *Node->light;
			if (Light.type == UFBX_LIGHT_AREA || Light.type == UFBX_LIGHT_VOLUME)
			{
				Model.Warnings.PushBack("Area and volume lights are omitted: " + ToString_Internal(Node->name));
				continue;
			}
			FModelLightInfo Info;
			Info.Name = ToString_Internal(Node->name);
			Info.Type = Light.type == UFBX_LIGHT_POINT
			                ? EModelLightType::Point
			                : (Light.type == UFBX_LIGHT_SPOT ? EModelLightType::Spot : EModelLightType::Directional);
			Info.Position = SceneVector_Internal(Node->node_to_world.cols[3]);
			Info.Direction = Toolbox::Normalize(
			    SceneVector_Internal(ufbx_transform_direction(&Node->node_to_world, Light.local_direction)));
			Info.Radiance = SceneVector_Internal(
			    {Light.color.x * Light.intensity, Light.color.y * Light.intensity, Light.color.z * Light.intensity});
			Info.bEnabled = Light.cast_light;
			if (Info.Type == EModelLightType::Spot)
			{
				Info.InnerAngle = static_cast<Toolbox::f32>(Light.inner_angle * Radians);
				Info.OuterAngle = static_cast<Toolbox::f32>(Light.outer_angle * Radians);
			}
			FRenderView3D Probe;
			// 無効な強度は消灯中でも拒否する。
			Probe.bModelLightOverride = true;
			Probe.ModelLightRadiance = Info.Radiance;
			if (!IsValidRenderView3D(Probe) || !Info.ApplyTo(Probe))
			{
				return TResult<void>::Failure(EErrorCode::InvalidArgument,
				                              "FBX light has invalid intensity, cone or transform");
			}
			if (Light.decay != UFBX_LIGHT_DECAY_NONE)
			{
				Model.Warnings.PushBack("Light decay requires explicit world-unit attenuation in ApplyTo: " +
				                        Info.Name);
			}
			if (Light.cast_shadows)
			{
				Model.Warnings.PushBack("Light shadows are not rendered: " + Info.Name);
			}
			Model.Lights.PushBack(Toolbox::Move(Info));
		}
	}
	for (const ufbx_camera* Camera : Scene.cameras)
	{
		if (Camera->instances.count == 0)
			Model.Warnings.PushBack("Unattached cameras and lights have no world transform and are omitted");
	}
	for (const ufbx_light* Light : Scene.lights)
	{
		if (Light->instances.count == 0)
			Model.Warnings.PushBack("Unattached cameras and lights have no world transform and are omitted");
	}
	if (!Model.Cameras.IsEmpty())
	{
		Model.Warnings.PushBack("Camera projection uses the render target aspect; film offsets, roll properties and "
		                        "look-at targets are not applied");
	}
	if ((!Model.Cameras.IsEmpty() || !Model.Lights.IsEmpty()) && Scene.anim_stacks.count > 0)
	{
		Model.Warnings.PushBack(
		    "File cameras and lights expose static transforms and properties; their animation is not applied");
	}
	return {};
}

// 現在の変換で失われる機能を記録し、姿勢を正しく保持できないスキンは拒否する。
// @param Scene ufbxによる解析結果。
// @param Warnings 部分読み込みの理由を追記する先。
TResult<void> CheckFeatures_Internal(const ufbx_scene& Scene, Toolbox::TVector<Toolbox::FString>& Warnings)
{
	// 複数メッシュに同じ未対応属性があっても、理由ごとに1件へまとめる。
	// 複数の色セットは先頭だけ使うため、省略を通知する。
	bool bExtraColor = false;
	for (const ufbx_mesh* Mesh : Scene.meshes)
	{
		if (Mesh->uv_sets.count > 2)
		{
			return TResult<void>::Failure(EErrorCode::InvalidArgument, "FBX meshes support at most two UV sets");
		}
		bExtraColor = bExtraColor || Mesh->color_sets.count > 1;
		if (Mesh->skin_deformers.count > 1)
		{
			return TResult<void>::Failure(EErrorCode::InvalidArgument,
			                              "FBX meshes with multiple skins are unsupported");
		}
	}
	for (const ufbx_skin_deformer* Skin : Scene.skin_deformers)
	{
		if (Skin->skinning_method != UFBX_SKINNING_METHOD_LINEAR && Skin->skinning_method != UFBX_SKINNING_METHOD_RIGID)
		{
			return TResult<void>::Failure(EErrorCode::InvalidArgument, "FBX dual-quaternion skinning is unsupported");
		}
	}
	for (const ufbx_blend_channel* Channel : Scene.blend_channels)
	{
		if (Channel->keyframes.count > 1 ||
		    (Channel->keyframes.count == 1 && (!(Channel->keyframes.data[0].target_weight > 0) ||
		                                       !Toolbox::IsFinite(Channel->keyframes.data[0].target_weight))))
		{
			return TResult<void>::Failure(EErrorCode::InvalidArgument,
			                              "FBX in-between or non-positive target morphs are unsupported");
		}
	}
	for (const ufbx_blend_deformer* Deformer : Scene.blend_deformers)
	{
		if (Deformer->channels.count == 0)
		{
			Warnings.PushBack("Morph deformer has no channels and is ignored");
		}
	}
	if (bExtraColor)
	{
		Warnings.PushBack("Additional vertex color sets are ignored; only the first color set is imported");
	}
	// UVを保持しても、標準材質はUV0で描く。異なる組の指定を黙って無視しない。
	bool bTextureUvSelection = false;
	for (const ufbx_mesh* Mesh : Scene.meshes)
	{
		for (const ufbx_material* Material : Mesh->materials)
		{
			const ufbx_texture* Texture = BaseTexture_Internal(Material);
			if (Texture != nullptr && Texture->uv_set.length > 0)
			{
				if (IsPbr_Internal(Material))
				{
					bool Found = false;
					for (const auto& Set : Mesh->uv_sets)
						Found = Found || ToString_Internal(Texture->uv_set) == ToString_Internal(Set.name);
					if (!Found)
						return TResult<void>::Failure(EErrorCode::InvalidArgument,
						                              "PBR base texture selects a missing UV set");
					continue;
				}
				bTextureUvSelection =
				    bTextureUvSelection || Mesh->uv_sets.count == 0 ||
				    ToString_Internal(Texture->uv_set) != ToString_Internal(Mesh->uv_sets.data[0].name);
			}
		}
	}
	if (bTextureUvSelection)
	{
		Warnings.PushBack("Texture UV selection is not applied by the standard material; it uses UV0 while preserving "
		                  "UV1 for custom shaders");
	}
	// 基本拡散色以外の材質属性を読み込めたものとして扱わない。
	bool bAdvancedMaterial = false;
	bool bPhong = false;
	for (const ufbx_material* Material : Scene.materials)
	{
		if (IsPbr_Internal(Material))
		{
			// 不正な係数をクランプして成功扱いにしない。
			const auto& Metal = Material->pbr.metalness;
			const auto& Rough = Material->pbr.roughness;
			Toolbox::f64 Color[4] = {0.8, 0.8, 0.8, 1};
			DiffuseColor_Internal(Material, Color);
			if ((Metal.has_value &&
			     (!Toolbox::IsFinite(Metal.value_real) || Metal.value_real < 0 || Metal.value_real > 1)) ||
			    (Rough.has_value &&
			     (!Toolbox::IsFinite(Rough.value_real) || Rough.value_real < 0 || Rough.value_real > 1)))
				return TResult<void>::Failure(EErrorCode::InvalidArgument,
				                              "PBR metallic and roughness must be finite and in 0..1");
			for (Toolbox::f64 Channel : Color)
				if (!Toolbox::IsFinite(Channel) || Channel < 0 || Channel > 1)
					return TResult<void>::Failure(EErrorCode::InvalidArgument,
					                              "PBR base color must be finite and in 0..1");
			Warnings.PushBack("Basic PBR imports base color, base texture, metallic and roughness scalars only; "
			                  "normal/metallic/roughness maps, emission, transparency, coating, transmission, IOR "
			                  "overrides and texture transforms are omitted");
			continue;
		}
		bAdvancedMaterial = bAdvancedMaterial || (Material->shader_type != UFBX_SHADER_FBX_LAMBERT &&
		                                          Material->shader_type != UFBX_SHADER_FBX_PHONG);
		bPhong = bPhong || Material->shader_type == UFBX_SHADER_FBX_PHONG;
	}
	if (bAdvancedMaterial)
	{
		Warnings.PushBack("PBR or unknown material shading is unsupported; only the FBX diffuse color and base texture "
		                  "fallback are imported");
	}
	if (bPhong)
	{
		Warnings.PushBack(
		    "Phong specular, shininess and emission are ignored; only diffuse color and texture are imported");
	}
	if (Scene.constraints.count > 0)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument,
		                              "FBX constraints must be baked into node animation before import");
	}
	// ufbxが補正して読み進めた内容も、呼出し側が確認できるように残す。
	for (const ufbx_warning& Warning : Scene.metadata.warnings)
	{
		Warnings.PushBack(Toolbox::FString("ufbx: ") +
		                  Toolbox::FString(Warning.description.data, Warning.description.length));
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
	// 対応するBlender出力では、Phongへの格納規則をufbxでPBRへ復元する。
	LoadOptions.use_blender_pbr_material = true;
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
		TotalAnimationKeys += (Duration * Options.SamplesPerSecond + 2.0) *
		                      static_cast<Toolbox::f64>(Scene->nodes.count + Scene->blend_channels.count);
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
	auto SceneObjects = ImportSceneObjects_Internal(*Scene, Model);
	if (!SceneObjects)
	{
		return TResult<FImportedModel>::Failure(SceneObjects.Error());
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
	Context.MeshNames.Resize(Scene->elements.count);
	for (const ufbx_node* Node : Scene->nodes)
	{
		if (Node->mesh == nullptr)
			continue;
		const Toolbox::FString Candidate = Context.NodeNames[Node->element_id] + "_mesh";
		const ufbx_string Name{Candidate.CStr(), Candidate.Size()};
		Context.MeshNames[Node->element_id] = MakeIdentifier_Internal("", Name, Node->element_id, Used);
		Used.PushBack(Context.MeshNames[Node->element_id]);
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
	auto Animations = WriteAnimations_Internal(Context, Writer, Options.SamplesPerSecond);
	if (!Animations)
	{
		return TResult<FImportedModel>::Failure(Animations.Error());
	}
	Model.ModelData.PushBack('\0');
	return TResult<FImportedModel>::Success(Toolbox::Move(Model));
}
} // namespace Dxf
