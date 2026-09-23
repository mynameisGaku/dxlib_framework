// SPDX-License-Identifier: NOASSERTION
// Autodesk FBX SDKで、試験用の小さなFBXとテクスチャを生成する。第三者のモデルは使用しない。
//   StaticBox.fbx      骨・アニメーションなしの箱。テクスチャ付き。
//   SkinnedColumn.fbx  2本の骨（Root→Bone1）でスキニングした柱。クリップ "Bend" と "Twist"。
//   ModelChecker.bmp   両モデルが相対パスで参照する64×64のチェッカー画像。
// 単位はセンチメートル、Y軸上向きの右手系（FBXの既定）で書き出す。DxLib側の変換は読込試験で確認する。
#include <fbxsdk.h>
#include <direct.h>
#include <stdio.h>
#include <string.h>

namespace
{
// 24bit BMPを書き出す。ModelChecker.bmpの生成専用。
bool WriteCheckerBmp_Internal(const char* Path)
{
	const int Size = 64;
	const int RowBytes = Size * 3;
	const int ImageBytes = RowBytes * Size;
	unsigned char Header[54] = {};
	Header[0] = 'B';
	Header[1] = 'M';
	const int FileSize = 54 + ImageBytes;
	memcpy(Header + 2, &FileSize, 4);
	const int Offset = 54;
	memcpy(Header + 10, &Offset, 4);
	const int InfoSize = 40;
	memcpy(Header + 14, &InfoSize, 4);
	memcpy(Header + 18, &Size, 4);
	memcpy(Header + 22, &Size, 4);
	Header[26] = 1;
	Header[28] = 24;
	memcpy(Header + 34, &ImageBytes, 4);
	FILE* File = nullptr;
	if (fopen_s(&File, Path, "wb") != 0 || File == nullptr)
	{
		return false;
	}
	fwrite(Header, 1, sizeof(Header), File);
	for (int Y = 0; Y < Size; ++Y)
	{
		for (int X = 0; X < Size; ++X)
		{
			// 8ピクセル単位の橙と青緑。上下の区別がつくよう最下段の行だけ白にする。
			const bool bCell = ((X / 8) + (Y / 8)) % 2 == 0;
			unsigned char Pixel[3] = {bCell ? (unsigned char)40 : (unsigned char)200, bCell ? (unsigned char)140 : (unsigned char)200,
			                          bCell ? (unsigned char)255 : (unsigned char)60};
			if (Y < 2)
			{
				Pixel[0] = 255;
				Pixel[1] = 255;
				Pixel[2] = 255;
			}
			fwrite(Pixel, 1, 3, File);
		}
	}
	fclose(File);
	return true;
}

// テクスチャ付きLambert材質を作り、ノードへ取り付ける。
void AttachTexturedMaterial_Internal(FbxScene* Scene, FbxNode* Node, FbxMesh* Mesh)
{
	FbxSurfaceLambert* Material = FbxSurfaceLambert::Create(Scene, "CheckerMaterial");
	Material->Diffuse.Set(FbxDouble3(1.0, 1.0, 1.0));
	FbxFileTexture* Texture = FbxFileTexture::Create(Scene, "CheckerTexture");
	Texture->SetFileName("ModelChecker.bmp");
	Texture->SetRelativeFileName("ModelChecker.bmp");
	Texture->SetTextureUse(FbxTexture::eStandard);
	Texture->SetMappingType(FbxTexture::eUV);
	Texture->SetMaterialUse(FbxFileTexture::eModelMaterial);
	Texture->UVSet.Set("UVMap");
	Material->Diffuse.ConnectSrcObject(Texture);
	Node->AddMaterial(Material);
	FbxGeometryElementMaterial* Element = Mesh->CreateElementMaterial();
	Element->SetMappingMode(FbxGeometryElement::eAllSame);
	Element->SetReferenceMode(FbxGeometryElement::eIndexToDirect);
	Element->GetIndexArray().Add(0);
}

// 幅Width・奥行きWidth・高さHeightの直方体を、高さ方向にSegments分割して作る。
// 各段の4頂点をまとめ、側面・上面・底面を四角形ポリゴンで張る。
FbxMesh* CreateColumnMesh_Internal(FbxScene* Scene, const char* Name, double Width, double Height, int Segments)
{
	FbxMesh* Mesh = FbxMesh::Create(Scene, Name);
	const int Rings = Segments + 1;
	Mesh->InitControlPoints(Rings * 4);
	const double H = Width * 0.5;
	const double Corners[4][2] = {{-H, -H}, {H, -H}, {H, H}, {-H, H}};
	for (int Ring = 0; Ring < Rings; ++Ring)
	{
		const double Y = Height * Ring / Segments;
		for (int Corner = 0; Corner < 4; ++Corner)
		{
			Mesh->SetControlPointAt(FbxVector4(Corners[Corner][0], Y, Corners[Corner][1]), Ring * 4 + Corner);
		}
	}
	FbxGeometryElementUV* UV = Mesh->CreateElementUV("UVMap");
	UV->SetMappingMode(FbxGeometryElement::eByPolygonVertex);
	UV->SetReferenceMode(FbxGeometryElement::eDirect);
	FbxGeometryElementNormal* Normal = Mesh->CreateElementNormal();
	Normal->SetMappingMode(FbxGeometryElement::eByPolygonVertex);
	Normal->SetReferenceMode(FbxGeometryElement::eDirect);
	const FbxVector4 SideNormals[4] = {FbxVector4(0, 0, -1), FbxVector4(1, 0, 0), FbxVector4(0, 0, 1), FbxVector4(-1, 0, 0)};
	// 側面。外向きが反時計回りになる順で頂点を並べる。
	for (int Ring = 0; Ring < Segments; ++Ring)
	{
		for (int Side = 0; Side < 4; ++Side)
		{
			const int A = Ring * 4 + Side;
			const int B = Ring * 4 + (Side + 1) % 4;
			const int Order[4] = {A, A + 4, B + 4, B};
			Mesh->BeginPolygon();
			for (int Index = 0; Index < 4; ++Index)
			{
				Mesh->AddPolygon(Order[Index]);
				const double U = (Index == 2 || Index == 3) ? 1.0 : 0.0;
				const double V = (Index == 1 || Index == 2) ? double(Ring + 1) / Segments : double(Ring) / Segments;
				UV->GetDirectArray().Add(FbxVector2(U, V));
				Normal->GetDirectArray().Add(SideNormals[Side]);
			}
			Mesh->EndPolygon();
		}
	}
	// 底面（下向き）と上面（上向き）。
	const int Bottom[4] = {0, 1, 2, 3};
	const int Top[4] = {Segments * 4 + 3, Segments * 4 + 2, Segments * 4 + 1, Segments * 4 + 0};
	const int* Caps[2] = {Bottom, Top};
	const FbxVector4 CapNormals[2] = {FbxVector4(0, -1, 0), FbxVector4(0, 1, 0)};
	for (int Cap = 0; Cap < 2; ++Cap)
	{
		Mesh->BeginPolygon();
		for (int Index = 0; Index < 4; ++Index)
		{
			Mesh->AddPolygon(Caps[Cap][Index]);
			UV->GetDirectArray().Add(FbxVector2((Index == 1 || Index == 2) ? 1.0 : 0.0, (Index >= 2) ? 1.0 : 0.0));
			Normal->GetDirectArray().Add(CapNormals[Cap]);
		}
		Mesh->EndPolygon();
	}
	return Mesh;
}

// Bone1のZ回転（曲げ）またはY回転（ねじり）のキーを1秒のクリップとして作る。
void CreateClip_Internal(FbxScene* Scene, FbxNode* Bone, const char* Name, const char* Channel, const double* Values, int Count)
{
	FbxAnimStack* Stack = FbxAnimStack::Create(Scene, Name);
	FbxAnimLayer* Layer = FbxAnimLayer::Create(Scene, "BaseLayer");
	Stack->AddMember(Layer);
	FbxAnimCurve* Curve = Bone->LclRotation.GetCurve(Layer, Channel, true);
	Curve->KeyModifyBegin();
	for (int Index = 0; Index < Count; ++Index)
	{
		FbxTime Time;
		Time.SetSecondDouble(double(Index) / (Count - 1));
		const int Key = Curve->KeyAdd(Time);
		Curve->KeySetValue(Key, float(Values[Index]));
		Curve->KeySetInterpolation(Key, FbxAnimCurveDef::eInterpolationLinear);
	}
	Curve->KeyModifyEnd();
	FbxTime Stop;
	Stop.SetSecondDouble(1.0);
	Stack->LocalStart = FbxTime(0);
	Stack->LocalStop = Stop;
}

bool Export_Internal(FbxManager* Manager, FbxScene* Scene, const char* Path)
{
	// 文書情報のURLはファイル名だけにする。既定では書き出し時の絶対パス（利用者名を含む）が埋め込まれる。
	FbxDocumentInfo* Info = FbxDocumentInfo::Create(Manager, "SceneInfo");
	Info->mTitle = Path;
	Info->Url.Set(FbxString(Path));
	Info->Original_FileName.Set(FbxString(Path));
	Info->LastSavedUrl.Set(FbxString(Path));
	Scene->SetSceneInfo(Info);
	FbxExporter* Exporter = FbxExporter::Create(Manager, "");
	// 既定のバイナリ形式で書き出す。
	const int Format = Manager->GetIOPluginRegistry()->GetNativeWriterFormat();
	if (!Exporter->Initialize(Path, Format, Manager->GetIOSettings()))
	{
		printf("export init failed: %s: %s\n", Path, Exporter->GetStatus().GetErrorString());
		Exporter->Destroy();
		return false;
	}
	const bool bResult = Exporter->Export(Scene);
	Exporter->Destroy();
	printf("%s %s\n", bResult ? "wrote" : "FAILED", Path);
	return bResult;
}

FbxScene* NewScene_Internal(FbxManager* Manager, const char* Name)
{
	FbxScene* Scene = FbxScene::Create(Manager, Name);
	FbxAxisSystem::MayaYUp.ConvertScene(Scene);
	FbxSystemUnit::cm.ConvertScene(Scene);
	return Scene;
}
} // namespace

