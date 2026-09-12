#pragma once
#include "Dxf/AssetBackend.h"
#include "Dxf/RenderBackend.h"
#include "Dxf/Platform.h"
#include "Dxf/Input.h"
#include "Toolbox/Map.h"
#include "Toolbox/Vector.h"
#include "Toolbox/Function.h"
namespace Dxf::Testing
{
/**
 * 疑似バックエンドの資源状態と呼び出し履歴。
 */
struct FBackendTrace
{
	/**
	 * 次に発行する疑似資源番号。
	 */
	Toolbox::int32 NextHandle = 1;
	/**
	 * 画像読み込みの呼び出し回数。
	 */
	Toolbox::int32 TextureLoads = 0;
	/**
	 * 音声読み込みの呼び出し回数。
	 */
	Toolbox::int32 SoundLoads = 0;
	/**
	 * 音声複製の呼び出し回数。
	 */
	Toolbox::int32 Clones = 0;
	/**
	 * 画面表示の呼び出し回数。
	 */
	Toolbox::int32 Presentations = 0;
	/**
	 * 描画状態リセットの呼び出し回数。
	 */
	Toolbox::int32 Resets = 0;
	/**
	 * 現在の描画先の疑似資源番号。
	 */
	Toolbox::int32 CurrentTarget = -1;
	/**
	 * 画像読み込み失敗を発生させるか。
	 */
	bool bFailTexture = false;
	/**
	 * 不正な画像番号を返すか。
	 */
	bool bInvalidTexture = false;
	/**
	 * 音声読み込み失敗を発生させるか。
	 */
	bool bFailSound = false;
	/**
	 * 再生開始失敗を発生させるか。
	 */
	bool bFailStart = false;
	/**
	 * 描画失敗を発生させるか。
	 */
	bool bFailDraw = false;
	/**
	 * プラットフォーム初期化を失敗させるか。
	 */
	bool bFailPlatform = false;
	/**
	 * 描画先変更を失敗させるか。
	 */
	bool bFailTarget = false;
	/**
	 * 終了を要求したか。
	 */
	bool bQuit = false;
	/**
	 * 入力取得失敗を発生させるか。
	 */
	bool bFailInput = false;
	/**
	 * 生存中の画像資源番号。
	 */
	Toolbox::TSet<Toolbox::int32> Textures;
	/**
	 * 生存中のフォント資源番号。
	 */
	Toolbox::TSet<Toolbox::int32> Fonts;
	/**
	 * 音声資源ごとの再生状態。
	 */
	Toolbox::TMap<Toolbox::int32, bool> Sounds;
	/**
	 * 音声資源ごとの設定音量。
	 */
	Toolbox::TMap<Toolbox::int32, Toolbox::f32> Volumes;
	/**
	 * 解放した画像資源番号の記録。
	 */
	Toolbox::TVector<Toolbox::int32> DeletedTextures;
	/**
	 * 解放した音声資源番号の記録。
	 */
	Toolbox::TVector<Toolbox::int32> DeletedSounds;
	/**
	 * 解放したフォント資源番号の記録。
	 */
	Toolbox::TVector<Toolbox::int32> DeletedFonts;
	/**
	 * 描画に使用した資源番号の記録。
	 */
	Toolbox::TVector<Toolbox::int32> DrawHandles;
	/**
	 * 描画時の不透明度の記録。
	 */
	Toolbox::TVector<Toolbox::f32> Opacities;
	/**
	 * 実行した操作の順序。
	 */
	Toolbox::TVector<Toolbox::FString> Events;
	/**
	 * 描画処理中の再入を再現するコールバック。
	 */
	Toolbox::TFunction<void()> OnDraw;
	/**
	 * 検証で配信する入力状態。
	 */
	FRawInput Input;
};
/**
 * 外部ライブラリを使わず資源・描画・入力を再現する。
 */
class FFakeBackend final : public ITextureBackend,
                           public ISoundBackend,
                           public IFontBackend,
                           public IRenderBackend,
                           public IPlatform,
                           public IInputSource
{
public:
	/**
	 * 操作履歴と資源状態を検証側へ返す。
	 */
	FBackendTrace& GetTrace() noexcept
	{
		return m_Trace;
	}
	const FBackendTrace& GetTrace() const noexcept
	{
		return m_Trace;
	}
	/**
	 * 画像の疑似資源番号を発行し読み込みを記録する。
	 */
	TResult<FTextureAllocation> LoadTexture(const Toolbox::FString&, const FTextureLoadOptions&) override
	{
		++m_Trace.TextureLoads;
		if (m_Trace.bFailTexture)
		{
			return TResult<FTextureAllocation>::Failure(EErrorCode::NotFound, "missing texture");
		}
		if (m_Trace.bInvalidTexture)
		{
			return TResult<FTextureAllocation>::Success({-1, 64, 64});
		}
		// 生存期間や世代を検証する登録ハンドル。
		Toolbox::int32 Handle = m_Trace.NextHandle++;
		m_Trace.Textures.Insert(Handle);
		return TResult<FTextureAllocation>::Success({Handle, 64, 64});
	}
	/**
	 * 指定サイズの描画先を疑似資源として作る。
	 */
	TResult<FTextureAllocation> CreateRenderTarget(Toolbox::int32 Width, Toolbox::int32 Height, bool) override
	{
		// 生存期間や世代を検証する登録ハンドル。
		Toolbox::int32 Handle = m_Trace.NextHandle++;
		m_Trace.Textures.Insert(Handle);
		return TResult<FTextureAllocation>::Success({Handle, Width, Height});
	}
	/**
	 * 画像資源の解放を記録する。
	 */
	void DeleteTexture(Toolbox::int32 Handle) noexcept override
	{
		m_Trace.Textures.Erase(Handle);
		m_Trace.DeletedTextures.PushBack(Handle);
		m_Trace.Events.PushBack("delete-texture");
	}
	/**
	 * 音声資源の読み込みを記録する。
	 */
	TResult<Toolbox::int32> LoadSound(const Toolbox::FString&, const FSoundLoadOptions&) override
	{
		++m_Trace.SoundLoads;
		if (m_Trace.bFailSound)
		{
			return TResult<Toolbox::int32>::Failure(EErrorCode::NotFound, "missing sound");
		}
		// 生存期間や世代を検証する登録ハンドル。
		Toolbox::int32 Handle = m_Trace.NextHandle++;
		m_Trace.Sounds[Handle] = false;
		return TResult<Toolbox::int32>::Success(Handle);
	}
	/**
	 * 複製した音声に別の資源番号を発行する。
	 */
	TResult<Toolbox::int32> DuplicateSound(Toolbox::int32) override
	{
		++m_Trace.Clones;
		// 生存期間や世代を検証する登録ハンドル。
		Toolbox::int32 Handle = m_Trace.NextHandle++;
		m_Trace.Sounds[Handle] = false;
		return TResult<Toolbox::int32>::Success(Handle);
	}
	/**
	 * 再生開始を記録し指定の失敗条件を適用する。
	 */
	TResult<void> StartSound(Toolbox::int32 Handle, bool) override
	{
		if (m_Trace.bFailStart)
		{
			return TResult<void>::Failure(EErrorCode::BackendFailure, "play failed");
		}
		m_Trace.Sounds[Handle] = true;
		return {};
	}
	/**
	 * 音声の再生状態を停止へ変更する。
	 */
	void StopSound(Toolbox::int32 Handle) noexcept override
	{
		m_Trace.Sounds[Handle] = false;
	}
	/**
	 * 音声資源の解放を記録する。
	 */
	void DeleteSound(Toolbox::int32 Handle) noexcept override
	{
		m_Trace.Sounds.Erase(Handle);
		m_Trace.DeletedSounds.PushBack(Handle);
		m_Trace.Events.PushBack("delete-sound");
	}
	/**
	 * 対象音声へ設定された音量を記録する。
	 */
	TResult<void> SetSoundVolume(Toolbox::int32 Handle, Toolbox::f32 Volume) override
	{
		m_Trace.Volumes[Handle] = Volume;
		return {};
	}
	/**
	 * 記録された音声の再生状態を返す。
	 */
	TResult<bool> IsSoundPlaying(Toolbox::int32 Handle) override
	{
		return TResult<bool>::Success(m_Trace.Sounds.At(Handle));
	}
	/**
	 * フォントの疑似資源番号を発行する。
	 */
	TResult<Toolbox::int32> CreateFont(const FFontOptions&) override
	{
		// 生存期間や世代を検証する登録ハンドル。
		Toolbox::int32 Handle = m_Trace.NextHandle++;
		m_Trace.Fonts.Insert(Handle);
		return TResult<Toolbox::int32>::Success(Handle);
	}
	/**
	 * フォント資源の解放を記録する。
	 */
	void DeleteFont(Toolbox::int32 Handle) noexcept override
	{
		m_Trace.Fonts.Erase(Handle);
		m_Trace.DeletedFonts.PushBack(Handle);
	}
	/**
	 * 指定された描画先を記録する。
	 */
	TResult<void> SetTarget(Toolbox::int32 Handle, Toolbox::int32, Toolbox::int32) override
	{
		m_Trace.CurrentTarget = Handle;
		if (m_Trace.bFailTarget)
		{
			return TResult<void>::Failure(EErrorCode::BackendFailure, "target failed after mutation");
		}
		return {};
	}
	/**
	 * 画面消去の呼び出しを記録する。
	 */
	TResult<void> Clear(FColor) override
	{
		m_Trace.Events.PushBack("clear");
		return {};
	}
	/**
	 * 描画状態を既定値へ戻したことを記録する。
	 */
	TResult<void> ResetState(Toolbox::int32, Toolbox::int32) override
	{
		++m_Trace.Resets;
		return {};
	}
	/**
	 * スプライト描画の引数を記録する。
	 */
	TResult<void> DrawSprite(const FSpriteCommand& Command) override
	{
		m_Trace.DrawHandles.PushBack(Command.Texture.GetNativeHandle_Internal());
		m_Trace.Opacities.PushBack(Command.Options.Opacity);
		if (m_Trace.OnDraw)
		{
			m_Trace.OnDraw();
		}
		return m_Trace.bFailDraw ? TResult<void>::Failure(EErrorCode::BackendFailure, "draw failed") : TResult<void>{};
	}
	/**
	 * 文字描画の呼び出しを記録する。
	 */
	TResult<void> DrawText(const FTextCommand&) override
	{
		return {};
	}
	/**
	 * 矩形描画の呼び出しを記録する。
	 */
	TResult<void> DrawRectangle(const FRectangleCommand&) override
	{
		return {};
	}
	/**
	 * 画面表示の回数を記録する。
	 */
	TResult<void> Present() override
	{
		++m_Trace.Presentations;
		m_Trace.Events.PushBack("present");
		return {};
	}
	/**
	 * 起動処理を記録し指定した失敗条件を適用する。
	 */
	TResult<void> Initialize(const FWindowSettings&) override
	{
		m_Trace.Events.PushBack("init");
		return m_Trace.bFailPlatform ? TResult<void>::Failure(EErrorCode::BackendFailure, "init failed")
		                             : TResult<void>{};
	}
	/**
	 * 終了処理を操作履歴へ記録する。
	 */
	void Shutdown() noexcept override
	{
		m_Trace.Events.PushBack("shutdown");
	}
	/**
	 * 検証で指定した終了要求を返す。
	 */
	TResult<bool> PumpEvents() override
	{
		return TResult<bool>::Success(!m_Trace.bQuit);
	}
	/**
	 * 検証で設定した入力または再生状態を返す。
	 */
	TResult<FRawInput> Poll() override
	{
		return m_Trace.bFailInput ? TResult<FRawInput>::Failure(EErrorCode::BackendFailure, "input failed")
		                          : TResult<FRawInput>::Success(m_Trace.Input);
	}

private:
	/**
	 * 疑似操作の履歴と資源状態。
	 */
	FBackendTrace m_Trace;
};
} // namespace Dxf::Testing
