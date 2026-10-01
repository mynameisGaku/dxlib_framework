// SPDX-License-Identifier: NOASSERTION
#include "Dxf/SceneContentSource.h"
#include "Dxf/ContentSchemaReader.h"
#include "Dxf/ContentExpansion.h"
#include "Dxf/ContentViews.h"
#include "Toolbox/ProjectPaths.h"
#include "Toolbox/JsonError.h"
namespace Dxf::ContentPrivate
{
// キャッシュは一要求内だけで保持する。同一ファイルの別個体を同じ実体へ結びつけない。
class FFileSession
{
public:
	FFileSession(const Toolbox::FPath& Root, FSceneContentLimits Limits) : m_Limits(Limits)
	{
		if (!m_Resolver.SetRoot(Root))
		{
			throw Toolbox::FException("Content ProjectRoot must be absolute");
		}
	}
	Toolbox::FString Read(const Toolbox::FString& Path)
	{
		Toolbox::FPath Absolute;
		if (Path.IsEmpty() || Path.Data()[0] == '/' || Path.Data()[0] == '\\')
		{
			Fail(Path, "Content path must be relative");
		}
		for (const char C : Path)
		{
			if (C == ':' || C == 0)
			{
				Fail(Path, "Invalid content path");
			}
		}
		if (!m_Resolver.Resolve(Path, Absolute))
		{
			Fail(Path, "Content path escapes ProjectRoot");
		}
		for (const auto& File : m_Files)
		{
			if (File.Absolute == Absolute.ToUtf8())
			{
				return File.Text;
			}
		}
		if (m_Files.Size() >= m_Limits.MaxFiles)
		{
			Fail(Path, "Definition file count limit");
		}
		Toolbox::TVector<Toolbox::uint8> Bytes;
		const auto Remaining = m_Limits.MaxTotalBytes - m_TotalBytes;
		try
		{
			if (!Toolbox::ReadFileBytes(Absolute, Bytes, Toolbox::Min(m_Limits.Json.MaxBytes, Remaining)))
			{
				Fail(Path, "Definition file not found");
			}
		}
		catch (const FSceneContentError&)
		{
			throw;
		}
		catch (const Toolbox::FException& E)
		{
			Fail(Path, E.What());
		}
		m_TotalBytes += Bytes.Size();
		Toolbox::FString Text(reinterpret_cast<const char*>(Bytes.Data()), Bytes.Size());
		m_Files.PushBack({Absolute.ToUtf8(), Text});
		return Text;
	}

	Toolbox::TVector<FContentAssetDefinition> SharedAssets;
	void Enter(const Toolbox::FString& Path)
	{
		Toolbox::FPath Absolute;
		if (!m_Resolver.Resolve(Path, Absolute))
		{
			Fail(Path, "Invalid Prefab path");
		}
		for (const auto& Ancestor : m_Stack)
		{
			if (Ancestor == Absolute.ToUtf8())
			{
				Fail(Path, "Cyclic Prefab dependency");
			}
		}
		if (m_Stack.Size() >= m_Limits.MaxPrefabDepth)
		{
			Fail(Path, "Prefab reference depth limit");
		}
		m_Stack.PushBack(Absolute.ToUtf8());
	}
	void Leave() noexcept
	{
		m_Stack.PopBack();
	}
	const Toolbox::FPath& GetRoot() const noexcept
	{
		return m_Resolver.GetRoot();
	}

private:
	[[noreturn]] static void Fail(const Toolbox::FString& Path, const char* Reason)
	{
		throw FSceneContentError({Path, {}, "$", Reason, 0, 0});
	}
	struct FFile
	{
		Toolbox::FString Absolute;
		Toolbox::FString Text;
	};
	Toolbox::FAssetPathResolver m_Resolver;
	FSceneContentLimits m_Limits;
	Toolbox::size_t m_TotalBytes = 0;
	Toolbox::TVector<FFile> m_Files;
	Toolbox::TVector<Toolbox::FString> m_Stack;
};
struct FFile2D
{
	using FDefinition = FPrefabDefinition2D;
	using FScene = FSceneDefinition2D;
	using FPlacement = FPrefabSpawnOptions2D;
	using FExpansion = FChildExpansion2D;
	static void Place(FDefinition& D, const FPlacement& P)
	{
		const auto C = static_cast<Toolbox::f32>(Toolbox::Cos(P.Rotation));
		const auto S = static_cast<Toolbox::f32>(Toolbox::Sin(P.Rotation));
		const auto Rotate = [C, S](Toolbox::FVector2 V)
		{
			return Toolbox::FVector2{C * V.X - S * V.Y, S * V.X + C * V.Y};
		};
		for (auto& Part : D.Parts)
		{
			auto& B = Part.Body;
			B.Position = P.Position + Rotate(B.Position);
			B.Velocity = Rotate(B.Velocity);
			B.Angle += P.Rotation;
			if (!B.Position.IsValid() || !B.Velocity.IsValid() || !Toolbox::IsFinite(B.Angle))
			{
				throw Toolbox::FException("Prefab placement overflow");
			}
		}
	}