int main(int ArgumentCount, char** Arguments)
{
	if (ArgumentCount < 2)
	{
		printf("usage: FbxSampleGen <output directory>\n");
		return 2;
	}
	// テクスチャの相対パスは現在のディレクトリを基準に計算されるため、出力先へ移動してから書き出す。
	// こうすると FileName・RelativeFilename の両方が "ModelChecker.bmp" になり、利用者の絶対パスを埋め込まない。
	if (_chdir(Arguments[1]) != 0)
	{
		printf("cannot enter output directory: %s\n", Arguments[1]);
		return 2;
	}
	char Path[1024];
	FbxManager* Manager = FbxManager::Create();
	Manager->SetIOSettings(FbxIOSettings::Create(Manager, IOSROOT));
	bool bOk = true;

	snprintf(Path, sizeof(Path), "ModelChecker.bmp");
	bOk = WriteCheckerBmp_Internal(Path) && bOk;
	printf("%s %s\n", bOk ? "wrote" : "FAILED", Path);

	// 静的な箱。
	{
		FbxScene* Scene = NewScene_Internal(Manager, "StaticBox");
		FbxNode* Node = FbxNode::Create(Scene, "Box");
		FbxMesh* Mesh = CreateColumnMesh_Internal(Scene, "BoxMesh", 100.0, 100.0, 1);
		Node->SetNodeAttribute(Mesh);
		AttachTexturedMaterial_Internal(Scene, Node, Mesh);
		Scene->GetRootNode()->AddChild(Node);
		snprintf(Path, sizeof(Path), "StaticBox.fbx");
		bOk = Export_Internal(Manager, Scene, Path) && bOk;
		Scene->Destroy();
	}
	// 2本の骨でスキニングした柱と2クリップ。
	{
		FbxScene* Scene = NewScene_Internal(Manager, "SkinnedColumn");
		FbxNode* MeshNode = FbxNode::Create(Scene, "Column");
		FbxMesh* Mesh = CreateColumnMesh_Internal(Scene, "ColumnMesh", 40.0, 200.0, 4);
		MeshNode->SetNodeAttribute(Mesh);
		AttachTexturedMaterial_Internal(Scene, MeshNode, Mesh);
		FbxSkeleton* RootAttribute = FbxSkeleton::Create(Scene, "RootSkeleton");
		RootAttribute->SetSkeletonType(FbxSkeleton::eRoot);
		FbxNode* Root = FbxNode::Create(Scene, "Root");
		Root->SetNodeAttribute(RootAttribute);
		FbxSkeleton* BoneAttribute = FbxSkeleton::Create(Scene, "Bone1Skeleton");
		BoneAttribute->SetSkeletonType(FbxSkeleton::eLimbNode);
		FbxNode* Bone = FbxNode::Create(Scene, "Bone1");
		Bone->SetNodeAttribute(BoneAttribute);
		Bone->LclTranslation.Set(FbxDouble3(0.0, 100.0, 0.0));
		Root->AddChild(Bone);
		Scene->GetRootNode()->AddChild(MeshNode);
		Scene->GetRootNode()->AddChild(Root);
		// 下半分（y<100）はRoot、上半分はBone1。中央の段は両方へ半分ずつ。
		FbxSkin* Skin = FbxSkin::Create(Scene, "ColumnSkin");
		FbxCluster* Clusters[2] = {FbxCluster::Create(Scene, "RootCluster"), FbxCluster::Create(Scene, "Bone1Cluster")};
		FbxNode* Links[2] = {Root, Bone};
		for (int Index = 0; Index < 2; ++Index)
		{
			Clusters[Index]->SetLink(Links[Index]);
			Clusters[Index]->SetLinkMode(FbxCluster::eNormalize);
		}
		for (int Point = 0; Point < Mesh->GetControlPointsCount(); ++Point)
		{
			const int Ring = Point / 4;
			if (Ring < 2)
			{
				Clusters[0]->AddControlPointIndex(Point, 1.0);
			}
			else if (Ring == 2)
			{
				Clusters[0]->AddControlPointIndex(Point, 0.5);
				Clusters[1]->AddControlPointIndex(Point, 0.5);
			}
			else
			{
				Clusters[1]->AddControlPointIndex(Point, 1.0);
			}
		}
		FbxAMatrix MeshMatrix = MeshNode->EvaluateGlobalTransform();
		for (int Index = 0; Index < 2; ++Index)
		{
			Clusters[Index]->SetTransformMatrix(MeshMatrix);
			Clusters[Index]->SetTransformLinkMatrix(Links[Index]->EvaluateGlobalTransform());
			Skin->AddCluster(Clusters[Index]);
		}
		Mesh->AddDeformer(Skin);
		// バインドポーズ。
		FbxPose* Pose = FbxPose::Create(Scene, "BindPose");
		Pose->SetIsBindPose(true);
		Pose->Add(MeshNode, FbxMatrix(MeshNode->EvaluateGlobalTransform()));
		Pose->Add(Root, FbxMatrix(Root->EvaluateGlobalTransform()));
		Pose->Add(Bone, FbxMatrix(Bone->EvaluateGlobalTransform()));
		Scene->AddPose(Pose);
		const double Bend[3] = {0.0, 60.0, 0.0};
		const double Twist[2] = {0.0, 90.0};
		CreateClip_Internal(Scene, Bone, "Bend", FBXSDK_CURVENODE_COMPONENT_Z, Bend, 3);
		CreateClip_Internal(Scene, Bone, "Twist", FBXSDK_CURVENODE_COMPONENT_Y, Twist, 2);
		snprintf(Path, sizeof(Path), "SkinnedColumn.fbx");
		bOk = Export_Internal(Manager, Scene, Path) && bOk;
		Scene->Destroy();
	}
	Manager->Destroy();
	return bOk ? 0 : 1;
}
