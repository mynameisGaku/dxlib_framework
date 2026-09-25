// SPDX-License-Identifier: NOASSERTION
// サンプルの受け入れの手順を2D／3Dで共通にし、次元ごとの違い（型・角の位置・画素の確認）は特性の構造体へ分ける。
#include "SmokeAcceptance.h"
#include "Dxf/ViewCoordinates.h"
#include <stdio.h>
namespace Dxf::GameplaySmoke
{
namespace
{
using namespace GameplaySample;

// 2Dの型と、次元ごとの手順。
struct FSmoke2D
{
	using FScene = DCharacterSample2DScene;
	using FVector = Toolbox::FVector2;
	static constexpr const char* Name = "2D";
	static FScene& Scene(FSmokeApp& App)
	{
		return App.Scene2D();
	}
	static FVector At(Toolbox::f32 X, Toolbox::f32 Y, Toolbox::f32 Z)
	{
		(void)Z;
		return {X, Y};
	}
	static Toolbox::FCircle2D Ball(FVector Center, Toolbox::f32 Radius)
	{
		return {Center, Radius};
	}
	// 右端の壁と床の角: (22, 0.52)から右へ歩くと、壁の面25から接触余裕を含む0.52手前で止まる。
	static void Corner(FSmokeApp& App, FScene& Scene)
	{
		Scene.GetPlayer().Get()->GetCharacter().Teleport({22, StartY});
		App.Hold(EKey::D, true);
		for (Toolbox::int32 Frame = 0; Frame < 120; ++Frame)
		{
			App.Step();
		}
		App.Hold(EKey::D, false);
		const auto& Character = Scene.GetPlayer().Get()->GetCharacter();
		Check(Toolbox::Abs(Character.GetCenter().X - 24.48f) < 1e-3f && Character.IsGrounded(),
		      "2D wall/floor corner stop");
	}
	// 描画位置の画素はプレイヤーの色。
	static void Pixel(FSmokeApp& App, FScene& Scene, const Toolbox::FPath& Output)
	{
		const Toolbox::FVector2 Center = Scene.GetPlayer().Get()->GetCharacter().GetRenderCenter();
		const FColor Pixel = App.Capture(Output / "walk2d.png", FScene::ToScreen(Center.X, Center.Y));
		Check(Pixel.R == PlayerColor.R && Pixel.G == PlayerColor.G && Pixel.B == PlayerColor.B,
		      "2D player pixel mismatch");
	}
};
// 3Dの型と、次元ごとの手順。
struct FSmoke3D
{
	using FScene = DCharacterSample3DScene;
	using FVector = Toolbox::FVector3;
	static constexpr const char* Name = "3D";
	static FScene& Scene(FSmokeApp& App)
	{
		return App.Scene3D();
	}
	static FVector At(Toolbox::f32 X, Toolbox::f32 Y, Toolbox::f32 Z)
	{
		return {X, Y, Z};
	}
	static Toolbox::FSphere Ball(FVector Center, Toolbox::f32 Radius)
	{
		return {Center, Radius};
	}
	// 奥の壁と右端の壁の角へ斜めに進むと、二つの壁の稜線で止まる（各面から接触余裕0.02）。
	static void Corner(FSmokeApp& App, FScene& Scene)
	{
		Scene.GetPlayer().Get()->GetCharacter().Teleport({22, StartY, 2});
		App.Hold(EKey::D, true);
		App.Hold(EKey::W, true);
		for (Toolbox::int32 Frame = 0; Frame < 120; ++Frame)
		{
			App.Step();
		}
		App.Hold(EKey::D, false);
		App.Hold(EKey::W, false);
		const auto& Character = Scene.GetPlayer().Get()->GetCharacter();
		const Toolbox::FVector3 Center = Character.GetCenter();
		Check(Toolbox::Abs(Center.X - 24.48f) < 1e-3f && Toolbox::Abs(Center.Z - 3.98f) < 1e-3f &&
		          Character.IsGrounded(),
		      "3D two-wall crease stop");
	}
	// 描画位置の画素はプレイヤーの色（陰影があるので黄色系で照合）。
	static void Pixel(FSmokeApp& App, FScene& Scene, const Toolbox::FPath& Output)
	{
		const auto Projected = ProjectWorldToScreen(Scene.GetView(0), 1280, 720,
		                                            Scene.GetPlayer().Get()->GetCharacter().GetRenderCenter());
		Check(Projected && Projected.Value().bInsideView, "3D projection");
		const FColor Pixel = App.Capture(Output / "walk3d.png", Projected.Value().Screen);
		Check(Pixel.R > 150 && Pixel.G > 100 && Pixel.B < 110 && Pixel.R > Pixel.B + 80, "3D player pixel mismatch");
	}
};

// 位置の成分。
FORCEINLINE Toolbox::f32 X_Internal(Toolbox::FVector2 Value) noexcept
{
	return Value.X;
}
FORCEINLINE Toolbox::f32 X_Internal(Toolbox::FVector3 Value) noexcept
{
	return Value.X;
}
FORCEINLINE Toolbox::f32 Y_Internal(Toolbox::FVector2 Value) noexcept
{
	return Value.Y;
}
FORCEINLINE Toolbox::f32 Y_Internal(Toolbox::FVector3 Value) noexcept
{
	return Value.Y;
}
// プレイヤーの移動Component。
template <typename T> auto& Player_Internal(typename T::FScene& Scene)
{
	auto* Player = Scene.GetPlayer().Get();
	Check(Player != nullptr, "player missing");
	return Player->GetCharacter();
}
// キャラクターが地形・他のキャラクターと重なっていない（符号付き距離が-1e-4以上）。
template <typename T, typename TCharacter>
void RequireNoOverlap_Internal(typename T::FScene& Scene, const TCharacter& Character, const char* Message)
{
	const auto Contacts =
	    Scene.GetPhysicsWorld().QueryContacts(T::Ball(Character.GetCenter(), 0.5f), 0, Character.GetBodyId());
	for (Toolbox::uint32 Index = 0; Index < Contacts.Count; ++Index)
	{
		Check(Contacts.Items[Index].Separation >= -1e-4, Message);
	}
}
// 名前付きの失敗（次元を前に付ける）。
template <typename T> void CheckNamed_Internal(bool bOk, const char* What)
{
	if (!bOk)
	{
		char Message[160];
		snprintf(Message, sizeof(Message), "%s %s", T::Name, What);
		Check(false, Message);
	}
}

template <typename T> void Run_Internal(FSmokeApp& App, const Toolbox::FPath& Output)
{
	auto& Scene = T::Scene(App);
	// 開始: 静止して接地する。
	App.Step();
	App.Step();
	auto& Character = Player_Internal<T>(Scene);
	CheckNamed_Internal<T>(Character.IsGrounded() && Character.GetCenter() == T::At(StartX, StartY, 0), "start state");
	// 右へ歩き、低い段差を上り、30度の坂を上って台へ、60度の急坂の手前で止まる。毎フレーム重ならない。
	App.Hold(EKey::D, true);
	bool bSteppedUp = false;
	Toolbox::f32 Highest = 0;
	const Toolbox::int64 WalkStart = Character.GetStepCount();
	for (Toolbox::int32 Frame = 0; Frame < 420; ++Frame)
	{
		App.Step();
		bSteppedUp = bSteppedUp || Character.GetLastStep().bSteppedUp;
		Highest = Toolbox::Max(Highest, Y_Internal(Character.GetCenter()));
		RequireNoOverlap_Internal<T>(Scene, Character, "character overlapped the level");
	}
	App.Hold(EKey::D, false);
	CheckNamed_Internal<T>(Character.GetStepCount() - WalkStart == 420, "fixed steps per frame");
	CheckNamed_Internal<T>(bSteppedUp && Highest > 3.4f && X_Internal(Character.GetCenter()) < 17.2f,
	                       "walk over step, 30-degree ramp and stop before the 60-degree slope");
	// 停止: 入力を離すと減速して止まり、接地を保つ。
	for (Toolbox::int32 Frame = 0; Frame < 30; ++Frame)
	{
		App.Step();
	}
	CheckNamed_Internal<T>(Character.IsGrounded() && Toolbox::Abs(X_Internal(Character.GetVelocity())) < 1e-6f,
	                       "stop and stay grounded");
	T::Pixel(App, Scene, Output);
	// リセット、ジャンプと着地。
	App.Press(EKey::R);
	CheckNamed_Internal<T>(Character.GetCenter() == T::At(StartX, StartY, 0), "reset");
	App.Step();
	App.Press(EKey::Space);
	bool bJumped = Character.GetLastStep().bJumped;
	bool bLanded = false;
	for (Toolbox::int32 Frame = 0; Frame < 90 && !bLanded; ++Frame)
	{
		App.Step();
		bJumped = bJumped || Character.GetLastStep().bJumped;
		bLanded = Character.GetLastStep().bLanded;
	}
	CheckNamed_Internal<T>(bJumped && bLanded && Character.IsGrounded(), "jump and landing");
	// 天井: 左へ歩いて低い天井（下面1.1、x∈[-4,-1]）の下で跳ぶと頭を打ち、落ちて着地する。
	App.Hold(EKey::A, true);
	for (Toolbox::int32 Frame = 0; Frame < 120 && X_Internal(Character.GetCenter()) > -2.3f; ++Frame)
	{
		App.Step();
	}
	App.Hold(EKey::A, false);
	CheckNamed_Internal<T>(X_Internal(Character.GetCenter()) <= -2.3f, "walk under the ceiling");
	App.Press(EKey::Space);
	bool bCeiling = Character.GetLastStep().bHitCeiling;
	bLanded = false;
	for (Toolbox::int32 Frame = 0; Frame < 90 && !bLanded; ++Frame)
	{
		App.Step();
		bCeiling = bCeiling || Character.GetLastStep().bHitCeiling;
		bLanded = bCeiling && Character.GetLastStep().bLanded;
		CheckNamed_Internal<T>(Y_Internal(Character.GetCenter()) <= 1.1f - 0.5f + 1e-3f, "ceiling penetration");
	}
	CheckNamed_Internal<T>(bCeiling && bLanded && Character.IsGrounded(), "ceiling hit and landing");
	// 床へ0.2めり込ませると、次の固定更新で押し出して接地する。
	App.Press(EKey::O);
	App.Step();
	CheckNamed_Internal<T>(Character.GetLastStep().Recovery.Status == ECharacterRecoveryStatus::Resolved &&
	                           Character.IsGrounded(),
	                       "overlap recovery");
	// 角で止まる（2Dは壁と床、3Dは二つの壁）。
	T::Corner(App, Scene);
	// 一時停止中は固定更新もアニメーション時間も進まず、再開で戻る。
	App.Press(EKey::P);
	const Toolbox::int64 Paused = Character.GetStepCount();
	const Toolbox::f64 PausedTime = Scene.GetAnimationSeconds();
	for (Toolbox::int32 Frame = 0; Frame < 10; ++Frame)
	{
		App.Step();
	}
	CheckNamed_Internal<T>(Character.GetStepCount() == Paused && Scene.GetAnimationSeconds() == PausedTime, "pause");
	App.Press(EKey::P);
	App.Step();
	CheckNamed_Internal<T>(Character.GetStepCount() > Paused && Scene.GetAnimationSeconds() > PausedTime, "resume");
	// 途中の生成と破棄: 歩行キャラクターは自分で歩き、破棄するとBodyを解放する。
	App.Press(EKey::R);
	const Toolbox::uint64 Colliders = Scene.GetPhysicsWorld().GetQueryDiagnostics().AliveColliders;
	App.Press(EKey::N);
	App.Step();
	CheckNamed_Internal<T>(Scene.GetWalkerCount() == 1 && Scene.GetWalker(0).Get() != nullptr, "walker spawn");
	auto& Walker = Scene.GetWalker(0).Get()->GetCharacter();
	const Toolbox::f32 WalkerStart = X_Internal(Walker.GetCenter());
	for (Toolbox::int32 Frame = 0; Frame < 90; ++Frame)
	{
		App.Step();
		RequireNoOverlap_Internal<T>(Scene, Walker, "walker overlapped");
	}
	CheckNamed_Internal<T>(Walker.IsGrounded() && X_Internal(Walker.GetCenter()) != WalkerStart &&
	                           Scene.GetPhysicsWorld().GetQueryDiagnostics().AliveColliders == Colliders + 1,
	                       "walker moves with its own body");
	App.Press(EKey::M);
	App.Step();
	CheckNamed_Internal<T>(Scene.GetWalkerCount() == 0 &&
	                           Scene.GetPhysicsWorld().GetQueryDiagnostics().AliveColliders == Colliders,
	                       "walker destroy releases its body");
}
} // namespace

void RunAcceptance2D(FSmokeApp& App, const Toolbox::FPath& Output)
{
	Run_Internal<FSmoke2D>(App, Output);
}
void RunAcceptance3D(FSmokeApp& App, const Toolbox::FPath& Output)
{
	Run_Internal<FSmoke3D>(App, Output);
}
} // namespace Dxf::GameplaySmoke
