// SPDX-License-Identifier: NOASSERTION
#include "Dxf/Model.h"
#include <math.h>
namespace Dxf
{
namespace
{
// インスタンスが操作できない状態の失敗。
TResult<void> InvalidInstance_Internal()
{
	return TResult<void>::Failure(EErrorCode::InvalidState, "Model instance is not valid");
}
} // namespace

// 名前でクリップ番号を探す。見つからなければ-1。
// @param Name FBX上のクリップ名（UTF-8）。
Toolbox::int32 FModel::FindClip(const Toolbox::FString& Name) const noexcept
{
	for (Toolbox::size_t Index = 0; Index < GetClipCount(); ++Index)
	{
		if (m_pResource->GetMetadata().Clips[Index].Name == Name)
		{
			return static_cast<Toolbox::int32>(Index);
		}
	}
	return -1;
}

// 名前に一致する最初のモーフ番号を返す。
Toolbox::int32 FModel::FindMorph(const Toolbox::FString& Name) const noexcept
{
	for (Toolbox::size_t Index = 0; Index < GetMorphCount(); ++Index)
	{
		if (GetMorph(Index)->Name == Name)
		{
			return static_cast<Toolbox::int32>(Index);
		}
	}
	return -1;
}

// 指定したインスタンスだけにモーフの上書きを保持する。
TResult<void> FModelInstance::SetMorphWeight(Toolbox::size_t Index, Toolbox::f32 Weight)
{
	if (!IsValid())
		return InvalidInstance_Internal();
	const Toolbox::size_t Count = GetModel().GetMorphCount();
	if (Index >= Count || !(Weight >= 0 && Weight <= 1))
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid morph index or weight");
	}
	if (m_MorphOverrides.Size() != Count)
	{
		m_MorphOverrides.Resize(Count);
		for (auto& Value : m_MorphOverrides)
			Value = -1;
	}
	m_MorphOverrides[Index] = Weight;
	return {};
}

// 上書きを解除する。別のモーフや再生時刻は変えない。
TResult<void> FModelInstance::ResetMorphWeight(Toolbox::size_t Index)
{
	if (!IsValid())
		return InvalidInstance_Internal();
	if (Index >= GetModel().GetMorphCount())
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid morph index");
	}
	if (Index < m_MorphOverrides.Size())
		m_MorphOverrides[Index] = -1;
	return {};
}

// 等間隔の2標本を線形補間する。形状差分そのものは読み込み済みのネイティブ側に保持する。
Toolbox::f32 FModelInstance::GetMorphWeight(Toolbox::size_t Index) const noexcept
{
	const FModel Model = GetModel();
	const FModelMorphInfo* Morph = Model.GetMorph(Index);
	if (Morph == nullptr)
		return 0;
	if (Index < m_MorphOverrides.Size() && m_MorphOverrides[Index] >= 0)
		return m_MorphOverrides[Index];
	const FModelClipInfo* Clip = m_Clip >= 0 ? Model.GetClip(static_cast<Toolbox::size_t>(m_Clip)) : nullptr;
	if (Clip != nullptr && Index < Clip->MorphWeights.Size() && !Clip->MorphWeights[Index].IsEmpty())
	{
		const auto& Values = Clip->MorphWeights[Index];
		const Toolbox::f64 Position = Clip->DurationSeconds > 0
		                                  ? Toolbox::Clamp(m_Time / Clip->DurationSeconds, 0.0, 1.0) *
		                                        static_cast<Toolbox::f64>(Values.Size() - 1)
		                                  : 0;
		const Toolbox::size_t First = static_cast<Toolbox::size_t>(Position);
		const Toolbox::size_t Next = Toolbox::Min(First + 1, Values.Size() - 1);
		const Toolbox::f32 Fraction = static_cast<Toolbox::f32>(Position - static_cast<Toolbox::f64>(First));
		return Values[First] + (Values[Next] - Values[First]) * Fraction;
	}
	return Morph->DefaultWeight;
}

// 不透明な基本材質だけを受け付け、共有モデルには変更を加えない。
TResult<void> FModelInstance::SetMaterial(const FModelMaterial3D& Material)
{
	if (Material.Tint.A != 255 || !Toolbox::IsFinite(Material.Metallic) || !Toolbox::IsFinite(Material.Roughness) ||
	    (Material.Metallic != -1 && (Material.Metallic < 0 || Material.Metallic > 1)) ||
	    (Material.Roughness != -1 && (Material.Roughness < 0 || Material.Roughness > 1)) || Material.BaseColorUv < -1 ||
	    Material.BaseColorUv > 1)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument,
		                              "Model tint must be opaque and PBR factors must be -1 or in 0..1");
	}
	if (Material.BaseColorUv == 1 && (!IsValid() || !m_pResource->GetMetadata().Model->GetMetadata().bAllMeshesHaveUv1))
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "UV1 override requires UV1 on every mesh");
	}
	m_Material = Material;
	return {};
}

