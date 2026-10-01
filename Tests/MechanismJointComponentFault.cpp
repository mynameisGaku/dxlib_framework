// SPDX-License-Identifier: NOASSERTION
#include "Support/FakeBackend.h"
#include "Dxf/RevoluteJointComponent2D.h"
#include "Dxf/RevoluteJointComponent3D.h"
#include "Dxf/FixedJointComponent2D.h"
#include "Dxf/FixedJointComponent3D.h"
#include "Dxf/PrismaticJointComponent2D.h"
#include "Dxf/PrismaticJointComponent3D.h"
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
	Invalid.Joint.FrameA.LocalAnchor.X = Toolbox::TNumericLimits<Toolbox::f32>::QuietNaN();
	CheckFault(!Scene.Spawn<TRegisteredJointOwner<TJoint>>(Invalid) && Scene.GetObjectCount() == 0, "constructor error registered an object");
}
// 2DのRevolute試験へ公開型と接続入口を渡す。
struct FRevolute2D
{
	using FWorld = FPhysicsWorld2D;
	using FBody = FBodyDescription2D;
	using FDescription = FRevoluteJointComponentDescription2D;
	using FComponent = DRevoluteJoint2DComponent;
	using FReference = FPhysicsBodyReference2D;
	static constexpr const char* Name = "Revolute-2D";
	static void Bind(FFixedTickContext& Context, FWorld& World)
	{
		Context.Physics2D = &World;
	}
	static void Fill(FWorld& World, FBodyId2D A, FBodyId2D B, FDescription Description)
	{
		World.CreateRevoluteJoint(A, B, Description.Joint);
	}
};
// 3DのRevolute試験へ公開型と接続入口を渡す。
struct FRevolute3D
{
	using FWorld = FPhysicsWorld3D;
	using FBody = FBodyDescription3D;
	using FDescription = FRevoluteJointComponentDescription3D;
	using FComponent = DRevoluteJoint3DComponent;
	using FReference = FPhysicsBodyReference3D;
	static constexpr const char* Name = "Revolute-3D";
	static void Bind(FFixedTickContext& Context, FWorld& World)
	{
		Context.Physics3D = &World;
	}
	static void Fill(FWorld& World, FBodyId3D A, FBodyId3D B, FDescription Description)
	{
		World.CreateRevoluteJoint(A, B, Description.Joint);
	}
};
// 2DのFixed試験へ公開型と接続入口を渡す。
struct FFixed2D
{
	using FWorld = FPhysicsWorld2D;
	using FBody = FBodyDescription2D;
	using FDescription = FFixedJointComponentDescription2D;
	using FComponent = DFixedJoint2DComponent;
	using FReference = FPhysicsBodyReference2D;
	static constexpr const char* Name = "Fixed-2D";
	static void Bind(FFixedTickContext& Context, FWorld& World)
	{
		Context.Physics2D = &World;
	}
	static void Fill(FWorld& World, FBodyId2D A, FBodyId2D B, FDescription Description)
	{
		World.CreateFixedJoint(A, B, Description.Joint);
	}
};
// 3DのFixed試験へ公開型と接続入口を渡す。
struct FFixed3D
{
	using FWorld = FPhysicsWorld3D;
	using FBody = FBodyDescription3D;
	using FDescription = FFixedJointComponentDescription3D;
	using FComponent = DFixedJoint3DComponent;
	using FReference = FPhysicsBodyReference3D;
	static constexpr const char* Name = "Fixed-3D";
	static void Bind(FFixedTickContext& Context, FWorld& World)
	{
		Context.Physics3D = &World;
	}
	static void Fill(FWorld& World, FBodyId3D A, FBodyId3D B, FDescription Description)
	{
		World.CreateFixedJoint(A, B, Description.Joint);
	}
};
// 2DのPrismatic試験へ公開型と接続入口を渡す。
struct FPrismatic2D
{
	using FWorld = FPhysicsWorld2D;
	using FBody = FBodyDescription2D;
	using FDescription = FPrismaticJointComponentDescription2D;
	using FComponent = DPrismaticJoint2DComponent;
	using FReference = FPhysicsBodyReference2D;
	static constexpr const char* Name = "Prismatic-2D";
	static void Bind(FFixedTickContext& Context, FWorld& World)
	{
		Context.Physics2D = &World;
	}
	static void Fill(FWorld& World, FBodyId2D A, FBodyId2D B, FDescription Description)
	{
		World.CreatePrismaticJoint(A, B, Description.Joint);
	}
};
// 3DのPrismatic試験へ公開型と接続入口を渡す。
struct FPrismatic3D
{
	using FWorld = FPhysicsWorld3D;
	using FBody = FBodyDescription3D;
	using FDescription = FPrismaticJointComponentDescription3D;
	using FComponent = DPrismaticJoint3DComponent;
	using FReference = FPhysicsBodyReference3D;
	static constexpr const char* Name = "Prismatic-3D";
	static void Bind(FFixedTickContext& Context, FWorld& World)
	{
		Context.Physics3D = &World;
	}
	static void Fill(FWorld& World, FBodyId3D A, FBodyId3D B, FDescription Description)
	{
		World.CreatePrismaticJoint(A, B, Description.Joint);
	}
};
template <typename TCase>
void RunInitial()
{
	// 全確保地点を検出できた試行数。
	Toolbox::int32 Injected = 0;
	for (Toolbox::int64 Countdown = 0; Countdown < 64; ++Countdown)
	{
		Testing::FFakeBackend Backend;
		FAssetService Assets(Backend, Backend, Backend);
		typename TCase::FWorld World;
		World.SetGravity({});
		typename TCase::FBody Anchor;
		Anchor.Type = EBodyType::Static;
		const auto A = World.CreateBody(Anchor);
		typename TCase::FBody Weight;
		Weight.Position.Y = 2;
		const auto B = World.CreateBody(Weight);
		DGameObject Owner;
		typename TCase::FDescription Description;
		Description.BodyA = TCase::FReference::FromBodyId(A);
		Description.BodyB = TCase::FReference::FromBodyId(B);
		Description.Joint.FrameA.LocalAnchor.Y = 2;
		const auto Joint = Owner.AddComponent<typename TCase::FComponent>(Description).Value();
		CheckFault(static_cast<bool>(Owner.Initialize_Internal({Assets})), "initial owner initialize");
		FInputStateTracker Input;
		Toolbox::TVector<FInputSnapshot> Pending;
		FPrePhysicsStepQueue Pre;
		FPostPhysicsStepQueue Post;
		FFixedTickContext Fixed{Input.GetSnapshot(), Pending, 1.0 / 60, 0, true, false, 0, nullptr, nullptr, nullptr, nullptr, &Pre, &Post};
		TCase::Bind(Fixed, World);
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
		// 固定更新を打ち切る即時終了では、非所有の予約を先に破棄する。
		CheckFault(static_cast<bool>(Owner.FixedTick_Internal(Fixed)), "shutdown reservation dispatch");
		const auto ShutdownAllocations = Toolbox::Testing::GetTotalTestAllocations();
		Pre.Clear_Internal();
		Post.Clear_Internal();
		Owner.Shutdown_Internal();
		CheckFault(Toolbox::Testing::GetTotalTestAllocations() == ShutdownAllocations, "aborted reservation shutdown allocated");
		CheckFault(World.IsAlive(A) && World.IsAlive(B), "initial cleanup destroyed endpoints");
		printf("MECHANISM_INITIAL_FAULT %s countdown=%lld injected=%d recovered=1\n", TCase::Name, Countdown, bInjected);
		if (!bInjected)
		{
			break;
		}
	}
	CheckFault(Injected >= 3, "initial allocation coverage");
}
template <typename TCase>
Toolbox::int32 RunReconnect()
{
	// 実際に注入地点へ到達した試行数。
	Toolbox::int32 Injected = 0;
	for (Toolbox::int64 Countdown = 0; Countdown < 64; ++Countdown)
	{
		Testing::FFakeBackend Backend;
		FAssetService Assets(Backend, Backend, Backend);
		// この試行が所有する実Physics World。
		typename TCase::FWorld World;
		World.SetGravity({});
		// 支点の生成条件。
		typename TCase::FBody Anchor;
		Anchor.Type = EBodyType::Static;
		// 接続のA側。
		const auto A = World.CreateBody(Anchor);
		// 荷物の生成条件。
		typename TCase::FBody Weight;
		Weight.Position.Y = 2;
		// 接続のB側。
		const auto B = World.CreateBody(Weight);
		DGameObject Owner;
		// 登録へ渡す生成条件。
		typename TCase::FDescription Description;
		Description.BodyA = TCase::FReference::FromBodyId(A);
		Description.BodyB = TCase::FReference::FromBodyId(B);
		Description.Joint.FrameA.LocalAnchor.Y = 2;
		bool bReached = false;
		// 今回調べる接続Component。
		const auto Joint = Owner.AddComponent<TTrackedJoint<typename TCase::FComponent>>(Description, bReached).Value();
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
		FFixedTickContext Fixed{Input.GetSnapshot(), Pending, 1.0 / 60, 0, true, false, 0, nullptr, nullptr, nullptr, nullptr, &Pre, &Post};
		TCase::Bind(Fixed, World);
		CheckFault(static_cast<bool>(Owner.FixedTick_Internal(Fixed)), "fault first dispatch");
		Pre.Run_Internal(Fixed);
		World.Step(1.0 / 60);
		Post.Run_Internal(Fixed);
		// 失敗前の世代付き接続ID。
		const auto Old = *Joint.Get()->GetJointId();
		// 登録領域の容量境界で、新Jointの確保失敗を確実に含める。
		for (Toolbox::int32 Index = 0; Index < 31; ++Index)
		{
			TCase::Fill(World, A, B, Description);
		}
		Description.Joint.FrameA.LocalAnchor.Y = 1.8f;
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
		// 予約中の破棄は要求だけを記録し、対象の実解放を固定更新境界まで遅らせる。
		const auto ReservedJoint = *Joint.Get()->GetJointId();
		FreshPre.Clear_Internal();
		FreshPost.Clear_Internal();
		Joint.Get()->RunBoundary(Fixed);
		const auto ReleaseAllocations = Toolbox::Testing::GetTotalTestAllocations();
		// Handleは破棄要求と同時に失効する。予約中だけ有効な実体を確認用に保持する。
		const auto* ReservedComponent = Joint.Get();
		Owner.Destroy();
		CheckFault(!Joint.Get() && !ReservedComponent->GetObservation() && !ReservedComponent->GetJointId(), "destroy request left observation visible");
		FreshPre.Run_Internal(Fixed);
		FreshPost.Run_Internal(Fixed);
		CheckFault(World.IsJointAlive(ReservedJoint), "destroy request released before reserved boundary");
		Owner.Shutdown_Internal();
		CheckFault(Toolbox::Testing::GetTotalTestAllocations() == ReleaseAllocations, "reserved owner shutdown allocated");
		CheckFault(!World.IsJointAlive(ReservedJoint), "reserved owner left joint alive");
		CheckFault(World.IsAlive(A) && World.IsAlive(B), "component shutdown destroyed bodies");
		printf("MECHANISM_COMPONENT_FAULT %s countdown=%lld injected=%d failed=%d recovered=1\n", TCase::Name, Countdown, bInjected, bFailed);
		if (!bInjected)
		{
			break;
		}
	}
	CheckFault(Injected >= 3, "component fault allocation coverage");
	return Injected;
}
} // namespace
void RunMechanismComponentFaults()
{
	RunRegistration<DRevoluteJoint2DComponent, FRevoluteJointComponentDescription2D>("Revolute-2D");
	RunInitial<FRevolute2D>();
	(void)RunReconnect<FRevolute2D>();
	RunRegistration<DRevoluteJoint3DComponent, FRevoluteJointComponentDescription3D>("Revolute-3D");
	RunInitial<FRevolute3D>();
	(void)RunReconnect<FRevolute3D>();
	RunRegistration<DFixedJoint2DComponent, FFixedJointComponentDescription2D>("Fixed-2D");
	RunInitial<FFixed2D>();
	(void)RunReconnect<FFixed2D>();
	RunRegistration<DFixedJoint3DComponent, FFixedJointComponentDescription3D>("Fixed-3D");
	RunInitial<FFixed3D>();
	(void)RunReconnect<FFixed3D>();
	RunRegistration<DPrismaticJoint2DComponent, FPrismaticJointComponentDescription2D>("Prismatic-2D");
	RunInitial<FPrismatic2D>();
	(void)RunReconnect<FPrismatic2D>();
	RunRegistration<DPrismaticJoint3DComponent, FPrismaticJointComponentDescription3D>("Prismatic-3D");
	RunInitial<FPrismatic3D>();
	(void)RunReconnect<FPrismatic3D>();
}
