// SPDX-License-Identifier: NOASSERTION
#include "Dxf/CheckpointResumeRequest3D.h"
#include "Dxf/CheckpointResumeRuntime.h"
namespace Dxf::SavePrivate
{
class FCheckpointResumeRuntime3D : public TCheckpointResumeRuntime<FCheckpointCatalog3D, FCheckpointDocument3D, FPreparedScene3D, ACheckpointScene3D>
{
public:
	using TCheckpointResumeRuntime::TCheckpointResumeRuntime;
};
} // namespace Dxf::SavePrivate
namespace Dxf
{
FCheckpointResumeRequest3D::FCheckpointResumeRequest3D(FGameSaveRequest& Saves, FSceneNavigator& Scenes, FAssetService& Assets, FCheckpointCatalog3D Catalog, Toolbox::TFunction<FCheckpointDocument3D(const FCheckpointDocument3D&)> RestoreProgress, Toolbox::TFunction<Toolbox::TUniquePtr<ACheckpointScene3D>(FPreparedScene3D, FCheckpointDocument3D)> CreateScene, FCheckpointLimits Limits) : m_pRuntime(Toolbox::MakeUnique<SavePrivate::FCheckpointResumeRuntime3D>(Saves, Scenes, Assets, Toolbox::Move(Catalog), Toolbox::Move(RestoreProgress), Toolbox::Move(CreateScene), Limits))
{
}
FCheckpointResumeRequest3D::~FCheckpointResumeRequest3D()
{
	m_pRuntime->Retire();
}
FGameSaveTicket FCheckpointResumeRequest3D::RequestLoad(Toolbox::FString Slot)
{
	return m_pRuntime->Load(Toolbox::Move(Slot));
}
bool FCheckpointResumeRequest3D::Cancel()
{
	return m_pRuntime->Cancel();
}
void FCheckpointResumeRequest3D::Poll()
{
	m_pRuntime->Poll();
}
bool FCheckpointResumeRequest3D::Retire() noexcept
{
	return m_pRuntime->Retire();
}
} // namespace Dxf
