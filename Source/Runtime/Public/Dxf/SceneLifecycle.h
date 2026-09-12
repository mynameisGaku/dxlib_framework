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
	TResult<void> Prepare_Internal(DScene& Scene, const FInitContext& Context)
	{
		return Scene.Initialize_Internal(Context);
	}
	void Activate_Internal(DScene& Scene, const FSceneActivationContext& Context) noexcept
	{
		Scene.Enter_Internal(Context);
	}
	void Stop_Internal(DScene& Scene) noexcept
	{
		Scene.Exit_Internal();
		Scene.Shutdown_Internal();
		// Stop after user cleanup too, so OnExit/OnDeinitialize cannot leave a scoped voice alive.
		if (Scene.GetAudioScope() != 0)
		{
			m_pAudio->StopScope(Scene.GetAudioScope());
		}
	}
private:
	FAudioPlayer* m_pAudio;
};
}
