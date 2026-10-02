// SPDX-License-Identifier: NOASSERTION
#include "SaveCourseSmoke.h"
#include "SaveCourseGameInstance.h"
#include "SaveCourseScene2D.h"
#include "SaveCourseScene3D.h"
#include "Dxf/SaveStore.h"
#include "Dxf/ViewCoordinates.h"
#include "DxLib.h"
namespace Dxf::GameplaySmoke
{
namespace
{
using namespace Toolbox;
using namespace GameplaySample;
// 2Dの表示は現在の画素寸法とデータの縮尺から独立して求める。
FVector2 Project(const FSceneDefinition2D& Definition, Toolbox::FVector2 Position, bool Split, int32 Side, int32 Width, int32 Height)
{
	const f32 Origin = !Split ? static_cast<f32>(Width) / 2 : Side == 0 ? static_cast<f32>(Width) / 4
	                                                                    : Width / 2 + static_cast<f32>(Width - Width / 2) / 2;
	// 16mのコース幅と40画素の余白。物理の座標は変更しない。
	const auto PixelsPerMeter = Min(Definition.Views[0].PixelsPerMeter, Max(1.0f, static_cast<f32>((Split ? Width / 2 : Width) - 40) / 16));
	return {Origin + Position.X * PixelsPerMeter, Height * .65f - Position.Y * PixelsPerMeter};
}
// 3Dは定義と同じカメラを現在のViewportへ投影する。
FVector2 Project(const FSceneDefinition3D& Definition, Toolbox::FVector3 Position, bool Split, int32 Side, int32 Width, int32 Height)
{
	auto View = Definition.Views[Split && Side == 1 && Definition.ViewCount == 2 ? 1 : 0];
	View.bViewport = Split;
	View.Viewport = Side == 0 ? FIntRect{0, 0, Width / 2, Height} : FIntRect{Width / 2, 0, Width, Height};
	const auto Screen = ProjectWorldToScreen(View, Width, Height, Position);
	Check(Screen && Screen.Value().bInsideView, "save projection outside view");
	return Screen.Value().Screen;
}
// 進行を起こす試験配置。製品の移動は通常Action・Joint要求を使う。
template <typename TScene, typename TBody>
void Place(TScene& Scene, TBody Body, f32 X, f32 Y)
{
	if constexpr (IsSame<TScene, ASaveCourseScene2D>)
	{
		Scene.GetPhysicsWorld().SetBodyTransform(Body, Toolbox::FVector2{X, Y}, 0);
	}
	else
	{
		Scene.GetPhysicsWorld().SetBodyTransform(Body, Toolbox::FVector3{X, Y, 0}, {});
	}
	Scene.GetPhysicsWorld().SetVelocity(Body, {});
}
// 旧handleの再利用や失効getterに頼らず、生存する論理構成を検査する。
template <typename TScene>
bool Has(const TScene& Scene, FStringView Id)
{
	for (const auto& Key : Scene.GetDefinition().Instances)
	{
		if (Key == Id)
		{
			return true;
		}
	}
	return false;
}
// 画素の条件はデータの色から固定し、失敗時に範囲を広げない。
bool Matches(FColor Color, FColor Expected, int32 Tolerance)
{
	return Abs(static_cast<int32>(Color.R) - Expected.R) <= Tolerance && Abs(static_cast<int32>(Color.G) - Expected.G) <= Tolerance && Abs(static_cast<int32>(Color.B) - Expected.B) <= Tolerance;
}
// 扉二個と取得物二個、動的荷物、モデルの領域を一回のGPU読戻しで確認する。
template <typename TScene>
void Pixels(FSmokeApp& App, TScene& Scene, const FPath& Output, const FString& File, bool Split, uint32 Phase)
{
	int Width = 0;
	int Height = 0;
	Check(DxLib::GetDrawScreenSize(&Width, &Height) == 0, "save target size");
	const auto A = Scene.GetPrefab("doorA").Get()->GetRigidBody("doorBody").Get();
	const auto B = Scene.GetPrefab("doorB").Get()->GetRigidBody("doorBody").Get();
	const auto PositionA = A->GetRenderPosition();
	const auto PositionB = B->GetRenderPosition();
	const auto Before = Scene.GetCompletedPhysicsStepCount();
	auto North = PositionA;
	North.X = -5;
	North.Y = 2;
	auto South = North;
	South.X = 5;
	auto Model = South;
	Model.X = 3;
	Model.Y = -1.2f;
	if constexpr (IsSame<TScene, ASaveCourseScene3D>)
	{
		North.Z = 0;
		South.Z = 0;
		Model.Z = .2f;
	}
	else
	{
		Model.Y = -2;
	}
	TVector<Dxf::FVector2> Points;
	const int32 Views = Split ? 2 : 1;
	const auto Cargo = Phase > 0 ? Scene.GetPrefab("cargo1").Get()->GetRigidBody("body").Get()->GetRenderPosition() : South;
	// 五つの物体領域と、別のモデル／画像領域。
	constexpr size_t Patch = 81;
	constexpr size_t AssetPatch = 961;
	constexpr size_t Stride = 5 * Patch + AssetPatch;
	for (int32 Side = 0; Side < Views; ++Side)
	{
		const decltype(PositionA) Positions[] = {PositionA, PositionB, North, South, Cargo};
		for (const auto Position : Positions)
		{
			const auto Screen = Project(Scene.GetDefinition(), Position, Split, Side, Width, Height);
			for (int32 Y = -4; Y <= 4; ++Y)
			{
				for (int32 X = -4; X <= 4; ++X)
				{
					Points.PushBack({Screen.X + X, Screen.Y + Y});
				}
			}
		}
		const auto Screen = Project(Scene.GetDefinition(), Model, Split, Side, Width, Height);
		for (int32 Y = -15; Y <= 15; ++Y)
		{
			for (int32 X = -15; X <= 15; ++X)
			{
				Points.PushBack({Screen.X + X, Screen.Y + Y});
			}
		}
	}
	TVector<FColor> Colors;
	Colors.Resize(Points.Size());
	App.CapturePoints(Output / FPath(File), Points.Data(), Colors.Data(), Points.Size());
	for (int32 Side = 0; Side < Views; ++Side)
	{
		const FColor Expected[] = {{80, 180, 240, 255}, {240, 120, 60, 255}, {240, 200, 60, 255}, {240, 200, 60, 255}, {80, 220, 130, 255}};
		for (size_t Region = 0; Region < 5; ++Region)
		{
			int32 Count = 0;
			for (size_t I = 0; I < Patch; ++I)
			{
				if (Matches(Colors[Side * Stride + Region * Patch + I], Expected[Region], Region == 2 || Region == 3 ? 12 : 35))
				{
					++Count;
				}
			}
			Out << "SAVE_PIXEL file=" << File << " side=" << Side << " region=" << static_cast<uint32>(Region) << " matches=" << Count << '\n';
			if (Region < 2 || (Region == 4 && Phase > 0) || (Region == 2 && Phase == 0) || (Region == 3 && Phase < 2))
			{
				Check(Count > 20, "save expected object pixels missing");
			}
			else if (Region == 2 || Region == 3)
			{
				Check(Count == 0, "save collected object pixels resurrected");
			}
		}
		int32 AssetCount = 0;
		for (size_t I = 0; I < AssetPatch; ++I)
		{
			const auto Color = Colors[Side * Stride + 5 * Patch + I];
			if (Color.G > 40 && Color.B > 40 && Abs(static_cast<int32>(Color.R) - Color.B) > 8)
			{
				++AssetCount;
			}
		}
		Out << "SAVE_ASSET_PIXEL file=" << File << " side=" << Side << " matches=" << AssetCount << '\n';
		Check(AssetCount > (IsSame<TScene, ASaveCourseScene2D> ? 200 : 30), "save Texture or Model missing");
	}
	Check(Scene.GetCompletedPhysicsStepCount() == Before && A->GetRenderPosition() == PositionA && B->GetRenderPosition() == PositionB, "save readback changed physics");
}
// 表示数と奇数寸法を変えてもゲーム更新を描画から追加しない。
template <typename TScene>
void Screens(FSmokeApp& App, TScene& Scene, const FPath& Output, const char* PhaseName, uint32 Phase)
{
	Pixels(App, Scene, Output, FString(PhaseName) + "-single.png", false, Phase);
	App.Press(EKey::F1);
	App.Press(EKey::Down);
	App.Press(EKey::Enter);
	App.Step();
	const auto Before = Scene.GetCompletedPhysicsStepCount();
	App.Press(EKey::F1);
	App.Step();
	Check(Scene.GetCompletedPhysicsStepCount() <= Before + 2, "view count added fixed steps");
	Pixels(App, Scene, Output, FString(PhaseName) + "-split.png", true, Phase);
	Check(DxLib::SetWindowSize(1001, 501) == 0, "save odd resize");
	App.Step();
	App.Step();
	Pixels(App, Scene, Output, FString(PhaseName) + "-odd.png", true, Phase);
	Check(DxLib::SetWindowSize(1280, 720) == 0, "save restore resize");
	App.Step();
	// 次の採取は一画面から開始する。
	App.Press(EKey::F1);
	App.Press(EKey::Down);
	App.Press(EKey::Enter);
	App.Step();
	App.Press(EKey::F1);
	App.Step();
}
// 同じexeの別起動で初期値・保存値・現在値を独立比較する。
template <typename TScene>
void Run(FDxLibBackends& Backends, const char* Root, const FPath& Output, const FPath& SaveRoot, uint32 Dimension, bool Read)
{
	auto Owner = MakeUnique<ASaveCourseGameInstance>(SaveRoot, FSceneContentSource{FPath(Root)}, false);
	auto* Game = Owner.Get();
	FSmokeApp App(Backends, Root, Move(Owner));
	App.StartInteraction(Dimension == 3);
	if (Read)
	{
		Check(Game->GetSettings().Volume == .35f && Game->GetSettings().UiScale == 1.5f, "native persisted settings missing");
		Game->RequestLoad(Dimension, "manual1");
	}
	else
	{
		Game->RequestNewGame(Dimension);
	}
	for (uint32 I = 0; I < 5; ++I)
	{
		App.Step();
	}
	auto* Scene = App.Application().GetScenes().GetCurrent()->TryCast<TScene>();
	Check(Scene && Scene->GetResumeTicket().GetState() == ECheckpointSceneState::Ready, "native save scene not Ready");
	const auto Body = Scene->GetPrefab("player").Get()->GetRigidBody("body").Get()->GetBodyId();
	if (!Read)
	{
		Check(Scene->GetProgress().Score == 0 && Scene->GetProgress().SpawnCounter == 0, "native initial progress wrong");
		Screens(App, *Scene, Output, "before", 0);
		Place(*Scene, Body, -5, 2);
		App.Step();
		Check(Scene->GetProgress().Score == 10, "native key not collected");
		Place(*Scene, Body, -6, -2);
		App.Step();
		App.Press(EKey::G);
		App.Step();
		Check(Scene->RequestDoor(0, true), "native unlocked door request");
		for (uint32 I = 0; I < 240; ++I)
		{
			App.Step();
		}
		Place(*Scene, Body, 0, 5);
		App.Step();
		Game->RequestSave("manual1");
		for (uint32 I = 0; I < 5; ++I)
		{
			App.Step();
		}
		Check(Game->GetSaveTicket().GetState() == EGameSaveRequestState::Committed, "native checkpoint did not commit");
		auto Settings = FSaveCourseInput::Defaults();
		Settings.Volume = .35f;
		Settings.UiScale = 1.5f;
		for (auto& Action : Settings.Input.Actions)
		{
			if (Action.Action == "Jump")
			{
				Action.Bindings[0].Key = EKey::K;
			}
		}
		Game->RequestSettings(Settings);
		Game->RequestSettingsSave();
		App.Step();
		App.Step();
		Check(Game->GetSettingsTicket().GetState() == EGameSaveRequestState::Committed, "native settings did not commit");
		Screens(App, *Scene, Output, "saved", 1);
		Place(*Scene, Body, 5, 2);
		App.Step();
		Place(*Scene, Body, -6, -2);
		App.Step();
		App.Press(EKey::G);
		App.Step();
		Check(Scene->RequestDoor(0, false) && Scene->RequestDoor(1, true), "native changed door requests");
		for (uint32 I = 0; I < 240; ++I)
		{
			App.Step();
		}
		Check(Scene->GetProgress().Score == 20 && Scene->GetProgress().SpawnCounter == 2 && !Scene->GetProgress().bDoorOpen[0] && Scene->GetProgress().bDoorOpen[1], "native changed progress not distinct");
		Screens(App, *Scene, Output, "changed", 2);
	}
	else
	{
		const auto& Progress = Scene->GetProgress();
		const auto Revision = Progress.Revision;
		Check(Game->GetSaveTicket().GetState() == EGameSaveRequestState::Completed, "native Prepared mislabeled as resume complete");
		Check(Progress.Score == 10 && Progress.Checkpoint == "north" && Progress.SpawnCounter == 1 && Progress.bDoorOpen[0] && !Progress.bDoorOpen[1], "native resumed progress incorrect");
		Check(!Has(*Scene, "northKey") && Has(*Scene, "southKey") && Has(*Scene, "cargo1") && !Has(*Scene, "cargo2"), "native resumed composition incorrect");
		Check(Abs(Scene->GetPhysicsWorld().GetPosition(Body).Y - 5) < .002, "native checkpoint player placement");
		const auto Door = Scene->GetPrefab("doorA").Get();
		Check(Door->GetDistanceJoint("hanger") && Door->GetRevoluteJoint("armDrive") && Door->GetFixedJoint("assembly") && Door->GetPrismaticJoint("doorDrive"), "native four joint reconstruction");
		Check(Abs(Door->GetPrismaticJoint("doorDrive").Get()->GetObservation()->State.Translation - 2) < .002, "native first step door not open");
		Screens(App, *Scene, Output, "continued", 1);
		Check(Scene->GetProgress().Score == 10 && Scene->GetProgress().Revision == Revision, "native duplicated acquisition after resume");
		Place(*Scene, Body, -6, -2);
		App.Press(EKey::Space);
		Check(Abs(Scene->GetPhysicsWorld().GetVelocity(Body).Y) < .001, "native old binding still active");
		App.Step();
		App.Press(EKey::K);
		Check(Scene->GetPhysicsWorld().GetVelocity(Body).Y > 1.9f, "native persisted binding not used by Body");
	}
	Check(FSaveStore(SaveRoot).Load("manual1").Document->Generation == 1, "native manual checkpoint changed unexpectedly");
	App.Application().GetScenes().RequestQuit();
	const auto Quit = App.Application().Step(20);
	Check(Quit && !Quit.Value(), "native save app shutdown failed");
	Out << "SAVE_COURSE_NATIVE_PASSED dimension=" << Dimension << " mode=" << (Read ? "read" : "write") << '\n';
}
} // namespace
void RunSaveCourseSmoke(FDxLibBackends& Backends, const char* Root, const Toolbox::FPath& Output, const Toolbox::FPath& SaveRoot, Toolbox::uint32 Dimension, bool bRead)
{
	if (Dimension == 2)
	{
		Run<GameplaySample::ASaveCourseScene2D>(Backends, Root, Output, SaveRoot, Dimension, bRead);
	}
	else if (Dimension == 3)
	{
		Run<GameplaySample::ASaveCourseScene3D>(Backends, Root, Output, SaveRoot, Dimension, bRead);
	}
	else
	{
		throw Toolbox::FException("Native save dimension must be 2 or 3");
	}
}
} // namespace Dxf::GameplaySmoke