	static constexpr Toolbox::uint32 Dimension = 2;
	static FDefinition Parse(const Toolbox::FString& Text, const Toolbox::FString& Path, const Toolbox::TVector<FContentParameterValue>& Overrides, FSceneContentLimits L, const FExpansion& Expand, const Toolbox::TVector<FContentAssetDefinition>& SharedAssets)
	{
		return ParseExpanded2D(Text, Path, Overrides, L, Expand, SharedAssets);
	}
	static void Gravity(FSchemaReader& R, Toolbox::int32 I, FScene& D)
	{
		D.Gravity = R.Vector2(I);
	}
	static FPlacement Placement(FSchemaReader& R, Toolbox::int32 I)
	{
		R.Fields(I, {"position", "angle"});
		FPlacement P;
		const auto Position = R.Find(I, "position");
		const auto Angle = R.Find(I, "angle");
		if (Position >= 0)
		{
			P.Position = R.Vector2(Position);
		}
		if (Angle >= 0)
		{
			P.Rotation = R.Scalar(Angle);
		}
		return P;
	}
};
struct FFile3D
{
	using FDefinition = FPrefabDefinition3D;
	using FScene = FSceneDefinition3D;
	using FPlacement = FPrefabSpawnOptions3D;
	using FExpansion = FChildExpansion3D;
	static void Place(FDefinition& D, const FPlacement& P)
	{
		const auto Q = P.Rotation.Normalized();
		for (auto& Part : D.Parts)
		{
			auto& B = Part.Body;
			B.Position = P.Position + Q.Rotate(B.Position);
			B.Velocity = Q.Rotate(B.Velocity);
			B.AngularVelocity = Q.Rotate(B.AngularVelocity);
			B.Orientation = (Q * B.Orientation).Normalized();
			if (!B.Position.IsValid() || !B.Velocity.IsValid() || !B.AngularVelocity.IsValid())
			{
				throw Toolbox::FException("Prefab placement overflow");
			}
		}
	}

