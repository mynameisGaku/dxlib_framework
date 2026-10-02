// SPDX-License-Identifier: NOASSERTION
#include "Dxf/CheckpointCapture3D.h"
#include "Dxf/CheckpointCaptureRuntime.h"
namespace Dxf
{
namespace SavePrivate
{
class FCheckpointCaptureRuntime3D : public TCheckpointCaptureRuntime<FCheckpointDocument3D, FCheckpointInstance3D, DContentScene3D, DPrefabInstance3D, FPreparedPrefab3D>
{
public:
	using TCheckpointCaptureRuntime::TCheckpointCaptureRuntime;
};
} // namespace SavePrivate
FCheckpointCapture3D::FCheckpointCapture3D(const FCheckpointCatalog3D& Catalog, FCheckpointLimits Limits) : FCheckpointCapture3D(FCheckpointRestore::MakeInitial(Catalog, "{}", Limits), Limits)
{
}
FCheckpointCapture3D::FCheckpointCapture3D(FCheckpointDocument3D Checkpoint, FCheckpointLimits Limits) : m_pRuntime(Toolbox::MakeUnique<SavePrivate::FCheckpointCaptureRuntime3D>(Toolbox::Move(Checkpoint), Limits))
{
}
FCheckpointCapture3D::~FCheckpointCapture3D() = default;
void FCheckpointCapture3D::Bind(DContentScene3D& Scene)
{
	m_pRuntime->Bind(Scene);
}
TResult<TObjectHandle<DPrefabInstance3D>> FCheckpointCapture3D::Spawn(DContentScene3D& Scene, FCheckpointInstance3D Instance, FPreparedPrefab3D Prepared)
{
	return m_pRuntime->Spawn(Scene, Toolbox::Move(Instance), Toolbox::Move(Prepared));
}
bool FCheckpointCapture3D::Destroy(DContentScene3D& Scene, Toolbox::FStringView Key) noexcept
{
	return m_pRuntime->Destroy(Scene, Key);
}
bool FCheckpointCapture3D::RequestCapture() noexcept
{
	return m_pRuntime->Request();
}
bool FCheckpointCapture3D::RequestCapture(Toolbox::FString CheckpointKey)
{
	return m_pRuntime->Request(Toolbox::Move(CheckpointKey));
}
bool FCheckpointCapture3D::HasCaptureRequest() const noexcept
{
	return m_pRuntime->Pending();
}
bool FCheckpointCapture3D::CaptureIfRequested(DContentScene3D& Scene, Toolbox::FStringView GamePayload)
{
	return m_pRuntime->Capture(Scene, GamePayload);
}
const Toolbox::TOptional<FCheckpointDocument3D>& FCheckpointCapture3D::GetLastCapture() const noexcept
{
	return m_pRuntime->Last();
}
Toolbox::uint64 FCheckpointCapture3D::GetCapturedStep() const noexcept
{
	return m_pRuntime->Step();
}
void FCheckpointCapture3D::Retire() noexcept
{
	m_pRuntime->Retire();
}
} // namespace Dxf
