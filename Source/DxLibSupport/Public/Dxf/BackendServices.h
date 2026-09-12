#pragma once
#include "Dxf/AssetBackend.h"
#include "Dxf/Input.h"
#include "Dxf/Platform.h"
#include "Dxf/RenderBackend.h"
namespace Dxf
{
/**
 * 構築時に渡す依存サービス。各バックエンドはアプリケーションより長く存続させる。
 */
struct FBackendServices
{
	/**
	 * OSとウィンドウ機能の呼び出し先。
	 */
	IPlatform& Platform;
	/**
	 * フレームの入力情報。
	 */
	IInputSource& Input;
	/**
	 * 管理するテクスチャ群。
	 */
	ITextureBackend& Textures;
	/**
	 * 管理する音声群。
	 */
	ISoundBackend& Sounds;
	/**
	 * 管理するフォント群。
	 */
	IFontBackend& Fonts;
	/**
	 * 描画を統括するサービス。
	 */
	IRenderBackend& Renderer;
};
} // namespace Dxf