	static constexpr Toolbox::uint32 Dimension = 3;
	static FDefinition Parse(const Toolbox::FString& Text, const Toolbox::FString& Path, const Toolbox::TVector<FContentParameterValue>& Overrides, FSceneContentLimits L, const FExpansion& Expand, const Toolbox::TVector<FContentAssetDefinition>& SharedAssets)
	{
		return ParseExpanded3D(Text, Path, Overrides, L, Expand, SharedAssets);
	}
	static void Gravity(FSchemaReader& R, Toolbox::int32 I, FScene& D)
	{
		D.Gravity = R.Vector3(I);
	}
	static FPlacement Placement(FSchemaReader& R, Toolbox::int32 I)
	{
		R.Fields(I, {"position", "rotation"});
		FPlacement P;
		const auto Position = R.Find(I, "position");
		const auto Rotation = R.Find(I, "rotation");
		if (Position >= 0)
		{
			P.Position = R.Vector3(Position);
		}
		if (Rotation >= 0)
		{
			P.Rotation = R.Rotation(Rotation);
		}
		return P;
	}
};
// JSON overrideは宣言された型で読む。配列の長さから色とベクトルを推測しない。
template <typename T>
Toolbox::TVector<FContentParameterValue> Overrides(FSchemaReader& R, Toolbox::int32 Node, const T& Definition)
{
	Toolbox::TVector<FContentParameterValue> Result;
	if (Node < 0)
	{
		return Result;
	}
	if (R.Get(Node).Kind != Toolbox::EJsonKind::Object)
	{
		R.Fail(Node, "Expected parameter overrides");
	}
	for (auto I = R.Get(Node).FirstChild; I >= 0; I = R.Get(I).NextSibling)
	{
		const FContentParameterValue* Declared = nullptr;
		for (const auto& P : Definition.Parameters)
		{
			if (P.Id == R.Get(I).Key)
			{
				Declared = &P;
			}
		}
		if (Declared == nullptr)
		{
			R.Fail(I, "Unknown parameter override");
		}
		auto P = *Declared;
		switch (P.Kind)
		{
		case EContentParameterKind::Boolean:
			P.Boolean = R.Boolean(I);
			break;
		case EContentParameterKind::Number:
			P.Number = R.Number(I);
			break;
		case EContentParameterKind::Vector2:
			P.Vector2 = R.Vector2(I);
			break;
		case EContentParameterKind::Vector3:
			P.Vector3 = R.Vector3(I);
			break;
		case EContentParameterKind::Color:
			P.Color = R.Color(I);
			break;
		case EContentParameterKind::Asset:
			P.Asset = R.Id(I);
			break;
		default:
			R.Fail(I, "Unsupported parameter type");
		}
		Result.PushBack(Toolbox::Move(P));
	}
	return Result;
}

// 子の公開先だけを親の解決表へ昇格し、内部名を文字列探索の対象にしない。
template <typename T>
void Merge(FSchemaReader& R, Toolbox::int32 Node, typename T::FDefinition& D, typename T::FDefinition Child, const Toolbox::FString& Id, FSceneContentLimits L)
{
	if (Child.Parts.Size() > L.MaxParts - D.Parts.Size() || Child.Joints.Size() > L.MaxJoints - D.Joints.Size() || Child.Assets.Size() > L.MaxAssets - D.Assets.Size() || Child.ExpandedPrefabCount > L.MaxParts - D.ExpandedPrefabCount)
	{
		R.Fail(Node, "Expanded Prefab total limit");
	}
	const auto PartOffset = static_cast<Toolbox::uint32>(D.Parts.Size());
	const auto JointOffset = static_cast<Toolbox::uint32>(D.Joints.Size());
	const auto AssetOffset = static_cast<Toolbox::uint32>(D.Assets.Size());
	D.ExpandedPrefabCount += Child.ExpandedPrefabCount;
	for (auto& P : Child.Parts)
	{
		P.Id = Id + "/" + P.Id;
		if (P.Visual.Asset >= 0)
		{
			P.Visual.Asset += static_cast<Toolbox::int32>(AssetOffset);
		}
		if (P.Visual.Font >= 0)
		{
			P.Visual.Font += static_cast<Toolbox::int32>(AssetOffset);
		}
		D.Parts.PushBack(Toolbox::Move(P));
	}
	for (auto& J : Child.Joints)
	{
		J.Id = Id + "/" + J.Id;
		J.BodyA += PartOffset;
		J.BodyB += PartOffset;
		D.Joints.PushBack(Toolbox::Move(J));
	}
	for (auto& A : Child.Assets)
	{
		A.Id = Id + "/" + A.Id;
		D.Assets.PushBack(Toolbox::Move(A));
	}
	for (auto E : Child.Exports)
	{
		E.Id = Id + "/" + E.Id;
		if (E.Kind == EContentExportKind::RigidBody || E.Kind == EContentExportKind::KinematicMover || E.Kind == EContentExportKind::Sensor)
		{
			E.Index += PartOffset;
		}
		else if (E.Kind == EContentExportKind::Distance || E.Kind == EContentExportKind::Revolute || E.Kind == EContentExportKind::Fixed || E.Kind == EContentExportKind::Prismatic)
		{
			E.Index += JointOffset;
		}
		else
		{
			E.Index += AssetOffset;
		}
		D.Imports.PushBack(Toolbox::Move(E));
	}
}
// overrideの型検査には宣言だけを読む。子の全展開を既定値確認のために繰り返さない。
template <typename T>
typename T::FDefinition ParameterDefaults(FFileSession& Files, const Toolbox::FString& Path, FSceneContentLimits L)
{
	const auto Text = Files.Read(Path);
	Toolbox::FJsonDocument Document;
	try
	{
		Document = Toolbox::FJsonDocument::Parse(Text, L.Json);
	}
	catch (const Toolbox::FJsonError& E)
	{
		throw FSceneContentError({Path, {}, "$", E.What(), E.GetLine(), E.GetColumn()});
	}
	FSchemaReader Reader(Document, Path, L);
	Reader.Parameters(Reader.Find(0, "parameters"), {});
	typename T::FDefinition Result;
	Result.Parameters = Reader.GetParameters();
	return Result;
}
template <typename T>
typename T::FDefinition LoadPrefab(FFileSession& Files, const Toolbox::FString& Path, const Toolbox::TVector<FContentParameterValue>& Values, FSceneContentLimits L)
{
	Files.Enter(Path);
	struct FLeave
	{
		FFileSession& Files;
		~FLeave()
		{
			Files.Leave();
		}
	} Leave{Files};
	const auto Text = Files.Read(Path);
	typename T::FExpansion Expand =
	    [&Files, &Path, L](FSchemaReader& R, Toolbox::int32 Node, typename T::FDefinition& D)
	{
		R.Array(Node);
		Toolbox::TVector<Toolbox::FString> Names;
		for (auto I = R.Get(Node).FirstChild; I >= 0; I = R.Get(I).NextSibling)
		{
			R.Fields(I, {"id", "prefab", "placement", "parameters"});
			const auto Id = R.Id(R.Required(I, "id"));
			for (const auto& Name : Names)
			{
				if (Name == Id)
				{
					R.Fail(I, "Duplicate child instance ID");
				}
			}
			for (const auto& P : D.Parts)
			{
				if (P.Id == Id)
				{
					R.Fail(I, "Child ID conflicts with part");
				}
			}
			for (const auto& A : D.Assets)
			{
				if (A.Id == Id)
				{
					R.Fail(I, "Child ID conflicts with asset");
				}
			}
			Names.PushBack(Id);
			if (Names.Size() > L.MaxParts)
			{
				R.Fail(I, "Child instance count limit");
			}
			const auto ChildPath = R.Path(R.Required(I, "prefab"));
			try
			{
				const auto Defaults = ParameterDefaults<T>(Files, ChildPath, L);
				auto Child = LoadPrefab<T>(Files, ChildPath, Overrides(R, R.Find(I, "parameters"), Defaults), L);
				const auto P = R.Find(I, "placement");
				if (P >= 0)
				{
					T::Place(Child, T::Placement(R, P));
				}
				Merge<T>(R, I, D, Toolbox::Move(Child), Id, L);
			}
			catch (const FSceneContentError& E)
			{
				auto Diagnostic = E.GetDiagnostic();
				Diagnostic.Location = Path + " -> " + Id + " -> " + Diagnostic.Location;
				throw FSceneContentError(Toolbox::Move(Diagnostic));
			}
		}
	};
	auto Result = T::Parse(Text, Path, Values, L, Expand, Files.SharedAssets);
	Result.ProjectRoot = Files.GetRoot().ToUtf8();
	return Result;
}

template <typename T>
typename T::FScene LoadScene(FFileSession& Files, const Toolbox::FString& Path, FSceneContentLimits L)
{
	const auto Text = Files.Read(Path);
	Toolbox::FJsonDocument Document;
	try
	{
		Document = Toolbox::FJsonDocument::Parse(Text, L.Json);
	}
	catch (const Toolbox::FJsonError& E)
	{
		throw FSceneContentError({Path, {}, "$", E.What(), E.GetLine(), E.GetColumn()});
	}
	FSchemaReader R(Document, Path, L);
	R.Fields(0, {"schema", "kind", "dimension", "gravity", "prefabs", "instances", "views", "assets", "connections", "fixedUpdate", "metadata"});
	if (R.Integer(R.Required(0, "schema"), 1) != 1 || R.String(R.Required(0, "kind")) != "scene" || R.Integer(R.Required(0, "dimension"), 3) != T::Dimension)
	{
		R.Fail(0, "Unsupported Scene schema, kind or dimension");
	}
	Files.SharedAssets = ReadSceneAssets(R, R.Find(0, "assets"), L);
	const auto Prefabs = R.Required(0, "prefabs");
	if (R.Get(Prefabs).Kind != Toolbox::EJsonKind::Object)
	{
		R.Fail(Prefabs, "Expected Prefab reference table");
	}
	for (auto Reference = R.Get(Prefabs).FirstChild; Reference >= 0; Reference = R.Get(Reference).NextSibling)
	{
		R.KeyId(Reference);
		const auto PrefabPath = R.Path(Reference);
		LoadPrefab<T>(Files, PrefabPath, {}, L);
	}
	const auto Instances = R.Required(0, "instances");
	R.Array(Instances);
	typename T::FScene D;
	D.Path = Path;
	D.ProjectRoot = Files.GetRoot().ToUtf8();
	D.Assets = Files.SharedAssets;
	const auto Fixed = R.Find(0, "fixedUpdate");
	if (Fixed >= 0)
	{
		R.Fields(Fixed, {"stepSeconds", "maxStepsPerFrame", "maximumFrameSeconds"});
		const auto Step = R.Find(Fixed, "stepSeconds");
		const auto MaximumSteps = R.Find(Fixed, "maxStepsPerFrame");
		const auto MaximumTime = R.Find(Fixed, "maximumFrameSeconds");
		if (Step >= 0)
		{
			D.FixedUpdate.StepSeconds = R.Number(Step);
		}
		if (MaximumSteps >= 0)
		{
			D.FixedUpdate.MaxStepsPerFrame = static_cast<Toolbox::uint32>(R.Integer(MaximumSteps, 1024));
		}
		if (MaximumTime >= 0)
		{
			D.FixedUpdate.MaximumFrameSeconds = R.Number(MaximumTime);
		}
		try
		{
			(void)Toolbox::FFixedStepScheduler(D.FixedUpdate);
		}
		catch (const Toolbox::FException& E)
		{
			R.Fail(Fixed, E.What());
		}
	}

	ReadViews(R, R.Find(0, "views"), D);
	const auto Gravity = R.Find(0, "gravity");
	if (Gravity >= 0)
	{
		T::Gravity(R, Gravity, D);
	}
	Toolbox::size_t Parts = 0;
	Toolbox::size_t Joints = 0;
	Toolbox::size_t Assets = 0;
	Toolbox::size_t PrefabCount = 0;
	for (auto I = R.Get(Instances).FirstChild; I >= 0; I = R.Get(I).NextSibling)
	{
		R.Fields(I, {"id", "prefab", "placement", "parameters"});
		const auto Id = R.Id(R.Required(I, "id"));
		for (const auto& Other : D.Instances)
		{
			if (Other == Id)
			{
				R.Fail(I, "Duplicate instance ID");
			}
		}
		if (D.Instances.Size() >= L.MaxParts)
		{
			R.Fail(I, "Instance count limit");
		}
		const auto Key = R.Id(R.Required(I, "prefab"));
		const auto Reference = Document.Find(Prefabs, Key);
		if (Reference < 0)
		{
			R.Fail(I, "Unknown Prefab reference");
		}
		const auto PrefabPath = R.Path(Reference);
		const auto Defaults = ParameterDefaults<T>(Files, PrefabPath, L);
		auto Definition = LoadPrefab<T>(Files, PrefabPath, Overrides(R, R.Find(I, "parameters"), Defaults), L);
		if (Definition.Parts.Size() > L.MaxParts - Parts || Definition.Joints.Size() > L.MaxJoints - Joints || Definition.Assets.Size() > L.MaxAssets - Assets || Definition.ExpandedPrefabCount > L.MaxParts - PrefabCount)
		{
			R.Fail(I, "Expanded Scene total limit");
		}
		Parts += Definition.Parts.Size();
		Joints += Definition.Joints.Size();
		Assets += Definition.Assets.Size();
		PrefabCount += Definition.ExpandedPrefabCount;
		typename T::FPlacement Placement;
		const auto P = R.Find(I, "placement");
		if (P >= 0)
		{
			Placement = T::Placement(R, P);
		}
		D.Instances.PushBack(Id);
		D.Placements.PushBack(Placement);
		D.Prefabs.PushBack(Toolbox::Move(Definition));
	}
	ReadSceneConnections(R, R.Find(0, "connections"), D, L);
	return D;
}
} // namespace Dxf::ContentPrivate
namespace Dxf
{
FSceneContentSource::FSceneContentSource(Toolbox::FPath Root, FSceneContentLimits Limits)
    : m_ProjectRoot(Toolbox::Move(Root)), m_Limits(Limits)
{
	Toolbox::FAssetPathResolver Resolver;
	if (!Resolver.SetRoot(m_ProjectRoot))
	{
		throw Toolbox::FException("Content ProjectRoot must be absolute");
	}
	if (Limits.MaxTotalBytes == 0 || Limits.MaxFiles == 0 || Limits.MaxParts == 0 || Limits.MaxJoints == 0 || Limits.MaxAssets == 0 || Limits.MaxPrefabDepth == 0 || Limits.MaxIdBytes == 0)
	{
		throw Toolbox::FException("Content limits must be nonzero");
	}
}
FPrefabDefinition2D FSceneContentSource::LoadPrefab2D(Toolbox::FString Path, const Toolbox::TVector<FContentParameterValue>& Overrides) const
{
	ContentPrivate::FFileSession Files(m_ProjectRoot, m_Limits);
	return ContentPrivate::LoadPrefab<ContentPrivate::FFile2D>(Files, Path, Overrides, m_Limits);
}
FPrefabDefinition3D FSceneContentSource::LoadPrefab3D(Toolbox::FString Path, const Toolbox::TVector<FContentParameterValue>& Overrides) const
{
	ContentPrivate::FFileSession Files(m_ProjectRoot, m_Limits);
	return ContentPrivate::LoadPrefab<ContentPrivate::FFile3D>(Files, Path, Overrides, m_Limits);
}
FSceneDefinition2D FSceneContentSource::LoadScene2D(Toolbox::FString Path) const
{
	ContentPrivate::FFileSession Files(m_ProjectRoot, m_Limits);
	return ContentPrivate::LoadScene<ContentPrivate::FFile2D>(Files, Path, m_Limits);
}
FSceneDefinition3D FSceneContentSource::LoadScene3D(Toolbox::FString Path) const
{
	ContentPrivate::FFileSession Files(m_ProjectRoot, m_Limits);
	return ContentPrivate::LoadScene<ContentPrivate::FFile3D>(Files, Path, m_Limits);
}
} // namespace Dxf
