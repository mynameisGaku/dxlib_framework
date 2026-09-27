// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/Application.h"
#include "InteractionScene2D.h"
#include "InteractionScene3D.h"
#include "CharacterSample2DScene.h"
using namespace Dxf;
using namespace Dxf::GameplaySample;
namespace
{
// このCPU試験では描画命令の到達だけを受け取る。画素はNativeGameplayDeviceSmokeで別に確認する。
class FSampleRender final : public IRenderBackend
{
public:
	explicit FSampleRender(Testing::FFakeBackend& Backend) : m_pBackend(&Backend)
	{
	}
	TResult<void> SetTarget(Toolbox::int32 Handle, Toolbox::int32 Width, Toolbox::int32 Height) override
	{
		return m_pBackend->SetTarget(Handle, Width, Height);
	}
	TResult<void> Clear(FColor Color) override
	{
		return m_pBackend->Clear(Color);
	}
	TResult<void> ResetState(Toolbox::int32 Width, Toolbox::int32 Height) override
	{
		return m_pBackend->ResetState(Width, Height);
	}
	TResult<void> DrawSprite(const FSpriteCommand& Command) override
	{
		return m_pBackend->DrawSprite(Command);
	}
	TResult<void> DrawText(const FTextCommand& Command) override
	{
		return m_pBackend->DrawText(Command);
	}
	TResult<void> DrawRectangle(const FRectangleCommand& Command) override
	{
		return m_pBackend->DrawRectangle(Command);
	}
	TResult<void> Present() override
	{
		return m_pBackend->Present();
	}
	bool SupportsShapes2D() const noexcept override
	{
		return true;
	}
	bool SupportsGeometry3D() const noexcept override
	{
		return true;
	}
	bool SupportsViewports3D() const noexcept override
	{
		return true;
	}
	bool SupportsClip2D() const noexcept override
	{
		return true;
	}
	TResult<void> SetClip2D(bool Enabled, FIntRect Rect) override
	{
		return m_pBackend->SetClip2D(Enabled, Rect);
	}
	TResult<void> DrawCircle2D(const FCircleCommand2D&) override
	{
		++m_Shapes;
		return {};
	}
	TResult<void> DrawTriangle2D(const FTriangleCommand2D&) override
	{
		++m_Shapes;
		return {};
	}
	TResult<void> BeginView3D(const FRenderView3D&) override
	{
		return {};
	}
	TResult<void> DrawGeometry3D(const FPreparedGeometry3D&) override
	{
		++m_Shapes;
		return {};
	}

private:
	// 資源と基本描画の記録先。
	Testing::FFakeBackend* m_pBackend;
	// 実行した基本形状の数（画素の検査ではない）。
	Toolbox::uint64 m_Shapes = 0;
};
// 実Applicationとサンプルを使い、入出力だけを記録用の代替にする。
template <typename TScene> class TSampleApp
{
public:
	TSampleApp() : m_Render(m_Backend), m_App({m_Backend, m_Backend, m_Backend, m_Backend, m_Backend, m_Render})
	{
		REQUIRE(m_App.Start(Toolbox::MakeUnique<TScene>()));
		const auto First = m_App.Step(0);
		REQUIRE(First && First.Value());
		Step();
	}
	TScene& Scene()
	{
		auto* Scene = m_App.GetScenes().GetCurrent()->template TryCast<TScene>();
		REQUIRE(Scene != nullptr);
		return *Scene;
	}
	void Step(Toolbox::int32 Count = 1)
	{
		for (Toolbox::int32 Index = 0; Index < Count; ++Index)
		{
			m_Time += 1.0 / 60.0;
			const auto Result = m_App.Step(m_Time);
			if (!Result)
			{
				throw Toolbox::FException(Result.Error().Message);
			}
			REQUIRE(Result.Value());
		}
	}
	void Hold(EKey Key, bool bDown)
	{
		m_Backend.GetTrace().Input.Keys[static_cast<Toolbox::size_t>(Key)] = bDown;
	}
	void Press(EKey Key)
	{
		Hold(Key, true);
		Step();
		Hold(Key, false);
		Step();
	}
	FApplication& Application()
	{
		return m_App;
	}
	Testing::FFakeBackend& Backend()
	{
		return m_Backend;
	}

private:
	// 入出力の記録。Applicationより長く生存する。
	Testing::FFakeBackend m_Backend;
	// 基本形状を受け付ける描画の代替。
	FSampleRender m_Render;
	// サンプルを所有する実Application。
	FApplication m_App;
	// 固定更新の丸めを境界から離した絶対時刻。
	Toolbox::f64 m_Time = 0.25 / 60.0;
};
// 次元ごとの実Sceneと位置を共通手順へ渡す。
struct FSample2D : FInteraction2D
{
	using FScene = DInteraction2DScene;
};
struct FSample3D : FInteraction3D
{
	using FScene = DInteraction3DScene;
};
// 旧サンプルの開始位置ではなく、このコースの開始点で接地する。
template <typename T> void Start_Internal()
{
	TSampleApp<typename T::FScene> App;
	App.Step(4);
	const auto& Character = App.Scene().GetPlayer()->GetCharacter();
	REQUIRE(Character.GetCenter() == T::At(0, 0.52f));
	REQUIRE(Character.IsGrounded());
	REQUIRE(App.Scene().GetGameRules().GetPlateOccupants() == 0);
	REQUIRE(!App.Scene().GetGameRules().IsDoorOpen());
}
// 通知で取得物の所有者を破棄し、繰り返し進めても取得は一度。
template <typename T> void Pickup_Internal()
{
	TSampleApp<typename T::FScene> App;
	auto& Scene = App.Scene();
	Scene.GetPlayer()->GetCharacter().Teleport(T::At(5.5f, 0.52f));
	App.Step(3);
	REQUIRE(Scene.GetGameRules().GetCollected() == 1);
	REQUIRE(!Scene.GetCourse().Pickups[0].Get());
	App.Step(6);
	REQUIRE(Scene.GetGameRules().GetCollected() == 1);
	App.Application().Shutdown();
	REQUIRE(App.Backend().GetTrace().Fonts.IsEmpty());
}
// 圧力板は複数Colliderを持つ二体を集約し、最後の一体が出るまで扉を保持する。
template <typename T> void Plate_Internal()
{
	TSampleApp<typename T::FScene> App;
	auto& Scene = App.Scene();
	auto& World = Scene.GetPhysicsWorld();
	typename T::FBodyDescription Description;
	Description.Type = EBodyType::Kinematic;
	Description.Position = T::At(10, 0.4f);
	const auto First = World.CreateBody(Description);
	const auto FirstCollider = World.AttachCollider(First, T::Ball(0.25f));
	(void)World.AttachCollider(First, T::Ball(0.25f));
	Description.Position = T::At(10.5f, 0.4f);
	const auto Second = World.CreateBody(Description);
	(void)World.AttachCollider(Second, T::Ball(0.25f));
	App.Step(2);
	REQUIRE(Scene.GetGameRules().GetPlateOccupants() == 2);
	REQUIRE(Scene.GetGameRules().IsDoorOpen());
	REQUIRE(World.DetachCollider(FirstCollider));
	App.Step();
	REQUIRE(Scene.GetGameRules().GetPlateOccupants() == 2);
	REQUIRE(World.DestroyBody(First));
	App.Step();
	REQUIRE(Scene.GetGameRules().GetPlateOccupants() == 1);
	REQUIRE(Scene.GetGameRules().IsDoorOpen());
	REQUIRE(World.DestroyBody(Second));
	App.Step();
	REQUIRE(Scene.GetGameRules().GetPlateOccupants() == 0);
	REQUIRE(Scene.GetGameRules().IsDoorOpen());
	App.Step(130);
	REQUIRE(!Scene.GetGameRules().IsDoorOpen());
}
// 歩行で箱へ触れ、離れるまでBeginを重ねない。
template <typename T> void Contact_Internal()
{
	TSampleApp<typename T::FScene> App;
	auto& Scene = App.Scene();
	Scene.GetPlayer()->GetCharacter().Teleport(T::At(0, 0.52f));
	App.Hold(EKey::D, true);
	App.Step(90);
	App.Hold(EKey::D, false);
	REQUIRE(Scene.GetGameRules().GetCrateBegins() == 1);
	REQUIRE(Scene.GetGameRules().IsTouchingCrate());
	App.Step(5);
	REQUIRE(Scene.GetGameRules().GetCrateBegins() == 1);
	Scene.GetPlayer()->GetCharacter().Teleport(T::At(0, 0.52f));
	REQUIRE(Scene.GetGameRules().GetCrateStays() > 0);
	App.Step(2);
	REQUIRE(Scene.GetGameRules().GetCrateEnds() == 1);
	REQUIRE(!Scene.GetGameRules().IsTouchingCrate());
}
// 危険領域への進入は同じSceneの記録済み地点へ戻し、支持と速度を初期化する。
template <typename T> void Checkpoint_Internal()
{
	TSampleApp<typename T::FScene> App;
	auto& Scene = App.Scene();
	auto& Character = Scene.GetPlayer()->GetCharacter();
	Character.Teleport(T::At(22.5f, 0.52f));
	App.Step(2);
	REQUIRE(Scene.GetGameRules().GetCheckpoint() == 1);
	Character.Teleport(T::At(20, -5));
	App.Step();
	REQUIRE(Scene.GetGameRules().GetRespawns() == 1);
	REQUIRE(Character.GetCenter() == T::At(22.5f, 0.52f));
	REQUIRE(Character.GetVelocity() == T::At(0, 0));
	App.Step(3);
	REQUIRE(Character.IsGrounded());
	REQUIRE(Scene.GetGameRules().GetRespawns() == 1);
}
// 同じ入力列で表示数だけを変えても、床・キャラクター・バッチの進み方は変わらない。
template <typename T> void Views_Internal()
{
	TSampleApp<typename T::FScene> Left;
	TSampleApp<typename T::FScene> Right;
	Right.Scene().ToggleSplit();
	for (Toolbox::int32 Frame = 0; Frame < 40; ++Frame)
	{
		Left.Step();
		Right.Step();
		const auto& A = Left.Scene();
		const auto& B = Right.Scene();
		REQUIRE(A.GetPlayer()->GetCharacter().GetCenter() == B.GetPlayer()->GetCharacter().GetCenter());
		REQUIRE(A.GetPlayer()->GetCharacter().GetStepCount() == B.GetPlayer()->GetCharacter().GetStepCount());
		REQUIRE(A.GetCourse().Platforms[0].Get()->GetMover()->GetPose().Position ==
		        B.GetCourse().Platforms[0].Get()->GetMover()->GetPose().Position);
		REQUIRE(A.GetPhysicsWorld().GetEventBatch().BatchId == B.GetPhysicsWorld().GetEventBatch().BatchId);
	}
	Left.Press(EKey::P);
	const auto Position = Left.Scene().GetCourse().Platforms[0].Get()->GetMover()->GetPose().Position;
	const auto Batch = Left.Scene().GetPhysicsWorld().GetEventBatch().BatchId;
	Left.Step(10);
	REQUIRE(Left.Scene().GetPhysicsWorld().GetEventBatch().BatchId == Batch);
	REQUIRE(Left.Scene().GetCourse().Platforms[0].Get()->GetMover()->GetPose().Position == Position);
	Left.Press(EKey::P);
	Left.Step(2);
	REQUIRE(Left.Scene().GetPhysicsWorld().GetEventBatch().BatchId > Batch);
}
// Modal中に押したジャンプを保持したまま閉じても、新しいゲーム入力にしない。
template <typename T> void Settings_Internal()
{
	TSampleApp<typename T::FScene> App;
	auto& Scene = App.Scene();
	App.Press(EKey::F1);
	REQUIRE(Scene.GetClock().IsPaused());
	const auto Batch = Scene.GetPhysicsWorld().GetEventBatch().BatchId;
	App.Hold(EKey::Space, true);
	App.Step(5);
	REQUIRE(Scene.GetPhysicsWorld().GetEventBatch().BatchId == Batch);
	App.Press(EKey::F1);
	REQUIRE(!Scene.GetClock().IsPaused());
	App.Step(3);
	REQUIRE(Scene.GetPlayer()->GetCharacter().IsGrounded());
	REQUIRE(!Scene.GetPlayer()->GetCharacter().GetLastStep().bJumped);
	App.Hold(EKey::Space, false);
	App.Step();
	App.Hold(EKey::Space, true);
	Toolbox::int32 Jumps = 0;
	for (Toolbox::int32 Frame = 0; Frame < 3; ++Frame)
	{
		App.Step();
		Jumps += Scene.GetPlayer()->GetCharacter().GetLastStep().bJumped ? 1 : 0;
	}
	REQUIRE(Jumps == 1);
	App.Hold(EKey::Space, false);
}
// UI描画・描画先寸法・DPIの変化を、同じ固定時刻と入力の実Worldへ入れて比較する。
template <typename T> void Window_Internal()
{
	TSampleApp<typename T::FScene> Original;
	TSampleApp<typename T::FScene> Changed;
	Original.Hold(EKey::D, true);
	Changed.Hold(EKey::D, true);
	Changed.Hold(EKey::H, true);
	Changed.Scene().ToggleSplit();
	for (Toolbox::int32 Frame = 0; Frame < 40; ++Frame)
	{
		auto& Window = Changed.Backend().GetTrace().Window;
		Window.bKnown = true;
		Window.ClientWidth = Window.RenderWidth = Frame % 2 == 0 ? 1024 : 1600;
		Window.ClientHeight = Window.RenderHeight = Frame % 2 == 0 ? 768 : 900;
		Window.Dpi = Frame % 2 == 0 ? 96 : 144;
		++Window.Revision;
		Original.Step();
		Changed.Step();
		const auto& A = Original.Scene().GetPlayer()->GetCharacter();
		const auto& B = Changed.Scene().GetPlayer()->GetCharacter();
		REQUIRE(A.GetCenter() == B.GetCenter());
		REQUIRE(A.GetVelocity() == B.GetVelocity());
		REQUIRE(A.GetStepCount() == B.GetStepCount());
		REQUIRE(Original.Scene().GetPhysicsWorld().GetEventBatch().BatchId ==
		        Changed.Scene().GetPhysicsWorld().GetEventBatch().BatchId);
		REQUIRE(Original.Scene().GetGameRules().GetCrateStays() == Changed.Scene().GetGameRules().GetCrateStays());
	}
}
// 箱の物理の位置（描画の補間ではない）。
template <typename T, typename TScene> typename T::FVector CratePosition_Internal(TScene& Scene, const auto& Handle)
{
	const auto* Crate = Handle.Get();
	REQUIRE(Crate != nullptr && Crate->GetRigid() != nullptr);
	return Scene.GetPhysicsWorld().GetPosition(Crate->GetRigid()->GetBodyId());
}
// カプセル: 立つと低い天井の手前で止まり、しゃがむと通れ、天井の下では立てず、出てから立つ（足元は同じ高さ）。
template <typename T> void CapsuleCrouch_Internal()
{
	TSampleApp<typename T::FScene> App;
	auto& Scene = App.Scene();
	Scene.ToggleShape();
	App.Step(3);
	auto& Character = Scene.GetPlayer()->GetCharacter();
	REQUIRE(Character.GetSettings().Shape == ECharacterShape::Capsule);
	REQUIRE(Character.GetSettings().HalfHeight == InteractionLayout::StandHalfHeight);
	// 中心は0.52f+0.4をf32へ丸めた値（0.92fと1ulp違い得る）。
	REQUIRE(Toolbox::Abs(Character.GetCenter().Y - 0.92) < 1e-6 && Character.GetCenter().X == 0 &&
	        Character.IsGrounded());
	Character.Teleport(T::At(40, 3.92f));
	App.Hold(EKey::D, true);
	App.Step(60);
	// 上端の球（中心y=4.32）と天井の角(41.5, 4.5)の距離が0.52で止まる。
	const Toolbox::f64 Blocked = 41.5 - Toolbox::Sqrt(0.52 * 0.52 - 0.18 * 0.18);
	REQUIRE(Toolbox::Abs(Character.GetCenter().X - Blocked) < 0.01);
	App.Hold(EKey::C, true);
	for (Toolbox::int32 Frame = 0; Frame < 120 && Character.GetCenter().X < 42.5f; ++Frame)
	{
		App.Step();
	}
	REQUIRE(Character.GetCenter().X >= 42.5f &&
	        Character.GetSettings().HalfHeight == InteractionLayout::CrouchHalfHeight);
	REQUIRE(Toolbox::Abs(Character.GetCenter().Y - 3.62) < 1e-4);
	// 天井の下では、しゃがむ入力を離しても立ち上がれない。
	App.Hold(EKey::D, false);
	App.Hold(EKey::C, false);
	App.Step(10);
	REQUIRE(Character.GetSettings().HalfHeight == InteractionLayout::CrouchHalfHeight);
	// 天井の角(41.5, 4.5)から0.47（√(0.5²−0.18²)）より左へ出ると立ち上がれる。
	App.Hold(EKey::A, true);
	for (Toolbox::int32 Frame = 0; Frame < 90 && Character.GetCenter().X > 40.9f; ++Frame)
	{
		App.Step();
	}
	App.Hold(EKey::A, false);
	App.Step(3);
	REQUIRE(Character.GetSettings().HalfHeight == InteractionLayout::StandHalfHeight);
	REQUIRE(Toolbox::Abs(Character.GetCenter().Y - 3.92) < 1e-4 && Character.IsGrounded());
	// 円／球へ戻すと足元を保って中心が下がる。
	Scene.ToggleShape();
	App.Step(3);
	REQUIRE(Character.GetSettings().Shape == ECharacterShape::Round);
	REQUIRE(Toolbox::Abs(Character.GetCenter().Y - 3.52) < 1e-4);
}
// 押し合い:
// 触れる箱を圧力板まで押すと扉が開き（プレイヤーは板に乗っていない）、重い箱は動かない。昇降床の箱は一緒に上がる。
template <typename T> void Push_Internal()
{
	TSampleApp<typename T::FScene> App;
	auto& Scene = App.Scene();
	Scene.TogglePush();
	auto& Character = Scene.GetPlayer()->GetCharacter();
	Character.Teleport(T::At(1.5f, 0.52f));
	App.Hold(EKey::D, true);
	for (Toolbox::int32 Frame = 0; Frame < 600 && CratePosition_Internal<T>(Scene, Scene.GetCourse().Crate).X < 9.3f;
	     ++Frame)
	{
		App.Step();
	}
	App.Hold(EKey::D, false);
	App.Step(10);
	REQUIRE(CratePosition_Internal<T>(Scene, Scene.GetCourse().Crate).X >= 9.3f);
	REQUIRE(Character.GetCenter().X + 0.5f < InteractionLayout::PlateX - InteractionLayout::PlateHalfX);
	REQUIRE(Scene.GetGameRules().GetPlateOccupants() >= 1 && Scene.GetGameRules().IsDoorOpen());
	// 重い箱。
	Character.Teleport(T::At(-1, 0.52f));
	App.Hold(EKey::A, true);
	App.Step(90);
	App.Hold(EKey::A, false);
	REQUIRE(Toolbox::Abs(CratePosition_Internal<T>(Scene, Scene.GetCourse().HeavyCrate).X -
	                     InteractionLayout::HeavyCrateX) < 0.05);
	REQUIRE(Character.GetCenter().X > InteractionLayout::HeavyCrateX + 0.9f);
	// 昇降床の箱: 一周期（約12.6秒）の間、床に載ったまま上面3の近くまで上がる。
	Toolbox::f32 Highest = 0;
	for (Toolbox::int32 Frame = 0; Frame < 760; ++Frame)
	{
		App.Step();
		const auto Lift = CratePosition_Internal<T>(Scene, Scene.GetCourse().LiftCrate);
		REQUIRE(Toolbox::Abs(Lift.X - InteractionLayout::LiftX) < InteractionLayout::LiftHalfX);
		Highest = Toolbox::Max(Highest, Lift.Y);
	}
	REQUIRE(Highest > 3.0f);
}
// カプセル・押し合いでも、表示数だけを変えた二つのサンプルは同じに進む。一時停止中の切り替えは固定更新を増やさない。
template <typename T> void ModeViews_Internal()
{
	TSampleApp<typename T::FScene> Left;
	TSampleApp<typename T::FScene> Right;
	for (auto* App : {&Left, &Right})
	{
		App->Scene().ToggleShape();
		App->Scene().TogglePush();
		App->Hold(EKey::D, true);
	}
	Right.Scene().ToggleSplit();
	for (Toolbox::int32 Frame = 0; Frame < 150; ++Frame)
	{
		Left.Step();
		Right.Step();
		REQUIRE(Left.Scene().GetPlayer()->GetCharacter().GetCenter() ==
		        Right.Scene().GetPlayer()->GetCharacter().GetCenter());
		REQUIRE(CratePosition_Internal<T>(Left.Scene(), Left.Scene().GetCourse().Crate) ==
		        CratePosition_Internal<T>(Right.Scene(), Right.Scene().GetCourse().Crate));
	}
	Left.Hold(EKey::D, false);
	Left.Press(EKey::F1);
	REQUIRE(Left.Scene().GetClock().IsPaused());
	const auto Steps = Left.Scene().GetPlayer()->GetCharacter().GetStepCount();
	Left.Scene().ToggleShape();
	Left.Scene().TogglePush();
	Left.Step(10);
	REQUIRE(Left.Scene().GetPlayer()->GetCharacter().GetStepCount() == Steps);
	REQUIRE(Left.Scene().GetPlayer()->GetCharacter().GetSettings().Shape == ECharacterShape::Capsule);
	Left.Press(EKey::F1);
	Left.Step(3);
	REQUIRE(Left.Scene().GetPlayer()->GetCharacter().GetSettings().Shape == ECharacterShape::Round);
	REQUIRE(!Left.Scene().GetPlayer()->GetCharacter().GetSettings().bPushDynamicBodies);
}

} // namespace
TEST("Interaction 2D capsule crouches under the low ceiling")
{
	CapsuleCrouch_Internal<FSample2D>();
}
TEST("Interaction 3D capsule crouches under the low ceiling")
{
	CapsuleCrouch_Internal<FSample3D>();
}
TEST("Interaction 2D pushes a crate onto the plate and not the heavy crate")
{
	Push_Internal<FSample2D>();
}
TEST("Interaction 3D pushes a crate onto the plate and not the heavy crate")
{
	Push_Internal<FSample3D>();
}
TEST("Interaction 2D capsule and push keep views and pause independent")
{
	ModeViews_Internal<FSample2D>();
}
TEST("Interaction 3D capsule and push keep views and pause independent")
{
	ModeViews_Internal<FSample3D>();
}
TEST("Interaction 2D starts on its course")
{
	Start_Internal<FSample2D>();
}
TEST("Interaction 3D starts on its course")
{
	Start_Internal<FSample3D>();
}
TEST("Interaction 2D pickup destroys its actual object once")
{
	Pickup_Internal<FSample2D>();
}
TEST("Interaction 3D pickup destroys its actual object once")
{
	Pickup_Internal<FSample3D>();
}
TEST("Interaction 2D pressure plate aggregates collider pairs and bodies")
{
	Plate_Internal<FSample2D>();
}
TEST("Interaction 3D pressure plate aggregates collider pairs and bodies")
{
	Plate_Internal<FSample3D>();
}
TEST("Interaction 2D walking reaches contact begin stay end")
{
	Contact_Internal<FSample2D>();
}
TEST("Interaction 3D walking reaches contact begin stay end")
{
	Contact_Internal<FSample3D>();
}
TEST("Interaction 2D hazard restores the recorded checkpoint")
{
	Checkpoint_Internal<FSample2D>();
}
TEST("Interaction 3D hazard restores the recorded checkpoint")
{
	Checkpoint_Internal<FSample3D>();
}
TEST("Interaction 2D views and pause do not multiply fixed updates")
{
	Views_Internal<FSample2D>();
}
TEST("Interaction 3D views and pause do not multiply fixed updates")
{
	Views_Internal<FSample3D>();
}

