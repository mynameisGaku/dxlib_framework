#pragma once
#include "Dxf/AssetBackend.h"
#include "Dxf/Input.h"
#include "Dxf/Platform.h"
#include "Dxf/RenderBackend.h"
namespace Dxf
{
/** Composition-root dependencies. All backends must outlive the application. */
struct FBackendServices
{
    IPlatform& Platform;
    IInputSource& Input;
    ITextureBackend& Textures;
    ISoundBackend& Sounds;
    IFontBackend& Fonts;
    IRenderBackend& Renderer;
};
}
