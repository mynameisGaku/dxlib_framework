// SPDX-License-Identifier: NOASSERTION
#include "SmokeTraces.h"
#include <stdio.h>
namespace Dxf::GameplaySmoke
{
namespace
{
using namespace GameplaySample;
// 1回の実行のフレーム数。
constexpr Toolbox::int32 TraceFrames = 180;

// 2Dのシーンの型と入り直し。
struct FTrace2D
{
	using FScene = DCharacterSample2DScene;
	using FVector = Toolbox::FVector2;
	static FScene& Enter(FSmokeApp& App)
	{
		return App.Scene2D();
	}
};
// 3Dのシーンの型と入り直し。
struct FTrace3D
{
	using FScene = DCharacterSample3DScene;
	using FVector = Toolbox::FVector3;
	static FScene& Enter(FSmokeApp& App)
	{
		App.SwitchDimension();
		return App.Scene3D();
	}
};
// 1フレームの記録。
template <typename TVector> struct TTraceFrame
{
	// プレイヤーの中心。
	TVector Player;
	// プレイヤーの速度。
	TVector Velocity;
	// プレイヤーの固定更新の数。
	Toolbox::int64 Steps = 0;
	// 直近の固定更新でジャンプしたか。
	bool bJumped = false;
	// 直近の固定更新で着地したか。
	bool bLanded = false;
	// 歩行キャラクターの中心（いなければ既定値）。
	TVector Walker;
	// アニメーション時間。
	Toolbox::f64 Animation = 0;
};
// 実行の違い。
enum class ETraceVariant
{
	// 1画面・索引あり。
	Single,
	// 2画面・索引あり。
	Split,
	// 1画面・総当たりの参照経路。
	Reference
};

// 新しいApplicationで固定入力を実行し、フレームごとに記録して終了する。
template <typename T>
void Record_Internal(FDxLibBackends& Backends, const char* ProjectRoot, ETraceVariant Variant,
                     TTraceFrame<typename T::FVector> (&Out)[TraceFrames])
{
	FSmokeApp App(Backends, ProjectRoot);
	App.Start();
	auto& Scene = T::Enter(App);
	Check(!Scene.IsSplit() && Scene.GetWalkerCount() == 0, "trace scene is not fresh");
	if (Variant == ETraceVariant::Reference)
	{
		Scene.GetPhysicsWorld().SetQueryIndexEnabled_Internal(false);
	}
	for (Toolbox::int32 Frame = 0; Frame < TraceFrames; ++Frame)
	{
		// 押すキー（1フレームだけ）と押し続けるキー。
		App.Hold(EKey::N, Frame == 0);
		App.Hold(EKey::V, Frame == 0 && Variant == ETraceVariant::Split);
		App.Hold(EKey::D, Frame >= 5 && Frame < 125);
		App.Hold(EKey::W, Frame >= 60 && Frame < 100);
		App.Hold(EKey::A, Frame >= 130 && Frame < 170);
		App.Hold(EKey::Space, Frame == 40 || Frame == 90 || Frame == 150);
		App.Step();
		const auto& Character = Scene.GetPlayer().Get()->GetCharacter();
		auto& Entry = Out[Frame];
		Entry.Player = Character.GetCenter();
		Entry.Velocity = Character.GetVelocity();
		Entry.Steps = Character.GetStepCount();
		Entry.bJumped = Character.GetLastStep().bJumped;
		Entry.bLanded = Character.GetLastStep().bLanded;
		Entry.Walker = {};
		// 生成した直後は次のフレーム境界まで初期化されていない。
		const auto* Walker = Scene.GetWalkerCount() > 0 ? Scene.GetWalker(0).Get() : nullptr;
		if (Walker != nullptr && Walker->IsInitialized())
		{
			Entry.Walker = Walker->GetCharacter().GetCenter();
		}
		Entry.Animation = Scene.GetAnimationSeconds();
	}
	App.Hold(EKey::D, false);
	App.Hold(EKey::W, false);
	App.Hold(EKey::A, false);
	App.Hold(EKey::Space, false);
	App.Hold(EKey::N, false);
	App.Hold(EKey::V, false);
	Check(Scene.IsSplit() == (Variant == ETraceVariant::Split), "trace split state");
	App.Quit();
}
// 二つの記録が最初に食い違うフレームと項目。一致すれば-1。
template <typename TVector>
Toolbox::int32 FirstDifference_Internal(const TTraceFrame<TVector> (&A)[TraceFrames],
                                        const TTraceFrame<TVector> (&B)[TraceFrames], const char*& OutField)
{
	for (Toolbox::int32 Frame = 0; Frame < TraceFrames; ++Frame)
	{
		const auto& X = A[Frame];
		const auto& Y = B[Frame];
		OutField = !(X.Player == Y.Player)       ? "player"
		           : !(X.Velocity == Y.Velocity) ? "velocity"
		           : X.Steps != Y.Steps          ? "steps"
		           : X.bJumped != Y.bJumped      ? "jumped"
		           : X.bLanded != Y.bLanded      ? "landed"
		           : !(X.Walker == Y.Walker)     ? "walker"
		           : X.Animation != Y.Animation  ? "animation"
		                                         : nullptr;
		if (OutField != nullptr)
		{
			return Frame;
		}
	}
	return -1;
}
// 二つの記録がビット単位で一致することを確かめる。食い違えばフレームと項目を示して失敗する。
template <typename TVector>
void CheckSame_Internal(const TTraceFrame<TVector> (&A)[TraceFrames], const TTraceFrame<TVector> (&B)[TraceFrames],
                        const char* Name, const char* What)
{
	const char* Field = nullptr;
	const Toolbox::int32 Frame = FirstDifference_Internal(A, B, Field);
	if (Frame >= 0)
	{
		char Message[160];
		snprintf(Message, sizeof(Message), "%s trace %s (frame %d, %s)", Name, What, static_cast<int>(Frame), Field);
		Check(false, Message);
	}
}
template <typename T> void Run_Internal(FDxLibBackends& Backends, const char* ProjectRoot, const char* Name)
{
	TTraceFrame<typename T::FVector> Single[TraceFrames];
	TTraceFrame<typename T::FVector> Split[TraceFrames];
	TTraceFrame<typename T::FVector> Reference[TraceFrames];
	Record_Internal<T>(Backends, ProjectRoot, ETraceVariant::Single, Single);
	Record_Internal<T>(Backends, ProjectRoot, ETraceVariant::Split, Split);
	Record_Internal<T>(Backends, ProjectRoot, ETraceVariant::Reference, Reference);
	// 固定入力で実際に動き、跳んだこと（比べる記録が空でないこと）。
	bool bJumped = false;
	for (Toolbox::int32 Frame = 0; Frame < TraceFrames; ++Frame)
	{
		bJumped = bJumped || Single[Frame].bJumped;
	}
	char Message[128];
	snprintf(Message, sizeof(Message), "%s trace did not move, jump and step 180 times", Name);
	Check(bJumped && Single[TraceFrames - 1].Steps - Single[0].Steps == TraceFrames - 1 &&
	          !(Single[TraceFrames - 1].Walker == typename T::FVector{}) &&
	          !(Single[TraceFrames - 1].Player == Single[0].Player),
	      Message);
	CheckSame_Internal(Single, Split, Name, "changed with the number of views");
	CheckSame_Internal(Single, Reference, Name, "differs between the query index and the reference path");
}
} // namespace

void RunTraceComparisons2D(FDxLibBackends& Backends, const char* ProjectRoot)
{
	Run_Internal<FTrace2D>(Backends, ProjectRoot, "2D");
}
void RunTraceComparisons3D(FDxLibBackends& Backends, const char* ProjectRoot)
{
	Run_Internal<FTrace3D>(Backends, ProjectRoot, "3D");
}
} // namespace Dxf::GameplaySmoke
