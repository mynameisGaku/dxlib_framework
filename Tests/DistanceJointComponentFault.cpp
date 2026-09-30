// SPDX-License-Identifier: NOASSERTION
#include "Support/FakeBackend.h"
#include "Dxf/DistanceJointComponent2D.h"
#include "Dxf/DistanceJointComponent3D.h"
#include "Dxf/AssetService.h"
#include "Dxf/AudioPlayer.h"
#include "Dxf/InputStateTracker.h"
#include "Dxf/GameScene.h"
#include "../Source/Toolbox/Private/Toolbox/Testing/AllocationFault.h"
#include <stdio.h>
namespace
{
using namespace Dxf;
// 注入が停止した状態でのみ合否を判定する。
void CheckFault(bool Condition, const char* Message)
{
	if (!Condition)
	{
		throw Toolbox::FException(Message);
	}
}
// 故障がComponentの固定更新より前か後かを区別する。
template <typename TJoint>
class TTrackedJoint final : public TJoint
{
public:
	template <typename TDescription>
	TTrackedJoint(TDescription Description, bool& Reached) : TJoint(Description), m_Reached(Reached)
	{
	}
	// Componentだけの境界費用を、所有階層の配送とは分離する。
	void RunBoundary(const FFixedTickContext& Context)
	{
		OnFixedTick(Context);
	}

protected:
	void OnFixedTick(const FFixedTickContext& Context) override
	{
		m_Reached = true;
		TJoint::OnFixedTick(Context);
	}

private:
	// 注入範囲の間、呼び出し側が保持する到達フラグ。
	bool& m_Reached;
};
// Consumerと同じ所有登録中に、Component構築も失敗できる実Object。
template <typename TJoint>
class TRegisteredJointOwner final : public DGameObject
{
public:
	// 通常の公開追加経路を構築の一部として使う。
	template <typename TDescription>
	explicit TRegisteredJointOwner(TDescription Description)
	{
		// 半登録を呼び出し側へ成功として返さない。
		const auto Added = AddComponent<TJoint>(Description);
		if (!Added)
		{
			throw Toolbox::FException(Added.Error().Message);
		}
	}
};
// Scene登録とComponent生成の全確保地点を、次元ごとに独立した試行で調べる。
template <typename TJoint, typename TDescription>
void RunRegistration(const char* Dimension)
{
	// 実際に失敗させた確保地点数。
	Toolbox::int32 Injected = 0;
	for (Toolbox::int64 Countdown = 0; Countdown < 64; ++Countdown)
	{
		// この試行の全所有物を解放した後に比較する件数。
		const auto Before = Toolbox::Testing::GetOutstandingTestAllocations();
		// 記録は注入を停止してから行う。
		bool bInjected = false;
		{
			// Scene自身の構築は登録試験の注入範囲外。
			DGameScene Scene;
			// 参照値の保持自体は確保を伴わない。
			TDescription Description;
			Toolbox::Testing::SetAllocationFailureCountdown(Countdown);
			const auto Added = Scene.Spawn<TRegisteredJointOwner<TJoint>>(Description);
			bInjected = Toolbox::Testing::WasAllocationFailureInjected();
			Toolbox::Testing::SetAllocationFailureCountdown(-1);
			CheckFault(bInjected ? !Added : static_cast<bool>(Added), "registration result must match injected failure");
			if (bInjected)
			{
				++Injected;
				CheckFault(Scene.GetObjectCount() == 0 && !Scene.FindObject<TRegisteredJointOwner<TJoint>>(), "failed registration published an object");
				// 同じSceneで注入を解除して回復する。
				CheckFault(static_cast<bool>(Scene.Spawn<TRegisteredJointOwner<TJoint>>(Description)), "registration recovery");
			}
			CheckFault(Scene.GetObjectCount() == 1, "registration recovery left a ghost object");
		}
		CheckFault(Toolbox::Testing::GetOutstandingTestAllocations() == Before, "registration allocation leaked");
		printf("JOINT_REGISTRATION_FAULT %s countdown=%lld injected=%d recovered=1 outstanding_delta=0\n", Dimension, Countdown, bInjected);
		if (!bInjected)
		{
			break;
		}
	}
	CheckFault(Injected > 0, "registration fault was never reached");
	// 数値検査による構築失敗でもSceneへObjectを残さない。
	DGameScene Scene;
	TDescription Invalid;
	Invalid.Joint.Length = -1;
	CheckFault(!Scene.Spawn<TRegisteredJointOwner<TJoint>>(Invalid) && Scene.GetObjectCount() == 0, "constructor error registered an object");
}
// 初回接続の予約・生成失敗を再接続の保持試験と分ける。
void RunInitial2D()
{
	// 全確保地点を検出できた試行数。
	Toolbox::int32 Injected = 0;
	for (Toolbox::int64 Countdown = 0; Countdown < 64; ++Countdown)
	{
		Testing::FFakeBackend Backend;
		FAssetService Assets(Backend, Backend, Backend);
		FPhysicsWorld2D World;
		World.SetGravity({});
		FBodyDescription2D Anchor;
		Anchor.Type = EBodyType::Static;
		const auto A = World.CreateBody(Anchor);
		FBodyDescription2D Weight;
		Weight.Position.Y = 2;
		const auto B = World.CreateBody(Weight);
		DGameObject Owner;
		FDistanceJointComponentDescription2D Description;
		Description.BodyA = FPhysicsBodyReference2D::FromBodyId(A);
		Description.BodyB = FPhysicsBodyReference2D::FromBodyId(B);
		Description.Joint.Length = 2;
		const auto Joint = Owner.AddComponent<DDistanceJoint2DComponent>(Description).Value();
		CheckFault(static_cast<bool>(Owner.Initialize_Internal({Assets})), "initial owner initialize");
		FInputStateTracker Input;
		Toolbox::TVector<FInputSnapshot> Pending;
		FPrePhysicsStepQueue Pre;
		FPostPhysicsStepQueue Post;
		FFixedTickContext Fixed{Input.GetSnapshot(), Pending, 1.0 / 60, 0, true, false, 0, &World, nullptr, nullptr, nullptr, &Pre, &Post};
		bool bFailed = false;
		Toolbox::Testing::SetAllocationFailureCountdown(Countdown);
		try
		{
			const auto Dispatch = Owner.FixedTick_Internal(Fixed);
			if (!Dispatch)
			{
				bFailed = true;
			}
			else
			{
				Pre.Run_Internal(Fixed);
			}
		}
		catch (const Toolbox::FException&)
		{
			bFailed = true;
		}
		const bool bInjected = Toolbox::Testing::WasAllocationFailureInjected();
		Toolbox::Testing::SetAllocationFailureCountdown(-1);
		if (bInjected)
		{
			++Injected;
			CheckFault(bFailed && !Joint.Get()->GetJointId() && !Joint.Get()->GetObservation(), "initial failure falsely connected");
			Pre.Clear_Internal();
			Post.Clear_Internal();
			CheckFault(static_cast<bool>(Owner.FixedTick_Internal(Fixed)), "initial recovery dispatch");
			Pre.Run_Internal(Fixed);
		}
		CheckFault(Joint.Get()->GetJointId() && Joint.Get()->GetJointId()->Index == 0, "initial recovery left a ghost joint");
		World.Step(1.0 / 60);
		Post.Run_Internal(Fixed);
		CheckFault(static_cast<bool>(Joint.Get()->GetObservation()), "initial recovery observation");
		Owner.Shutdown_Internal();
		CheckFault(World.IsAlive(A) && World.IsAlive(B), "initial cleanup destroyed endpoints");
		printf("JOINT_INITIAL_FAULT 2D countdown=%lld injected=%d recovered=1\n", Countdown, bInjected);
		if (!bInjected)
		{
			break;
		}
	}
	CheckFault(Injected >= 3, "initial allocation coverage");
}
// 初回接続の予約・生成失敗を再接続の保持試験と分ける。
void RunInitial3D()
{
	// 全確保地点を検出できた試行数。
	Toolbox::int32 Injected = 0;
	for (Toolbox::int64 Countdown = 0; Countdown < 64; ++Countdown)
	{
		Testing::FFakeBackend Backend;
		FAssetService Assets(Backend, Backend, Backend);
		FPhysicsWorld3D World;
		World.SetGravity({});
		FBodyDescription3D Anchor;
		Anchor.Type = EBodyType::Static;
		const auto A = World.CreateBody(Anchor);
		FBodyDescription3D Weight;
		Weight.Position.Y = 2;
		const auto B = World.CreateBody(Weight);
		DGameObject Owner;
		FDistanceJointComponentDescription3D Description;
		Description.BodyA = FPhysicsBodyReference3D::FromBodyId(A);
		Description.BodyB = FPhysicsBodyReference3D::FromBodyId(B);
		Description.Joint.Length = 2;
		const auto Joint = Owner.AddComponent<DDistanceJoint3DComponent>(Description).Value();
		CheckFault(static_cast<bool>(Owner.Initialize_Internal({Assets})), "initial owner initialize");
		FInputStateTracker Input;
		Toolbox::TVector<FInputSnapshot> Pending;
		FPrePhysicsStepQueue Pre;
		FPostPhysicsStepQueue Post;
		FFixedTickContext Fixed{Input.GetSnapshot(), Pending, 1.0 / 60, 0, true, false, 0, nullptr, &World, nullptr, nullptr, &Pre, &Post};
		bool bFailed = false;
		Toolbox::Testing::SetAllocationFailureCountdown(Countdown);
		try
		{
			const auto Dispatch = Owner.FixedTick_Internal(Fixed);
			if (!Dispatch)
			{
				bFailed = true;
			}
			else
			{
				Pre.Run_Internal(Fixed);
			}
		}
		catch (const Toolbox::FException&)
		{
			bFailed = true;
		}
		const bool bInjected = Toolbox::Testing::WasAllocationFailureInjected();
		Toolbox::Testing::SetAllocationFailureCountdown(-1);
		if (bInjected)
		{
			++Injected;
			CheckFault(bFailed && !Joint.Get()->GetJointId() && !Joint.Get()->GetObservation(), "initial failure falsely connected");
			Pre.Clear_Internal();
			Post.Clear_Internal();
			CheckFault(static_cast<bool>(Owner.FixedTick_Internal(Fixed)), "initial recovery dispatch");
			Pre.Run_Internal(Fixed);
		}
		CheckFault(Joint.Get()->GetJointId() && Joint.Get()->GetJointId()->Index == 0, "initial recovery left a ghost joint");
		World.Step(1.0 / 60);
		Post.Run_Internal(Fixed);
		CheckFault(static_cast<bool>(Joint.Get()->GetObservation()), "initial recovery observation");
		Owner.Shutdown_Internal();
		CheckFault(World.IsAlive(A) && World.IsAlive(B), "initial cleanup destroyed endpoints");
		printf("JOINT_INITIAL_FAULT 3D countdown=%lld injected=%d recovered=1\n", Countdown, bInjected);
		if (!bInjected)
		{
			break;
		}
	}
	CheckFault(Injected >= 3, "initial allocation coverage");
}
// Body登録・Component予約・Joint生成の各確保を実製品で一つずつ失敗させる。
Toolbox::int32 Run2D()
{
	// 実際に注入地点へ到達した試行数。
	Toolbox::int32 Injected = 0;
	for (Toolbox::int64 Countdown = 0; Countdown < 64; ++Countdown)
	{
		Testing::FFakeBackend Backend;
		FAssetService Assets(Backend, Backend, Backend);
		// この試行が所有する実Physics World。
		FPhysicsWorld2D World;
		World.SetGravity({});
		// 支点の生成条件。
		FBodyDescription2D Anchor;
		Anchor.Type = EBodyType::Static;
		// 接続のA側。
		const auto A = World.CreateBody(Anchor);
		// 荷物の生成条件。
		FBodyDescription2D Weight;
		Weight.Position = {0, 2};
		// 接続のB側。
		const auto B = World.CreateBody(Weight);
		DGameObject Owner;
		// 登録へ渡す生成条件。
		FDistanceJointComponentDescription2D Description;
		Description.BodyA = FPhysicsBodyReference2D::FromBodyId(A);
		Description.BodyB = FPhysicsBodyReference2D::FromBodyId(B);
		Description.Joint.Length = 2;
		bool bReached = false;
		// 今回調べる接続Component。
		const auto Joint = Owner.AddComponent<TTrackedJoint<DDistanceJoint2DComponent>>(Description, bReached).Value();
		CheckFault(static_cast<bool>(Owner.Initialize_Internal({Assets})), "fault owner initialize");
		// 固定更新へ渡す入力の保存先。
		FInputStateTracker Input;
		// 未配達の入力を保持する列。
		Toolbox::TVector<FInputSnapshot> Pending;
		// 今回の固定更新の直前予約。
		FPrePhysicsStepQueue Pre;
		// 今回の固定更新の成功後予約。
		FPostPhysicsStepQueue Post;
		// 同じWorldと予約先を持つ固定更新の環境。
		FFixedTickContext Fixed{Input.GetSnapshot(), Pending, 1.0 / 60, 0, true, false, 0, &World, nullptr, nullptr, nullptr, &Pre, &Post};
		CheckFault(static_cast<bool>(Owner.FixedTick_Internal(Fixed)), "fault first dispatch");
		Pre.Run_Internal(Fixed);
		World.Step(1.0 / 60);
		Post.Run_Internal(Fixed);
		// 失敗前の世代付き接続ID。
		const auto Old = *Joint.Get()->GetJointId();
		// 登録領域の容量境界で、新Jointの確保失敗を確実に含める。
		for (Toolbox::int32 Index = 0; Index < 31; ++Index)
		{
			World.CreateDistanceJoint(A, B, Description.Joint);
		}
		Description.Joint.Length = 1.8;
		Joint.Get()->RequestConnect(Description);
		// 失敗試行で新しく作る直前予約。
		FPrePhysicsStepQueue FreshPre;
		// 失敗試行で新しく作る成功後予約。
		FPostPhysicsStepQueue FreshPost;
		Fixed.PrePhysicsStep = &FreshPre;
		Fixed.PostPhysicsStep = &FreshPost;
		bool bFailed = false;
		bReached = false;
		Toolbox::Testing::SetAllocationFailureCountdown(Countdown);
		try
		{
			const auto Dispatch = Owner.FixedTick_Internal(Fixed);
			if (!Dispatch)
			{
				bFailed = true;
			}
			else
			{
				FreshPre.Run_Internal(Fixed);
			}
		}
		catch (const Toolbox::FException&)
		{
			bFailed = true;
		}
		const bool bInjected = Toolbox::Testing::WasAllocationFailureInjected();
		Toolbox::Testing::SetAllocationFailureCountdown(-1);
		if (bInjected)
		{
			++Injected;
			CheckFault(bFailed && Joint.Get()->GetJointId() && *Joint.Get()->GetJointId() == Old && World.IsJointAlive(Old), "failed reconnect must retain old joint");
			CheckFault(!bReached || !Joint.Get()->GetObservation(), "failed component frame must invalidate observation");
			FreshPre.Clear_Internal();
			FreshPost.Clear_Internal();
			CheckFault(static_cast<bool>(Owner.FixedTick_Internal(Fixed)), "reconnect recovery dispatch");
			FreshPre.Run_Internal(Fixed);
		}
		CheckFault(Joint.Get()->GetJointId() && Joint.Get()->GetJointId()->Index == 32 && *Joint.Get()->GetJointId() != Old && !World.IsJointAlive(Old), "reconnect recovery generation and no ghost slot");
		World.Step(1.0 / 60);
		FreshPost.Run_Internal(Fixed);
		// 慣らし後の参照解決・予約・読み取り・観察は追加確保しない。
		const auto Before = Toolbox::Testing::GetTotalTestAllocations();
		FreshPre.Clear_Internal();
		FreshPost.Clear_Internal();
		Joint.Get()->RunBoundary(Fixed);
		FreshPre.Run_Internal(Fixed);
		FreshPost.Run_Internal(Fixed);
		for (Toolbox::int32 Read = 0; Read < 100; ++Read)
		{
			(void)Joint.Get()->GetJointId();
			(void)Joint.Get()->GetObservation();
		}
		CheckFault(Toolbox::Testing::GetTotalTestAllocations() == Before, "steady component boundary allocated");
		Owner.Shutdown_Internal();
		CheckFault(World.IsAlive(A) && World.IsAlive(B), "component shutdown destroyed bodies");
		printf("JOINT_COMPONENT_FAULT 2D countdown=%lld injected=%d failed=%d recovered=1\n", Countdown, bInjected, bFailed);
		if (!bInjected)
		{
			break;
		}
	}
	CheckFault(Injected >= 3, "component fault allocation coverage");
	return Injected;
}
Toolbox::int32 Run3D()
{
	// 実際に注入地点へ到達した試行数。
	Toolbox::int32 Injected = 0;
	for (Toolbox::int64 Countdown = 0; Countdown < 64; ++Countdown)
	{
		Testing::FFakeBackend Backend;
		FAssetService Assets(Backend, Backend, Backend);
		// この試行が所有する実Physics World。
		FPhysicsWorld3D World;
		World.SetGravity({});
		// 支点の生成条件。
		FBodyDescription3D Anchor;
		Anchor.Type = EBodyType::Static;
		// 接続のA側。
		const auto A = World.CreateBody(Anchor);
		// 荷物の生成条件。
		FBodyDescription3D Weight;
		Weight.Position = {0, 2, 0};
		// 接続のB側。
		const auto B = World.CreateBody(Weight);
		DGameObject Owner;
		// 登録へ渡す生成条件。
		FDistanceJointComponentDescription3D Description;
		Description.BodyA = FPhysicsBodyReference3D::FromBodyId(A);
		Description.BodyB = FPhysicsBodyReference3D::FromBodyId(B);
		Description.Joint.Length = 2;
		bool bReached = false;
		// 今回調べる接続Component。
		const auto Joint = Owner.AddComponent<TTrackedJoint<DDistanceJoint3DComponent>>(Description, bReached).Value();
		CheckFault(static_cast<bool>(Owner.Initialize_Internal({Assets})), "fault owner initialize");
		// 固定更新へ渡す入力の保存先。
		FInputStateTracker Input;
		// 未配達の入力を保持する列。
		Toolbox::TVector<FInputSnapshot> Pending;
		// 今回の固定更新の直前予約。
		FPrePhysicsStepQueue Pre;
		// 今回の固定更新の成功後予約。
		FPostPhysicsStepQueue Post;
		// 同じWorldと予約先を持つ固定更新の環境。
		FFixedTickContext Fixed{Input.GetSnapshot(), Pending, 1.0 / 60, 0, true, false, 0, nullptr, &World, nullptr, nullptr, &Pre, &Post};
		CheckFault(static_cast<bool>(Owner.FixedTick_Internal(Fixed)), "fault first dispatch");
		Pre.Run_Internal(Fixed);
		World.Step(1.0 / 60);
		Post.Run_Internal(Fixed);
		// 失敗前の世代付き接続ID。
		const auto Old = *Joint.Get()->GetJointId();
		// 登録領域の容量境界で、新Jointの確保失敗を確実に含める。
		for (Toolbox::int32 Index = 0; Index < 31; ++Index)
		{
			World.CreateDistanceJoint(A, B, Description.Joint);
		}
		Description.Joint.Length = 1.8;
		Joint.Get()->RequestConnect(Description);
		// 失敗試行で新しく作る直前予約。
		FPrePhysicsStepQueue FreshPre;
		// 失敗試行で新しく作る成功後予約。
		FPostPhysicsStepQueue FreshPost;
		Fixed.PrePhysicsStep = &FreshPre;
		Fixed.PostPhysicsStep = &FreshPost;
		bool bFailed = false;
		bReached = false;
		Toolbox::Testing::SetAllocationFailureCountdown(Countdown);
		try
		{
			const auto Dispatch = Owner.FixedTick_Internal(Fixed);
			if (!Dispatch)
			{
				bFailed = true;
			}
			else
			{
				FreshPre.Run_Internal(Fixed);
			}
		}
		catch (const Toolbox::FException&)
		{
			bFailed = true;
		}
		const bool bInjected = Toolbox::Testing::WasAllocationFailureInjected();
		Toolbox::Testing::SetAllocationFailureCountdown(-1);
		if (bInjected)
		{
			++Injected;
			CheckFault(bFailed && Joint.Get()->GetJointId() && *Joint.Get()->GetJointId() == Old && World.IsJointAlive(Old), "failed reconnect must retain old joint");
			CheckFault(!bReached || !Joint.Get()->GetObservation(), "failed component frame must invalidate observation");
			FreshPre.Clear_Internal();
			FreshPost.Clear_Internal();
			CheckFault(static_cast<bool>(Owner.FixedTick_Internal(Fixed)), "reconnect recovery dispatch");
			FreshPre.Run_Internal(Fixed);
		}
		CheckFault(Joint.Get()->GetJointId() && Joint.Get()->GetJointId()->Index == 32 && *Joint.Get()->GetJointId() != Old && !World.IsJointAlive(Old), "reconnect recovery generation and no ghost slot");
		World.Step(1.0 / 60);
		FreshPost.Run_Internal(Fixed);
		// 慣らし後の参照解決・予約・読み取り・観察は追加確保しない。
		const auto Before = Toolbox::Testing::GetTotalTestAllocations();
		FreshPre.Clear_Internal();
		FreshPost.Clear_Internal();
		Joint.Get()->RunBoundary(Fixed);
		FreshPre.Run_Internal(Fixed);
		FreshPost.Run_Internal(Fixed);
		for (Toolbox::int32 Read = 0; Read < 100; ++Read)
		{
			(void)Joint.Get()->GetJointId();
			(void)Joint.Get()->GetObservation();
		}
		CheckFault(Toolbox::Testing::GetTotalTestAllocations() == Before, "steady component boundary allocated");
		Owner.Shutdown_Internal();
		CheckFault(World.IsAlive(A) && World.IsAlive(B), "component shutdown destroyed bodies");
		printf("JOINT_COMPONENT_FAULT 3D countdown=%lld injected=%d failed=%d recovered=1\n", Countdown, bInjected, bFailed);
		if (!bInjected)
		{
			break;
		}
	}
	CheckFault(Injected >= 3, "component fault allocation coverage");
	return Injected;
}
} // namespace
int main()
{
	try
	{
		setvbuf(stdout, nullptr, _IONBF, 0);
		RunRegistration<DDistanceJoint2DComponent, FDistanceJointComponentDescription2D>("2D");
		RunRegistration<DDistanceJoint3DComponent, FDistanceJointComponentDescription3D>("3D");
		RunInitial2D();
		RunInitial3D();
		// 最初の試行または接続。
		const auto First = Run2D();
		// 二つ目の次元の結果。
		const auto Second = Run3D();
		printf("JOINT_COMPONENT_FAULT_PASSED 2D=%d 3D=%d\n", First, Second);
		return 0;
	}
	catch (const Toolbox::FException& Error)
	{
		Toolbox::Testing::SetAllocationFailureCountdown(-1);
		fprintf(stderr, "%s\n", Error.What());
		return 1;
	}
}
