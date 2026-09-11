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
/** Own this aggregate outside (and longer than) FApplication. No Windows headers leak here. */
class FDxLibBackends
{
public:
	FBackendServices GetServices() noexcept
	{
		return {m_Platform, m_Input, m_Textures, m_Sounds, m_Fonts, m_Renderer};
	}
private:
	FDxLibPlatform m_Platform;
	FDxLibInputSource m_Input;
	FDxLibTextureBackend m_Textures;
	FDxLibSoundBackend m_Sounds;
	FDxLibFontBackend m_Fonts;
	FDxLibRenderBackend m_Renderer;
};
}