// ワールド変換を設定する。有限値でなければ以前の値を保つ。
// @param World モデル空間からワールド空間への変換。
TResult<void> FModelInstance::SetTransform(const Toolbox::FMatrix4& World)
{
	for (const Toolbox::f32 Value : World.Values)
	{
		if (!Toolbox::IsFinite(Value))
		{
			return TResult<void>::Failure(EErrorCode::InvalidArgument, "Model transform must be finite");
		}
	}
	m_World = World;
	return {};
}

// クリップを先頭から再生する。
// @param Clip クリップ番号。
// @param bLoop 終端で先頭へ戻るか。
TResult<void> FModelInstance::Play(Toolbox::size_t Clip, bool bLoop)
{
	if (!IsValid())
	{
		return InvalidInstance_Internal();
	}
	if (Clip >= GetModel().GetClipCount())
	{
		return TResult<void>::Failure(EErrorCode::NotFound, "Model clip index out of range");
	}
	m_Clip = static_cast<Toolbox::int32>(Clip);
	m_Time = 0.0;
	m_bLoop = bLoop;
	m_bPlaying = true;
	return {};
}

// 名前で指定したクリップを先頭から再生する。
// @param Name FBX上のクリップ名（UTF-8）。
// @param bLoop 終端で先頭へ戻るか。
TResult<void> FModelInstance::Play(const Toolbox::FString& Name, bool bLoop)
{
	if (!IsValid())
	{
		return InvalidInstance_Internal();
	}
	// 名前に対応するクリップ番号。
	const Toolbox::int32 Clip = GetModel().FindClip(Name);
	if (Clip < 0)
	{
		return TResult<void>::Failure(EErrorCode::NotFound, "Model clip not found: " + Name);
	}
	return Play(static_cast<Toolbox::size_t>(Clip), bLoop);
}

// 再生速度の倍率を設定する。
// @param Speed 1で等速。
TResult<void> FModelInstance::SetSpeed(double Speed)
{
	if (!Toolbox::IsFinite(Speed) || Speed < 0.0)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument,
		                              "Model playback speed must be finite and nonnegative");
	}
	m_Speed = Speed;
	return {};
}

// 再生時刻を設定する。クリップの範囲へ収める。
// @param Seconds クリップ先頭からの秒数。
TResult<void> FModelInstance::SetTime(double Seconds)
{
	if (!Toolbox::IsFinite(Seconds))
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Model playback time must be finite");
	}
	if (m_Clip < 0)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "No model clip is selected");
	}
	// クリップの長さ（秒）。
	const double Duration = ClipDuration_Internal();
	if (Duration <= 0.0)
	{
		m_Time = 0.0;
	}
	else if (m_bLoop)
	{
		m_Time = fmod(Seconds, Duration);
		m_Time = m_Time < 0.0 ? m_Time + Duration : m_Time;
	}
	else
	{
		m_Time = Seconds < 0.0 ? 0.0 : (Seconds > Duration ? Duration : Seconds);
	}
	return {};
}

// 経過時間だけ再生時刻を進める。
// @param DeltaSeconds 経過秒数。
TResult<void> FModelInstance::Advance(double DeltaSeconds)
{
	if (!Toolbox::IsFinite(DeltaSeconds) || DeltaSeconds < 0.0)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Model time step must be finite and nonnegative");
	}
	if (!m_bPlaying || m_Clip < 0)
	{
		return {};
	}
	// クリップの長さ（秒）。
	const double Duration = ClipDuration_Internal();
	// 進めた後の時刻。
	const double Next = m_Time + DeltaSeconds * m_Speed;
	if (Duration <= 0.0)
	{
		m_Time = 0.0;
	}
	else if (m_bLoop)
	{
		m_Time = fmod(Next, Duration);
	}
	else if (Next >= Duration)
	{
		// ループしないクリップは終端の姿勢で止める。
		m_Time = Duration;
		m_bPlaying = false;
	}
	else
	{
		m_Time = Next;
	}
	return {};
}

// 描画時に使うネイティブの時刻を求める。秒からネイティブの時間単位への変換はここだけで行う。
float FModelInstance::GetNativeTime_Internal() const noexcept
{
	if (m_Clip < 0 || !m_pResource || !m_pResource->GetMetadata().Model)
	{
		return 0.0f;
	}
	// 再生中のクリップの情報。
	const auto& Clips = m_pResource->GetMetadata().Model->GetMetadata().Clips;
	if (static_cast<Toolbox::size_t>(m_Clip) >= Clips.Size() || Clips[m_Clip].DurationSeconds <= 0.0)
	{
		return 0.0f;
	}
	return static_cast<float>(m_Time / Clips[m_Clip].DurationSeconds * Clips[m_Clip].NativeDuration);
}

// 再生中のクリップの長さ（秒）。
double FModelInstance::ClipDuration_Internal() const noexcept
{
	// 複製元のモデル。
	const FModel Model = GetModel();
	const FModelClipInfo* Clip = m_Clip >= 0 ? Model.GetClip(static_cast<Toolbox::size_t>(m_Clip)) : nullptr;
	return Clip != nullptr ? Clip->DurationSeconds : 0.0;
}
} // namespace Dxf
