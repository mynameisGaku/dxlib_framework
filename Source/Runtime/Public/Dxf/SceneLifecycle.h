#pragma once
#include "Dxf/Scene.h"
#include "Dxf/AudioPlayer.h"
namespace Dxf
{
class FSceneLifecycle
{
public:
	explicit FSceneLifecycle(FAudioPlayer& Audio) : m_pAudio(&Audio)
	{
	}
	TResult<void> Prepare(DScene& Scene, const FInitContext& Context)
	{
		return Scene.Initialize_Internal(Context);
	}
	void Activate(DScene& Scene, const FSceneActivationContext& Context) noexcept
	{
		Scene.Enter_Internal(Context);
	}
	void Stop(DScene& Scene) noexcept
	{
		Scene.Exit_Internal();
		Scene.Shutdown_Internal();
		// Stop after user cleanup too, so OnExit/OnDeinitialize cannot leave a scoped voice alive.
		m_pAudio->StopScope(Scene.GetAudioScope());
	}
private:
	FAudioPlayer* m_pAudio;
};
}
