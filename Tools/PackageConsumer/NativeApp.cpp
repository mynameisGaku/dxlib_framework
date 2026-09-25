// SPDX-License-Identifier: NOASSERTION
// 再配置したパッケージのdxf::nativeだけを使う外部のApplication。実DxLibで2Dと3Dのシーンを順に起動し、
// 固定入力でキャラクター移動Componentを歩かせて接地・移動・2Dの画素を確かめ、終了する。
// 使い方: NativeApp <ProjectRoot（絶対パスのディレクトリ）>。成功で"NATIVE_CONSUMER_PASSED"と終了コード0。
#include "Dxf/Application.h"
#include "Dxf/CharacterMovementComponent2D.h"
#include "Dxf/CharacterMovementComponent3D.h"
#include "Dxf/NativeBackends.h"
#include "Dxf/PhysicsScene2D.h"
#include "Dxf/PhysicsScene3D.h"
#include "Dxf/RenderContext.h"
#include "Dxf/RigidBodyComponent2D.h"
#include "Dxf/RigidBodyComponent3D.h"
#include "Dxf/SceneNavigator.h"
#include "DxLib.h"
namespace
{
using namespace Dxf;
// プレイヤーの色（画素の確認に使う）。
constexpr FColor PlayerColor = {255, 200, 40, 255};

// 失敗を例外にする。
void Check(bool bOk, const char* Message)
{
	if (!bOk)
	{
		throw Toolbox::FException(Message);
	}
}
// 入力は使わない（キャラクターは常に右へ歩く）。
class FNoInput final : public IInputSource
{
public:
	TResult<FRawInput> Poll() override
	{
		return TResult<FRawInput>::Success(FRawInput{});
	}
};
// 上面y=0の床（2D／3D）。
template <typename TRigid, typename TCollider, typename TBody, typename TDescription>
class DFloor final : public DGameObject
{
public:
	explicit DFloor(TDescription Collider) : m_Collider(Collider)
	{
	}

protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		TBody Body;
		Body.Type = EBodyType::Static;
		auto Rigid = AddComponent<TRigid>(Body);
		if (!Rigid)
		{
			return TResult<void>::Failure(Rigid.Error());
		}
		auto Attached = AddComponent<TCollider>(m_Collider);
		if (!Attached)
		{
			return TResult<void>::Failure(Attached.Error());
		}
		return {};
	}

private:
	// 床のCollider。
	TDescription m_Collider;
};
// 右へ歩き続けるキャラクター（2D／3D）。
template <typename TComponent, typename TDescription, typename TVector> class DWalker final : public DGameObject
{
public:
	DWalker(TVector Start, TVector Right) : m_Start(Start), m_Right(Right)
	{
	}
	TComponent& GetCharacter() const
	{
		TComponent* Character = m_Character.Get();
		Check(Character != nullptr, "walker is not initialized");
		return *Character;
	}

protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		TDescription Description;
		Description.Center = m_Start;
		auto Added = AddComponent<TComponent>(Description);
		if (!Added)
		{
			return TResult<void>::Failure(Added.Error());
		}
		m_Character = Added.Value();
		return {};
	}
	void OnTick(const FTickContext&) override
	{
		GetCharacter().SetMoveInput(m_Right);
	}

private:
	// 移動Component。
	TObjectHandle<TComponent> m_Character;
	// 開始時の中心。
	TVector m_Start;
	// 入力の向き。
	TVector m_Right;
};
using DWalker2D = DWalker<DCharacterMovement2DComponent, FCharacterMovementDescription2D, Toolbox::FVector2>;
using DWalker3D = DWalker<DCharacterMovement3DComponent, FCharacterMovementDescription3D, Toolbox::FVector3>;

// 2Dのシーン: 床と、右へ歩くキャラクター。キャラクターを円で描く（1メートル40ピクセル、原点は画面(100, 500)）。
class DScene2D final : public DPhysicsScene2D
{
public:
	TObjectHandle<DWalker2D> GetWalker() const noexcept
	{
		return m_Walker;
	}
	static FVector2 ToScreen(Toolbox::FVector2 Point) noexcept
	{
		return {100 + Point.X * 40, 500 - Point.Y * 40};
	}

protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		using DFloor2D =
		    DFloor<DRigidBody2DComponent, DCollider2DComponent, FBodyDescription2D, FColliderDescription2D>;
		FColliderDescription2D Collider;
		Collider.Shape = Toolbox::FOrientedBox2D{{0, -1}, {50, 1}, 0};
		auto Floor = Spawn<DFloor2D>(Collider);
		auto Walker = Spawn<DWalker2D>(Toolbox::FVector2{0, 0.52f}, Toolbox::FVector2{1, 0});
		if (!Floor || !Walker)
		{
			return TResult<void>::Failure(EErrorCode::InvalidState, "2D consumer scene");
		}
		m_Walker = Walker.Value();
		return {};
	}
	void OnDraw(FRenderContext& Render) const override
	{
		FDrawStyle Background;
		Background.Color = {20, 20, 28, 255};
		Background.Layer = -1;
		(void)Render.Get2D().FillRectangle({0, 0, 1280, 720}, Background);
		if (const DWalker2D* Walker = m_Walker.Get(); Walker != nullptr && Walker->IsInitialized())
		{
			FDrawStyle Style;
			Style.Color = PlayerColor;
			(void)Render.Get2D().FillCircle(ToScreen(Walker->GetCharacter().GetRenderCenter()), 20, Style);
		}
	}

