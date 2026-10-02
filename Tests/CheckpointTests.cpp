// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/CheckpointCodec.h"
#include "Dxf/SaveCodec.h"
#include "Dxf/SaveError.h"
#include "Toolbox/JsonWriter.h"
using namespace Dxf;
using namespace Toolbox;
namespace
{
// カタログとの接続試験は後続で行い、codecには論理値だけを渡す。
const char* Fingerprint = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
template <typename TDocument, typename TInstance>
TDocument Document(uint32 Dimension)
{
	TDocument D;
	D.Identity.GameId = "checkpoint_test";
	D.Identity.ContentRevision = "asset-bundle-v1";
	D.Identity.SceneKey = "course/main";
	D.Identity.CheckpointKey = "north";
	D.Identity.Dimension = Dimension;
	D.Identity.Generation = 9007199254740993ULL;
	D.DefinitionFingerprint = Fingerprint;
	TInstance I;
	I.Key = "doorA";
	I.Prefab = "course/door";
	I.DefinitionFingerprint = Fingerprint;
	FContentParameterValue P;
	P.Id = "speed";
	P.Number = -0.8;
	I.Parameters.PushBack(P);
	P.Id = "unlocked";
	P.Kind = EContentParameterKind::Boolean;
	P.Boolean = true;
	I.Parameters.PushBack(P);
	D.Instances.PushBack(I);
	I.Key = "removedKey";
	I.bPresent = false;
	D.Instances.PushBack(I);
	I.Key = "cargo1";
	I.bDynamic = true;
	I.bPresent = true;
	I.Parameters.Clear();
	D.Instances.PushBack(I);
	const EJointKind Kinds[] = {EJointKind::Distance, EJointKind::Revolute, EJointKind::Fixed, EJointKind::Prismatic};
	for (size_t K = 0; K < 4; ++K)
	{
		FCheckpointJointState S;
		S.Instance = "doorA";
		S.Export = FString("drive_") + FJsonWriter::Unsigned(K);
		S.Kind = Kinds[K];
		S.bConnected = K != 2;
		if (S.Kind == EJointKind::Distance)
		{
			S.DistanceLength = 2.5;
		}
		if (S.Kind == EJointKind::Revolute)
		{
			S.AngularDrive = {true, -1.25, 7};
			S.AngularLimits = {true, -1, 1};
			S.AngularTarget = FAngularJointTargetSettings{};
		}
		if (S.Kind == EJointKind::Prismatic)
		{
			S.LinearDrive = {true, 0.8, 15};
			S.LinearLimits = {true, -2, 3};
			S.LinearTarget = FLinearJointTargetSettings{};
		}
		D.Joints.PushBack(S);
	}
	D.GamePayload = "{\"score\":7,\"collected\":[\"keyNorth\"],\"spawnCounter\":\"9007199254740993\"}";
	return D;
}
template <typename TAction>
void Reject(TAction&& Action, ESaveError Code = ESaveError::InvalidDocument)
{
	bool Rejected = false;
	try
	{
		Action();
	}
	catch (const FSaveError& E)
	{
		REQUIRE(E.GetCode() == Code);
		Rejected = true;
	}
	REQUIRE(Rejected);
}
template <typename TDocument>
void RoundTrip(const TDocument& Source, const TDocument& Restored)
{
	REQUIRE(Source.Identity.Generation == Restored.Identity.Generation);
	REQUIRE(Restored.Instances.Size() == 3 && Restored.Joints.Size() == 4);
	REQUIRE(Restored.Instances[0].Key == "doorA");
	REQUIRE(!Restored.Instances[1].bPresent && Restored.Instances[2].bDynamic);
	REQUIRE(Restored.Instances[0].Parameters.Size() == 2);
	REQUIRE(Restored.Instances[0].Parameters[0].Number == -0.8);
	REQUIRE(Restored.Joints[0].DistanceLength && *Restored.Joints[0].DistanceLength == 2.5);
	REQUIRE(Restored.Joints[1].AngularDrive.TargetAngularSpeed == -1.25);
	REQUIRE(Restored.Joints[1].AngularTarget && Restored.Joints[1].AngularLimits.bEnabled);
	REQUIRE(!Restored.Joints[2].bConnected && Restored.Joints[2].Kind == EJointKind::Fixed);
	REQUIRE(Restored.Joints[3].LinearDrive.MaxForce == 15 && Restored.Joints[3].LinearTarget);
	REQUIRE(FSaveCodec::Encode(FCheckpointCodec::Encode(Source)) == FSaveCodec::Encode(FCheckpointCodec::Encode(Restored)));
}
} // namespace
TEST("Checkpoint 2D logical instances four Joint kinds and exact counter round trip")
{
	auto D = Document<FCheckpointDocument2D, FCheckpointInstance2D>(2);
	D.Instances[0].Placement = {{3, -2}, 1.57079632679f};
	const auto Result = FCheckpointCodec::Decode2D(FCheckpointCodec::Encode(D));
	RoundTrip(D, Result);
	REQUIRE(Result.Instances[0].Placement.Position.X == 3);
	REQUIRE(Result.Instances[0].Placement.Rotation == D.Instances[0].Placement.Rotation);
	REQUIRE(D.Instances[0].Parameters[0].Number == -0.8);
}
TEST("Checkpoint 3D placement Quaternion and independent instance values")
{
	auto D = Document<FCheckpointDocument3D, FCheckpointInstance3D>(3);
	D.Instances[0].Placement = {{1, 2, -3}, FQuaternion::FromAxisAngle({1, 2, 3}, 2.09439510239f)};
	const auto Result = FCheckpointCodec::Decode3D(FCheckpointCodec::Encode(D));
	RoundTrip(D, Result);
	const auto A = D.Instances[0].Placement.Rotation.Rotate({1, 0, 0});
	const auto B = Result.Instances[0].Placement.Rotation.Rotate({1, 0, 0});
	REQUIRE(Length(A - B) < 0.000001f);
	REQUIRE(Result.Instances[2].Placement.Position.X == 0);
}
TEST("Checkpoint rejects duplicate instances exports parameters and absent endpoints")
{
	auto D = Document<FCheckpointDocument2D, FCheckpointInstance2D>(2);
	D.Instances[2].Key = D.Instances[0].Key;
	Reject([&]
	       {
		       FCheckpointCodec::Encode(D);
	       });
	D.Instances[2].Key = "cargo1";
	D.Joints[1].Export = D.Joints[0].Export;
	Reject([&]
	       {
		       FCheckpointCodec::Encode(D);
	       });
	D.Joints[1].Export = "drive_1";
	D.Joints[0].Instance = "removedKey";
	Reject([&]
	       {
		       FCheckpointCodec::Encode(D);
	       });
	D.Joints[0].Instance = "doorA";
	D.Instances[0].Parameters.PushBack(D.Instances[0].Parameters[0]);
	Reject([&]
	       {
		       FCheckpointCodec::Encode(D);
	       });
}
TEST("Checkpoint refuses wrong dimension future version runtime IDs and invalid settings")
{
	auto D = Document<FCheckpointDocument2D, FCheckpointInstance2D>(2);
	const auto Normal = FCheckpointCodec::Encode(D);
	Reject([&]
	       {
		       FCheckpointCodec::Decode3D(Normal);
	       });
	auto Future = Normal;
	Future.Payload = "{\"checkpointVersion\":2,\"futureField\":true}";
	Reject([&]
	       {
		       FCheckpointCodec::Decode2D(Future);
	       },
	       ESaveError::UnsupportedVersion);
	auto Unknown = Normal;
	Unknown.Payload = "{\"bodyId\":7,\"checkpointVersion\":1}";
	Reject([&]
	       {
		       FCheckpointCodec::Decode2D(Unknown);
	       });
	D.Joints[3].LinearDrive.MaxForce = -1;
	Reject([&]
	       {
		       FCheckpointCodec::Encode(D);
	       });
	D.Joints[3].LinearDrive.MaxForce = 15;
	D.Joints[1].AngularLimits.UpperAngle = 3.141592653589793;
	Reject([&]
	       {
		       FCheckpointCodec::Encode(D);
	       });
}
TEST("Checkpoint checks instance parameter state byte limits and empty fingerprints")
{
	auto D = Document<FCheckpointDocument2D, FCheckpointInstance2D>(2);
	FCheckpointLimits L;
	L.MaxInstances = 2;
	Reject([&]
	       {
		       FCheckpointCodec::Encode(D, L);
	       });
	L.MaxInstances = 3;
	L.MaxJoints = 3;
	Reject([&]
	       {
		       FCheckpointCodec::Encode(D, L);
	       });
	L.MaxJoints = 4;
	L.MaxParameters = 1;
	Reject([&]
	       {
		       FCheckpointCodec::Encode(D, L);
	       });
	L.MaxParameters = 2;
	REQUIRE(FCheckpointCodec::Decode2D(FCheckpointCodec::Encode(D, L), L).Instances.Size() == 3);
	L.Document.MaxBytes = 512;
	L.Document.MaxStringBytes = 512;
	Reject([&]
	       {
		       FCheckpointCodec::Encode(D, L);
	       });
	D.DefinitionFingerprint = "";
	Reject([&]
	       {
		       FCheckpointCodec::Encode(D);
	       });
}
TEST("Checkpoint rejects unsafe logical keys zero Quaternion nonfinite and target outside limits")
{
	auto D = Document<FCheckpointDocument3D, FCheckpointInstance3D>(3);
	D.Instances[0].Prefab = "../escape";
	Reject([&]
	       {
		       FCheckpointCodec::Encode(D);
	       });
	D.Instances[0].Prefab = "course/door";
	D.Instances[0].Placement.Rotation = {0, 0, 0, 0};
	Reject([&]
	       {
		       FCheckpointCodec::Encode(D);
	       });
	D.Instances[0].Placement.Rotation = {};
	D.Instances[0].Parameters[0].Number = Sqrt(-1.0);
	Reject([&]
	       {
		       FCheckpointCodec::Encode(D);
	       });
	D.Instances[0].Parameters[0].Number = -0.8;
	D.Joints[3].LinearTarget->TargetTranslation = 4;
	Reject([&]
	       {
		       FCheckpointCodec::Encode(D);
	       });
}
TEST("Checkpoint applies smaller key limit to instances parameters assets and exports")
{
	auto D = Document<FCheckpointDocument2D, FCheckpointInstance2D>(2);
	FCheckpointLimits L;
	L.Document.MaxKeyBytes = 32;
	D.Instances[0].Key = "abcdefghijklmnopqrstuvwxyz_abcdefghi";
	const auto TooLongInstance = [&]
	{
		FCheckpointCodec::Encode(D, L);
	};
	Reject(TooLongInstance);
	D = Document<FCheckpointDocument2D, FCheckpointInstance2D>(2);
	D.Instances[0].Parameters[0].Id = "abcdefghijklmnopqrstuvwxyz_abcdefghi";
	const auto TooLongParameter = [&]
	{
		FCheckpointCodec::Encode(D, L);
	};
	Reject(TooLongParameter);
	D.Instances[0].Parameters[0].Id = "asset";
	D.Instances[0].Parameters[0].Kind = EContentParameterKind::Asset;
	D.Instances[0].Parameters[0].Asset = "abcdefghijklmnopqrstuvwxyz_abcdefghi";
	const auto TooLongAsset = [&]
	{
		FCheckpointCodec::Encode(D, L);
	};
	Reject(TooLongAsset);
	D = Document<FCheckpointDocument2D, FCheckpointInstance2D>(2);
	D.Joints[0].Export = "abcdefghijklmnopqrstuvwxyz_abcdefghi";
	const auto Encoded = FCheckpointCodec::Encode(D);
	const auto TooLongExport = [&]
	{
		FCheckpointCodec::Decode2D(Encoded, L);
	};
	Reject(TooLongExport);
}
TEST("JSON number writer round trips finite f64 boundaries without integer conversion")
{
	const f64 Values[] = {0, -0.0, 0.8, -123456789.125, 1e-300, 1.7976931348623157e308, 1.2345678912345678};
	for (f64 Value : Values)
	{
		const auto Text = FJsonWriter::Number(Value);
		const auto J = FJsonDocument::Parse(Text);
		REQUIRE(J.Get(0).Kind == EJsonKind::Number && J.Get(0).Number == Value);
	}
	bool Failed = false;
	try
	{
		FJsonWriter::Number(Sqrt(-1.0));
	}
	catch (const FException&)
	{
		Failed = true;
	}
	REQUIRE(Failed);
}
