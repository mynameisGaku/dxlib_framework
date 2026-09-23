// SPDX-License-Identifier: NOASSERTION
// FBX読込の最小実SDK試験。.fbxを事前変換せずに読み、
// メッシュ・骨階層・クリップ・アニメーションの効果・描画・解放と再読込・日本語パス・異常ファイルを確認する。
// DXF_PROBE_UFBX=1では、ufbxで変換したデータをMV1LoadModelFromMemへ渡す（FBX SDK不要）。
// それ以外では、FBX対応でビルドしたDxLibのMV1LoadModelへ.fbxを直接渡す。
// 使い方: FbxModelProbe.exe <Assets/Modelsの絶対パス> <出力ディレクトリ>
// 全項目成功で終了コード0。各項目の結果を標準出力とLog.txtへ残す。
#include "DxLib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#if DXF_PROBE_UFBX
#include "Dxf/ModelImport.h"
#endif

namespace
{
int g_Failures = 0;

#if DXF_PROBE_UFBX
// 最後に変換したモデル。クリップ名から番号を引くのに使う。
Dxf::FImportedModel g_LastImport;
char g_LastDirectory[1024];

// UTF-8のパスのファイル全体を読む。失敗時はnullptr。
char* ReadFile_Internal(const char* Path, size_t* pSize)
{
	wchar_t Wide[1024];
	if (MultiByteToWideChar(CP_UTF8, 0, Path, -1, Wide, 1024) == 0)
	{
		return nullptr;
	}
	FILE* File = nullptr;
	if (_wfopen_s(&File, Wide, L"rb") != 0 || File == nullptr)
	{
		return nullptr;
	}
	fseek(File, 0, SEEK_END);
	const long Size = ftell(File);
	fseek(File, 0, SEEK_SET);
	char* Data = Size > 0 ? static_cast<char*>(malloc(static_cast<size_t>(Size))) : nullptr;
	if (Data != nullptr && fread(Data, 1, static_cast<size_t>(Size), File) != static_cast<size_t>(Size))
	{
		free(Data);
		Data = nullptr;
	}
	fclose(File);
	*pSize = static_cast<size_t>(Size);
	return Data;
}

// MV1LoadModelFromMemがテクスチャを求めたときに呼ばれる。変換結果の参照名から実ファイルか埋め込みデータを返す。
int ReadTexture_Internal(const TCHAR* FilePath, void** FileImageAddr, int* FileSize, void*)
{
	const char* Base = FilePath;
	for (const char* Cursor = FilePath; *Cursor != 0; ++Cursor)
	{
		if (*Cursor == '/' || *Cursor == '\\')
		{
			Base = Cursor + 1;
		}
	}
	for (const Dxf::FImportedModelTexture& Texture : g_LastImport.Textures)
	{
		if (strcmp(Texture.Name.CStr(), Base) != 0)
		{
			continue;
		}
		if (!Texture.Embedded.IsEmpty())
		{
			void* Copy = malloc(Texture.Embedded.Size());
			memcpy(Copy, Texture.Embedded.Data(), Texture.Embedded.Size());
			*FileImageAddr = Copy;
			*FileSize = static_cast<int>(Texture.Embedded.Size());
			return 0;
		}
		char Path[2048];
		snprintf(Path, sizeof(Path), "%s/%s", g_LastDirectory, Texture.RelativePath.CStr());
		size_t Size = 0;
		char* Data = ReadFile_Internal(Path, &Size);
		if (Data == nullptr && !Texture.AbsolutePath.IsEmpty())
		{
			Data = ReadFile_Internal(Texture.AbsolutePath.CStr(), &Size);
		}
		if (Data == nullptr)
		{
			return -1;
		}
		*FileImageAddr = Data;
		*FileSize = static_cast<int>(Size);
		return 0;
	}
	return -1;
}

int ReleaseTexture_Internal(void* MemoryAddr, void*)
{
	free(MemoryAddr);
	return 0;
}
#endif

// .fbxを読み込む。構成により、ufbx変換またはFBX対応DxLibの直接読込を使う。
int LoadModel_Internal(const char* Path)
{
#if DXF_PROBE_UFBX
	size_t Size = 0;
	char* Data = ReadFile_Internal(Path, &Size);
	if (Data == nullptr)
	{
		return -1;
	}
	auto Imported = Dxf::ImportFbxModel(Data, Size);
	free(Data);
	if (!Imported)
	{
		printf("  import error: %s\n", Imported.Error().Message.CStr());
		return -1;
	}
	g_LastImport = Toolbox::Move(Imported).Value();
	snprintf(g_LastDirectory, sizeof(g_LastDirectory), "%s", Path);
	char* Slash = strrchr(g_LastDirectory, '/');
	if (Slash != nullptr)
	{
		*Slash = 0;
	}
	// 末尾の終端文字はDxLibへ渡さない。
	return MV1LoadModelFromMem(g_LastImport.ModelData.Data(), static_cast<int>(g_LastImport.ModelData.Size() - 1), ReadTexture_Internal,
	                           ReleaseTexture_Internal, nullptr);
#else
	return MV1LoadModel(Path);
#endif
}

// クリップ名から番号を返す。ufbx変換では変換結果の順序がネイティブの番号と一致する。
int ClipIndex_Internal(int Model, const char* Name)
{
#if DXF_PROBE_UFBX
	(void)Model;
	for (size_t Index = 0; Index < g_LastImport.Clips.Size(); ++Index)
	{
		if (strcmp(g_LastImport.Clips[Index].Name.CStr(), Name) == 0)
		{
			return static_cast<int>(Index);
		}
	}
	return -1;
#else
	return MV1GetAnimIndex(Model, Name);
#endif
}

// 1項目の結果を記録する。
void Check_Internal(bool bOk, const char* Name, const char* Detail = "")
{
	// 詳細がない項目では末尾に空白を出さない。
	if (Detail[0] != 0)
	{
		printf("%s %s %s\n", bOk ? "PASS" : "FAIL", Name, Detail);
	}
	else
	{
		printf("%s %s\n", bOk ? "PASS" : "FAIL", Name);
	}
	fflush(stdout);
	if (!bOk)
	{
		++g_Failures;
	}
}

// 名前付きフレームの親の名前を返す。見つからなければ空。
const char* ParentName_Internal(int Model, int Frame)
{
	const int Parent = MV1GetFrameParent(Model, Frame);
	return Parent >= 0 ? MV1GetFrameName(Model, Parent) : "";
}

// 2つの行列がほぼ等しいか。
bool NearlyEqual_Internal(const MATRIX& A, const MATRIX& B)
{
	for (int Row = 0; Row < 4; ++Row)
	{
		for (int Column = 0; Column < 4; ++Column)
		{
			const float Delta = A.m[Row][Column] - B.m[Row][Column];
			if (Delta > 1e-3f || Delta < -1e-3f)
			{
				return false;
			}
		}
	}
	return true;
}

// 1フレーム描画してPNGへ保存する。
void DrawAndSave_Internal(int Model, const char* OutDir, const char* Name)
{
	ClearDrawScreen();
	SetCameraNearFar(1.0f, 2000.0f);
	SetCameraPositionAndTarget_UpVecY(VGet(0.0f, 150.0f, -450.0f), VGet(0.0f, 100.0f, 0.0f));
	MV1DrawModel(Model);
	char Path[1024];
	snprintf(Path, sizeof(Path), "%s/%s.png", OutDir, Name);
	SaveDrawScreenToPNG(0, 0, 640, 480, Path);
	ScreenFlip();
}

// 静的モデル: メッシュ・テクスチャ・描画・解放と再読込。
void ProbeStatic_Internal(const char* Models, const char* OutDir)
{
	char Path[1024];
	snprintf(Path, sizeof(Path), "%s/StaticBox.fbx", Models);
	for (int Round = 0; Round < 2; ++Round)
	{
		const int Model = LoadModel_Internal(Path);
		Check_Internal(Model >= 0, Round == 0 ? "static load" : "static reload after delete", Path);
		if (Model < 0)
		{
			return;
		}
		char Detail[256];
		snprintf(Detail, sizeof(Detail), "meshes=%d textures=%d materials=%d", MV1GetMeshNum(Model), MV1GetTextureNum(Model),
		         MV1GetMaterialNum(Model));
		Check_Internal(MV1GetMeshNum(Model) >= 1 && MV1GetTextureNum(Model) >= 1 && MV1GetMaterialNum(Model) >= 1,
		               "static mesh texture material", Detail);
		const int Texture = MV1GetTextureGraphHandle(Model, 0);
		int TextureWidth = 0;
		int TextureHeight = 0;
		GetGraphSize(Texture, &TextureWidth, &TextureHeight);
		snprintf(Detail, sizeof(Detail), "handle=%d name=%s size=%dx%d", Texture, MV1GetTextureName(Model, 0), TextureWidth,
		         TextureHeight);
		// 生成したModelChecker.bmpは64×64。既定の白画像などで代用されていないことを寸法で確かめる。
		Check_Internal(Texture >= 0 && TextureWidth == 64 && TextureHeight == 64, "static texture image loaded", Detail);
		Check_Internal(MV1GetAnimNum(Model) == 0, "static has no animation");
		if (Round == 0)
		{
			DrawAndSave_Internal(Model, OutDir, "static-box");
			// 描画直後の裏画面を読み戻し、チェッカーの色（灰色でない画素）が箱の上に出ているかを確かめる。
			ClearDrawScreen();
			SetCameraNearFar(1.0f, 2000.0f);
			SetCameraPositionAndTarget_UpVecY(VGet(0.0f, 150.0f, -450.0f), VGet(0.0f, 100.0f, 0.0f));
			MV1DrawModel(Model);
			int Colored = 0;
			for (int Y = 200; Y < 360; Y += 4)
			{
				for (int X = 240; X < 400; X += 4)
				{
					int Red = 0;
					int Green = 0;
					int Blue = 0;
					GetColor2(GetPixel(X, Y), &Red, &Green, &Blue);
					int High = Red > Green ? Red : Green;
					High = High > Blue ? High : Blue;
					int Low = Red < Green ? Red : Green;
					Low = Low < Blue ? Low : Blue;
					// 灰色は各成分がほぼ等しい。成分差の大きい画素をテクスチャ由来の色として数える。
					if (High - Low > 60)
					{
						++Colored;
					}
				}
			}
			ScreenFlip();
			snprintf(Detail, sizeof(Detail), "colored samples=%d", Colored);
			Check_Internal(Colored > 20, "static texture visible in the rendered frame", Detail);
		}
		Check_Internal(MV1DeleteModel(Model) == 0, "static delete");
	}
}

// 骨付きモデル: 骨階層・クリップ・時間更新による姿勢変化・複製の独立性。
void ProbeSkinned_Internal(const char* Models, const char* OutDir)
{
	char Path[1024];
	snprintf(Path, sizeof(Path), "%s/SkinnedColumn.fbx", Models);
	const int Model = LoadModel_Internal(Path);
	Check_Internal(Model >= 0, "skinned load", Path);
	if (Model < 0)
	{
		return;
	}
	char Detail[512];
	const int Root = MV1SearchFrame(Model, "Root");
	const int Bone = MV1SearchFrame(Model, "Bone1");
	snprintf(Detail, sizeof(Detail), "frames=%d meshes=%d root=%d bone1=%d bone1.parent=%s", MV1GetFrameNum(Model),
	         MV1GetMeshNum(Model), Root, Bone, Bone >= 0 ? ParentName_Internal(Model, Bone) : "-");
	Check_Internal(Root >= 0 && Bone >= 0 && MV1GetFrameParent(Model, Bone) == Root, "skeleton hierarchy Root->Bone1", Detail);
	Check_Internal(MV1GetMeshNum(Model) >= 1, "skinned mesh exists");
	const int Clips = MV1GetAnimNum(Model);
	const int Bend = ClipIndex_Internal(Model, "Bend");
	const int Twist = ClipIndex_Internal(Model, "Twist");
	snprintf(Detail, sizeof(Detail), "clips=%d bend=%d twist=%d", Clips, Bend, Twist);
	Check_Internal(Clips >= 2 && Bend >= 0 && Twist >= 0, "animation clips by name", Detail);
	if (Bend < 0 || Bone < 0)
	{
		MV1DeleteModel(Model);
		return;
	}
	const float Total = MV1GetAnimTotalTime(Model, Bend);
	snprintf(Detail, sizeof(Detail), "total=%f (native time units)", Total);
	Check_Internal(Total > 0.0f, "clip length positive", Detail);
	const int Attach = MV1AttachAnim(Model, Bend);
	Check_Internal(Attach >= 0, "attach Bend");
	MV1SetAttachAnimTime(Model, Attach, 0.0f);
	const MATRIX AtStart = MV1GetFrameLocalWorldMatrix(Model, Bone);
	MV1SetAttachAnimTime(Model, Attach, Total * 0.5f);
	const MATRIX AtMiddle = MV1GetFrameLocalWorldMatrix(Model, Bone);
	Check_Internal(!NearlyEqual_Internal(AtStart, AtMiddle), "animation time changes Bone1 pose");
	DrawAndSave_Internal(Model, OutDir, "skinned-bend-middle");
	MV1SetAttachAnimTime(Model, Attach, 0.0f);
	DrawAndSave_Internal(Model, OutDir, "skinned-bend-start");
	// 複製は同じ基礎データを共有しつつ、再生時刻を独立に持つ。
	const int Copy = MV1DuplicateModel(Model);
	Check_Internal(Copy >= 0, "duplicate model");
	if (Copy >= 0)
	{
		const int CopyAttach = MV1AttachAnim(Copy, Bend);
		MV1SetAttachAnimTime(Copy, CopyAttach, Total * 0.5f);
		MV1SetAttachAnimTime(Model, Attach, 0.0f);
		const MATRIX Original = MV1GetFrameLocalWorldMatrix(Model, Bone);
		const MATRIX Duplicate = MV1GetFrameLocalWorldMatrix(Copy, Bone);
		Check_Internal(NearlyEqual_Internal(Original, AtStart) && NearlyEqual_Internal(Duplicate, AtMiddle),
		               "duplicate keeps an independent animation time");
		Check_Internal(MV1DeleteModel(Copy) == 0, "delete duplicate");
		Check_Internal(NearlyEqual_Internal(MV1GetFrameLocalWorldMatrix(Model, Bone), AtStart),
		               "original survives duplicate deletion");
	}
	Check_Internal(MV1DetachAnim(Model, Attach) == 0, "detach animation");
	Check_Internal(MV1DeleteModel(Model) == 0, "skinned delete");
}

// 日本語を含むパスと、不在・破損ファイルの扱い。
void ProbePaths_Internal(const char* Models, const char* OutDir)
{
	char Source[1024];
	char Directory[1024];
	char Target[1024];
	snprintf(Source, sizeof(Source), "%s/StaticBox.fbx", Models);
	// UTF-8の日本語ディレクトリへ、モデルとテクスチャを複製する。
	snprintf(Directory, sizeof(Directory), "%s/日本語パス", OutDir);
	wchar_t WideDirectory[1024];
	MultiByteToWideChar(CP_UTF8, 0, Directory, -1, WideDirectory, 1024);
	CreateDirectoryW(WideDirectory, nullptr);
	const char* Files[2] = {"StaticBox.fbx", "ModelChecker.bmp"};
	for (int Index = 0; Index < 2; ++Index)
	{
		char From[1024];
		char To[1024];
		wchar_t WideFrom[1024];
		wchar_t WideTo[1024];
		snprintf(From, sizeof(From), "%s/%s", Models, Files[Index]);
		snprintf(To, sizeof(To), "%s/%s", Directory, Index == 0 ? "箱モデル.fbx" : Files[Index]);
		MultiByteToWideChar(CP_UTF8, 0, From, -1, WideFrom, 1024);
		MultiByteToWideChar(CP_UTF8, 0, To, -1, WideTo, 1024);
		CopyFileW(WideFrom, WideTo, FALSE);
	}
	snprintf(Target, sizeof(Target), "%s/箱モデル.fbx", Directory);
	const int Japanese = LoadModel_Internal(Target);
	Check_Internal(Japanese >= 0 && MV1GetMeshNum(Japanese) >= 1, "load from a Japanese path", Target);
	if (Japanese >= 0)
	{
		MV1DeleteModel(Japanese);
	}
	snprintf(Target, sizeof(Target), "%s/DoesNotExist.fbx", Models);
	Check_Internal(LoadModel_Internal(Target) == -1, "missing file returns -1");
	// 先頭だけFBXらしい破損ファイル。
	snprintf(Target, sizeof(Target), "%s/Corrupt.fbx", OutDir);
	FILE* File = nullptr;
	if (fopen_s(&File, Target, "wb") == 0 && File != nullptr)
	{
		const char Garbage[] = "Kaydara FBX Binary  \0\x1a\0 broken payload that is not a valid node record";
		fwrite(Garbage, 1, sizeof(Garbage), File);
		fclose(File);
	}
	Check_Internal(LoadModel_Internal(Target) == -1, "corrupt file returns -1 without crashing");
}
} // namespace

