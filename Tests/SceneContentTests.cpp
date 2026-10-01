// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/SceneContentValidation.h"
#include "Dxf/SceneContentParser.h"
#include "Dxf/PrefabInstance2D.h"
#include "Dxf/PrefabInstance3D.h"
#include "Dxf/PhysicsScene2D.h"
#include "Dxf/PhysicsScene3D.h"
#include "Dxf/InputStateTracker.h"
#include "Support/FakeBackend.h"
#include "Dxf/SceneContentSource.h"
#include "Dxf/ContentScene2D.h"
#include "Dxf/ContentScene3D.h"
namespace
{
// 最初の利用シナリオ。Jointをpartsより先に宣言し前方参照も検査する。
const char* Door2D =
    R"json({"schema":1,"kind":"prefab","dimension":2,"parameters":{"speed":{"type":"number","default":0.8,"min":-3,"max":3},"travel":{"type":"number","default":2,"min":0.1,"max":5}},"joints":[{"id":"rail","kind":"Prismatic","bodyA":"frame","bodyB":"leaf","frame":{"space":"Prefab","anchor":[0,1],"angle":0},"drive":{"enabled":true,"speed":{"parameter":"speed"},"maxForce":15},"limits":{"enabled":true,"lower":0,"upper":{"parameter":"travel"}}}],"parts":[{"id":"frame","body":{"type":"Static","position":[0,0]}},{"id":"leaf","body":{"type":"Dynamic","position":[0,1]},"colliders":[{"shape":"Box","halfExtents":[0.4,0.8],"response":"Solid"}],"visual":{"kind":"Box","halfExtents":[0.4,0.8],"color":[80,180,240,255]}}],"exports":{"doorBody":"leaf/body","doorDrive":"rail"}})json";
// 既定姿勢はIdentity。3D固有のFrameと異方性慣性を通す。
const char* Door3D =
    R"json({"schema":1,"kind":"prefab","dimension":3,"parts":[{"id":"support","body":{"type":"Static","position":[0,0,0],"rotation":[0,0,0,1]}},{"id":"leaf","body":{"type":"Dynamic","position":[0,1,0],"rotation":[0,0,0,-1],"inertia":[1,2,3]},"colliders":[{"shape":"Box","halfExtents":[0.4,0.8,0.2]}]}],"joints":[{"id":"rail","kind":"Prismatic","bodyA":"support","bodyB":"leaf","frame":{"space":"Prefab","anchor":[0,1,0],"rotation":[0,0,0,1]},"drive":{"enabled":true,"speed":0.8,"maxForce":15}}],"exports":{"body":"leaf/body","drive":"rail"}})json";
// 改変は原文を保持し、別の要求だけに適用する。
Toolbox::FString Replace(const char* Text, const char* From, const char* To)
{
	const Toolbox::FString Source(Text);
	const Toolbox::FString Needle(From);
	for (Toolbox::size_t Index = 0; Index + Needle.Size() <= Source.Size(); ++Index)
	{
		if (Toolbox::FStringView(Source.Data() + Index, Needle.Size()) == From)
		{
			return Toolbox::FString(Source.Data(), Index) + To +
			       Toolbox::FString(Source.Data() + Index + Needle.Size(), Source.Size() - Index - Needle.Size());
		}
	}
	throw Toolbox::FException("Test replacement not found");
}
bool Reject2D(const Toolbox::FString& Text)
{
	try
	{
		Dxf::ParsePrefab2D({Text.Data(), Text.Size()}, "Assets/test.dxfprefab.json");
	}
	catch (const Toolbox::FException&)
	{
		return true;
	}
	return false;
}
// 失効や配列範囲の別エラーへ落ちる前に、公開先の種類違いを明示する。
template <typename TAction>
bool RejectExportKind(TAction&& Action)
{
	try
	{
		Action();
	}
	catch (const Toolbox::FException& Error)
	{
		return Toolbox::FStringView(Error.What()) == "Prefab export kind mismatch";
	}
	return false;
}
} // namespace
TEST("Content parses typed 2D forward references and instance parameters without shared changes")
{
	const auto Default = Dxf::ParsePrefab2D(Door2D, "Assets/door2d.dxfprefab.json");
	REQUIRE(Default.Parts.Size() == 2 && Default.Joints.Size() == 1);
	REQUIRE(Default.Joints[0].BodyA == 0 && Default.Joints[0].BodyB == 1);
	REQUIRE(Default.Joints[0].Prismatic.Drive.TargetSpeed == 0.8);
	REQUIRE(Default.Exports[1].Kind == Dxf::EContentExportKind::Prismatic);
	Dxf::FContentParameterValue Speed;
	Speed.Id = "speed";
	Speed.Number = -1.5;
	const auto Other = Dxf::ParsePrefab2D(Door2D, "Assets/door2d.dxfprefab.json", {Speed});
	REQUIRE(Other.Joints[0].Prismatic.Drive.TargetSpeed == -1.5);
	REQUIRE(Default.Joints[0].Prismatic.Drive.TargetSpeed == 0.8);
	REQUIRE(Dxf::ParsePrefab2D(Door2D).Joints[0].Prismatic.Drive.TargetSpeed == 0.8);
}
TEST("Content parses 3D Quaternion Frames with q and minus q equivalent")
{
	const auto Definition = Dxf::ParsePrefab3D(Door3D);
	REQUIRE(Definition.Parts.Size() == 2 && Definition.Joints.Size() == 1);
	REQUIRE(Definition.Parts[1].Body.DiagonalInertia.Y == 2);
	REQUIRE(Definition.Joints[0].Prismatic.FrameB.LocalAnchor.IsValid());
	REQUIRE(Toolbox::Abs(Definition.Joints[0].Prismatic.FrameB.LocalRotation.W) == 1);
}
TEST("Content rejects version dimension unknown fields IDs references shapes and drive units")
{
	const char* From[] = {"\"schema\":1", "\"dimension\":2", "\"kind\":\"prefab\"", "\"type\":\"Static\"",
	                      "\"id\":\"leaf\"", "\"bodyB\":\"leaf\"", "\"bodyB\":\"leaf\"", "\"halfExtents\":[0.4,0.8]",
	                      "\"maxForce\":15", "\"position\":[0,1]"};
	const char* To[] = {
	    "\"schema\":2", "\"dimension\":3", "\"kind\":\"unknown\"", "\"type\":\"Dynamic\",\"unknown\":0",
	    "\"id\":\"frame\"", "\"bodyB\":\"missing\"", "\"bodyB\":\"frame\"", "\"halfExtents\":[-1,0.8]",
	    "\"maxTorque\":15", "\"position\":[1e39,1]"};
	for (Toolbox::size_t Index = 0; Index < sizeof(From) / sizeof(From[0]); ++Index)
	{
		REQUIRE(Reject2D(Replace(Door2D, From[Index], To[Index])));
	}
	REQUIRE(Reject2D(Replace(Door2D, "\"frame\"", "\"9frame\"")));
	REQUIRE(Reject2D(Replace(Door2D, "\"doorBody\":\"leaf/body\"", "\"doorBody\":\"leaf\"")));
}
TEST("Content preserves path and position diagnostics and exact part joint limits")
{
	try
	{
		const auto Text = Replace(Door2D, "\"maxForce\":15", "\"maxForce\":-1");
		Dxf::ParsePrefab2D(Text, "Assets/扉.dxfprefab.json");
		REQUIRE(false);
	}
	catch (const Dxf::FSceneContentError& Error)
	{
		REQUIRE(Error.GetDiagnostic().Path == "Assets/扉.dxfprefab.json");
		REQUIRE(Error.GetDiagnostic().Line == 1 && Error.GetDiagnostic().Column > 1);
	}
	Dxf::FSceneContentLimits Limits;
	Limits.MaxParts = 1;
	bool Failed = false;
	try
	{
		Dxf::ParsePrefab2D(Door2D, {}, {}, Limits);
	}
	catch (const Toolbox::FException&)
	{
		Failed = true;
	}
	REQUIRE(Failed);
	Limits.MaxParts = 2;
	Limits.MaxJoints = 1;
	REQUIRE(Dxf::ParsePrefab2D(Door2D, {}, {}, Limits).Joints.Size() == 1);
}
TEST("Content rejects invalid defaults integer tokens irrelevant options and sensor exports")
{
	REQUIRE(Reject2D(Replace(Door2D, "\"schema\":1", "\"schema\":1.0")));
	REQUIRE(Reject2D(Replace(Door2D, "\"doorBody\":\"leaf/body\"", "\"doorBody\":\"leaf/sensor\"")));
	REQUIRE(Reject2D(Replace(Door2D, "\"parts\":[", "\"assets\":{\"t\":{\"kind\":\"Texture\",\"path\":\"Assets/a.png\",\"storage\":\"Memory\"}},\"parts\":[")));
	Dxf::FContentParameterValue Speed;
	Speed.Id = "speed";
	Speed.Number = 0;
	bool InvalidDefault = false;
	try
	{
		Dxf::ParsePrefab2D(Replace(Door2D, "\"default\":0.8", "\"default\":9"), {}, {Speed});
	}
	catch (const Toolbox::FException&)
	{
		InvalidDefault = true;
	}
	REQUIRE(InvalidDefault);
	try
	{
		Dxf::ParsePrefab2D(Replace(Door2D, "\"halfExtents\":[0.4,0.8]", "\"halfExtents\":[-1,0.8]"), "Assets/invalid.json");
		REQUIRE(false);
	}
	catch (const Dxf::FSceneContentError& E)
	{
		REQUIRE(E.GetDiagnostic().Location == "$/parts[1]/colliders[0]");
	}
	try
	{
		Dxf::ParsePrefab2D("{\n\"schema\":", "Assets/truncated.json");
		REQUIRE(false);
	}
	catch (const Dxf::FSceneContentError& E)
	{
		REQUIRE(E.GetDiagnostic().Path == "Assets/truncated.json");
		REQUIRE(E.GetDiagnostic().Line == 2);
	}
}
TEST("Content owner resource preparation shares conditions and cleans only failed references")
{
	Dxf::Testing::FFakeBackend Backend;
	Dxf::FAssetService Assets{Backend, Backend, Backend};
	Dxf::FContentAssetDefinition Image;
	Image.Id = "image";
	Image.Path = "Assets/image.png";
	Dxf::FContentAssetDefinition Cue;
	Cue.Id = "cue";
	Cue.Kind = Dxf::EContentAssetKind::Sound;
	Cue.Path = "Assets/cue.wav";
	const auto Existing = Dxf::FContentResources::Prepare({Image}, Assets, "first.json");
	REQUIRE(Backend.GetTrace().TextureLoads == 1);
	Backend.GetTrace().bFailSound = true;
	bool Failed = false;
	try
	{
		Dxf::FContentResources::Prepare({Image, Cue}, Assets, "second.json");
	}
	catch (const Dxf::FSceneContentError& E)
	{
		Failed = E.GetDiagnostic().Key == "cue";
		REQUIRE(E.GetDiagnostic().Line == 0 && E.GetDiagnostic().Column == 0);
	}
	REQUIRE(Failed);
	REQUIRE(Existing.GetTexture(0).IsValid());
	REQUIRE(Backend.GetTrace().TextureLoads == 1);
	REQUIRE(Backend.GetTrace().DeletedTextures.IsEmpty());
	Backend.GetTrace().bFailSound = false;
	{
		const auto Retry = Dxf::FContentResources::Prepare({Image, Cue}, Assets, "second.json");
		REQUIRE(Retry.GetSound(1).IsValid());
		REQUIRE(Retry.GetTexture(0).GetNativeHandle_Internal() == Existing.GetTexture(0).GetNativeHandle_Internal());
	}
	REQUIRE(Backend.GetTrace().DeletedSounds.Size() == 1);
	Image.Texture.bUse3D = false;
	const auto DifferentOptions = Dxf::FContentResources::Prepare({Image}, Assets, "third.json");
	REQUIRE(DifferentOptions.GetTexture(0).GetNativeHandle_Internal() != Existing.GetTexture(0).GetNativeHandle_Internal());
	REQUIRE(Backend.GetTrace().TextureLoads == 2);
	// 共有先がない画像だけを、後続のSound失敗で回収する。
	Image.Path = "Assets/unique.png";
	Backend.GetTrace().bFailSound = true;
	try
	{
		Dxf::FContentResources::Prepare({Image, Cue}, Assets, "failed.json");
		REQUIRE(false);
	}
	catch (const Dxf::FSceneContentError&)
	{
	}
	REQUIRE(Backend.GetTrace().DeletedTextures.Size() == 1);
}