TEST("Interaction 2D settings consume held jump through resume")
{
	Settings_Internal<FSample2D>();
}
TEST("Interaction 3D settings consume held jump through resume")
{
	Settings_Internal<FSample3D>();
}
TEST("Interaction 2D UI resize and DPI do not affect simulation")
{
	Window_Internal<FSample2D>();
}
TEST("Interaction 3D UI resize and DPI do not affect simulation")
{
	Window_Internal<FSample3D>();
}

namespace
{
// 設定を閉じる瞬間に初めて押した操作も、Modalの入力として離すまで消費する。
template <typename T> void CloseFrame_Internal()
{
	TSampleApp<typename T::FScene> App;
	App.Press(EKey::F1);
	REQUIRE(App.Scene().GetClock().IsPaused());
	App.Hold(EKey::F1, true);
	App.Hold(EKey::Space, true);
	App.Hold(EKey::Escape, true);
	App.Step();
	REQUIRE(!App.Scene().GetClock().IsPaused());
	App.Hold(EKey::F1, false);
	App.Step(3);
	REQUIRE(App.Scene().GetPlayer()->GetCharacter().IsGrounded());
	App.Hold(EKey::Space, false);
	App.Hold(EKey::Escape, false);
	App.Step();
}
// 奇数幅の描画先を、隙間のない二つの領域に分ける。
template <typename T> void OddWidth_Internal()
{
	TSampleApp<typename T::FScene> App;
	auto& Window = App.Backend().GetTrace().Window;
	Window.bKnown = true;
	Window.ClientWidth = Window.RenderWidth = 1281;
	Window.ClientHeight = Window.RenderHeight = 721;
	App.Scene().ToggleSplit();
	App.Step();
	if constexpr (sizeof(typename T::FVector) == sizeof(Toolbox::FVector2))
	{
		const auto Center = App.Scene().GetPlayer()->GetCharacter().GetRenderCenter();
		REQUIRE(App.Scene().ToScreen(Center, 0).X == 320.0f);
		REQUIRE(App.Scene().ToScreen(Center, 1).X == 960.5f);
	}
	else
	{
		REQUIRE(App.Scene().GetView(0).Viewport.Right == 640);
		REQUIRE(App.Scene().GetView(1).Viewport.Left == 640);
		REQUIRE(App.Scene().GetView(1).Viewport.Right == 1281);
	}
}
} // namespace
TEST("Interaction 2D modal close consumes simultaneous new keys")
{
	CloseFrame_Internal<FSample2D>();
}
TEST("Interaction 3D modal close consumes simultaneous new keys")
{
	CloseFrame_Internal<FSample3D>();
}
TEST("Interaction 2D odd render width shares exact view boundaries")
{
	OddWidth_Internal<FSample2D>();
}
TEST("Interaction 3D odd render width shares exact view boundaries")
{
	OddWidth_Internal<FSample3D>();
}
