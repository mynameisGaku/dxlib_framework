// SPDX-License-Identifier: NOASSERTION
// DxLibのMV1関数はこのファイルだけで呼ぶ。公式VCパッケージのRelease版はMV1の読込部がFBX SDKを必要とするため、
// DXF_DXLIB_MODELS=0の構成ではMV1関数を参照せず、読み込みと描画を失敗として返す。
#include "Dxf/DxLibModelBackend.h"
#include "Dxf/DxLibRenderBackend.h"
#include "NativeApi.h"
#include "Toolbox/Log.h"
#include "Toolbox/Platform.h"
#ifndef DXF_DXLIB_MODELS
#error "DXF_DXLIB_MODELS must be defined by FindDxLib (1: models available, 0: official package only)"
#endif
#if defined(DXF_DXLIB_MODEL_EXTENSION) && DXF_DXLIB_MODEL_EXTENSION >= 1
extern "C" const Dxf::FImportedModel* DxfSetModelExtension(const Dxf::FImportedModel* Model);
#endif
namespace Dxf
{
namespace
{
// テクスチャ1枚として受け付ける最大バイト数。
constexpr Toolbox::size_t MaxTextureFileBytes = 256u * 1024u * 1024u;

// モデル機能のない構成の失敗。
TResult<void> Unavailable_Internal()
{
	return TResult<void>::Failure(
	    EErrorCode::BackendFailure,
	    "Model support is unavailable in this DxLib build (run Setup.cmd to build DxLib from source)");
}

#if DXF_DXLIB_MODELS
/**
 * MV1LoadModelFromMemの読み込み中だけ有効な、テクスチャの供給元。
 */
struct FTextureSource_Internal
{
	const FImportedModel* pModel = nullptr;
	const Toolbox::FString* pDirectory = nullptr;
	/**
	 * DxLibへ渡した画像データ。読み込みが終わるまで保持する。
	 */
	Toolbox::TVector<Toolbox::TVector<Toolbox::uint8>> Loaded;
	/**
	 * 見つからなかったテクスチャの数。
	 */
	Toolbox::uint32 Missing = 0;
	/**
	 * 最初に見つからなかった参照名。ログに使う。
	 */
	Toolbox::FString FirstMissing;
};

// DxLibがテクスチャを求めたときに呼ばれる。変換結果の参照名から埋め込みデータか外部ファイルを返す。
// @param FilePath モデルデータ内の参照名（ディレクトリが付く場合がある）。
// @param FileImageAddr 画像データの格納先。
// @param FileSize 画像データの長さの格納先。
// @param Context 供給元。
int ReadTexture_Internal(const char* FilePath, void** FileImageAddr, int* FileSize, void* Context)
{
	auto* Source = static_cast<FTextureSource_Internal*>(Context);
	// 参照名のファイル名部分。
	const char* Name = FilePath;
	for (const char* Cursor = FilePath; *Cursor != 0; ++Cursor)
	{
		if (*Cursor == '/' || *Cursor == '\\')
		{
			Name = Cursor + 1;
		}
	}
	for (const FImportedModelTexture& Texture : Source->pModel->Textures)
	{
		if (!(Texture.Name == Toolbox::FString(Name)))
		{
			continue;
		}
		try
		{
			Toolbox::TVector<Toolbox::uint8> Bytes;
			if (!Texture.Embedded.IsEmpty())
			{
				Bytes = Texture.Embedded;
			}
			else
			{
				// 相対パスはモデルのディレクトリから、見つからなければFBXに記録された絶対パスを試す。
				bool bFound =
				    !Texture.RelativePath.IsEmpty() &&
				    Toolbox::ReadFileBytes(Toolbox::FPath(*Source->pDirectory) / Toolbox::FPath(Texture.RelativePath),
				                           Bytes, MaxTextureFileBytes);
				if (!bFound && !Texture.AbsolutePath.IsEmpty())
				{
					bFound = Toolbox::ReadFileBytes(Toolbox::FPath(Texture.AbsolutePath), Bytes, MaxTextureFileBytes);
				}
				if (!bFound)
				{
					Bytes.Clear();
				}
			}
			if (!Bytes.IsEmpty() &&
			    Bytes.Size() <= static_cast<Toolbox::size_t>(Toolbox::TNumericLimits<Toolbox::int32>::Max()))
			{
				Source->Loaded.PushBack(Toolbox::Move(Bytes));
				*FileImageAddr = Source->Loaded[Source->Loaded.Size() - 1].Data();
				*FileSize = static_cast<int>(Source->Loaded[Source->Loaded.Size() - 1].Size());
				return 0;
			}
		}
		catch (...)
		{
		}
		// モデルが参照するテクスチャを読めなかった。DxLibは代わりの画像で描く。
		if (Source->Missing++ == 0)
		{
			Source->FirstMissing = Toolbox::FString(FilePath);
		}
		return -1;
	}
	// 変換結果にない名前（DxLibが任意で探す「名前_a」のアルファ画像など）は、存在しないものとして返す。
	return -1;
}

// 画像データは供給元が読み込み完了まで保持するため、ここでは何もしない。
int ReleaseTexture_Internal(void*, void*)
{
	return 0;
}

// フレームワークの列ベクトル形式の行列を、DxLibの行ベクトル形式へ変換する。
DxLib::MATRIX NativeMatrix_Internal(const Toolbox::FMatrix4& Value)
{
	DxLib::MATRIX Result;
	for (int Row = 0; Row < 4; ++Row)
	{
		for (int Column = 0; Column < 4; ++Column)
		{
			Result.m[Row][Column] = Value.Values[Column * 4 + Row];
		}
	}
	return Result;
}
#endif
} // namespace

// リンクしたDxLibでモデルを扱えるかを調べる。
bool FDxLibModelBackend::IsAvailable() noexcept
{
	return DXF_DXLIB_MODELS != 0;
}

// 変換済みのモデルデータからネイティブモデルを作る。
// @param Model 変換済みのモデルデータ。
// @param Directory 外部テクスチャを探すディレクトリ。
TResult<FModelAllocation> FDxLibModelBackend::LoadModel(const FImportedModel& Model, const Toolbox::FString& Directory)
{
#if DXF_DXLIB_MODELS
	// 末尾の終端文字はDxLibへ渡さない。
	if (Model.ModelData.Size() < 2 ||
	    Model.ModelData.Size() - 1 > static_cast<Toolbox::size_t>(Toolbox::TNumericLimits<Toolbox::int32>::Max()))
	{
		return TResult<FModelAllocation>::Failure(EErrorCode::InvalidArgument,
		                                          "Converted model data is empty or too large");
	}
#if !defined(DXF_DXLIB_MODEL_EXTENSION) || DXF_DXLIB_MODEL_EXTENSION < 2
	if (!Model.Morphs.IsEmpty())
	{
		return TResult<FModelAllocation>::Failure(EErrorCode::BackendFailure,
		                                          "Morph targets require an updated source DxLib build; run Setup.cmd");
	}
#endif
	FTextureSource_Internal Source;
	Source.pModel = &Model;
	Source.pDirectory = &Directory;
#if defined(DXF_DXLIB_MODEL_EXTENSION) && DXF_DXLIB_MODEL_EXTENSION >= 1
	// 拡張属性はこの同期呼出しの間だけ参照する。
	if (DxLib::GetUseASyncLoadFlag())
	{
		return TResult<FModelAllocation>::Failure(EErrorCode::InvalidState,
		                                          "Model import requires synchronous native loading");
	}
	const FImportedModel* PreviousExtension = DxfSetModelExtension(&Model);
#else
	if (!Model.MeshExtensions.IsEmpty())
	{
		return TResult<FModelAllocation>::Failure(
		    EErrorCode::BackendFailure,
		    "Additional model attributes require an updated source DxLib build; run Setup.cmd");
	}
#endif
	const Toolbox::int32 Handle =
	    DxLib::MV1LoadModelFromMem(Model.ModelData.Data(), static_cast<int>(Model.ModelData.Size() - 1),
	                               &ReadTexture_Internal, &ReleaseTexture_Internal, &Source);
#if defined(DXF_DXLIB_MODEL_EXTENSION) && DXF_DXLIB_MODEL_EXTENSION >= 1
	DxfSetModelExtension(PreviousExtension);
#endif
	if (Handle < 0)
	{
		return TResult<FModelAllocation>::Failure(EErrorCode::BackendFailure, "MV1LoadModelFromMem failed");
	}
	// DxLibは頂点色を既定で無効にするため、色を持つフレームだけ明示的に有効にする。
	for (const Toolbox::FString& Name : Model.VertexColorFrames)
	{
		const Toolbox::int32 Frame = DxLib::MV1SearchFrame(Handle, Name.CStr());
		const Toolbox::int32 MeshCount = Frame < 0 ? -1 : DxLib::MV1GetFrameMeshNum(Handle, Frame);
		if (MeshCount <= 0)
		{
			DxLib::MV1DeleteModel(Handle);
			return TResult<FModelAllocation>::Failure(EErrorCode::BackendFailure,
			                                          "Vertex color frame was lost during native import");
		}
		for (Toolbox::int32 Index = 0; Index < MeshCount; ++Index)
		{
			const Toolbox::int32 Mesh = DxLib::MV1GetFrameMesh(Handle, Frame, Index);
			if (Mesh < 0 || DxLib::MV1SetMeshUseVertDifColor(Handle, Mesh, TRUE) < 0)
			{
				DxLib::MV1DeleteModel(Handle);
				return TResult<FModelAllocation>::Failure(EErrorCode::BackendFailure,
				                                          "Native vertex color setup failed");
			}
		}
	}
	if (DxLib::MV1GetShapeNum(Handle) != static_cast<Toolbox::int32>(Model.Morphs.Size()))
	{
		DxLib::MV1DeleteModel(Handle);
		return TResult<FModelAllocation>::Failure(EErrorCode::BackendFailure,
		                                          "Native morph count differs from imported data");
	}
	if (Source.Missing > 0)
	{
		DXF_LOG_WARNING("Model", "%u texture(s) could not be read from %s (first: %s)", Source.Missing,
		                Directory.CStr(), Source.FirstMissing.CStr());
	}
	FModelAllocation Allocation;
	Allocation.NativeHandle = Handle;
	// クリップの順序と数は変換結果と一致する。長さはネイティブの時間単位で計測し、秒からの変換に使う。
	if (DxLib::MV1GetAnimNum(Handle) != static_cast<int>(Model.Clips.Size()))
	{
		DxLib::MV1DeleteModel(Handle);
		return TResult<FModelAllocation>::Failure(EErrorCode::BackendFailure,
		                                          "Model clip count differs from the converted data");
	}
	for (Toolbox::size_t Index = 0; Index < Model.Clips.Size(); ++Index)
	{
		Allocation.NativeClipDurations.PushBack(DxLib::MV1GetAnimTotalTime(Handle, static_cast<int>(Index)));
	}
	return TResult<FModelAllocation>::Success(Toolbox::Move(Allocation));
#else
	(void)Model;
	(void)Directory;
	return TResult<FModelAllocation>::Failure(Unavailable_Internal().Error());
#endif
}

// 同じモデルデータを共有するネイティブモデルを作る。
// @param Handle 複製元のハンドル。
TResult<Toolbox::int32> FDxLibModelBackend::DuplicateModel(Toolbox::int32 Handle)
{
#if DXF_DXLIB_MODELS
	const Toolbox::int32 Duplicate = DxLib::MV1DuplicateModel(Handle);
	if (Duplicate < 0)
	{
		return TResult<Toolbox::int32>::Failure(EErrorCode::BackendFailure, "MV1DuplicateModel failed");
	}
	return TResult<Toolbox::int32>::Success(Duplicate);
#else
	(void)Handle;
	return TResult<Toolbox::int32>::Failure(Unavailable_Internal().Error());
#endif
}

// ネイティブモデルを解放する。
// @param Handle ハンドル。
void FDxLibModelBackend::DeleteModel(Toolbox::int32 Handle) noexcept
{
#if DXF_DXLIB_MODELS
	DxLib::MV1DeleteModel(Handle);
#else
	(void)Handle;
#endif
}

// バックエンドを直接使った場合も、開始済みのビューを残さない。
FDxLibRenderBackend::~FDxLibRenderBackend()
{
	try
	{
		EndView3D();
	}
	catch (...)
	{
		RestoreModelLights_Internal();
	}
}

// 1ビューで1回だけ確保し、モデル間では同じライトを使う。
TResult<void> FDxLibRenderBackend::PrepareModelLight_Internal()
{
#if DXF_DXLIB_MODELS
	if (m_ModelLight >= 0)
	{
		return {};
	}
	// 外部の光は一時的に無効にする。色・方向・減衰には触れない。
	m_DefaultLightEnabled = DxLib::GetLightEnable();
	const Toolbox::int32 Count = DxLib::GetEnableLightHandleNum();
	if (m_DefaultLightEnabled < 0 || Count < 0)
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "Model light state query failed");
	}
	m_ExternalLights.Clear();
	for (Toolbox::int32 Index = 0; Index < Count; ++Index)
	{
		const Toolbox::int32 Handle = DxLib::GetEnableLightHandle(Index);
		if (Handle < 0)
		{
			return TResult<void>::Failure(EErrorCode::BackendFailure, "External light query failed");
		}
		m_ExternalLights.PushBack(Handle);
	}
	m_bSavedLights = true;
	if (DxLib::SetLightEnable(FALSE) < 0)
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "Default light isolation failed");
	}
	for (Toolbox::int32 Handle : m_ExternalLights)
	{
		if (DxLib::SetLightEnableHandle(Handle, FALSE) < 0)
		{
			return TResult<void>::Failure(EErrorCode::BackendFailure, "External light isolation failed");
		}
	}
	// ビューは検証済み。指向性光が無効でも環境光は残す。
	const auto Direction = Toolbox::Normalize(m_ModelView.bModelLightOverride ? m_ModelView.ModelLightDirection
	                                                                          : m_ModelView.LightDirection);
	const auto& Position = m_ModelView.ModelLightPosition;
	const auto& Attenuation = m_ModelView.ModelLightAttenuation;
	const EModelLightType Type =
	    m_ModelView.bModelLightOverride ? m_ModelView.ModelLightType : EModelLightType::Directional;
	if (Type == EModelLightType::Point)
	{
		m_ModelLight =
		    DxLib::CreatePointLightHandle(DxLib::VGet(Position.X, Position.Y, Position.Z), m_ModelView.ModelLightRange,
		                                  Attenuation.X, Attenuation.Y, Attenuation.Z);
	}
	else if (Type == EModelLightType::Spot)
	{
		m_ModelLight = DxLib::CreateSpotLightHandle(
		    DxLib::VGet(Position.X, Position.Y, Position.Z), DxLib::VGet(Direction.X, Direction.Y, Direction.Z),
		    m_ModelView.ModelLightOuterAngle, m_ModelView.ModelLightInnerAngle, m_ModelView.ModelLightRange,
		    Attenuation.X, Attenuation.Y, Attenuation.Z);
	}
	else
	{
		m_ModelLight = DxLib::CreateDirLightHandle(DxLib::VGet(Direction.X, Direction.Y, Direction.Z));
	}
	if (m_ModelLight < 0)
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "Model light creation failed");
	}
	const FColor Diffuse = m_ModelView.bLightEnabled ? m_ModelView.LightColor : FColor{0, 0, 0, 255};
	const FColor Ambient = m_ModelView.AmbientColor;
	// 浮動小数点で保持し、ファイル由来の1を超える強度を8ビットへ切り詰めない。
	const auto Radiance = m_ModelView.bModelLightOverride
	                          ? m_ModelView.ModelLightRadiance
	                          : Toolbox::FVector3{Diffuse.R / 255.0f, Diffuse.G / 255.0f, Diffuse.B / 255.0f};
	if (DxLib::SetLightDifColorHandle(m_ModelLight, DxLib::COLOR_F{Radiance.X, Radiance.Y, Radiance.Z, 1.0f}) < 0 ||
	    DxLib::SetLightAmbColorHandle(
	        m_ModelLight, DxLib::COLOR_F{Ambient.R / 255.0f, Ambient.G / 255.0f, Ambient.B / 255.0f, 1.0f}) < 0 ||
	    DxLib::SetLightSpcColorHandle(m_ModelLight, DxLib::COLOR_F{0, 0, 0, 1}) < 0 ||
	    DxLib::SetLightEnableHandle(m_ModelLight, TRUE) < 0)
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "Model light setup failed");
	}
	return {};
