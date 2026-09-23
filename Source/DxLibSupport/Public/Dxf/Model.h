// SPDX-License-Identifier: NOASSERTION
#pragma once
#include "Dxf/ModelMaterial3D.h"
#include "Dxf/ModelMorphInfo.h"
#include "Dxf/ResourceRegistry.h"
#include "Dxf/Result.h"
#include "Toolbox/Matrix4.h"
#include "Toolbox/String.h"
namespace Dxf
{
/**
 * モデルの読み込み設定を管理する型。
 */
struct FModelLoadOptions
{
	/**
	 * 1ワールド単位を何メートルとして扱うか。0ならファイルの長さの単位をそのまま使う。
	 * 単位の変換はFBXの変換時だけで行い、描画・再生では行わない。
	 */
	double TargetUnitMeters = 0.0;
	/**
	 * アニメーションをサンプリングする1秒あたりのキー数（1～1000）。
	 */
	Toolbox::uint32 SamplesPerSecond = 30;
};
/**
 * アニメーションクリップの情報を管理する型。
 */
struct FModelClipInfo
{
	/**
	 * FBX上のクリップ名（UTF-8）。
	 */
	Toolbox::FString Name;
	/**
	 * クリップの長さ（秒）。
	 */
	double DurationSeconds = 0.0;
	/**
	 * ネイティブ側で計測したクリップの長さ（ネイティブの時間単位）。秒からの変換にだけ使う。
	 */
	double NativeDuration = 0.0;
	/**
	 * モーフ番号ごとの等間隔標本。
	 */
	Toolbox::TVector<Toolbox::TVector<Toolbox::f32>> MorphWeights;
};
/**
 * 共有するモデルデータの情報を管理する型。
 */
struct FModelMetadata
{
	/**
	 * 解決済みの読み込みパス。
	 */
	Toolbox::FString Path;
	/**
	 * FBX上の順序どおりのクリップ。
	 */
	Toolbox::TVector<FModelClipInfo> Clips;
	/**
	 * モーフ番号順の初期値と表示名。
	 */
	Toolbox::TVector<FModelMorphInfo> Morphs;
	/**
	 * 読み込みで省略した機能などの警告。キャッシュから取得しても保持する。
	 */
	Toolbox::TVector<Toolbox::FString> ImportWarnings;
};
/**
 * 読み込んだモデルデータ（インスタンスの複製元）を保持するレコード。
 */
using FModelResource = TResourceRecord<FModelMetadata>;
/**
 * ネイティブ側で現在アタッチしているクリップ。所有スレッドの描画処理だけが読み書きする。
 */
struct FModelNativeAnimationState
{
	/**
	 * アタッチ中のクリップ番号。-1ならなし。
	 */
	Toolbox::int32 Clip = -1;
	/**
	 * ネイティブ側のアタッチ番号。-1ならなし。
	 */
	Toolbox::int32 Attachment = -1;
};
/**
 * 1体分のネイティブモデルの情報を管理する型。
 */
struct FModelInstanceMetadata
{
	/**
	 * 複製元のモデル。インスタンスが生存する間は解放しない。
	 */
	Toolbox::TSharedPtr<FModelResource> Model;
	/**
	 * ネイティブ側のアニメーション状態。描画時に所有スレッドで更新する。
	 */
	mutable FModelNativeAnimationState Animation;
};
/**
 * 1体分のネイティブモデルを保持するレコード。
 */
using FModelInstanceResource = TResourceRecord<FModelInstanceMetadata>;

/**
 * 描画時にバックエンドへ渡す、記録時点のモデルの状態。
 */
struct FModelDraw3D
{
	/**
	 * ワールド変換（列ベクトル形式、平行移動は3・7・11番目）。16バイト境界の型を先頭に置き、詰め物を避ける。
	 */
	Toolbox::FMatrix4 World;
	/**
	 * 描画するインスタンス。記録した描画命令が生存を保証する。
	 */
	const FModelInstanceResource* pInstance = nullptr;
	/**
	 * 適用するクリップ番号。-1なら基本姿勢。
	 */
	Toolbox::int32 Clip = -1;
	/**
	 * クリップのネイティブ時刻。
	 */
	Toolbox::f32 NativeTime = 0.0f;
	/**
	 * 記録時点の基本材質。
	 */
	FModelMaterial3D Material;
	/**
	 * 記録時点の各モーフの変形量。
	 */
	Toolbox::TVector<Toolbox::f32> MorphWeights;
};
/**
 * 読み込んだモデルデータを参照する型。複製しても同じデータを共有する。
 */
class FModel
{
public:
	/**
	 * 無効なモデルを構築する。
	 */
	FModel() = default;
	/**
	 * 読み込み済みのレコードを参照する。
	 */
	explicit FModel(Toolbox::TSharedPtr<FModelResource> Resource) : m_pResource(Toolbox::Move(Resource))
	{
	}
	/**
	 * ネイティブのモデルが有効かを調べる。
	 */
	bool IsValid() const noexcept
	{
		return m_pResource && m_pResource->GetHandle_Internal() >= 0;
	}
	/**
	 * クリップ数を取得する。
	 */
	Toolbox::size_t GetClipCount() const noexcept
	{
		return m_pResource ? m_pResource->GetMetadata().Clips.Size() : 0;
	}
	/**
	 * クリップの情報を取得する。範囲外ならnullptr。
	 * @param Index クリップ番号。
	 */
	const FModelClipInfo* GetClip(Toolbox::size_t Index) const noexcept
	{
		return Index < GetClipCount() ? &m_pResource->GetMetadata().Clips[Index] : nullptr;
	}
	/**
	 * 名前でクリップ番号を探す。見つからなければ-1。
	 * @param Name FBX上のクリップ名（UTF-8）。
	 */
	Toolbox::int32 FindClip(const Toolbox::FString& Name) const noexcept;
	/**
	 * モーフを名前で探す。見つからなければ-1。
	 * @param Name GetMorphで取得できるノード名とチャンネル名。
	 */
	Toolbox::int32 FindMorph(const Toolbox::FString& Name) const noexcept;
	/**
	 * モーフ数を取得する。無効なモデルなら0。
	 */
	FORCEINLINE Toolbox::size_t GetMorphCount() const noexcept
	{
		return m_pResource ? m_pResource->GetMetadata().Morphs.Size() : 0;
	}
	/**
	 * モーフの名前と初期値を取得する。範囲外はnullptr。
	 * @param Index モーフ番号。
	 */
	FORCEINLINE const FModelMorphInfo* GetMorph(Toolbox::size_t Index) const noexcept
	{
		return Index < GetMorphCount() ? &m_pResource->GetMetadata().Morphs[Index] : nullptr;
	}
	/**
	 * 部分読み込みの警告数。無効なモデルでは0。
	 */
	Toolbox::size_t GetImportWarningCount() const noexcept
	{
		return m_pResource ? m_pResource->GetMetadata().ImportWarnings.Size() : 0;
	}
	/**
	 * 警告本文を取得する。範囲外はnullptr。モデルの生存中だけ有効。
	 * @param Index 警告番号。
	 */
	const Toolbox::FString* GetImportWarning(Toolbox::size_t Index) const noexcept
	{
		return Index < GetImportWarningCount() ? &m_pResource->GetMetadata().ImportWarnings[Index] : nullptr;
	}

