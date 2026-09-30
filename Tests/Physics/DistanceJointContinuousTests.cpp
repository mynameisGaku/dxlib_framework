// SPDX-License-Identifier: NOASSERTION
#include "DistanceJointTestSupport.h"
using namespace Toolbox;
using namespace Dxf;
using namespace PhysicsTest::JointTest;
namespace
{
// 薄い静止壁を球の経路に置く。離れたCCD試験とJoint試験で同じ形状を使う。
template <typename T>
void Wall_Internal(typename T::FWorld& World, f32 X, f32 Y)
{
	// 共有接続先を動かさないための生成条件。
	typename T::FBodyDescription Static;
	Static.Type = EBodyType::Static;
	Static.Position = T::At(X, Y, 0);
	// この試験で観測するBody ID。
	const auto Body = World.CreateBody(Static);
	// 壁または球の反発と形状。
	typename T::FColliderDescription Collider;
	Collider.Restitution = 1;
	if constexpr (T::b3D)
	{
		Collider.Shape = FOBB{{}, {0.05f, 5, 5}};
	}
	else
	{
		Collider.Shape = FOrientedBox2D{{}, {0.05f, 5}};
	}
	World.AttachCollider(Body, Collider);
}
// 同じJoint島へ、離れたCCD島の衝突回数だけを変更する。
template <typename T>
TVector<f64> IsolationRun_Internal(int32 Mode, uint32& Hits)
{
	// 試験ごとに独立して構築するWorld。
	typename T::FWorld World;
	World.SetGravity({});
	// 離れた衝突の回数を比較するCCD設定。
	typename T::FContinuous Continuous;
	Continuous.bEnabled = true;
	Continuous.MaxIterations = 16;
	World.SetContinuousSettings(Continuous);
	// 再利用値の影響を確認する速度求解の反復設定。
	typename T::FContact Contact;
	Contact.VelocityIterations = 1;
	World.SetContactSettings(Contact);
	// 共有接続先を動かさないための生成条件。
	typename T::FBodyDescription Static;
	Static.Type = EBodyType::Static;
	// 拘束の反対側の接続先ID。
	const auto Anchor = World.CreateBody(Static);
	// 観測するBodyの生成条件。
	typename T::FBodyDescription Body;
	Body.Position = T::At(2, 1, 0);
	Body.Velocity = T::At(0.37f, -0.71f, 0.13f);
	Body.bAllowSleep = false;
	// 離れたCCD島から独立したJointの動くBody。
	const auto Weight = World.CreateBody(Body);
	// AnchorとBodyを結ぶ距離拘束の設定。
	typename T::FJointDescription Joint;
	Joint.Length = 2;
	Joint.LocalAnchorB = T::At(0.3f, 0.4f, 0);
	// 検査するJointの世代付きID。
	const auto Id = World.CreateDistanceJoint(Anchor, Weight, Joint);
	// もう一つの拘束を加え、TOIでの余分なJoint反復が観測値を変える配置にする。
	Static.Position = T::At(0, 3, 0);
	// 二本目の拘束を接続する静止Body。
	const auto Upper = World.CreateBody(Static);
	Joint.Length = 2.5;
	Joint.LocalAnchorB = T::At(-0.2f, 0.1f, 0);
	World.CreateDistanceJoint(Upper, Weight, Joint);
	// Mode0も同じCCD移動区間を通す。離れた球に当たる壁の有無と幅だけが異なる。
	Body.Position = T::At(-2, 100, 0);
	Body.Velocity = T::At(100, 0, 0);
	Body.bUseContinuous = true;
	// Joint島から離したCCD対象Body。
	const auto Fast = World.CreateBody(Body);
	// 壁または球の反発と形状。
	auto Collider = T::Ball({}, 0.25f);
	Collider.Restitution = 1;
	World.AttachCollider(Fast, Collider);
	if (Mode > 0)
	{
		Wall_Internal<T>(World, 0, 100);
	}
	if (Mode == 2)
	{
		Wall_Internal<T>(World, -4, 100);
	}
	// 時系列のBody・Joint観測値。
	TVector<f64> Values;
	World.Step(0.1);
	Hits = World.GetContinuousDiagnostics().HitsResolved;
	PHYSICS_REQUIRE(World.GetContinuousDiagnostics().UnprocessedSeconds == 0);
	AppendBody<T>(World, Weight, Values);
	Values.PushBack(World.GetDistanceJoint(Id).CurrentLength);
	Values.PushBack(World.GetDistanceJoint(Id).Error);
	// 次のStepにも余分なTOI求解の再利用値が漏れない。
	World.SetContinuousSettings({});
	World.Step(1.0 / 60.0);
	AppendBody<T>(World, Weight, Values);
	Values.PushBack(World.GetDistanceJoint(Id).Error);
	return Values;
}
template <typename T>
void Isolation_Internal()
{
	// 壁を置かない対照の衝突数。
	uint32 NoHits = 0;
	// 片側の壁だけを置いた場合の衝突数。
	uint32 OneHit = 0;
	// 両側の壁を置いた場合の衝突数。
	uint32 ManyHits = 0;
	// Jobを借用しない対照経路の観測値。
	const auto Reference = IsolationRun_Internal<T>(0, NoHits);
	// 一回衝突の観測値、または一分割の距離誤差。
	const auto One = IsolationRun_Internal<T>(1, OneHit);
	// 複数衝突のJoint観測値。
	const auto Many = IsolationRun_Internal<T>(2, ManyHits);
	printf("J4 %s unrelated CCD hits=%u/%u/%u\n", T::Name, NoHits, OneHit, ManyHits);
	PHYSICS_REQUIRE(NoHits == 0 && OneHit >= 1 && ManyHits > OneHit);
	RequireBits(Reference, One);
	RequireBits(Reference, Many);
}
// Jointに直接つながった球の接線方向の高速移動を壁で止める。
template <typename T>
f64 ConnectedRun_Internal(uint32 SubSteps, bool bContinuous)
{
	// 試験ごとに独立して構築するWorld。
	typename T::FWorld World;
	World.SetGravity({});
	// 接続したBodyへCCDを適用するかの設定。
	typename T::FContinuous Settings;
	Settings.bEnabled = bContinuous;
	World.SetContinuousSettings(Settings);
	Wall_Internal<T>(World, 0, 0);
	// 共有接続先を動かさないための生成条件。
	typename T::FBodyDescription Static;
	Static.Type = EBodyType::Static;
	Static.Position = T::At(-2, -2, 0);
	// 拘束の反対側の接続先ID。
	const auto Anchor = World.CreateBody(Static);
	// 動くBodyの初期配置と運動条件。
	typename T::FBodyDescription Description;
	Description.Position = T::At(-2, 0, 0);
	Description.Velocity = T::At(100, 0, 0);
	Description.bUseContinuous = true;
	Description.bAllowSleep = false;
	// この試験で観測するBody ID。
	const auto Body = World.CreateBody(Description);
	World.AttachCollider(Body, T::Ball({}, 0.25f));
	// AnchorとBodyを結ぶ距離拘束の設定。
	typename T::FJointDescription Joint;
	Joint.Length = 2;
	// 検査するJointの世代付きID。
	const auto Id = World.CreateDistanceJoint(Anchor, Body, Joint);
	World.Step(0.04, SubSteps);
	// 現在の登録とBody姿勢から取得するJointの距離と誤差。
	const auto State = World.GetDistanceJoint(Id);
	PHYSICS_REQUIRE(World.IsJointAlive(Id));
	PHYSICS_REQUIRE(IsFinite(State.CurrentLength) && IsFinite(State.Error));
	PHYSICS_REQUIRE(IsFinite(World.GetPosition(Body).X) && IsFinite(World.GetVelocity(Body).X));
	if (bContinuous)
	{
		PHYSICS_REQUIRE(World.GetContinuousDiagnostics().HitsResolved > 0);
		PHYSICS_REQUIRE(World.GetPosition(Body).X <= -0.24f);
	}
	printf("J4 %s connected CCD=%d substeps=%u hits=%u x=%.9g error=%.17g\n", T::Name, bContinuous ? 1 : 0, SubSteps, World.GetContinuousDiagnostics().HitsResolved, World.GetPosition(Body).X, State.Error);
	return Abs(State.Error);
}
template <typename T>
void Connected_Internal()
{
	// 一回衝突の観測値、または一分割の距離誤差。
	const f64 One = ConnectedRun_Internal<T>(1, true);
	// 四分割で進めた場合の距離誤差。
	const f64 Four = ConnectedRun_Internal<T>(4, true);
	// 分割で必ず単調改善する契約ではないが、この配置で大幅に悪化してはいけない。
	PHYSICS_REQUIRE(Four <= One + 0.05);
	(void)ConnectedRun_Internal<T>(1, false);
}
const PhysicsTest::FCase Cases_Internal[] = {
    {"J4 2D unrelated CCD does not repeat joint solve or integration", &Isolation_Internal<F2D>},
    {"J4 3D unrelated CCD does not repeat joint solve or integration", &Isolation_Internal<F3D>},
    {"J4 2D connected joint remains finite and respects CCD wall", &Connected_Internal<F2D>},
    {"J4 3D connected joint remains finite and respects CCD wall", &Connected_Internal<F3D>}};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetDistanceJointContinuousCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
