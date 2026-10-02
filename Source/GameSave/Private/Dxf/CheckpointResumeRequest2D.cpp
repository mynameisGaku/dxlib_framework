// SPDX-License-Identifier: NOASSERTION
#include "Dxf/CheckpointResumeRequest2D.h"
#include "Dxf/CheckpointResumeRuntime.h"
namespace Dxf::SavePrivate
{
class FCheckpointResumeRuntime2D : public TCheckpointResumeRuntime<FCheckpointCatalog2D, FCheckpointDocument2D, FPreparedScene2D, ACheckpointScene2D>
{
public:
	using TCheckpointResumeRuntime::TCheckpointResumeRuntime;
};
} // namespace Dxf::SavePrivate
namespace Dxf
{
FCheckpointResumeRequest2D::FCheckpointResumeRequest2D(FGameSaveRequest& Saves, FSceneNavigator& Scenes, FAssetService& Assets, FCheckpointCatalog2D Catalog, Toolbox::TFunction<FCheckpointDocument2D(const FCheckpointDocument2D&)> RestoreProgress, Toolbox::TFunction<Toolbox::TUniquePtr<ACheckpointScene2D>(FPreparedScene2D, FCheckpointDocument2D)> CreateScene, FCheckpointLimits Limits) : m_pRuntime(Toolbox::MakeUnique<SavePrivate::FCheckpointResumeRuntime2D>(Saves, Scenes, Assets, Toolbox::Move(Catalog), Toolbox::Move(RestoreProgress), Toolbox::Move(CreateScene), Limits))
{
}
FCheckpointResumeRequest2D::~FCheckpointResumeRequest2D()
{
	m_pRuntime->Retire();
}
FGameSaveTicket FCheckpointResumeRequest2D::RequestLoad(Toolbox::FString Slot)
{
	return m_pRuntime->Load(Toolbox::Move(Slot));
}
bool FCheckpointResumeRequest2D::Cancel()
{
	return m_pRuntime->Cancel();
}
void FCheckpointResumeRequest2D::Poll()
{
	m_pRuntime->Poll();
}
bool FCheckpointResumeRequest2D::Retire() noexcept
{
	return m_pRuntime->Retire();
}
} // namespace Dxf