int main(int ArgumentCount, char** Arguments)
{
	if (ArgumentCount < 3)
	{
		printf("usage: FbxModelProbe <Assets/Models directory> <output directory>\n");
		return 2;
	}
	printf("DxLib build: %s\n", DXF_PROBE_LIBRARIES);
	printf("DXF_DXLIB_HAS_FBX=%d\n", DXF_DXLIB_HAS_FBX);
	printf("loader: %s\n", DXF_PROBE_UFBX ? "ufbx -> MV1LoadModelFromMem" : "MV1LoadModel (DxLib FBX loader)");
	SetUseCharCodeFormat(DX_CHARCODEFORMAT_UTF8);
	ChangeWindowMode(TRUE);
	SetGraphMode(640, 480, 32);
	SetOutApplicationLogValidFlag(TRUE);
	if (DxLib_Init() != 0)
	{
		printf("FAIL DxLib_Init\n");
		return 1;
	}
	SetDrawScreen(DX_SCREEN_BACK);
	SetUseZBuffer3D(TRUE);
	SetWriteZBuffer3D(TRUE);
	ProbeStatic_Internal(Arguments[1], Arguments[2]);
	ProbeSkinned_Internal(Arguments[1], Arguments[2]);
	ProbePaths_Internal(Arguments[1], Arguments[2]);
	DxLib_End();
	printf("RESULT %s failures=%d\n", g_Failures == 0 ? "PASS" : "FAIL", g_Failures);
	fflush(stdout);
	return g_Failures == 0 ? 0 : 1;
}
