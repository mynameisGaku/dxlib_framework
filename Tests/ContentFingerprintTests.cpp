// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/SceneContentSource.h"
#include "Toolbox/FileWriter.h"
#include "Toolbox/FilePublication.h"
#include "Toolbox/JsonWriter.h"
#include "Toolbox/Sha256.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
// 原本のAssetsを変更せず、この試験だけが所有する新しいRootを作る。
FPath Root()
{
	CreateDirectory(FPath(DXF_CONTENT_STAMP_TEST_ROOT));
	const FPath Path = FPath(DXF_CONTENT_STAMP_TEST_ROOT) / FPath(FJsonWriter::Unsigned(MonotonicNanoseconds()));
	CreateDirectory(Path);
	return Path;
}
// 新規コピーだけを書き込む。
void Write(const FPath& Path, const FString& Bytes)
{
	FFileWriter Writer(Path);
	Writer.Write(Bytes);
	Writer.Flush();
	Writer.Close();
}
// 少数の部品を持つ初期構成。位置変更だけでも生バイトの指紋が変わる。
FString Prefab(uint32 Dimension, uint32 Position)
{
	return FString("{\"schema\":1,\"kind\":\"prefab\",\"dimension\":") + FJsonWriter::Unsigned(Dimension) + ",\"parts\":[{\"id\":\"part\",\"body\":{\"type\":\"Static\",\"position\":[" + FJsonWriter::Unsigned(Position) + (Dimension == 2 ? ",0]}}]}" : ",0,0]}}]}");
}
// 同じPrefabの二つの個体は依存ファイル一件として数える。
FString Scene(uint32 Dimension)
{
	return FString("{\"schema\":1,\"kind\":\"scene\",\"dimension\":") + FJsonWriter::Unsigned(Dimension) + ",\"prefabs\":{\"door\":\"leaf.dxfprefab.json\"},\"instances\":[{\"id\":\"A\",\"prefab\":\"door\"},{\"id\":\"B\",\"prefab\":\"door\"}]}";
}
// 同じ検証を次元別の実パーサーとファイル準備で実行する。
template <typename TLoad>
void Exercise(uint32 Dimension, TLoad Load)
{
	const FPath First = Root();
	const FPath Second = Root();
	const FString Original = Prefab(Dimension, 0);
	const FPath* Paths[] = {&First, &Second};
	for (const FPath* Path : Paths)
	{
		Write(*Path / FPath("leaf.dxfprefab.json"), Original);
		Write(*Path / FPath("scene.dxfscene.json"), Scene(Dimension));
	}
	const auto PreparedBytes = Load(FSceneContentSource(First));
	const auto Relocated = Load(FSceneContentSource(Second));
	REQUIRE(PreparedBytes.ContentFingerprint.Size() == 64);
	REQUIRE(PreparedBytes.ContentFingerprint == Relocated.ContentFingerprint);
	REQUIRE(PreparedBytes.DefinitionStamps.Size() == 2);
	REQUIRE(PreparedBytes.DefinitionStamps[0].Path == "leaf.dxfprefab.json");
	REQUIRE(PreparedBytes.DefinitionStamps[0].ByteCount == Original.Size());
	REQUIRE(PreparedBytes.DefinitionStamps[0].Sha256 == FSha256::Compute(Original));
	// 準備後のディスク変更を旧定義の指紋へ書き戻さない。
	REQUIRE(RemoveOwnedFile(First / FPath("leaf.dxfprefab.json")));
	Write(First / FPath("leaf.dxfprefab.json"), Prefab(Dimension, 2));
	const auto Reloaded = Load(FSceneContentSource(First));
	REQUIRE(Reloaded.ContentFingerprint != PreparedBytes.ContentFingerprint);
	REQUIRE(PreparedBytes.DefinitionStamps[0].Sha256 == FSha256::Compute(Original));
	REQUIRE(PreparedBytes.Prefabs[0].Parts[0].Body.Position.X == 0);
	REQUIRE(Reloaded.Prefabs[0].Parts[0].Body.Position.X == 2);
}
} // namespace
TEST("Content 2D fingerprints loaded dependency bytes across relocation and reload")
{
	Exercise(2, [](const FSceneContentSource& Source)
	         {
		         return Source.LoadScene2D("scene.dxfscene.json");
	         });
}
TEST("Content 3D fingerprints loaded dependency bytes across relocation and reload")
{
	Exercise(3, [](const FSceneContentSource& Source)
	         {
		         return Source.LoadScene3D("scene.dxfscene.json");
	         });
}
TEST("Content nested DAG fingerprint records each loaded file once in both dimensions")
{
	for (uint32 Dimension = 2; Dimension <= 3; ++Dimension)
	{
		const FPath Path = Root();
		Write(Path / FPath("leaf.dxfprefab.json"), Prefab(Dimension, 0));
		Write(Path / FPath("parent.dxfprefab.json"), FString("{\"schema\":1,\"kind\":\"prefab\",\"dimension\":") + FJsonWriter::Unsigned(Dimension) + ",\"parts\":[],\"children\":[{\"id\":\"left\",\"prefab\":\"leaf.dxfprefab.json\"},{\"id\":\"right\",\"prefab\":\"leaf.dxfprefab.json\"}]}");
		const FSceneContentSource Source(Path);
		if (Dimension == 2)
		{
			const auto Value = Source.LoadPrefab2D("parent.dxfprefab.json");
			REQUIRE(Value.DefinitionStamps.Size() == 2 && Value.Parts.Size() == 2);
			REQUIRE(Value.ContentFingerprint.Size() == 64);
		}
		else
		{
			const auto Value = Source.LoadPrefab3D("parent.dxfprefab.json");
			REQUIRE(Value.DefinitionStamps.Size() == 2 && Value.Parts.Size() == 2);
			REQUIRE(Value.ContentFingerprint.Size() == 64);
		}
	}
}
TEST("Content string-only parser does not claim file preparation provenance")
{
	REQUIRE(ParsePrefab2D(Prefab(2, 0), "unread").ContentFingerprint.IsEmpty());
	REQUIRE(ParsePrefab3D(Prefab(3, 0), "unread").DefinitionStamps.IsEmpty());
}
