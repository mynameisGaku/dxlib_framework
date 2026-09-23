// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_FAKE_MODEL_API_H
#define DXF_FAKE_MODEL_API_H
// 手書きの追加テストダブル。MV1関数への変換だけを確かめ、実SDKのABI・モデル読込・実画面は検証しない。
#include "RenderViewsApi.h"
#include "Toolbox/Utility.h"
#include "Toolbox/Vector.h"
namespace DxLib
{
/**
 * DxLibの行ベクトル形式の行列。
 */
// ネイティブ境界で使う正規化された色。
struct COLOR_F
{
	Toolbox::f32 r;
	Toolbox::f32 g;
	Toolbox::f32 b;
	Toolbox::f32 a;
};
struct MATRIX
{
	Toolbox::f32 m[4][4];
};
/**
 * モデル関数の呼び出し記録。
 */
struct FModelTrace
{
	Toolbox::int32 NextHandle = 500;
	Toolbox::int32 AnimCount = 2;
	Toolbox::f32 AnimTotal = 30.0f;
	Toolbox::int32 Loads = 0;
	Toolbox::int32 LoadedBytes = 0;
	Toolbox::int32 TextureReads = 0;
	Toolbox::int32 TextureBytes = 0;
	Toolbox::int32 Duplicates = 0;
	Toolbox::int32 Deleted = 0;
	Toolbox::int32 Attaches = 0;
	Toolbox::int32 Detaches = 0;
	Toolbox::int32 LastAttachedClip = -1;
	Toolbox::f32 LastTime = -1.0f;
	Toolbox::int32 Draws = 0;
	// ライトの独立性と解放・失敗経路を検査する状態。
	Toolbox::int32 DefaultLight = 1;
	Toolbox::int32 ExternalLight = 1;
	Toolbox::int32 OwnedLight = 0;
	Toolbox::int32 CreatedLights = 0;
	Toolbox::int32 DeletedLights = 0;
	bool bFailLightCreate = false;
	bool bFailLightSetup = false;
	COLOR_F Tint{1, 1, 1, 1};
	Toolbox::TVector<COLOR_F> DrawTints;
	Toolbox::TVector<Toolbox::int32> DrawLighting;
	Toolbox::TVector<Toolbox::int32> DrawForeignLights;
	// 頂点色の設定回数と、検索・設定の失敗を再現する指定。
	Toolbox::int32 ColorMeshes = 0;
	bool bFailColorFrame = false;
	bool bFailColorSetup = false;
	Toolbox::int32 LightingAtDraw = -1;
	Toolbox::int32 DepthAtDraw = -1;
	MATRIX LastMatrix{};
	bool bFailLoad = false;
	bool bFailDraw = false;
	/**
	 * 読み込み時にテクスチャを要求する参照名。空なら要求しない。
	 */
	const char* RequestTexture = nullptr;
};
inline FModelTrace ModelTrace;
/**
 * メモリ上のモデルの読み込みを再現する。指定があればテクスチャの読み込みを1回要求する。
 */
inline Toolbox::int32 MV1LoadModelFromMem(const void*, int FileSize,
                                          int (*FileReadFunc)(const char*, void**, int*, void*),
                                          int (*FileReleaseFunc)(void*, void*), void* FileReadFuncData)
{
	++ModelTrace.Loads;
	ModelTrace.LoadedBytes = FileSize;
	if (ModelTrace.RequestTexture != nullptr)
	{
		void* Image = nullptr;
		int Size = 0;
		if (FileReadFunc(ModelTrace.RequestTexture, &Image, &Size, FileReadFuncData) == 0)
		{
			++ModelTrace.TextureReads;
			ModelTrace.TextureBytes = Size;
			FileReleaseFunc(Image, FileReadFuncData);
		}
	}
	return ModelTrace.bFailLoad ? -1 : ModelTrace.NextHandle++;
}
// 頂点色フレームは材質ごとの2メッシュに分かれる。
inline Toolbox::int32 MV1SearchFrame(Toolbox::int32, const char*)
{
	return ModelTrace.bFailColorFrame ? -1 : 1;
}
inline Toolbox::int32 MV1GetFrameMeshNum(Toolbox::int32, Toolbox::int32)
{
	return 2;
}
inline Toolbox::int32 MV1GetFrameMesh(Toolbox::int32, Toolbox::int32, Toolbox::int32 Index)
{
	return Index + 3;
}
inline Toolbox::int32 MV1SetMeshUseVertDifColor(Toolbox::int32, Toolbox::int32, Toolbox::int32)
{
	++ModelTrace.ColorMeshes;
	return ModelTrace.bFailColorSetup ? -1 : 0;
}
// ライト901は外部所有、902はモデル用の一時ライト。
inline Toolbox::int32 GetLightEnable()
{
	return ModelTrace.DefaultLight;
}
inline Toolbox::int32 SetLightEnable(Toolbox::int32 Value)
{
	ModelTrace.DefaultLight = Value;
	return 0;
}
inline Toolbox::int32 GetEnableLightHandleNum()
{
	return ModelTrace.ExternalLight + ModelTrace.OwnedLight;
}
inline Toolbox::int32 GetEnableLightHandle(Toolbox::int32 Index)
{
	return ModelTrace.ExternalLight && Index == 0 ? 901 : 902;
}
inline Toolbox::int32 SetLightEnableHandle(Toolbox::int32 Handle, Toolbox::int32 Value)
{
	(Handle == 901 ? ModelTrace.ExternalLight : ModelTrace.OwnedLight) = Value;
	return 0;
}
inline Toolbox::int32 CreateDirLightHandle(VECTOR)
{
	++ModelTrace.CreatedLights;
	return ModelTrace.bFailLightCreate ? -1 : 902;
}
inline Toolbox::int32 DeleteLightHandle(Toolbox::int32)
{
	++ModelTrace.DeletedLights;
	ModelTrace.OwnedLight = 0;
	return 0;
}
inline Toolbox::int32 SetLightDifColorHandle(Toolbox::int32, COLOR_F)
{
	return ModelTrace.bFailLightSetup ? -1 : 0;
}
inline Toolbox::int32 SetLightAmbColorHandle(Toolbox::int32, COLOR_F)
{
	return 0;
}
inline Toolbox::int32 SetLightSpcColorHandle(Toolbox::int32, COLOR_F)
{
	return 0;
}
inline Toolbox::int32 MV1SetDifColorScale(Toolbox::int32, COLOR_F Color)
{
	ModelTrace.Tint = Color;
	return 0;
}
inline Toolbox::int32 MV1SetAmbColorScale(Toolbox::int32, COLOR_F)
{
	return 0;
}
inline Toolbox::int32 MV1GetAnimNum(Toolbox::int32)
{
	return ModelTrace.AnimCount;
}
inline Toolbox::f32 MV1GetAnimTotalTime(Toolbox::int32, Toolbox::int32)
{
	return ModelTrace.AnimTotal;
}
inline Toolbox::int32 MV1DuplicateModel(Toolbox::int32)
{
	++ModelTrace.Duplicates;
	return ModelTrace.NextHandle++;
}
inline Toolbox::int32 MV1DeleteModel(Toolbox::int32)
{
	++ModelTrace.Deleted;
	return 0;
}
inline Toolbox::int32 MV1AttachAnim(Toolbox::int32, Toolbox::int32 AnimIndex, Toolbox::int32, Toolbox::int32)
{
	++ModelTrace.Attaches;
	ModelTrace.LastAttachedClip = AnimIndex;
	return 0;
}
inline Toolbox::int32 MV1DetachAnim(Toolbox::int32, Toolbox::int32)
{
	++ModelTrace.Detaches;
	return 0;
}
inline Toolbox::int32 MV1SetAttachAnimTime(Toolbox::int32, Toolbox::int32, Toolbox::f32 Time)
{
	ModelTrace.LastTime = Time;
	return 0;
}
inline Toolbox::int32 MV1SetMatrix(Toolbox::int32, MATRIX Matrix)
{
	ModelTrace.LastMatrix = Matrix;
	return 0;
}
inline Toolbox::int32 MV1DrawModel(Toolbox::int32)
{
	++ModelTrace.Draws;
	ModelTrace.DrawTints.PushBack(ModelTrace.Tint);
	ModelTrace.DrawLighting.PushBack(ViewTrace.Lighting);
	ModelTrace.DrawForeignLights.PushBack(ModelTrace.DefaultLight + ModelTrace.ExternalLight);
	ModelTrace.LightingAtDraw = ViewTrace.Lighting;
	ModelTrace.DepthAtDraw = ViewTrace.Z3D * 2 + ViewTrace.WriteZ3D;
	return ModelTrace.bFailDraw ? -1 : 0;
}
} // namespace DxLib
#endif
