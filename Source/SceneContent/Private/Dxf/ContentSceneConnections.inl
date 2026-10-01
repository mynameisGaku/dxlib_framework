// SPDX-License-Identifier: NOASSERTION
namespace Dxf::ContentPrivate
{
template <typename T, typename TScene>
void Connections(FSchemaReader& R, Toolbox::int32 Index, TScene& Scene, FSceneContentLimits L)
{
	if (Index < 0)
	{
		return;
	}
	typename T::FDefinition Bodies;
	Toolbox::TVector<FContentSceneEndpoint> Endpoints;
	// 全宣言を揃えてから接続を読む。JSONメンバー順とBody登録順に依存しない。
	for (Toolbox::size_t I = 0; I < Scene.Prefabs.Size(); ++I)
	{
		const auto& P = Scene.Prefabs[I];
		const auto& Placement = Scene.Placements[I];
		for (const auto& E : P.Exports)
		{
			if (E.Kind != EContentExportKind::RigidBody && E.Kind != EContentExportKind::KinematicMover)
			{
				continue;
			}
			auto Part = P.Parts[E.Index];
			Part.Colliders.Clear();
			Part.Visual = {};
			Part.Id = Scene.Instances[I] + "/" + E.Id;
			Part.Body.Position = Placement.Position + T::Rotate(Placement.Rotation, Part.Body.Position);
			if constexpr (T::Dimension == 2)
			{
				Part.Body.Angle += Placement.Rotation;
			}
			else
			{
				Part.Body.Orientation = (Placement.Rotation * Part.Body.Orientation).Normalized();
			}
			const auto Slot = static_cast<Toolbox::uint32>(Bodies.Parts.Size());
			Bodies.Imports.PushBack({Part.Id, E.Kind, Slot});
			Bodies.Parts.PushBack(Toolbox::Move(Part));
			Endpoints.PushBack({static_cast<Toolbox::uint32>(I), E});
		}
	}
	// Scene座標の共通Frameを、一度だけBodyローカルへ変換する。
	R.SetFrameSpace("Scene");
	ReadJoints<T>(R, Index, Bodies, L);
	Toolbox::size_t Total = Bodies.Joints.Size();
	for (const auto& P : Scene.Prefabs)
	{
		if (P.Joints.Size() > L.MaxJoints - Total)
		{
			R.Fail(Index, "Expanded Scene Joint limit including connections");
		}
		Total += P.Joints.Size();
	}
	for (const auto& J : Bodies.Joints)
	{
		const auto& A = Endpoints[J.BodyA];
		const auto& B = Endpoints[J.BodyB];
		if (A.Instance == B.Instance && A.Export.Index == B.Export.Index)
		{
			R.Fail(Index, "Connection refers to same Body through aliases");
		}
		typename Toolbox::TRemoveReference<decltype(Scene.Connections[0])>::Type Connection;
		Connection.BodyA = A;
		Connection.BodyB = B;
		Connection.Joint = J;
		Scene.Connections.PushBack(Toolbox::Move(Connection));
	}
}
void ReadSceneConnections(FSchemaReader& R, Toolbox::int32 Index, FSceneDefinition2D& Scene, FSceneContentLimits L)
{
	Connections<FSchema2D>(R, Index, Scene, L);
}
void ReadSceneConnections(FSchemaReader& R, Toolbox::int32 Index, FSceneDefinition3D& Scene, FSceneContentLimits L)
{
	Connections<FSchema3D>(R, Index, Scene, L);
}
} // namespace Dxf::ContentPrivate
