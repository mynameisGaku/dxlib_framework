// SPDX-License-Identifier: NOASSERTION
#include "Dxf/CheckpointCapture2D.h"
#include "Dxf/CheckpointCaptureRuntime.h"
namespace Dxf
{
namespace SavePrivate
{
class FCheckpointCaptureRuntime2D : public TCheckpointCaptureRuntime<FCheckpointDocument2D, FCheckpointInstance2D, DContentScene2D, DPrefabInstance2D, FPreparedPrefab2D>
{
public:
	using TCheckpointCaptureRuntime::TCheckpointCaptureRuntime;
};
} // namespace SavePrivate
FCheckpointCapture2D::FCheckpointCapture2D(const FCheckpointCatalog2D& Catalog, FCheckpointLimits Limits) : FCheckpointCapture2D(SavePrivate::InitialDocument<FCheckpointDocument2D, FCheckpointCatalog2D, FCheckpointInstance2D>(Catalog, Limits), Limits)
{
}
FCheckpointCapture2D::FCheckpointCapture2D(FCheckpointDocument2D Checkpoint, FCheckpointLimits Limits) : m_pRuntime(Toolbox::MakeUnique<SavePrivate::FCheckpointCaptureRuntime2D>(Toolbox::Move(Checkpoint), Limits))
{
}
FCheckpointCapture2D::~FCheckpointCapture2D() = default;
void FCheckpointCapture2D::Bind(DContentScene2D& Scene)
{
	m_pRuntime->Bind(Scene);
}
TResult<TObjectHandle<DPrefabInstance2D>> FCheckpointCapture2D::Spawn(DContentScene2D& Scene, FCheckpointInstance2D Instance, FPreparedPrefab2D Prepared)
{
	return m_pRuntime->Spawn(Scene, Toolbox::Move(Instance), Toolbox::Move(Prepared));
}
bool FCheckpointCapture2D::Destroy(DContentScene2D& Scene, Toolbox::FStringView Key) noexcept
{
	return m_pRuntime->Destroy(Scene, Key);
}
bool FCheckpointCapture2D::RequestCapture() noexcept
{
	return m_pRuntime->Request();
}
bool FCheckpointCapture2D::RequestCapture(Toolbox::FString CheckpointKey)
{
	return m_pRuntime->Request(Toolbox::Move(CheckpointKey));
}
bool FCheckpointCapture2D::HasCaptureRequest() const noexcept
{
	return m_pRuntime->Pending();
}
bool FCheckpointCapture2D::CaptureIfRequested(DContentScene2D& Scene, Toolbox::FStringView GamePayload)
{
	return m_pRuntime->Capture(Scene, GamePayload);
}
const Toolbox::TOptional<FCheckpointDocument2D>& FCheckpointCapture2D::GetLastCapture() const noexcept
{
	return m_pRuntime->Last();
}
Toolbox::uint64 FCheckpointCapture2D::GetCapturedStep() const noexcept
{
	return m_pRuntime->Step();
}
void FCheckpointCapture2D::Retire() noexcept
{
	m_pRuntime->Retire();
}
} // namespace Dxf