namespace
{
// 描画・I/O境界だけ置換し、SceneとComponentとWorldは製品を使う。
template <typename TScene>
struct TContentWorld
{
	Dxf::Testing::FFakeBackend Backend;
	Dxf::FAssetService Assets{Backend, Backend, Backend};
	Dxf::FInputStateTracker Input;
	TScene Scene;
	~TContentWorld()
	{
		Scene.Shutdown_Internal();
	}
	Dxf::TResult<void> Tick(Toolbox::f64 Seconds = 1.0 / 60.0)
	{
		Scene.GetChildren_Internal()->FreezeBoundary_Internal();
		const auto Committed = Scene.GetChildren_Internal()->CommitBoundary_Internal({Assets});
		if (!Committed)
		{
			return Committed;
		}
		Dxf::FFrameTime Time;
		Time.DeltaSeconds = Seconds;
		Time.UnscaledDeltaSeconds = Seconds;
		return Scene.Tick_Internal({Input.GetSnapshot(), Time});
	}
};
} // namespace

TEST("Content 2D real prefab spawns pending then ready with independent drive and lifetime")
{
	TContentWorld<Dxf::DPhysicsScene2D> F;
	auto Prepared = Dxf::PreparePrefab(Dxf::ParsePrefab2D(Door2D), F.Assets);
	Dxf::FPrefabSpawnOptions2D Placement;
	Placement.Position.X = 10;
	Placement.Rotation = static_cast<Toolbox::f32>(1.5707963267948966);
	const auto A = F.Scene.Spawn<Dxf::DPrefabInstance2D>(Prepared);
	const auto B = F.Scene.Spawn<Dxf::DPrefabInstance2D>(Prepared, Placement);
	REQUIRE(A && B);
	REQUIRE(A.Value().Get()->GetState() == Dxf::EPrefabInstanceState::PendingInitialization);
	REQUIRE(F.Scene.Initialize_Internal({F.Assets}));
	REQUIRE(A.Value().Get()->GetState() == Dxf::EPrefabInstanceState::PendingPhysics);
	REQUIRE(F.Tick(0));
	REQUIRE(A.Value().Get()->GetState() == Dxf::EPrefabInstanceState::PendingPhysics);
	REQUIRE(F.Tick());
	REQUIRE(A.Value().Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	REQUIRE(B.Value().Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	const auto BodyA = A.Value().Get()->GetRigidBody("doorBody");
	const auto BodyB = B.Value().Get()->GetRigidBody("doorBody");
	const auto DriveA = A.Value().Get()->GetPrismaticJoint("doorDrive");
	const auto DriveB = B.Value().Get()->GetPrismaticJoint("doorDrive");
	REQUIRE(RejectExportKind([&]()
	                         {
		                         A.Value().Get()->GetPrismaticJoint("doorBody");
	                         }));
	REQUIRE(RejectExportKind([&]()
	                         {
		                         A.Value().Get()->GetRigidBody("doorDrive");
	                         }));
	REQUIRE(BodyA.Get()->GetBodyId() != BodyB.Get()->GetBodyId());
	const auto InitialB = F.Scene.GetPhysicsWorld().GetPosition(BodyB.Get()->GetBodyId());
	// Scene内の最初のStatic Bodyへ誤って取り付けていないことも実問い合わせで検出する。
	auto RayStart = InitialB;
	auto RayEnd = InitialB;
	RayStart.X -= 3;
	RayEnd.X += 3;
	const auto ColliderHit = F.Scene.GetPhysicsWorld().RaycastClosest(RayStart, RayEnd);
	REQUIRE(ColliderHit && ColliderHit->Collider.Body == BodyB.Get()->GetBodyId());
	// 原点のx=0,y=1を90度配置するとx=9,y=0。ColliderとLocal Frameは回さない。
	REQUIRE(Toolbox::Abs(InitialB.X - 9) < 0.01f);
	REQUIRE(Toolbox::Abs(InitialB.Y) < 0.03f);
	const auto OldIdB = DriveB.Get()->GetJointId();
	Dxf::FLinearJointDrive Stop;
	Stop.bEnabled = true;
	Stop.TargetSpeed = 0;
	Stop.MaxForce = 15;
	DriveA.Get()->RequestDrive(Stop);
	for (Toolbox::uint32 I = 0; I < 30; ++I)
	{
		REQUIRE(F.Tick());
	}
	REQUIRE(DriveA.Get()->GetObservation());
	REQUIRE(DriveB.Get()->GetObservation());
	REQUIRE(DriveA.Get()->GetObservation()->State.Translation < 0.02);
	REQUIRE(DriveB.Get()->GetObservation()->State.Translation > 0.3);
	REQUIRE(DriveB.Get()->GetJointId() && OldIdB && *DriveB.Get()->GetJointId() == *OldIdB);
	bool WrongType = false;
	try
	{
		A.Value().Get()->GetFixedJoint("doorDrive");
	}
	catch (const Toolbox::FException&)
	{
		WrongType = true;
	}
	REQUIRE(WrongType);
	const auto DestroyedBody = BodyA.Get()->GetBodyId();
	A.Value().Get()->Destroy();
	REQUIRE(!A.Value());
	REQUIRE(!BodyA);
	REQUIRE(F.Tick());
	REQUIRE(!F.Scene.GetPhysicsWorld().IsAlive(DestroyedBody));
	REQUIRE(B.Value().Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	REQUIRE(DriveB.Get()->GetJointId() && OldIdB && *DriveB.Get()->GetJointId() == *OldIdB);
	// Worldからの外部破棄は新世代へ接続し直さない。
	const auto OldBody = BodyB.Get()->GetBodyId();
	REQUIRE(F.Scene.GetPhysicsWorld().DestroyBody(OldBody));
	REQUIRE(B.Value().Get()->GetState() == Dxf::EPrefabInstanceState::EndpointLost);
}

TEST("Content 3D real prefab spawns pending then ready with independent drive and lifetime")
{
	TContentWorld<Dxf::DPhysicsScene3D> F;
	auto Prepared = Dxf::PreparePrefab(Dxf::ParsePrefab3D(Door3D), F.Assets);
	Dxf::FPrefabSpawnOptions3D Placement;
	Placement.Position.X = 10;
	Placement.Rotation = Toolbox::FQuaternion{0, 0, 0.7071067811865475f, 0.7071067811865475f};
	const auto A = F.Scene.Spawn<Dxf::DPrefabInstance3D>(Prepared);
	const auto B = F.Scene.Spawn<Dxf::DPrefabInstance3D>(Prepared, Placement);
	REQUIRE(A && B);
	REQUIRE(A.Value().Get()->GetState() == Dxf::EPrefabInstanceState::PendingInitialization);
	REQUIRE(F.Scene.Initialize_Internal({F.Assets}));
	REQUIRE(A.Value().Get()->GetState() == Dxf::EPrefabInstanceState::PendingPhysics);
	REQUIRE(F.Tick(0));
	REQUIRE(A.Value().Get()->GetState() == Dxf::EPrefabInstanceState::PendingPhysics);
	REQUIRE(F.Tick());
	REQUIRE(A.Value().Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	REQUIRE(B.Value().Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	const auto BodyA = A.Value().Get()->GetRigidBody("body");
	const auto BodyB = B.Value().Get()->GetRigidBody("body");
	const auto DriveA = A.Value().Get()->GetPrismaticJoint("drive");
	const auto DriveB = B.Value().Get()->GetPrismaticJoint("drive");
	REQUIRE(RejectExportKind([&]()
	                         {
		                         A.Value().Get()->GetPrismaticJoint("body");
	                         }));
	REQUIRE(RejectExportKind([&]()
	                         {
		                         A.Value().Get()->GetRigidBody("drive");
	                         }));
	REQUIRE(BodyA.Get()->GetBodyId() != BodyB.Get()->GetBodyId());
	const auto InitialB = F.Scene.GetPhysicsWorld().GetPosition(BodyB.Get()->GetBodyId());
	// Scene内の最初のStatic Bodyへ誤って取り付けていないことも実問い合わせで検出する。
	auto RayStart = InitialB;
	auto RayEnd = InitialB;
	RayStart.X -= 3;
	RayEnd.X += 3;
	const auto ColliderHit = F.Scene.GetPhysicsWorld().RaycastClosest(RayStart, RayEnd);
	REQUIRE(ColliderHit && ColliderHit->Collider.Body == BodyB.Get()->GetBodyId());
	// 原点のx=0,y=1を90度配置するとx=9,y=0。ColliderとLocal Frameは回さない。
	REQUIRE(Toolbox::Abs(InitialB.X - 9) < 0.01f);
	REQUIRE(Toolbox::Abs(InitialB.Y) < 0.03f);
	const auto OldIdB = DriveB.Get()->GetJointId();
	Dxf::FLinearJointDrive Stop;
	Stop.bEnabled = true;
	Stop.TargetSpeed = 0;
	Stop.MaxForce = 15;
	DriveA.Get()->RequestDrive(Stop);
	for (Toolbox::uint32 I = 0; I < 30; ++I)
	{
		REQUIRE(F.Tick());
	}
	REQUIRE(DriveA.Get()->GetObservation());
	REQUIRE(DriveB.Get()->GetObservation());
	REQUIRE(DriveA.Get()->GetObservation()->State.Translation < 0.02);
	REQUIRE(DriveB.Get()->GetObservation()->State.Translation > 0.3);
	REQUIRE(DriveB.Get()->GetJointId() && OldIdB && *DriveB.Get()->GetJointId() == *OldIdB);
	bool WrongType = false;
	try
	{
		A.Value().Get()->GetFixedJoint("drive");
	}
	catch (const Toolbox::FException&)
	{
		WrongType = true;
	}
	REQUIRE(WrongType);
	const auto DestroyedBody = BodyA.Get()->GetBodyId();
	A.Value().Get()->Destroy();
	REQUIRE(!A.Value());
	REQUIRE(!BodyA);
	REQUIRE(F.Tick());
	REQUIRE(!F.Scene.GetPhysicsWorld().IsAlive(DestroyedBody));
	REQUIRE(B.Value().Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	REQUIRE(DriveB.Get()->GetJointId() && OldIdB && *DriveB.Get()->GetJointId() == *OldIdB);
	// Worldからの外部破棄は新世代へ接続し直さない。
	const auto OldBody = BodyB.Get()->GetBodyId();
	REQUIRE(F.Scene.GetPhysicsWorld().DestroyBody(OldBody));
	REQUIRE(B.Value().Get()->GetState() == Dxf::EPrefabInstanceState::EndpointLost);
}

TEST("Content 2D reads Scene data from explicit Root and initializes real instances")
{
	Dxf::FSceneContentSource Source{Toolbox::FPath(DXF_CONTENT_TEST_ROOT)};
	const auto A = Source.LoadScene2D("scene2d-a.dxfscene.json");
	const auto B = Source.LoadScene2D("scene2d-b.dxfscene.json");
	REQUIRE(A.Prefabs.Size() == 2 && B.Prefabs.Size() == 2);
	REQUIRE(A.Placements[1].Position.X == 10 && B.Placements[1].Position.X == 20);
	REQUIRE(A.Prefabs[0].Joints[0].Prismatic.Drive.TargetSpeed == 0);
	REQUIRE(A.Prefabs[1].Joints[0].Prismatic.Drive.TargetSpeed == 0.8);
	REQUIRE(B.Prefabs[1].Joints[0].Prismatic.Drive.TargetSpeed == -0.4);
	// Worldや資源を作る前に二つの定義が異なることを確認する。
	Dxf::Testing::FFakeBackend Backend;
	Dxf::FAssetService Assets{Backend, Backend, Backend};
	const auto Prepared = Dxf::PrepareScene(A, Assets);
	Dxf::DContentScene2D Scene(Prepared);
	struct FShutdown
	{
		Dxf::DContentScene2D& Scene;
		~FShutdown()
		{
			Scene.Shutdown_Internal();
		}
	} Guard{Scene};
	REQUIRE(Scene.Initialize_Internal({Assets}));
	REQUIRE(Scene.GetObjectCount() == 2);
	const auto Left = Scene.GetPrefab("doorA");
	const auto Right = Scene.GetPrefab("doorB");
	REQUIRE(Left.Get()->GetState() == Dxf::EPrefabInstanceState::PendingPhysics);
	Dxf::FInputStateTracker Input;
	Dxf::FFrameTime Time;
	Time.DeltaSeconds = 1.0 / 60.0;
	Time.UnscaledDeltaSeconds = Time.DeltaSeconds;
	for (Toolbox::uint32 I = 0; I < 30; ++I)
	{
		REQUIRE(Scene.Tick_Internal({Input.GetSnapshot(), Time}));
	}
	REQUIRE(Left.Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	REQUIRE(Right.Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	REQUIRE(Left.Get()->GetPrismaticJoint("doorDrive").Get()->GetObservation()->State.Translation < 0.01);
	REQUIRE(Right.Get()->GetPrismaticJoint("doorDrive").Get()->GetObservation()->State.Translation > 0.3);
	REQUIRE(Scene.GetPhysicsWorld().GetPosition(Right.Get()->GetRigidBody("doorBody").Get()->GetBodyId()).X > 10.3);
	// 同じ要求内では二つの個体でもファイルはSceneとPrefabの2つだけ。
	Dxf::FSceneContentLimits Limits;
	Limits.MaxFiles = 2;
	REQUIRE(Dxf::FSceneContentSource(Toolbox::FPath(DXF_CONTENT_TEST_ROOT), Limits).LoadScene2D("scene2d-a.dxfscene.json").Instances.Size() == 2);
	Limits.MaxFiles = 1;
	bool Rejected = false;
	try
	{
		Dxf::FSceneContentSource(Toolbox::FPath(DXF_CONTENT_TEST_ROOT), Limits).LoadScene2D("scene2d-a.dxfscene.json");
	}
	catch (const Dxf::FSceneContentError&)
	{
		Rejected = true;
	}
	REQUIRE(Rejected);
	Limits.MaxFiles = 2;
	Limits.MaxParts = 3;
	Rejected = false;
	try
	{
		Dxf::FSceneContentSource(Toolbox::FPath(DXF_CONTENT_TEST_ROOT), Limits).LoadScene2D("scene2d-a.dxfscene.json");
	}
	catch (const Dxf::FSceneContentError&)
	{
		Rejected = true;
	}
	REQUIRE(Rejected);
	for (const char* Path : {"../door2d.dxfprefab.json", "C:door.json", "/door.json"})
	{
		Rejected = false;
		try
		{
			Source.LoadPrefab2D(Path);
		}
		catch (const Dxf::FSceneContentError&)
		{
			Rejected = true;
		}
		REQUIRE(Rejected);
	}
}

TEST("Content 3D reads Scene data from explicit Root and initializes real instances")
{
	Dxf::FSceneContentSource Source{Toolbox::FPath(DXF_CONTENT_TEST_ROOT)};
	const auto A = Source.LoadScene3D("scene3d-a.dxfscene.json");
	const auto B = Source.LoadScene3D("scene3d-b.dxfscene.json");
	REQUIRE(A.Prefabs.Size() == 2 && B.Prefabs.Size() == 2);
	REQUIRE(A.Placements[1].Position.X == 10 && B.Placements[1].Position.X == 20);
	REQUIRE(A.Prefabs[0].Joints[0].Prismatic.Drive.TargetSpeed == 0);
	REQUIRE(A.Prefabs[1].Joints[0].Prismatic.Drive.TargetSpeed == 0.8);
	REQUIRE(B.Prefabs[1].Joints[0].Prismatic.Drive.TargetSpeed == -0.4);
	// Worldや資源を作る前に二つの定義が異なることを確認する。
	Dxf::Testing::FFakeBackend Backend;
	Dxf::FAssetService Assets{Backend, Backend, Backend};
	const auto Prepared = Dxf::PrepareScene(A, Assets);
	Dxf::DContentScene3D Scene(Prepared);
	struct FShutdown
	{
		Dxf::DContentScene3D& Scene;
		~FShutdown()
		{
			Scene.Shutdown_Internal();
		}
	} Guard{Scene};
	REQUIRE(Scene.Initialize_Internal({Assets}));
	REQUIRE(Scene.GetObjectCount() == 2);
	const auto Left = Scene.GetPrefab("doorA");
	const auto Right = Scene.GetPrefab("doorB");
	REQUIRE(Left.Get()->GetState() == Dxf::EPrefabInstanceState::PendingPhysics);
	Dxf::FInputStateTracker Input;
	Dxf::FFrameTime Time;
	Time.DeltaSeconds = 1.0 / 60.0;
	Time.UnscaledDeltaSeconds = Time.DeltaSeconds;
	for (Toolbox::uint32 I = 0; I < 30; ++I)
	{
		REQUIRE(Scene.Tick_Internal({Input.GetSnapshot(), Time}));
	}
	REQUIRE(Left.Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	REQUIRE(Right.Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	REQUIRE(Left.Get()->GetPrismaticJoint("drive").Get()->GetObservation()->State.Translation < 0.01);
	REQUIRE(Right.Get()->GetPrismaticJoint("drive").Get()->GetObservation()->State.Translation > 0.3);
	REQUIRE(Scene.GetPhysicsWorld().GetPosition(Right.Get()->GetRigidBody("body").Get()->GetBodyId()).X > 10.3);
	// 同じ要求内では二つの個体でもファイルはSceneとPrefabの2つだけ。
	Dxf::FSceneContentLimits Limits;
	Limits.MaxFiles = 2;
	REQUIRE(Dxf::FSceneContentSource(Toolbox::FPath(DXF_CONTENT_TEST_ROOT), Limits).LoadScene3D("scene3d-a.dxfscene.json").Instances.Size() == 2);
	Limits.MaxFiles = 1;
	bool Rejected = false;
	try
	{
		Dxf::FSceneContentSource(Toolbox::FPath(DXF_CONTENT_TEST_ROOT), Limits).LoadScene3D("scene3d-a.dxfscene.json");
	}
	catch (const Dxf::FSceneContentError&)
	{
		Rejected = true;
	}
	REQUIRE(Rejected);
	Limits.MaxFiles = 2;
	Limits.MaxParts = 3;
	Rejected = false;
	try
	{
		Dxf::FSceneContentSource(Toolbox::FPath(DXF_CONTENT_TEST_ROOT), Limits).LoadScene3D("scene3d-a.dxfscene.json");
	}
	catch (const Dxf::FSceneContentError&)
	{
		Rejected = true;
	}
	REQUIRE(Rejected);
	for (const char* Path : {"../door3d.dxfprefab.json", "C:door.json", "/door.json"})
	{
		Rejected = false;
		try
		{
			Source.LoadPrefab3D(Path);
		}
		catch (const Dxf::FSceneContentError&)
		{
			Rejected = true;
		}
		REQUIRE(Rejected);
	}
}

TEST("Content 2D adapts all four existing joint components and preserves explicit disconnect")
{
	for (const auto Kind : {Dxf::EJointKind::Distance, Dxf::EJointKind::Revolute, Dxf::EJointKind::Fixed, Dxf::EJointKind::Prismatic})
	{
		TContentWorld<Dxf::DPhysicsScene2D> F;
		auto Definition = Dxf::ParsePrefab2D(Door2D);
		auto& J = Definition.Joints[0];
		J.Kind = Kind;
		J.Distance.Length = 1;
		J.Revolute.FrameA = J.Prismatic.FrameA;
		J.Revolute.FrameB = J.Prismatic.FrameB;
		J.Fixed.FrameA = J.Prismatic.FrameA;
		J.Fixed.FrameB = J.Prismatic.FrameB;
		J.Prismatic.Drive.bEnabled = false;
		J.Prismatic.Limits.bEnabled = false;
		Definition.Exports[1].Kind = Kind == Dxf::EJointKind::Distance   ? Dxf::EContentExportKind::Distance
		                             : Kind == Dxf::EJointKind::Revolute ? Dxf::EContentExportKind::Revolute
		                             : Kind == Dxf::EJointKind::Fixed    ? Dxf::EContentExportKind::Fixed
		                                                                 : Dxf::EContentExportKind::Prismatic;
		const auto Spawned =
		    F.Scene.Spawn<Dxf::DPrefabInstance2D>(Dxf::PreparePrefab(Toolbox::Move(Definition), F.Assets));
		REQUIRE(Spawned);
		REQUIRE(F.Scene.Initialize_Internal({F.Assets}));
		REQUIRE(F.Tick());
		const auto P = Spawned.Value();
		REQUIRE(P.Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
		Dxf::FJointId2D Id;
		switch (Kind)
		{
		case Dxf::EJointKind::Distance:
			Id = *P.Get()->GetDistanceJoint("doorDrive").Get()->GetJointId();
			P.Get()->GetDistanceJoint("doorDrive").Get()->RequestDisconnect();
			break;
		case Dxf::EJointKind::Revolute:
			Id = *P.Get()->GetRevoluteJoint("doorDrive").Get()->GetJointId();
			P.Get()->GetRevoluteJoint("doorDrive").Get()->RequestDisconnect();
			break;
		case Dxf::EJointKind::Fixed:
			Id = *P.Get()->GetFixedJoint("doorDrive").Get()->GetJointId();
			P.Get()->GetFixedJoint("doorDrive").Get()->RequestDisconnect();
			break;
		case Dxf::EJointKind::Prismatic:
			Id = *P.Get()->GetPrismaticJoint("doorDrive").Get()->GetJointId();
			P.Get()->GetPrismaticJoint("doorDrive").Get()->RequestDisconnect();
			break;
		}
		REQUIRE(F.Scene.GetPhysicsWorld().IsJointAlive(Id));
		REQUIRE(F.Tick());
		REQUIRE(!F.Scene.GetPhysicsWorld().IsJointAlive(Id));
		REQUIRE(P.Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	}
}

TEST("Content 3D adapts all four existing joint components and preserves explicit disconnect")
{
	for (const auto Kind : {Dxf::EJointKind::Distance, Dxf::EJointKind::Revolute, Dxf::EJointKind::Fixed, Dxf::EJointKind::Prismatic})
	{
		TContentWorld<Dxf::DPhysicsScene3D> F;
		auto Definition = Dxf::ParsePrefab3D(Door3D);
		auto& J = Definition.Joints[0];
		J.Kind = Kind;
		J.Distance.Length = 1;
		J.Revolute.FrameA = J.Prismatic.FrameA;
		J.Revolute.FrameB = J.Prismatic.FrameB;
		J.Fixed.FrameA = J.Prismatic.FrameA;
		J.Fixed.FrameB = J.Prismatic.FrameB;
		J.Prismatic.Drive.bEnabled = false;
		J.Prismatic.Limits.bEnabled = false;
		Definition.Exports[1].Kind = Kind == Dxf::EJointKind::Distance   ? Dxf::EContentExportKind::Distance
		                             : Kind == Dxf::EJointKind::Revolute ? Dxf::EContentExportKind::Revolute
		                             : Kind == Dxf::EJointKind::Fixed    ? Dxf::EContentExportKind::Fixed
		                                                                 : Dxf::EContentExportKind::Prismatic;
		const auto Spawned =
		    F.Scene.Spawn<Dxf::DPrefabInstance3D>(Dxf::PreparePrefab(Toolbox::Move(Definition), F.Assets));
		REQUIRE(Spawned);
		REQUIRE(F.Scene.Initialize_Internal({F.Assets}));
		REQUIRE(F.Tick());
		const auto P = Spawned.Value();
		REQUIRE(P.Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
		Dxf::FJointId3D Id;
		switch (Kind)
		{
		case Dxf::EJointKind::Distance:
			Id = *P.Get()->GetDistanceJoint("drive").Get()->GetJointId();
			P.Get()->GetDistanceJoint("drive").Get()->RequestDisconnect();
			break;
		case Dxf::EJointKind::Revolute:
			Id = *P.Get()->GetRevoluteJoint("drive").Get()->GetJointId();
			P.Get()->GetRevoluteJoint("drive").Get()->RequestDisconnect();
			break;
		case Dxf::EJointKind::Fixed:
			Id = *P.Get()->GetFixedJoint("drive").Get()->GetJointId();
			P.Get()->GetFixedJoint("drive").Get()->RequestDisconnect();
			break;
		case Dxf::EJointKind::Prismatic:
			Id = *P.Get()->GetPrismaticJoint("drive").Get()->GetJointId();
			P.Get()->GetPrismaticJoint("drive").Get()->RequestDisconnect();
			break;
		}
		REQUIRE(F.Scene.GetPhysicsWorld().IsJointAlive(Id));
		REQUIRE(F.Tick());
		REQUIRE(!F.Scene.GetPhysicsWorld().IsJointAlive(Id));
		REQUIRE(P.Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	}
}

namespace
{
// 同じPrefabを二回展開しても、Body、設定、公開先を個体ごとに保持する。
template <typename TScene, typename TPrefab>
void CheckNestedPrefab()
{
	TContentWorld<TScene> F;
	Dxf::FSceneContentSource Source{Toolbox::FPath(DXF_CONTENT_TEST_ROOT)};
	const auto Definition = [&Source]()
	{
		if constexpr (Toolbox::IsSame<TPrefab, Dxf::DPrefabInstance2D>)
		{
			return Source.LoadPrefab2D("pair2d.dxfprefab.json");
		}
		else
		{
			return Source.LoadPrefab3D("pair3d.dxfprefab.json");
		}
	}();
	REQUIRE(Definition.ExpandedPrefabCount == 3);
	REQUIRE(Definition.Parts.Size() == 4 && Definition.Joints.Size() == 2);
	REQUIRE(Definition.Joints[0].Prismatic.Drive.TargetSpeed == 0);
	REQUIRE(Definition.Joints[1].Prismatic.Drive.TargetSpeed == 0.8);
	const auto Spawn = F.Scene.template Spawn<TPrefab>(Dxf::PreparePrefab(Definition, F.Assets));
	REQUIRE(Spawn);
	REQUIRE(F.Scene.Initialize_Internal({F.Assets}));
	REQUIRE(F.Tick());
	const auto P = Spawn.Value();
	REQUIRE(P.Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	const auto A = P.Get()->GetRigidBody("leftBody");
	const auto B = P.Get()->GetRigidBody("rightBody");
	REQUIRE(A.Get()->GetBodyId() != B.Get()->GetBodyId());
	const auto Position = F.Scene.GetPhysicsWorld().GetPosition(A.Get()->GetBodyId());
	if constexpr (Toolbox::IsSame<TPrefab, Dxf::DPrefabInstance2D>)
	{
		REQUIRE(Toolbox::Abs(Position.X - 9) < 0.001);
		REQUIRE(Toolbox::Abs(Position.Y) < 0.001);
	}
	else
	{
		REQUIRE(Toolbox::Abs(Position.X - 10) < 0.001);
		REQUIRE(Toolbox::Abs(Position.Y) < 0.001);
		REQUIRE(Toolbox::Abs(Position.Z - 1) < 0.001);
	}
	const auto Left = P.Get()->GetPrismaticJoint("leftDrive");
	const auto Right = P.Get()->GetPrismaticJoint("rightDrive");
	for (Toolbox::uint32 I = 0; I < 30; ++I)
	{
		REQUIRE(F.Tick());
	}
	REQUIRE(Left.Get()->GetObservation()->State.Translation < 0.001);
	REQUIRE(Right.Get()->GetObservation()->State.Translation > 0.3);
	REQUIRE(Left.Get()->GetObservation()->State.AnchorError < 0.001);
}
template <typename TLoad>
void CheckPrefabGraph(TLoad Load, const char* Dimension)
{
	Dxf::FSceneContentLimits Limits;
	// 同じ子定義を複数利用するDAGは循環ではない。
	REQUIRE(Load(Toolbox::FString("pair") + Dimension + ".dxfprefab.json", Limits).Parts.Size() == 4);
	const auto Bridge = Load(Toolbox::FString("bridge") + Dimension + ".dxfprefab.json", Limits);
	REQUIRE(Bridge.Joints.Size() == 3);
	REQUIRE(Bridge.Joints[2].BodyA == 1 && Bridge.Joints[2].BodyB == 3);
	for (const char* Name : {"private", "wrongtype", "cycle-a", "direct"})
	{
		bool Rejected = false;
		try
		{
			Load(Toolbox::FString(Name) + Dimension + ".dxfprefab.json", Limits);
		}
		catch (const Dxf::FSceneContentError& E)
		{
			Rejected = true;
			REQUIRE(!E.GetDiagnostic().Path.IsEmpty());
			REQUIRE(!E.GetDiagnostic().Location.IsEmpty());
		}
		REQUIRE(Rejected);
	}
	REQUIRE(Load(Toolbox::FString("depth-1-") + Dimension + ".dxfprefab.json", Limits).ExpandedPrefabCount == 16);
	bool Rejected = false;
	try
	{
		Load(Toolbox::FString("depth-0-") + Dimension + ".dxfprefab.json", Limits);
	}
	catch (const Dxf::FSceneContentError&)
	{
		Rejected = true;
	}
	REQUIRE(Rejected);
	// 空のPrefabでも展開総数を制限する。
	Limits.MaxParts = 3;
	Rejected = false;
	try
	{
		Load(Toolbox::FString("depth-1-") + Dimension + ".dxfprefab.json", Limits);
	}
	catch (const Dxf::FSceneContentError&)
	{
		Rejected = true;
	}
	REQUIRE(Rejected);
}
} // namespace
TEST("Content 2D nested DAG creates independent rotated bodies and local joint frames")
{
	CheckNestedPrefab<Dxf::DPhysicsScene2D, Dxf::DPrefabInstance2D>();
}
TEST("Content 3D nested DAG creates independent 120 degree bodies and local joint frames")
{
	CheckNestedPrefab<Dxf::DPhysicsScene3D, Dxf::DPrefabInstance3D>();
}
TEST("Content 2D bounds nested references and hides child internal names")
{
	CheckPrefabGraph(
	    [](Toolbox::FString Path, Dxf::FSceneContentLimits Limits)
	    {
		    return Dxf::FSceneContentSource(Toolbox::FPath(DXF_CONTENT_TEST_ROOT), Limits).LoadPrefab2D(Path);
	    },
	    "2d");
}
TEST("Content 3D bounds nested references and hides child internal names")
{
	CheckPrefabGraph(
	    [](Toolbox::FString Path, Dxf::FSceneContentLimits Limits)
	    {
		    return Dxf::FSceneContentSource(Toolbox::FPath(DXF_CONTENT_TEST_ROOT), Limits).LoadPrefab3D(Path);
	    },
	    "3d");
}

namespace
{
// 手書きの型付き値がNative準備へ到達する前に拒否されることも守る。
template <typename T>
void CheckTypedValidation(T Definition)
{
	Dxf::ValidatePrefabDefinition(Definition);
	const auto Reject = [](const auto& Value)
	{
		bool Failed = false;
		try
		{
			Dxf::ValidatePrefabDefinition(Value);
		}
		catch (const Dxf::FSceneContentError&)
		{
			Failed = true;
		}
		REQUIRE(Failed);
	};
	auto Bad = Definition;
	Bad.Joints[0].BodyB = 999999;
	Reject(Bad);
	Bad = Definition;
	Bad.Exports[0].Index = 999999;
	Reject(Bad);
	Bad = Definition;
	Bad.Exports[0].Kind = Dxf::EContentExportKind::Model;
	Reject(Bad);
	Bad = Definition;
	Bad.Parts[1].Body.Mass = -1;
	Reject(Bad);
	Bad = Definition;
	Bad.Parts[1].Visual.Kind = Dxf::EContentVisualKind::Model;
	Bad.Parts[1].Visual.Asset = 999999;
	Reject(Bad);
	Bad = Definition;
	Bad.Parts[1].Id = Bad.Parts[0].Id;
	Reject(Bad);
	Bad = Definition;
	Dxf::FContentAssetDefinition Asset;
	Asset.Id = "outside";
	Asset.Path = "Assets/../../outside.bmp";
	Bad.Assets.PushBack(Asset);
	Reject(Bad);
}
} // namespace
TEST("Content typed 2D and 3D definitions reject invalid values and indices before resource preparation")
{
	Dxf::FSceneContentSource Source{Toolbox::FPath(DXF_CONTENT_TEST_ROOT)};
	CheckTypedValidation(Source.LoadPrefab2D("door2d.dxfprefab.json"));
	CheckTypedValidation(Source.LoadPrefab3D("door3d.dxfprefab.json"));
}

TEST("Content Scene validates every typed definition and transformed endpoint before any Native resource")
{
	Dxf::Testing::FFakeBackend Backend;
	Dxf::FAssetService Assets{Backend, Backend, Backend};
	Dxf::FSceneContentSource Source{Toolbox::FPath(DXF_CONTENT_TEST_ROOT)};
	auto Scene = Source.LoadScene2D("scene2d-connected.dxfscene.json");
	// 先頭の共通Fontが有効でも、最後のPrefab不正で作成を始めない。
	Scene.Prefabs[1].Parts[1].Body.Mass = -1;
	bool Rejected = false;
	try
	{
		(void)Dxf::PrepareScene(Scene, Assets);
	}
	catch (const Toolbox::FException&)
	{
		Rejected = true;
	}
	REQUIRE(Rejected && Backend.GetTrace().Fonts.IsEmpty());
	auto Scene3D = Source.LoadScene3D("scene3d-connected.dxfscene.json");
	Scene3D.Placements[1].Rotation = {0, 0, 0, 0};
	Rejected = false;
	try
	{
		(void)Dxf::PrepareScene(Scene3D, Assets);
	}
	catch (const Toolbox::FException&)
	{
		Rejected = true;
	}
	REQUIRE(Rejected && Backend.GetTrace().Fonts.IsEmpty());
	Scene = Source.LoadScene2D("scene2d-connected.dxfscene.json");
	Scene.Connections[0].BodyB.Export.Index = 4096;
	Rejected = false;
	try
	{
		(void)Dxf::PrepareScene(Scene, Assets);
	}
	catch (const Toolbox::FException&)
	{
		Rejected = true;
	}
	REQUIRE(Rejected && Backend.GetTrace().Fonts.IsEmpty());
}
TEST("Content empty Scene still owns mandatory common assets and definition fixed update controls registration")
{
	Dxf::Testing::FFakeBackend Backend;
	Dxf::FAssetService Assets{Backend, Backend, Backend};
	Dxf::FSceneDefinition2D Empty;
	Dxf::FContentAssetDefinition Image;
	Image.Id = "image";
	Image.Path = "Assets/image.png";
	Empty.Assets.PushBack(Image);
	const auto Shared = Dxf::PrepareScene(Empty, Assets);
	REQUIRE(Shared.Resources && Shared.Resources->GetTexture(0).IsValid());
	REQUIRE(Backend.GetTrace().TextureLoads == 1);
	Dxf::FSceneDefinition2D Definition;
	Definition.Prefabs.PushBack(Dxf::ParsePrefab2D(Door2D));
	Definition.Placements.PushBack({});
	Definition.Instances.PushBack("door");
	Definition.FixedUpdate.StepSeconds = 1.0 / 30.0;
	const auto Prepared = Dxf::PrepareScene(Definition, Assets);
	Dxf::DContentScene2D Scene(Prepared);
	struct FShutdown
	{
		Dxf::DContentScene2D& Scene;
		~FShutdown()
		{
			Scene.Shutdown_Internal();
		}
	} Shutdown{Scene};
	REQUIRE(Scene.Initialize_Internal({Assets}));
	const auto Door = Scene.GetPrefab("door");
	Dxf::FInputStateTracker Input;
	Dxf::FFrameTime Time;
	Time.DeltaSeconds = 1.0 / 60.0;
	Time.UnscaledDeltaSeconds = Time.DeltaSeconds;
	REQUIRE(Scene.Tick_Internal({Input.GetSnapshot(), Time}));
	REQUIRE(Door.Get()->GetState() == Dxf::EPrefabInstanceState::PendingPhysics);
	REQUIRE(Scene.Tick_Internal({Input.GetSnapshot(), Time}));
	REQUIRE(Door.Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
}

TEST("Content typed label Font and material errors fail before owner Native calls in both dimensions")
{
	Dxf::Testing::FFakeBackend Backend;
	Dxf::FAssetService Assets(Backend, Backend, Backend);
	const auto Check = [&](auto Definition)
	{
		bool Failed = false;
		try
		{
			(void)Dxf::PreparePrefab(Toolbox::Move(Definition), Assets);
		}
		catch (const Dxf::FSceneContentError&)
		{
			Failed = true;
		}
		REQUIRE(Failed);
		REQUIRE(Backend.GetTrace().TextureLoads == 0 && Backend.GetTrace().Fonts.IsEmpty());
	};
	auto A = Dxf::ParsePrefab2D(Door2D);
	auto B = Dxf::ParsePrefab3D(Door3D);
	A.Parts[0].DisplayName = Toolbox::FString("bad\0name", 8);
	B.Parts[0].DisplayName = Toolbox::FString("bad\0name", 8);
	Check(A);
	Check(B);
	A = Dxf::ParsePrefab2D(Door2D);
	B = Dxf::ParsePrefab3D(Door3D);
	A.Parts[1].Visual.Material.Roughness = 2;
	B.Parts[1].Visual.Material.Roughness = 2;
	Check(A);
	Check(B);
}

namespace
{
// 定義の値をコピーせず、明示したC++ Worldを独立した対照として構築する。
template <typename TScene, typename TInstance, typename TWorld, typename TBody, typename TVector>
void CompareExplicitWorld(const char* Text, const char* BodyExport, const char* DriveExport, TBody Support, TBody Leaf, TVector Anchor, bool Limits)
{
	TContentWorld<TScene> F;
	F.Scene.GetPhysicsWorld().SetGravity({});
	const auto Prepared = [&]()
	{
		if constexpr (Toolbox::IsSame<TInstance, Dxf::DPrefabInstance2D>)
		{
			return Dxf::PreparePrefab(Dxf::ParsePrefab2D(Text), F.Assets);
		}
		else
		{
			return Dxf::PreparePrefab(Dxf::ParsePrefab3D(Text), F.Assets);
		}
	}();
	const auto Instance = F.Scene.template Spawn<TInstance>(Prepared);
	REQUIRE(Instance && F.Scene.Initialize_Internal({F.Assets}));
	const auto Body = Instance.Value().Get()->GetRigidBody(BodyExport);
	const auto Drive = Instance.Value().Get()->GetPrismaticJoint(DriveExport);
	TWorld Explicit;
	Explicit.SetGravity({});
	const auto A = Explicit.CreateBody(Support);
	const auto B = Explicit.CreateBody(Leaf);
	auto Settings = Explicit.MakePrismaticJointDescription(A, B, Anchor);
	Settings.Limits = {Limits, 0, 2};
	Settings.Drive = {true, .8, 15};
	const auto Joint = Explicit.CreatePrismaticJoint(A, B, Settings);
	for (Toolbox::uint32 Frame = 0; Frame < 60; ++Frame)
	{
		if (Frame == 20 || Frame == 40)
		{
			const Dxf::FLinearJointDrive Input{true, Frame == 20 ? -.4 : 0, 15};
			Drive.Get()->RequestDrive(Input);
			Explicit.SetPrismaticJointDrive(Joint, Input);
		}
		REQUIRE(F.Tick());
		Explicit.Step(1.0 / 60.0);
		const auto ActualPosition = F.Scene.GetPhysicsWorld().GetPosition(Body.Get()->GetBodyId());
		const auto ExpectedPosition = Explicit.GetPosition(B);
		const auto ActualVelocity = F.Scene.GetPhysicsWorld().GetVelocity(Body.Get()->GetBodyId());
		const auto ExpectedVelocity = Explicit.GetVelocity(B);
		// 比較するのは物理成分。World番号と構造体の余白は比較しない。
		REQUIRE(Toolbox::Abs(ActualPosition.X - ExpectedPosition.X) < 0.000001f);
		REQUIRE(Toolbox::Abs(ActualPosition.Y - ExpectedPosition.Y) < 0.000001f);
		REQUIRE(Toolbox::Abs(ActualVelocity.X - ExpectedVelocity.X) < 0.000001f);
		REQUIRE(Toolbox::Abs(ActualVelocity.Y - ExpectedVelocity.Y) < 0.000001f);
		if constexpr (Toolbox::IsSame<TWorld, Dxf::FPhysicsWorld3D>)
		{
			REQUIRE(Toolbox::Abs(ActualPosition.Z - ExpectedPosition.Z) < 0.000001f);
			REQUIRE(Toolbox::Abs(ActualVelocity.Z - ExpectedVelocity.Z) < 0.000001f);
		}
	}
	REQUIRE(Instance.Value().Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
}
} // namespace
TEST("Content 2D definition construction matches an explicit C++ World under identical motor input")
{
	Dxf::FBodyDescription2D Support;
	Support.Type = Dxf::EBodyType::Static;
	Dxf::FBodyDescription2D Leaf;
	Leaf.Position = {0, 1};
	CompareExplicitWorld<Dxf::DPhysicsScene2D, Dxf::DPrefabInstance2D, Dxf::FPhysicsWorld2D>(Door2D, "doorBody", "doorDrive", Support, Leaf, Toolbox::FVector2{0, 1}, true);
}
TEST("Content 3D definition construction matches an explicit C++ World under identical motor input")
{
	Dxf::FBodyDescription3D Support;
	Support.Type = Dxf::EBodyType::Static;
	Dxf::FBodyDescription3D Leaf;
	Leaf.Position = {0, 1, 0};
	Leaf.Orientation = {0, 0, 0, -1};
	Leaf.DiagonalInertia = {1, 2, 3};
	CompareExplicitWorld<Dxf::DPhysicsScene3D, Dxf::DPrefabInstance3D, Dxf::FPhysicsWorld3D>(Door3D, "body", "drive", Support, Leaf, Toolbox::FVector3{0, 1, 0}, false);
}

namespace
{
// データ由来の動く支点へ、既存の目標と経路を型付き公開先から渡す。
template <typename TScene, typename TInstance, typename TDefinition, typename TPlacement>
void CheckContentMover(TDefinition Definition, TPlacement Placement, const char* DriveName)
{
	TContentWorld<TScene> F;
	const auto Prepared = Dxf::PreparePrefab(Toolbox::Move(Definition), F.Assets);
	const auto A = F.Scene.template Spawn<TInstance>(Prepared);
	Placement.Position.X = 10;
	const auto B = F.Scene.template Spawn<TInstance>(Prepared, Placement);
	REQUIRE(A && B);
	REQUIRE(F.Scene.Initialize_Internal({F.Assets}));
	REQUIRE(A.Value().Get()->GetState() == Dxf::EPrefabInstanceState::PendingPhysics);
	REQUIRE(F.Tick());
	REQUIRE(A.Value().Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	const auto Mover = A.Value().Get()->GetKinematicMover("supportMover");
	const auto Other = B.Value().Get()->GetKinematicMover("supportMover");
	REQUIRE(Mover && Other);
	REQUIRE(RejectExportKind([&]()
	                         {
		                         A.Value().Get()->GetRigidBody("supportMover");
	                         }));
	const auto Id = Mover.Get()->GetBodyId();
	const auto OtherId = Other.Get()->GetBodyId();
	REQUIRE(Id && OtherId && *Id != *OtherId);
	const auto Joint = A.Value().Get()->GetPrismaticJoint(DriveName);
	const auto JointId = Joint.Get()->GetJointId();
	REQUIRE(JointId);
	auto Target = Mover.Get()->GetPose();
	Target.Position.X = 0.1f;
	Mover.Get()->SetTarget(Target);
	REQUIRE(F.Tick());
	REQUIRE(Toolbox::Abs(F.Scene.GetPhysicsWorld().GetPosition(*Id).X - 0.1f) < 0.00001f);
	REQUIRE(Toolbox::Abs(F.Scene.GetPhysicsWorld().GetPosition(*OtherId).X - 10) < 0.00001f);
	Mover.Get()->SetPath([Target](Toolbox::f64 Seconds) mutable
	                     {
		                     auto Pose = Target;
		                     Pose.Position.X += static_cast<Toolbox::f32>(0.2 * Seconds);
		                     return Pose;
	                     });
	for (Toolbox::uint32 Index = 0; Index < 10; ++Index)
	{
		REQUIRE(F.Tick());
	}
	REQUIRE(F.Scene.GetPhysicsWorld().GetPosition(*Id).X > 0.12f);
	REQUIRE(Toolbox::Abs(F.Scene.GetPhysicsWorld().GetPosition(*OtherId).X - 10) < 0.00001f);
	REQUIRE(Joint.Get()->GetJointId() && *Joint.Get()->GetJointId() == *JointId);
	REQUIRE(Joint.Get()->GetObservation());
	REQUIRE(Toolbox::IsFinite(Joint.Get()->GetObservation()->State.Translation));
	A.Value().Get()->Destroy();
	REQUIRE(!Mover);
	REQUIRE(F.Tick());
	REQUIRE(!F.Scene.GetPhysicsWorld().IsAlive(*Id));
	REQUIRE(F.Scene.GetPhysicsWorld().IsAlive(*OtherId));
	REQUIRE(B.Value().Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
}
} // namespace
TEST("Content 2D kinematic export moves a joint support through existing target and path")
{
	auto Text = Replace(Door2D, "\"type\":\"Static\"", "\"type\":\"Kinematic\",\"adapter\":\"KinematicMover\"");
	Text = Replace(Text.CStr(), "\"exports\":{", "\"exports\":{\"supportMover\":\"frame/body\",");
	CheckContentMover<Dxf::DPhysicsScene2D, Dxf::DPrefabInstance2D>(Dxf::ParsePrefab2D({Text.Data(), Text.Size()}), Dxf::FPrefabSpawnOptions2D{}, "doorDrive");
}
TEST("Content 3D kinematic export moves a joint support through existing target and path")
{
	auto Text = Replace(Door3D, "\"type\":\"Static\"", "\"type\":\"Kinematic\",\"adapter\":\"KinematicMover\"");
	Text = Replace(Text.CStr(), "\"exports\":{", "\"exports\":{\"supportMover\":\"support/body\",");
	CheckContentMover<Dxf::DPhysicsScene3D, Dxf::DPrefabInstance3D>(Dxf::ParsePrefab3D({Text.Data(), Text.Size()}), Dxf::FPrefabSpawnOptions3D{}, "drive");
}
