#pragma once
#include "Dxf/Sound.h"
#include "Dxf/SlotMap.h"
namespace Dxf
{
/**
 * 音量・ループ・再生スコープの設定を管理する型。
 */
struct FPlaybackOptions
{
	/**
	 * 繰り返し再生するか。
	 */
	bool bLoop = false;
	/**
	 * 再生音量。
	 */
	Toolbox::f32 Volume = 1.0f;
	/**
	 * 再生音声をまとめる識別番号。
	 */
	Toolbox::uint64 Scope = 0;
};
/**
 * 音声の再生状態を管理する型。
 */
class DPlayback final : public DObject
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Handle ハンドル。
	 * @param Scope 再生音声をまとめる識別番号。
	 */
	DPlayback(FNativeHandle Handle, Toolbox::uint64 Scope) : m_Handle(Toolbox::Move(Handle)), m_Scope(Scope)
	{
	}
	/**
	 * ハンドルを取得する。
	 */
	FORCEINLINE Toolbox::int32 GetHandle_Internal() const noexcept
	{
		return m_Handle.Get();
	}
	/**
	 * 再生音声をまとめる識別番号を取得する。
	 */
	FORCEINLINE Toolbox::uint64 GetScope() const noexcept
	{
		return m_Scope;
	}
	/**
	 * ハンドルを通して参照できる状態かを調べる。
	 */
	bool IsHandleAccessible_Internal() const noexcept override
	{
		return m_Handle.Get() >= 0;
	}

private:
	/**
	 * ハンドル。
	 */
	FNativeHandle m_Handle;
	/**
	 * 再生音声をまとめる識別番号。
	 */
	Toolbox::uint64 m_Scope;
};
/**
 * 再生音声を世代付きで参照するハンドル。
 */
using FPlaybackHandle = TObjectHandle<DPlayback>;
/**
 * 音声の再生と寿命を管理するサービス。
 */
class FAudioPlayer
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Backend ネイティブ処理の呼び出し先。
	 */
	explicit FAudioPlayer(ISoundBackend& Backend) : m_pBackend(&Backend)
	{
	}
	/**
	 * 所有する状態を終了し、必要なリソースを解放する。
	 */
	~FAudioPlayer()
	{
		Shutdown();
	}
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	FAudioPlayer(const FAudioPlayer&) = delete;
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	FAudioPlayer& operator=(const FAudioPlayer&) = delete;
	/**
	 * 指定した音声の再生を開始する。
	 * @param Sound 再生する音声。
	 * @param Options 処理に適用する設定。
	 */
	TResult<FPlaybackHandle> Play(const FSound& Sound, const FPlaybackOptions& Options = {});
	/**
	 * 対象の音声再生を停止する。
	 * @param Handle ハンドル。
	 */
	bool Stop(FPlaybackHandle Handle) noexcept;
	/**
	 * 再生音量を設定する。
	 * @param Handle ハンドル。
	 * @param Volume 再生音量。
	 */
	TResult<void> SetVolume(FPlaybackHandle Handle, Toolbox::f32 Volume);
	/**
	 * 同じスコープに属する再生音声を停止する。
	 * @param Scope 再生音声をまとめる識別番号。
	 */
	void StopScope(Toolbox::uint64 Scope) noexcept;
	/**
	 * 更新対象へフレーム更新を通知する。
	 */
	TResult<void> Tick();
	/**
	 * 管理する処理とリソースを順序どおり終了する。
	 */
	void Shutdown() noexcept;

private:
	/**
	 * 指定した音声の再生を開始する。
	 * @param Sound 再生する音声。
	 * @param Options 処理に適用する設定。
	 */
	TResult<FPlaybackHandle> Play_Internal(const FSound& Sound, const FPlaybackOptions& Options);
	/**
	 * ネイティブ処理の呼び出し先。
	 */
	ISoundBackend* m_pBackend;
	/**
	 * 管理中の再生音声。
	 */
	TSlotMap<DPlayback> m_Playbacks;
	/**
	 * 終了処理が完了しているか。
	 */
	bool m_bShutdown = false;
};
} // namespace Dxf
