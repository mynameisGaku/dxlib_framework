// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_TEST_SCENE_TASK_BACKEND_H
#define DXF_TEST_SCENE_TASK_BACKEND_H
#include "Dxf/BackendServices.h"
namespace Dxf::Testing
{
/**
 * Sceneの終了境界で観測する外部バックエンドの呼び出し回数。
 */
struct FSceneTaskBackendTrace
{
	/**
	 * プラットフォームの終了回数。
	 */
	Toolbox::uint32 Shutdowns = 0;
	/**
	 * 描画開始時の消去回数。
	 */
	Toolbox::uint32 Clears = 0;
	/**
	 * 画面への提示回数。
	 */
	Toolbox::uint32 Presents = 0;
};
/**
 * OS・入力・資源・描画の外部境界だけを置き換える。Runtimeは本体を使う。
 */
class FSceneTaskBackend final : public IPlatform, public IInputSource, public ITextureBackend,
                                public ISoundBackend, public IFontBackend, public IRenderBackend
{
public:
	/**
	 * Applicationより長く生存する観測先を借用する。
	 * @param Trace 呼び出し回数の記録先。
	 */
	explicit FSceneTaskBackend(FSceneTaskBackendTrace& Trace) noexcept : m_pTrace(&Trace)
	{
	}
	/**
	 * 同じ検証用Backendを各インターフェースへ接続する。
	 */
	FBackendServices Services() noexcept
	{
		return {*this, *this, *this, *this, *this, *this};
	}
	/**
	 * 外部ウィンドウを作らずに起動する。
	 */
	TResult<void> Initialize(const FWindowSettings&) override
	{
		return {};
	}
	/**
	 * プラットフォームの終了を記録する。
	 */
	void Shutdown() noexcept override
	{
		++m_pTrace->Shutdowns;
	}
	/**
	 * OSからの終了要求がないフレームを返す。
	 */
	TResult<bool> PumpEvents() override
	{
		return TResult<bool>::Success(true);
	}
	/**
	 * 操作されていない入力を返す。
	 */
	TResult<FRawInput> Poll() override
	{
		return TResult<FRawInput>::Success({});
	}
	/**
	 * この検証で使わないテクスチャ生成を拒否する。
	 */
	TResult<FTextureAllocation> LoadTexture(const Toolbox::FString&, const FTextureLoadOptions&) override
	{
		return Unsupported_Internal<FTextureAllocation>();
	}
	/**
	 * この検証で使わないメモリからのテクスチャ生成を拒否する。
	 */
	TResult<FTextureAllocation> LoadTextureMemory(const void*, Toolbox::size_t, const FTextureLoadOptions&) override
	{
		return Unsupported_Internal<FTextureAllocation>();
	}
	/**
	 * この検証で使わない描画先生成を拒否する。
	 */
	TResult<FTextureAllocation> CreateRenderTarget(Toolbox::int32, Toolbox::int32, bool) override
	{
		return Unsupported_Internal<FTextureAllocation>();
	}
	/**
	 * 実資源を所有しないため何もしない。
	 */
	void DeleteTexture(Toolbox::int32) noexcept override
	{
	}
	/**
	 * この検証で使わない音声生成を拒否する。
	 */
	TResult<Toolbox::int32> LoadSound(const Toolbox::FString&, const FSoundLoadOptions&) override
	{
		return Unsupported_Internal<Toolbox::int32>();
	}
	/**
	 * この検証で使わないメモリからの音声生成を拒否する。
	 */
	TResult<Toolbox::int32> LoadSoundMemory(const void*, Toolbox::size_t, const FSoundLoadOptions&) override
	{
		return Unsupported_Internal<Toolbox::int32>();
	}
	/**
	 * この検証で使わない音声複製を拒否する。
	 */
	TResult<Toolbox::int32> DuplicateSound(Toolbox::int32) override
	{
		return Unsupported_Internal<Toolbox::int32>();
	}
	/**
	 * この検証で使わない音声再生を拒否する。
	 */
	TResult<void> StartSound(Toolbox::int32, bool) override
	{
		return Unsupported_Internal<void>();
	}
	/**
	 * 実音声を所有しないため何もしない。
	 */
	void StopSound(Toolbox::int32) noexcept override
	{
	}
	/**
	 * 実音声を所有しないため何もしない。
	 */
	void DeleteSound(Toolbox::int32) noexcept override
	{
	}
	/**
	 * この検証で使わない音量設定を拒否する。
	 */
	TResult<void> SetSoundVolume(Toolbox::int32, Toolbox::f32) override
	{
		return Unsupported_Internal<void>();
	}
	/**
	 * 再生中の音声がないことを返す。
	 */
	TResult<bool> IsSoundPlaying(Toolbox::int32) override
	{
		return TResult<bool>::Success(false);
	}
	/**
	 * この検証で使わないフォント生成を拒否する。
	 */
	TResult<Toolbox::int32> CreateFont(const FFontOptions&) override
	{
		return Unsupported_Internal<Toolbox::int32>();
	}
	/**
	 * 実フォントを所有しないため何もしない。
	 */
	void DeleteFont(Toolbox::int32) noexcept override
	{
	}
	/**
	 * 描画先を受け付ける。
	 */
	TResult<void> SetTarget(Toolbox::int32, Toolbox::int32, Toolbox::int32) override
	{
		return {};
	}
	/**
	 * フレーム開始を記録する。
	 */
	TResult<void> Clear(FColor) override
	{
		++m_pTrace->Clears;
		return {};
	}
	/**
	 * 2D状態の復元を受け付ける。
	 */
	TResult<void> ResetState(Toolbox::int32, Toolbox::int32) override
	{
		return {};
	}
	/**
	 * 本検証では空描画のため、想定外の描画要求を拒否する。
	 */
	TResult<void> DrawSprite(const FSpriteCommand&) override
	{
		return Unsupported_Internal<void>();
	}
	/**
	 * 本検証では空描画のため、想定外の文字描画を拒否する。
	 */
	TResult<void> DrawText(const FTextCommand&) override
	{
		return Unsupported_Internal<void>();
	}
	/**
	 * 本検証では空描画のため、想定外の矩形描画を拒否する。
	 */
	TResult<void> DrawRectangle(const FRectangleCommand&) override
	{
		return Unsupported_Internal<void>();
	}
	/**
	 * フレームの提示を記録する。
	 */
	TResult<void> Present() override
	{
		++m_pTrace->Presents;
		return {};
	}

private:
	/**
	 * 検証範囲外の外部処理を成功に見せない。
	 */
	template <typename T> static TResult<T> Unsupported_Internal()
	{
		return TResult<T>::Failure(EErrorCode::BackendFailure, "Unused scene-task test backend capability");
	}
	/**
	 * 外部呼び出しの記録先。
	 */
	FSceneTaskBackendTrace* m_pTrace;
};
} // namespace Dxf::Testing
#endif
