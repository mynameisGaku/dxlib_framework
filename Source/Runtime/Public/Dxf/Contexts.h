#pragma once
#include "Dxf/Clock.h"
#include "Dxf/Input.h"
namespace Dxf
{
class FAssetService;
class FAudioPlayer;
class FSceneNavigator;
class DGameInstance;
/** Preparation must not mutate active scene/game state or begin playback. */
struct FInitContext
{
    FAssetService& Assets;
};
struct FTickContext
{
    const FInputSnapshot& Input;
    FFrameTime Time;
    FSceneNavigator* Scenes = nullptr;
    DGameInstance* Game = nullptr;
    FAudioPlayer* Audio = nullptr;
    std::uint64_t AudioScope = 0;
};
struct FSceneActivationContext
{
    FAudioPlayer& Audio;
    FSceneNavigator& Scenes;
    DGameInstance* Game = nullptr;
    std::uint64_t AudioScope = 0;
};
}