	/**
	 * 解決済みの読み込みパスを取得する。
	 */
	Toolbox::FString GetPath() const
	{
		return m_pResource ? m_pResource->GetMetadata().Path : Toolbox::FString();
	}
	/**
	 * 共有するレコードを取得する。
	 */
	FORCEINLINE const Toolbox::TSharedPtr<FModelResource>& GetResource_Internal() const noexcept
	{
		return m_pResource;
	}

private:
	/**
	 * 共有するレコード。
	 */
	Toolbox::TSharedPtr<FModelResource> m_pResource;
};

/**
 * 描画する1体分のモデルと、その変換・再生状態を管理する型。
 * 変換と再生状態はインスタンスごとに独立し、ネイティブへは描画時にだけ反映する。
 * 移動のみ可能で、同時に1つのスレッドから操作する。
 */
class FModelInstance
{
public:
	/**
	 * 無効なインスタンスを構築する。
	 */
	FModelInstance() = default;
	/**
	 * 生成済みのレコードを所有する。
	 */
	explicit FModelInstance(Toolbox::TSharedPtr<FModelInstanceResource> Resource) : m_pResource(Toolbox::Move(Resource))
	{
	}
	FModelInstance(const FModelInstance&) = delete;
	FModelInstance& operator=(const FModelInstance&) = delete;
	FModelInstance(FModelInstance&&) noexcept = default;
	FModelInstance& operator=(FModelInstance&&) noexcept = default;
	/**
	 * ネイティブのインスタンスが有効かを調べる。
	 */
	bool IsValid() const noexcept
	{
		return m_pResource && m_pResource->GetHandle_Internal() >= 0;
	}
	/**
	 * 複製元のモデルを取得する。
	 */
	FModel GetModel() const
	{
		return m_pResource ? FModel(m_pResource->GetMetadata().Model) : FModel();
	}
	/**
	 * ワールド変換を設定する。有限値でなければ失敗し、以前の値を保つ。
	 * @param World モデル空間からワールド空間への変換。
	 */
	TResult<void> SetTransform(const Toolbox::FMatrix4& World);
	/**
	 * 不透明な基本材質を設定する。Tint.Aが255以外なら失敗して以前の値を保つ。
	 * @param Material インスタンスごとの色と照明設定。
	 */
	TResult<void> SetMaterial(const FModelMaterial3D& Material);
	/**
	 * モーフの変形量を手動指定する。0〜1の有限値のみ。範囲外なら以前の値を保つ。
	 * @param Index モーフ番号。
	 * @param Weight 0は基本形状、1は目標形状。指定中はクリップの値より優先する。
	 */
	TResult<void> SetMorphWeight(Toolbox::size_t Index, Toolbox::f32 Weight);
	/**
	 * 手動指定を解除し、再生中のクリップまたは初期値へ戻す。
	 * @param Index モーフ番号。範囲外なら失敗する。
	 */
	TResult<void> ResetMorphWeight(Toolbox::size_t Index);
	/**
	 * 現在の時刻と手動指定を反映した変形量。範囲外は0。
	 * @param Index モーフ番号。
	 */
	Toolbox::f32 GetMorphWeight(Toolbox::size_t Index) const noexcept;
	/**
	 * 現在の基本材質を取得する。
	 */
	FORCEINLINE const FModelMaterial3D& GetMaterial() const noexcept
	{
		return m_Material;
	}
	/**
	 * ワールド変換を取得する。
	 */
	FORCEINLINE const Toolbox::FMatrix4& GetTransform() const noexcept
	{
		return m_World;
	}
	/**
	 * クリップを先頭から再生する。
	 * @param Clip クリップ番号。
	 * @param bLoop 終端で先頭へ戻るか。
	 */
	TResult<void> Play(Toolbox::size_t Clip, bool bLoop = true);
	/**
	 * 名前で指定したクリップを先頭から再生する。
	 * @param Name FBX上のクリップ名（UTF-8）。
	 * @param bLoop 終端で先頭へ戻るか。
	 */
	TResult<void> Play(const Toolbox::FString& Name, bool bLoop = true);
	/**
	 * 再生を一時停止する。時刻は保つ。
	 */
	void Pause() noexcept
	{
		m_bPlaying = false;
	}
	/**
	 * 一時停止した再生を再開する。クリップがなければ何もしない。
	 */
	void Resume() noexcept
	{
		m_bPlaying = m_Clip >= 0;
	}
	/**
	 * 再生を止め、クリップを外して基本姿勢へ戻す。
	 */
	void Stop() noexcept
	{
		m_Clip = -1;
		m_Time = 0.0;
		m_bPlaying = false;
	}
	/**
	 * 終端で先頭へ戻るかを設定する。
	 */
	void SetLooping(bool bLoop) noexcept
	{
		m_bLoop = bLoop;
	}
	/**
	 * 再生速度の倍率を設定する。0以上の有限値。
	 * @param Speed 1で等速。
	 */
	TResult<void> SetSpeed(double Speed);
	/**
	 * 再生時刻を設定する。クリップの範囲へ収める（ループ時は折り返す）。
	 * @param Seconds クリップ先頭からの秒数。
	 */
	TResult<void> SetTime(double Seconds);
	/**
	 * 経過時間だけ再生時刻を進める。一時停止中は進めない。
	 * ループしないクリップは終端で止まり、再生中でなくなる。
	 * @param DeltaSeconds 経過秒数（0以上の有限値）。
	 */
	TResult<void> Advance(double DeltaSeconds);
	/**
	 * 再生中のクリップ番号を取得する。なければ-1。
	 */
	FORCEINLINE Toolbox::int32 GetClip() const noexcept
	{
		return m_Clip;
	}
	/**
	 * 再生時刻（秒）を取得する。
	 */
	FORCEINLINE double GetTime() const noexcept
	{
		return m_Time;
	}
	/**
	 * 再生速度の倍率を取得する。
	 */
	FORCEINLINE double GetSpeed() const noexcept
	{
		return m_Speed;
	}
	/**
	 * 再生中かを調べる。
	 */
	FORCEINLINE bool IsPlaying() const noexcept
	{
		return m_bPlaying;
	}
	/**
	 * 終端で先頭へ戻るかを調べる。
	 */
	FORCEINLINE bool IsLooping() const noexcept
	{
		return m_bLoop;
	}
	/**
	 * 描画時に使うネイティブの時刻を求める。クリップがなければ0。
	 */
	float GetNativeTime_Internal() const noexcept;
	/**
	 * 共有するレコードを取得する。
	 */
	FORCEINLINE const Toolbox::TSharedPtr<FModelInstanceResource>& GetResource_Internal() const noexcept
	{
		return m_pResource;
	}

private:
	/**
	 * 再生中のクリップの長さ（秒）。クリップがなければ0。
	 */
	double ClipDuration_Internal() const noexcept;
	/**
	 * 所有するレコード。
	 */
	Toolbox::TSharedPtr<FModelInstanceResource> m_pResource;
	/**
	 * ワールド変換。
	 */
	Toolbox::FMatrix4 m_World;
	/**
	 * インスタンス固有の基本材質。
	 */
	FModelMaterial3D m_Material;
	/**
	 * 手動指定した変形量。-1はクリップまたは初期値を使う。
	 */
	Toolbox::TVector<Toolbox::f32> m_MorphOverrides;
	/**
	 * 再生中のクリップ番号。-1ならなし。
	 */
	Toolbox::int32 m_Clip = -1;
	/**
	 * 再生時刻（秒）。
	 */
	double m_Time = 0.0;
	/**
	 * 再生速度の倍率。
	 */
	double m_Speed = 1.0;
	/**
	 * 再生中か。
	 */
	bool m_bPlaying = false;
	/**
	 * 終端で先頭へ戻るか。
	 */
	bool m_bLoop = true;
};
} // namespace Dxf
