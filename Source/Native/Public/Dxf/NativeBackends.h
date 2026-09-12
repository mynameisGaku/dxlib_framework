#pragma once
#include "Dxf/BackendServices.h"
#include "Dxf/DxLibPlatform.h"
#include "Dxf/DxLibInputSource.h"
#include "Dxf/DxLibTextureBackend.h"
#include "Dxf/DxLibSoundBackend.h"
#include "Dxf/DxLibFontBackend.h"
#include "Dxf/DxLibRenderBackend.h"
namespace Dxf
{
/**
 * FApplicationの外側で所有し、アプリケーションより長く存続させる。Windowsヘッダーは公開しない。
 */
class FDxLibBackends
{
public:
	/**
	 * アプリケーションが利用するサービスを取得する。
	 */
	FORCEINLINE FBackendServices GetServices() noexcept
	{
		return {m_Platform, m_Input, m_Textures, m_Sounds, m_Fonts, m_Renderer};
	}

private:
	/**
	 * OSとウィンドウ機能の呼び出し先。
	 */
	FDxLibPlatform m_Platform;
	/**
	 * フレームの入力情報。
	 */
	FDxLibInputSource m_Input;
	/**
	 * 管理するテクスチャ群。
	 */
	FDxLibTextureBackend m_Textures;
	/**
	 * 管理する音声群。
	 */
	FDxLibSoundBackend m_Sounds;
	/**
	 * 管理するフォント群。
	 */
	FDxLibFontBackend m_Fonts;
	/**
	 * 描画を統括するサービス。
	 */
	FDxLibRenderBackend m_Renderer;
};
} // namespace Dxf
