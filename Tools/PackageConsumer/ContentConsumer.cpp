// SPDX-License-Identifier: NOASSERTION
#include "ContentConsumer.h"
#include "Dxf/SceneContentSource.h"
#include "Dxf/InputStateTracker.h"
#include "Dxf/SceneContentRequest.h"
namespace
{
// 最初の異常を失敗にする。公開ライブラリの内部コードは追加コンパイルしない。
void Check(bool Value)
{
	if (!Value)
	{
		throw Toolbox::FException("External SceneContent contract failed");
	}
}
// 種類別型を呼出し側で指定するだけで、所有・登録順・後始末を再実装しない。
template <typename TScene, typename TInstance, typename TPrepared>
void Test(const TPrepared& Prepared, Dxf::FAssetService& Assets)
{
	TScene Scene;
	struct FShutdown
	{
		TScene& Scene;
		~FShutdown()
		{
			Scene.Shutdown_Internal();
		}
	} Shutdown{Scene};
	const auto A = Scene.template Spawn<TInstance>(Prepared);
	const auto B = Scene.template Spawn<TInstance>(Prepared);
	Check(A && B);
	Check(A.Value().Get()->GetState() == Dxf::EPrefabInstanceState::PendingInitialization);
	Check(static_cast<bool>(Scene.Initialize_Internal({Assets})));
	Dxf::FInputStateTracker Input;
	Dxf::FFrameTime Time;
	Time.DeltaSeconds = 1.0 / 60.0;
	Time.UnscaledDeltaSeconds = Time.DeltaSeconds;
	Check(static_cast<bool>(Scene.Tick_Internal({Input.GetSnapshot(), Time})));
	Check(A.Value().Get()->GetState() == Dxf::EPrefabInstanceState::Ready && B.Value().Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	const auto Drive = A.Value().Get()->GetPrismaticJoint("drive");
	const auto Other = B.Value().Get()->GetPrismaticJoint("drive");
	Check(Drive.Get()->GetJointId() && Other.Get()->GetJointId() && *Drive.Get()->GetJointId() != *Other.Get()->GetJointId());
	Drive.Get()->RequestDrive({true, .5, 15});
	Check(static_cast<bool>(Scene.Tick_Internal({Input.GetSnapshot(), Time})));
	Check(Drive.Get()->GetObservation()->State.Drive.TargetSpeed == .5 && Other.Get()->GetObservation()->State.Drive.TargetSpeed == 0);
	A.Value().Get()->Destroy();
	Check(!A.Value() && !Drive && B.Value());
	Scene.GetChildren_Internal()->FreezeBoundary_Internal();
	Check(static_cast<bool>(Scene.GetChildren_Internal()->CommitBoundary_Internal({Assets})));
	Check(static_cast<bool>(Scene.Tick_Internal({Input.GetSnapshot(), Time})));
	Scene.Shutdown_Internal();
	Check(!B.Value() && !Other);
}
} // namespace
void RunContentConsumer(Dxf::FAssetService& Assets, const Toolbox::FPath& Root)
{
	Dxf::FSceneContentSource Source(Root);
	Dxf::FSceneContentRequest Request(Source);
	const auto AReady = Request.LoadScene2D("Data/scene2d.dxfscene.json");
	Request.Poll(Assets);
	Check(AReady.GetState() == Dxf::ESceneContentRequestState::Ready);
	const auto BReady = Request.LoadScene3D("Data/scene3d.dxfscene.json");
	Request.Poll(Assets);
	Check(BReady.GetState() == Dxf::ESceneContentRequestState::Ready);
	const auto Failed = Request.LoadScene2D("Data/missing.dxfscene.json");
	Request.Poll(Assets);
	Check(Failed.GetState() == Dxf::ESceneContentRequestState::Failed && Request.GetAcceptedSequence() == BReady.GetSequence());
	const auto Canceled = Request.LoadScene2D("Data/scene2d.dxfscene.json");
	Request.Cancel();
	Request.Poll(Assets);
	Check(Canceled.GetState() == Dxf::ESceneContentRequestState::Canceled && Request.GetAcceptedSequence() == BReady.GetSequence());
	const auto A = Dxf::PreparePrefab(Source.LoadPrefab2D("Data/prefab2d.dxfprefab.json"), Assets);
	const auto B = Dxf::PreparePrefab(Source.LoadPrefab3D("Data/prefab3d.dxfprefab.json"), Assets);
	Test<Dxf::DPhysicsScene2D, Dxf::DPrefabInstance2D>(A, Assets);
	Test<Dxf::DPhysicsScene3D, Dxf::DPrefabInstance3D>(B, Assets);
}