#else
	return Unavailable_Internal();
#endif
}

// 失敗時も残りの復元を試し、別の描画へ専用ライトを漏らさない。
bool FDxLibRenderBackend::RestoreModelLights_Internal() noexcept
{
	bool Success = true;
#if DXF_DXLIB_MODELS
	if (m_ModelLight >= 0)
	{
		Success = DxLib::DeleteLightHandle(m_ModelLight) >= 0;
		m_ModelLight = -1;
	}
	if (m_bSavedLights)
	{
		if (DxLib::SetLightEnable(m_DefaultLightEnabled) < 0)
		{
			Success = false;
		}
		for (Toolbox::int32 Handle : m_ExternalLights)
		{
			if (DxLib::SetLightEnableHandle(Handle, TRUE) < 0)
			{
				Success = false;
			}
		}
		m_bSavedLights = false;
		m_ExternalLights.Clear();
	}
#endif
	return Success;
}

// リンクしたDxLibでモデルを扱える構成か。
bool FDxLibRenderBackend::SupportsModels3D() const noexcept
{
	return FDxLibModelBackend::IsAvailable();
}

// 不透明なモデルを1体描画する。クリップの切替と時刻・変換の反映は描画直前に所有スレッドで行う。
// @param Model 記録時点の変換と再生状態。
TResult<void> FDxLibRenderBackend::DrawModel3D(const FModelDraw3D& Model)
{
#if DXF_DXLIB_MODELS
	if (!m_bView3D)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "No active 3D view");
	}
	if (Model.pInstance == nullptr || Model.pInstance->GetHandle_Internal() < 0)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Model instance was released before drawing");
	}
	const Toolbox::int32 Handle = Model.pInstance->GetHandle_Internal();
	for (Toolbox::size_t Index = 0; Index < Model.MorphWeights.Size(); ++Index)
	{
		if (DxLib::MV1SetShapeRate(Handle, static_cast<Toolbox::int32>(Index), Model.MorphWeights[Index],
		                           DX_MV1_SHAPERATE_OVERWRITE) < 0)
		{
			return TResult<void>::Failure(EErrorCode::BackendFailure, "Native morph weight setup failed");
		}
	}
	FModelNativeAnimationState& Animation = Model.pInstance->GetMetadata().Animation;
	if (Animation.Clip != Model.Clip)
	{
		if (Animation.Attachment >= 0 && DxLib::MV1DetachAnim(Handle, Animation.Attachment) < 0)
		{
			return TResult<void>::Failure(EErrorCode::BackendFailure, "MV1DetachAnim failed");
		}
		Animation = {};
		if (Model.Clip >= 0)
		{
			const Toolbox::int32 Attachment = DxLib::MV1AttachAnim(Handle, Model.Clip, -1, FALSE);
			if (Attachment < 0)
			{
				return TResult<void>::Failure(EErrorCode::BackendFailure, "MV1AttachAnim failed");
			}
			Animation = {Model.Clip, Attachment};
		}
	}
	if (Animation.Attachment >= 0 && DxLib::MV1SetAttachAnimTime(Handle, Animation.Attachment, Model.NativeTime) < 0)
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "MV1SetAttachAnimTime failed");
	}
	// 基本材質とビューの調査設定から、モデル固有のGPU照明を選ぶ。
	const bool Lit = Model.Material.bLit && m_ModelView.Debug.Lighting == ELightingMode3D::Normal;
	if (Lit)
	{
		auto Light = PrepareModelLight_Internal();
		if (!Light)
		{
			return Light;
		}
	}
	const FColor Tint =
	    m_ModelView.Debug.Lighting == ELightingMode3D::LightsOff ? FColor{0, 0, 0, 255} : Model.Material.Tint;
	const DxLib::COLOR_F Scale{Tint.R / 255.0f, Tint.G / 255.0f, Tint.B / 255.0f, 1.0f};
	// 不透明として深度を検査・書込みする。色倍率は共有データではなくインスタンスへ適用する。
	if (DxLib::MV1SetDifColorScale(Handle, Scale) < 0 || DxLib::MV1SetAmbColorScale(Handle, Scale) < 0 ||
	    DxLib::MV1SetMatrix(Handle, NativeMatrix_Internal(Model.World)) < 0 || DxLib::SetUseZBuffer3D(TRUE) < 0 ||
	    DxLib::SetWriteZBuffer3D(TRUE) < 0 || DxLib::SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 255) < 0)
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "Model draw state failed");
	}
	// この呼出しの後は、成功・失敗にかかわらずCPU照明済み基本形状用の状態へ戻す。
	const Toolbox::int32 LightingResult = DxLib::SetUseLighting(Lit ? TRUE : FALSE);
	const Toolbox::int32 DrawResult = LightingResult < 0 ? -1 : DxLib::MV1DrawModel(Handle);
	const Toolbox::int32 RestoreResult = DxLib::SetUseLighting(FALSE);
	return Detail::CheckNative_Internal(DrawResult < 0 || RestoreResult < 0 ? -1 : 0, "Model lighting or draw failed");
#else
	(void)Model;
	return Unavailable_Internal();
#endif
}
} // namespace Dxf
