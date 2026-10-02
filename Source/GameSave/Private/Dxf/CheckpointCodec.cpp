// SPDX-License-Identifier: NOASSERTION
#include "Dxf/CheckpointCodec.h"
#include "Dxf/SaveCodec.h"
#include "Dxf/SaveError.h"
#include "Dxf/MechanismComponentValidation.h"
#include "Dxf/JointTargetMotion.h"
#include "Toolbox/JsonWriter.h"
#include "Toolbox/JsonError.h"
namespace Dxf
{
using namespace Toolbox;
namespace
{
// 型付きの値も、受信したJSONも同じ失敗種別へ揃える。
[[noreturn]] void Invalid(const char* Reason)
{
	throw FSaveError(ESaveError::InvalidDocument, Reason);
}
void Require(bool Condition, const char* Reason)
{
	if (!Condition)
	{
		Invalid(Reason);
	}
}
void Bounds(FCheckpointLimits L)
{
	Require(L.MaxInstances > 0 && L.MaxInstances <= 4096 && L.MaxJoints > 0 && L.MaxJoints <= 16384 && L.MaxParameters > 0 && L.MaxParameters <= 128 && L.MaxCatalogEntries > 0 && L.MaxCatalogEntries <= 128, "Invalid checkpoint count limits");
}
// 下げた論理キー上限も、全個体・公開値・操作先へ同じ意味で適用する。
template <typename TDocument>
void KeyLimits(const TDocument& D, size_t Limit)
{
	for (const auto& I : D.Instances)
	{
		Require(I.Key.Size() <= Limit && I.Prefab.Size() <= Limit, "Checkpoint logical key byte limit");
		for (const auto& P : I.Parameters)
		{
			Require(P.Id.Size() <= Limit && (P.Kind != EContentParameterKind::Asset || P.Asset.Size() <= Limit), "Checkpoint parameter key byte limit");
		}
	}
	for (const auto& S : D.Joints)
	{
		Require(S.Instance.Size() <= Limit && S.Export.Size() <= Limit, "Checkpoint Joint key byte limit");
	}
}
void Key(FStringView Value, bool Hierarchy = false)
{
	Require(!Value.IsEmpty() && Value.Size() <= (Hierarchy ? 512 : 128), "Invalid checkpoint logical key");
	size_t Segment = 0;
	for (size_t I = 0; I < Value.Size(); ++I)
	{
		const char C = Value[I];
		if (Hierarchy && C == '/')
		{
			Require(Segment > 0 && Segment <= 128, "Invalid checkpoint key segment");
			Segment = 0;
			continue;
		}
		const bool Letter = (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || C == '_';
		Require(Letter || (Segment > 0 && ((C >= '0' && C <= '9') || C == '-')), "Invalid checkpoint key character");
		++Segment;
	}
	Require(Segment > 0 && Segment <= 128, "Invalid checkpoint key segment");
}
void Fingerprint(FStringView Value)
{
	Require(Value.Size() == 64, "Checkpoint definition fingerprint is missing");
	for (size_t I = 0; I < Value.Size(); ++I)
	{
		const char C = Value[I];
		Require((C >= '0' && C <= '9') || (C >= 'a' && C <= 'f'), "Invalid checkpoint fingerprint");
	}
}
// 許可したschema名だけを受付する。部分一致やオブジェクト順序には依存しない。
void Fields(const FJsonDocument& J, int32 I, FStringView Allowed)
{
	Require(J.Get(I).Kind == EJsonKind::Object, "Checkpoint object expected");
	for (int32 C = J.Get(I).FirstChild; C >= 0; C = J.Get(C).NextSibling)
	{
		bool Found = false;
		size_t Start = 0;
		for (size_t K = 0; K <= Allowed.Size(); ++K)
		{
			if (K == Allowed.Size() || Allowed[K] == '|')
			{
				Found = Found || J.Get(C).Key == FStringView(Allowed.Data() + Start, K - Start);
				Start = K + 1;
			}
		}
		Require(Found, "Unknown checkpoint field");
	}
}
int32 Field(const FJsonDocument& J, int32 Parent, const char* Name, EJsonKind Kind)
{
	const int32 I = J.Find(Parent, Name);
	Require(I >= 0 && J.Get(I).Kind == Kind, "Missing or wrongly typed checkpoint field");
	return I;
}
const FString& String(const FJsonDocument& J, int32 I, const char* Name)
{
	return J.Get(Field(J, I, Name, EJsonKind::String)).Text;
}
f64 Number(const FJsonDocument& J, int32 I, const char* Name)
{
	return J.Get(Field(J, I, Name, EJsonKind::Number)).Number;
}
bool Boolean(const FJsonDocument& J, int32 I, const char* Name)
{
	return J.Get(Field(J, I, Name, EJsonKind::Boolean)).Boolean;
}
template <size_t N>
TArray<f64, N> Numbers(const FJsonDocument& J, int32 I)
{
	Require(J.Get(I).Kind == EJsonKind::Array, "Checkpoint numeric array expected");
	TArray<f64, N> Result;
	size_t Count = 0;
	for (int32 C = J.Get(I).FirstChild; C >= 0; C = J.Get(C).NextSibling)
	{
		Require(Count < N && J.Get(C).Kind == EJsonKind::Number, "Checkpoint vector size or type mismatch");
		Result[Count++] = J.Get(C).Number;
	}
	Require(Count == N, "Checkpoint vector size mismatch");
	return Result;
}
f32 Scalar(f64 Value)
{
	const f32 Result = static_cast<f32>(Value);
	Require(IsFinite(Result), "Checkpoint value exceeds f32");
	return Result;
}
// 各追加前に長さを確認し、巨大な仮出力を作ってから拒否しない。
class FBuilder
{
public:
	explicit FBuilder(size_t Limit) : m_Limit(Limit)
	{
	}
	void Add(FStringView Text)
	{
		Require(m_Bytes.Size() <= m_Limit && Text.Size() <= m_Limit - m_Bytes.Size(), "Checkpoint output byte limit");
		m_Bytes += FString(Text.Data(), Text.Size());
	}
	void Quote(FStringView Text)
	{
		Add(FJsonWriter::Quote(Text));
	}
	void Number(f64 Value)
	{
		Add(FJsonWriter::Number(Value));
	}
	void Boolean(bool Value)
	{
		Add(Value ? "true" : "false");
	}
	FString Finish()
	{
		return Move(m_Bytes);
	}

private:
	// 確保前に適用する出力長。
	size_t m_Limit;
	// 今回のencodeだけが所有する出力。
	FString m_Bytes;
};
void Vector(FBuilder& B, const Toolbox::FVector2& V)
{
	B.Add("[");
	B.Number(V.X);
	B.Add(",");
	B.Number(V.Y);
	B.Add("]");
}
void Vector(FBuilder& B, const Toolbox::FVector3& V)
{
	B.Add("[");
	B.Number(V.X);
	B.Add(",");
	B.Number(V.Y);
	B.Add(",");
	B.Number(V.Z);
	B.Add("]");
}
void Rotation(FBuilder& B, f32 R)
{
	B.Number(R);
}
void Rotation(FBuilder& B, const FQuaternion& R)
{
	const auto Q = R.Normalized();
	B.Add("[");
	B.Number(Q.X);
	B.Add(",");
	B.Number(Q.Y);
	B.Add(",");
	B.Number(Q.Z);
	B.Add(",");
	B.Number(Q.W);
	B.Add("]");
}
void Parameter(FBuilder& B, const FContentParameterValue& P)
{
	Key(P.Id);
	B.Quote(P.Id);
	B.Add(":{\"type\":");
	switch (P.Kind)
	{
	case EContentParameterKind::Boolean:
		B.Add("\"bool\",\"value\":");
		B.Boolean(P.Boolean);
		break;
	case EContentParameterKind::Number:
		B.Add("\"number\",\"value\":");
		B.Number(P.Number);
		break;
	case EContentParameterKind::Vector2:
		B.Add("\"vector2\",\"value\":");
		Vector(B, P.Vector2);
		break;
	case EContentParameterKind::Vector3:
		B.Add("\"vector3\",\"value\":");
		Vector(B, P.Vector3);
		break;
	case EContentParameterKind::Color:
		B.Add("\"color\",\"value\":[");
		B.Number(P.Color.R);
		B.Add(",");
		B.Number(P.Color.G);
		B.Add(",");
		B.Number(P.Color.B);
		B.Add(",");
		B.Number(P.Color.A);
		B.Add("]");
		break;
	case EContentParameterKind::Asset:
		Key(P.Asset, true);
		B.Add("\"asset\",\"value\":");
		B.Quote(P.Asset);
		break;
	default:
		Invalid("Unknown checkpoint parameter type");
	}
	B.Add("}");
}
FContentParameterValue Parameter(const FJsonDocument& J, int32 I)
{
	Fields(J, I, "type|value");
	FContentParameterValue P;
	P.Id = J.Get(I).Key;
	Key(P.Id);
	const auto& Type = String(J, I, "type");
	const int32 V = J.Find(I, "value");
	Require(V >= 0, "Missing checkpoint parameter value");
	if (Type == "bool")
	{
		P.Kind = EContentParameterKind::Boolean;
		P.Boolean = Boolean(J, I, "value");
	}
	else if (Type == "number")
	{
		P.Kind = EContentParameterKind::Number;
		P.Number = Number(J, I, "value");
	}
	else if (Type == "vector2")
	{
		const auto Values = Numbers<2>(J, V);
		P.Kind = EContentParameterKind::Vector2;
		P.Vector2 = {Scalar(Values[0]), Scalar(Values[1])};
	}
	else if (Type == "vector3")
	{
		const auto Values = Numbers<3>(J, V);
		P.Kind = EContentParameterKind::Vector3;
		P.Vector3 = {Scalar(Values[0]), Scalar(Values[1]), Scalar(Values[2])};
	}
	else if (Type == "color")
	{
		const auto Values = Numbers<4>(J, V);
		for (f64 C : Values)
		{
			Require(C >= 0 && C <= 255 && C == static_cast<uint32>(C), "Invalid checkpoint color");
		}
		P.Kind = EContentParameterKind::Color;
		P.Color = {static_cast<uint8>(Values[0]), static_cast<uint8>(Values[1]), static_cast<uint8>(Values[2]), static_cast<uint8>(Values[3])};
	}
	else if (Type == "asset")
	{
		P.Kind = EContentParameterKind::Asset;
		P.Asset = String(J, I, "value");
		Key(P.Asset, true);
	}
	else
	{
		Invalid("Unknown checkpoint parameter type");
	}
	return P;
}
void Drive(FBuilder& B, bool Enabled, f64 Speed, f64 Maximum)
{
	B.Add("{\"enabled\":");
	B.Boolean(Enabled);
	B.Add(",\"speed\":");
	B.Number(Speed);
	B.Add(",\"maximum\":");
	B.Number(Maximum);
	B.Add("}");
}
void Limits(FBuilder& B, bool Enabled, f64 Lower, f64 Upper)
{
	B.Add("{\"enabled\":");
	B.Boolean(Enabled);
	B.Add(",\"lower\":");
	B.Number(Lower);
	B.Add(",\"upper\":");
	B.Number(Upper);
	B.Add("}");
}
void Target(FBuilder& B, f64 Coordinate, f64 MaximumSpeed, f64 Slowdown, f64 Tolerance, f64 SpeedTolerance, f64 MaximumForce)
{
	B.Add("{\"coordinate\":");
	B.Number(Coordinate);
	B.Add(",\"maximumSpeed\":");
	B.Number(MaximumSpeed);
	B.Add(",\"slowdown\":");
	B.Number(Slowdown);
	B.Add(",\"tolerance\":");
	B.Number(Tolerance);
	B.Add(",\"speedTolerance\":");
	B.Number(SpeedTolerance);
	B.Add(",\"maximumForce\":");
	B.Number(MaximumForce);
	B.Add("}");
}
void Joint(FBuilder& B, const FCheckpointJointState& S)
{
	if (!S.Instance.IsEmpty())
	{
		Key(S.Instance);
	}
	Key(S.Export, true);
	Require(!S.DistanceLength || S.Kind == EJointKind::Distance, "Distance setting on different Joint kind");
	Require(!S.AngularTarget || S.Kind == EJointKind::Revolute, "Angular target on different Joint kind");
	Require(!S.LinearTarget || S.Kind == EJointKind::Prismatic, "Linear target on different Joint kind");
	B.Add("{\"instance\":");
	B.Quote(S.Instance);
	B.Add(",\"export\":");
	B.Quote(S.Export);
	B.Add(",\"connected\":");
	B.Boolean(S.bConnected);
	B.Add(",\"kind\":");
	switch (S.Kind)
	{
	case EJointKind::Distance:
		B.Quote("Distance");
		B.Add(",\"length\":");
		if (S.DistanceLength)
		{
			Require(IsFinite(*S.DistanceLength) && *S.DistanceLength >= 0, "Invalid checkpoint Distance length");
			B.Number(*S.DistanceLength);
		}
		else
		{
			B.Add("null");
		}
		break;
	case EJointKind::Fixed:
		B.Quote("Fixed");
		break;
	case EJointKind::Revolute:
		B.Quote("Revolute");
		B.Add(",\"drive\":");
		Drive(B, S.AngularDrive.bEnabled, S.AngularDrive.TargetAngularSpeed, S.AngularDrive.MaxTorque);
		B.Add(",\"limits\":");
		Limits(B, S.AngularLimits.bEnabled, S.AngularLimits.LowerAngle, S.AngularLimits.UpperAngle);
		B.Add(",\"target\":");
		if (S.AngularTarget)
		{
			const auto& T = *S.AngularTarget;
			Target(B, T.TargetAngle, T.MaxAngularSpeed, T.SlowdownAngle, T.AngleTolerance, T.AngularSpeedTolerance, T.MaxTorque);
		}
		else
		{
			B.Add("null");
		}
		break;
	case EJointKind::Prismatic:
		B.Quote("Prismatic");
		B.Add(",\"drive\":");
		Drive(B, S.LinearDrive.bEnabled, S.LinearDrive.TargetSpeed, S.LinearDrive.MaxForce);
		B.Add(",\"limits\":");
		Limits(B, S.LinearLimits.bEnabled, S.LinearLimits.LowerTranslation, S.LinearLimits.UpperTranslation);
		B.Add(",\"target\":");
		if (S.LinearTarget)
		{
			const auto& T = *S.LinearTarget;
			Target(B, T.TargetTranslation, T.MaxSpeed, T.SlowdownDistance, T.PositionTolerance, T.SpeedTolerance, T.MaxForce);
		}
		else
		{
			B.Add("null");
		}
		break;
	default:
		Invalid("Unknown checkpoint Joint kind");
	}
	B.Add("}");
}
FCheckpointJointState Joint(const FJsonDocument& J, int32 I)
{
	FCheckpointJointState S;
	S.Instance = String(J, I, "instance");
	if (!S.Instance.IsEmpty())
	{
		Key(S.Instance);
	}
	S.Export = String(J, I, "export");
	Key(S.Export, true);
	S.bConnected = Boolean(J, I, "connected");
	const auto& Kind = String(J, I, "kind");
	if (Kind == "Fixed")
	{
		Fields(J, I, "instance|export|connected|kind");
		S.Kind = EJointKind::Fixed;
		return S;
	}
	if (Kind == "Distance")
	{
		Fields(J, I, "instance|export|connected|kind|length");
		S.Kind = EJointKind::Distance;
		const int32 L = J.Find(I, "length");
		Require(L >= 0, "Missing checkpoint length");
		if (J.Get(L).Kind != EJsonKind::Null)
		{
			S.DistanceLength = Number(J, I, "length");
			Require(*S.DistanceLength >= 0, "Invalid checkpoint Distance length");
		}
		return S;
	}
	Require(Kind == "Revolute" || Kind == "Prismatic", "Unknown checkpoint Joint kind");
	Fields(J, I, "instance|export|connected|kind|drive|limits|target");
	const int32 D = Field(J, I, "drive", EJsonKind::Object);
	Fields(J, D, "enabled|speed|maximum");
	const int32 L = Field(J, I, "limits", EJsonKind::Object);
	Fields(J, L, "enabled|lower|upper");
	const int32 T = J.Find(I, "target");
	Require(T >= 0, "Missing checkpoint target");
	if (J.Get(T).Kind != EJsonKind::Null)
	{
		Fields(J, T, "coordinate|maximumSpeed|slowdown|tolerance|speedTolerance|maximumForce");
	}
	if (Kind == "Revolute")
	{
		S.Kind = EJointKind::Revolute;
		S.AngularDrive = {Boolean(J, D, "enabled"), Number(J, D, "speed"), Number(J, D, "maximum")};
		S.AngularLimits = {Boolean(J, L, "enabled"), Number(J, L, "lower"), Number(J, L, "upper")};
		GameplayPrivate::ValidateJointDrive(S.AngularDrive);
		GameplayPrivate::ValidateJointLimits(S.AngularLimits);
		if (J.Get(T).Kind != EJsonKind::Null)
		{
			S.AngularTarget = FAngularJointTargetSettings{Number(J, T, "coordinate"), Number(J, T, "maximumSpeed"), Number(J, T, "slowdown"), Number(J, T, "tolerance"), Number(J, T, "speedTolerance"), Number(J, T, "maximumForce")};
			ComputeJointTargetDrive(*S.AngularTarget, 0, 0, S.AngularLimits, 1);
		}
	}
	else
	{
		S.Kind = EJointKind::Prismatic;
		S.LinearDrive = {Boolean(J, D, "enabled"), Number(J, D, "speed"), Number(J, D, "maximum")};
		S.LinearLimits = {Boolean(J, L, "enabled"), Number(J, L, "lower"), Number(J, L, "upper")};
		GameplayPrivate::ValidateJointDrive(S.LinearDrive);
		GameplayPrivate::ValidateJointLimits(S.LinearLimits);
		if (J.Get(T).Kind != EJsonKind::Null)
		{
			S.LinearTarget = FLinearJointTargetSettings{Number(J, T, "coordinate"), Number(J, T, "maximumSpeed"), Number(J, T, "slowdown"), Number(J, T, "tolerance"), Number(J, T, "speedTolerance"), Number(J, T, "maximumForce")};
			ComputeJointTargetDrive(*S.LinearTarget, 0, 0, S.LinearLimits, 1);
		}
	}
	return S;
}
template <typename TDocument>
FSaveDocument Encode(const TDocument& D, FCheckpointLimits L)
{
	Bounds(L);
	KeyLimits(D, L.Document.MaxKeyBytes);
	Require(D.Identity.Payload == "{}", "Checkpoint identity Payload must be empty");
	FSaveCodec::Encode(D.Identity, L.Document);
	Fingerprint(D.DefinitionFingerprint);
	Require(D.Instances.Size() <= L.MaxInstances && D.Joints.Size() <= L.MaxJoints, "Checkpoint count limit");
	FBuilder B(L.Document.MaxBytes);
	B.Add("{\"checkpointVersion\":1,\"definitionFingerprint\":");
	B.Quote(D.DefinitionFingerprint);
	B.Add(",\"instances\":[");
	bool First = true;
	for (const auto& S : D.Instances)
	{
		if (!First)
		{
			B.Add(",");
		}
		First = false;
		Key(S.Key);
		Key(S.Prefab, true);
		Fingerprint(S.DefinitionFingerprint);
		Require(S.Parameters.Size() <= L.MaxParameters, "Checkpoint parameter count limit");
		B.Add("{\"key\":");
		B.Quote(S.Key);
		B.Add(",\"prefab\":");
		B.Quote(S.Prefab);
		B.Add(",\"definitionFingerprint\":");
		B.Quote(S.DefinitionFingerprint);
		B.Add(",\"present\":");
		B.Boolean(S.bPresent);
		B.Add(",\"dynamic\":");
		B.Boolean(S.bDynamic);
		B.Add(",\"position\":");
		Vector(B, S.Placement.Position);
		B.Add(",\"rotation\":");
		Rotation(B, S.Placement.Rotation);
		B.Add(",\"parameters\":{");
		bool FirstParameter = true;
		for (const auto& P : S.Parameters)
		{
			if (!FirstParameter)
			{
				B.Add(",");
			}
			FirstParameter = false;
			Parameter(B, P);
		}
		B.Add("}}");
	}
	B.Add("],\"joints\":[");
	First = true;
	for (const auto& S : D.Joints)
	{
		if (!First)
		{
			B.Add(",");
		}
		First = false;
		Joint(B, S);
	}
	B.Add("],\"game\":");
	B.Add(D.GamePayload);
	B.Add("}");
	FSaveDocument Result = D.Identity;
	Result.Payload = B.Finish();
	// 辞書順、封筒を含む深度、バイト上限も共通コアで検査する。
	return FSaveCodec::Decode(FSaveCodec::Encode(Result, L.Document), L.Document);
}
template <typename TDocument, typename TInstance>
TDocument Decode(const FSaveDocument& Input, FCheckpointLimits L, uint32 Dimension)
{
	Bounds(L);
	const auto Document = FSaveCodec::Decode(FSaveCodec::Encode(Input, L.Document), L.Document);
	Require(Document.Kind == ESaveDocumentKind::Progress && Document.Dimension == Dimension, "Checkpoint dimension or document kind mismatch");
	const auto J = FJsonDocument::Parse(Document.Payload, {L.Document.MaxBytes, L.Document.MaxDepth, L.Document.MaxStringBytes, L.Document.MaxValues});
	const int32 Version = Field(J, 0, "checkpointVersion", EJsonKind::Number);
	if (J.Get(Version).Text != "1")
	{
		throw FSaveError(ESaveError::UnsupportedVersion, "Unsupported checkpoint version");
	}
	Fields(J, 0, "checkpointVersion|definitionFingerprint|instances|joints|game");
	TDocument Result;
	Result.Identity = Document;
	Result.Identity.Payload = "{}";
	Result.DefinitionFingerprint = String(J, 0, "definitionFingerprint");
	Fingerprint(Result.DefinitionFingerprint);
	const int32 Instances = Field(J, 0, "instances", EJsonKind::Array);
	for (int32 I = J.Get(Instances).FirstChild; I >= 0; I = J.Get(I).NextSibling)
	{
		Require(Result.Instances.Size() < L.MaxInstances, "Checkpoint instance count limit");
		Fields(J, I, "key|prefab|definitionFingerprint|present|dynamic|position|rotation|parameters");
		TInstance S;
		S.Key = String(J, I, "key");
		Key(S.Key);
		for (const auto& Existing : Result.Instances)
		{
			Require(Existing.Key != S.Key, "Duplicate checkpoint instance key");
		}
		S.Prefab = String(J, I, "prefab");
		Key(S.Prefab, true);
		S.DefinitionFingerprint = String(J, I, "definitionFingerprint");
		Fingerprint(S.DefinitionFingerprint);
		S.bPresent = Boolean(J, I, "present");
		S.bDynamic = Boolean(J, I, "dynamic");
		const int32 Position = Field(J, I, "position", EJsonKind::Array);
		if constexpr (IsSame<TInstance, FCheckpointInstance2D>)
		{
			const auto V = Numbers<2>(J, Position);
			S.Placement.Position = {Scalar(V[0]), Scalar(V[1])};
			S.Placement.Rotation = Scalar(Number(J, I, "rotation"));
		}
		else
		{
			const auto V = Numbers<3>(J, Position);
			S.Placement.Position = {Scalar(V[0]), Scalar(V[1]), Scalar(V[2])};
			const auto R = Numbers<4>(J, Field(J, I, "rotation", EJsonKind::Array));
			S.Placement.Rotation = FQuaternion{Scalar(R[0]), Scalar(R[1]), Scalar(R[2]), Scalar(R[3])}.Normalized();
		}
		const int32 Parameters = Field(J, I, "parameters", EJsonKind::Object);
		for (int32 P = J.Get(Parameters).FirstChild; P >= 0; P = J.Get(P).NextSibling)
		{
			Require(S.Parameters.Size() < L.MaxParameters, "Checkpoint parameter count limit");
			S.Parameters.PushBack(Parameter(J, P));
		}
		Result.Instances.PushBack(Move(S));
	}
	const int32 Joints = Field(J, 0, "joints", EJsonKind::Array);
	for (int32 I = J.Get(Joints).FirstChild; I >= 0; I = J.Get(I).NextSibling)
	{
		Require(Result.Joints.Size() < L.MaxJoints, "Checkpoint Joint count limit");
		auto S = Joint(J, I);
		for (const auto& Existing : Result.Joints)
		{
			Require(Existing.Instance != S.Instance || Existing.Export != S.Export, "Duplicate checkpoint Joint key");
		}
		if (!S.Instance.IsEmpty())
		{
			bool Found = false;
			for (const auto& Instance : Result.Instances)
			{
				Found = Found || (Instance.Key == S.Instance && Instance.bPresent);
			}
			Require(Found, "Checkpoint Joint refers to absent instance");
		}
		Result.Joints.PushBack(Move(S));
	}
	Result.GamePayload = FJsonWriter::Write(J, Field(J, 0, "game", EJsonKind::Object), L.Document.MaxBytes);
	KeyLimits(Result, L.Document.MaxKeyBytes);
	return Result;
}
// JSON位置は保存コアと同じ診断へ保ち、ゲーム値の検証例外も握りつぶさない。
template <typename TAction>
auto Checked(TAction&& Action)
{
	try
	{
		return Action();
	}
	catch (const FSaveError&)
	{
		throw;
	}
	catch (const FJsonError& E)
	{
		throw FSaveError(ESaveError::InvalidDocument, E.What(), E.GetLine(), E.GetColumn());
	}
	catch (const FException& E)
	{
		throw FSaveError(ESaveError::InvalidDocument, E.What());
	}
}
} // namespace
FSaveDocument FCheckpointCodec::Encode(const FCheckpointDocument2D& D, FCheckpointLimits L)
{
	return Checked([&]
	               {
		               auto Result = Dxf::Encode(D, L);
		               Decode2D(Result, L);
		               return Result;
	               });
}
FSaveDocument FCheckpointCodec::Encode(const FCheckpointDocument3D& D, FCheckpointLimits L)
{
	return Checked([&]
	               {
		               auto Result = Dxf::Encode(D, L);
		               Decode3D(Result, L);
		               return Result;
	               });
}
FCheckpointDocument2D FCheckpointCodec::Decode2D(const FSaveDocument& D, FCheckpointLimits L)
{
	return Checked([&]
	               {
		               return Decode<FCheckpointDocument2D, FCheckpointInstance2D>(D, L, 2);
	               });
}
FCheckpointDocument3D FCheckpointCodec::Decode3D(const FSaveDocument& D, FCheckpointLimits L)
{
	return Checked([&]
	               {
		               return Decode<FCheckpointDocument3D, FCheckpointInstance3D>(D, L, 3);
	               });
}
} // namespace Dxf