private:
	// 歩くキャラクター。
	TObjectHandle<DWalker2D> m_Walker;
};
// 3Dのシーン: 床と、右へ歩くキャラクター。
class DScene3D final : public DPhysicsScene3D
{
public:
	TObjectHandle<DWalker3D> GetWalker() const noexcept
	{
		return m_Walker;
	}

protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		using DFloor3D =
		    DFloor<DRigidBody3DComponent, DCollider3DComponent, FBodyDescription3D, FColliderDescription3D>;
		FColliderDescription3D Collider;
		Collider.Shape = Toolbox::FOBB{{0, -1, 0}, {50, 1, 50}};
		auto Floor = Spawn<DFloor3D>(Collider);
		auto Walker = Spawn<DWalker3D>(Toolbox::FVector3{0, 0.52f, 0}, Toolbox::FVector3{1, 0, 0});
		if (!Floor || !Walker)
		{
			return TResult<void>::Failure(EErrorCode::InvalidState, "3D consumer scene");
		}
		m_Walker = Walker.Value();
		return {};
	}
	void OnDraw(FRenderContext& Render) const override
	{
		FRenderView3D View;
		View.Eye = {0, 4, -9};
		View.Target = {0, 0.5f, 0};
		(void)Render.Get3D().SetView(View);
		if (const DWalker3D* Walker = m_Walker.Get(); Walker != nullptr && Walker->IsInitialized())
		{
			FDrawStyle3D Style;
			Style.Color = PlayerColor;
			(void)Render.Get3D().DrawSphere({Walker->GetCharacter().GetRenderCenter(), 0.5f}, Style, 16);
		}
	}

private:
	// 歩くキャラクター。
	TObjectHandle<DWalker3D> m_Walker;
};
} // namespace

int main(int Count, char** Args)
{
	if (Count != 2)
	{
		Toolbox::Err << "Usage: NativeApp <ProjectRoot>\n";
		return 2;
	}
	try
	{
		FDxLibBackends Backends;
		FNoInput Input;
		const auto Services = Backends.GetServices();
		FApplicationSettings Settings;
		Settings.ProjectRoot = Args[1];
		Settings.Window.Width = 1280;
		Settings.Window.Height = 720;
		Settings.Window.bVSync = false;
		Settings.ExecutionThreadCount = 1;
		FApplication App(
		    {Services.Platform, Input, Services.Textures, Services.Sounds, Services.Fonts, Services.Renderer},
		    Settings);
		Check(static_cast<bool>(App.Start(Toolbox::MakeUnique<DScene2D>())), "start failed");
		Toolbox::f64 Time = 0;
		auto Step = [&]
		{
			const auto Result = App.Step(Time);
			Time += 1.0 / 60.0;
			Check(Result && Result.Value(), "step failed");
		};
		for (Toolbox::int32 Frame = 0; Frame < 61; ++Frame)
		{
			Step();
		}
		auto* Scene2D = App.GetScenes().GetCurrent()->TryCast<DScene2D>();
		Check(Scene2D != nullptr, "2D scene missing");
		const auto& Walker2D = Scene2D->GetWalker().Get()->GetCharacter();
		Check(Walker2D.IsGrounded() && Walker2D.GetCenter().X > 2 && Walker2D.GetStepCount() >= 59, "2D walk");
		// 表示した画面の、キャラクターの描画位置の色。
		Check(DxLib::SetDrawScreen(DX_SCREEN_FRONT) == 0, "front buffer");
		// DxLibのABIに合わせた出力先。
		int R = 0;
		int G = 0;
		int B = 0;
		const FVector2 Point = DScene2D::ToScreen(Walker2D.GetRenderCenter());
		DxLib::GetColor2(DxLib::GetPixel(static_cast<int>(Point.X), static_cast<int>(Point.Y)), &R, &G, &B);
		Check(DxLib::SetDrawScreen(DX_SCREEN_BACK) == 0, "back buffer");
		Check(R == PlayerColor.R && G == PlayerColor.G && B == PlayerColor.B, "2D pixel");
		Check(static_cast<bool>(App.GetScenes().RequestChange<DScene3D>()), "change to 3D failed");
		for (Toolbox::int32 Frame = 0; Frame < 62; ++Frame)
		{
			Step();
		}
		auto* Scene3D = App.GetScenes().GetCurrent()->TryCast<DScene3D>();
		Check(Scene3D != nullptr, "3D scene missing");
		const auto& Walker3D = Scene3D->GetWalker().Get()->GetCharacter();
		Check(Walker3D.IsGrounded() && Walker3D.GetCenter().X > 2 && Walker3D.GetStepCount() >= 59, "3D walk");
		App.GetScenes().RequestQuit();
		const auto Quit = App.Step(Time);
		Check(Quit && !Quit.Value(), "quit failed");
		Toolbox::Out << "NATIVE_CONSUMER_PASSED\n";
		return 0;
	}
	catch (const Toolbox::FException& Error)
	{
		Toolbox::Err << Error.What() << "\n";
		return 1;
	}
}
