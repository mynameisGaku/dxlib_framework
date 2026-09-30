// SPDX-License-Identifier: NOASSERTION
#include "InteractionSmoke.h"
#include "Dxf/ViewCoordinates.h"
namespace Dxf::GameplaySmoke
{
namespace
{
using namespace GameplaySample;

// 同じ時刻と入力で比較するフレーム数。実機試験の既存timeout内で実行する。
constexpr Toolbox::int32 InteractionFrames = 320;

// 別Applicationごとに変わるWorldの番号を除いた、登録番号と世代。
struct FTraceCollider
{
	// Colliderの登録番号。
	Toolbox::size_t Collider = 0;
	// Colliderの世代。
	Toolbox::uint64 Generation = 0;
	// Bodyの登録番号。
	Toolbox::size_t Body = 0;
	// Bodyの世代。
	Toolbox::uint64 BodyGeneration = 0;
	// 空の支持と登録0を区別する。
	bool bPresent = false;
	// 対応する二つのWorldで同じ登録か。
	bool operator==(const FTraceCollider&) const = default;
};

// 一つの発行イベント。順序と法線も比較し、件数だけの比較にしない。
struct FTraceEvent
{
	// イベントの種類。
	EWorldEventKind Kind = EWorldEventKind::Contact;
	// Begin／Stay／End。
	EWorldEventPhase Phase = EWorldEventPhase::Begin;
	// Endの理由。
	EWorldEventEndReason EndReason = EWorldEventEndReason::None;
	// 登録番号が先のCollider。
	FTraceCollider A;
	// 登録番号が後のCollider。
	FTraceCollider B;
	// 任意の接触法線（2DのZ成分は0）。
	Toolbox::FVector3 Normal;
	// 法線が発行されたか。
	bool bNormal = false;
	// 同じイベント内容か。
	bool operator==(const FTraceEvent&) const = default;
};

// 描画の回数が変えてはいけない、1フレーム分の数値とゲーム状態。
struct FInteractionState
{
	// プレイヤーの物理位置。
	Toolbox::FVector3 Player;
	// プレイヤーの速度。
	Toolbox::FVector3 Velocity;
	// 補間した描画位置。
	Toolbox::FVector3 RenderPlayer;
	// 固定更新の回数。
	Toolbox::int64 Steps = 0;
	// 現在の支持Collider。
	FTraceCollider Ground;
	// 追従した床のCollider。
	FTraceCollider Carrier;
	// 床から要求された変位。
	Toolbox::FVector3 CarryRequested;
	// 検査を通って実際に追従した変位。
	Toolbox::FVector3 CarryApplied;
	// 水平方向の停止理由。
	ECharacterMoveStop Horizontal = ECharacterMoveStop::NoMovement;
	// 鉛直方向の停止理由。
	ECharacterMoveStop Vertical = ECharacterMoveStop::NoMovement;
	// 直近の固定更新のジャンプ。
	bool bJumped = false;
	// 直近の固定更新の着地。
	bool bLanded = false;
	// 直近の固定更新の追従。
	bool bCarried = false;
	// 追従の阻害。
	bool bCarryBlocked = false;
	// 支持速度の継承。
	bool bInherited = false;
	// 床の位置（削除した床は0）。
	Toolbox::FVector3 Platforms[3];
	// 床の回転を示すローカルX軸。
	Toolbox::FVector3 PlatformAxes[3];
	// 床の固定更新の累計時間。
	Toolbox::f64 PlatformSeconds[3]{};
	// 扉の物理位置。
	Toolbox::FVector3 Door;
	// 接触する箱の物理位置。
	Toolbox::FVector3 Crate;
	// 接触する箱の速度。
	Toolbox::FVector3 CrateVelocity;
	// 一度だけ取得された物の数。
	Toolbox::int32 Collected = 0;
	// 圧力板を占めるBodyの数。
	Toolbox::int32 Occupants = 0;
	// 箱との接触の開始回数。
	Toolbox::int32 CrateBegins = 0;
	// 箱との接触の終了回数。
	Toolbox::int32 CrateEnds = 0;
	// 最後に通過したチェックポイント。
	Toolbox::int32 Checkpoint = 0;
	// 復帰を行った回数。
	Toolbox::int32 Respawns = 0;
	// 圧力板から決まる扉の目標。
	bool bDoorOpen = false;
	// イベントを発行した成功Step。
	Toolbox::uint64 EventStep = 0;
	// イベントバッチの番号。
	Toolbox::uint64 Batch = 0;
	// 観測中の接触・Triggerの組数。
	Toolbox::uint32 Pairs = 0;
	// すべての数値を誤差なしで比較する。
	bool operator==(const FInteractionState&) const = default;
};

// 数値とイベント列の記録。
struct FInteractionFrame
{
	// 固定更新の結果とゲーム状態。
	FInteractionState State;
	// Worldが発行した順序のイベント列。
	Toolbox::TVector<FTraceEvent> Events;
	// Jointコースの実World成分と世代。World番号と構造体の余白は除く。
	Toolbox::TVector<Toolbox::f64> JointValues;
};

// 2Dの値を共通の記録へ変換する。
Toolbox::FVector3 Vector_Internal(Toolbox::FVector2 Value)
{
	return {Value.X, Value.Y, 0};
}
// 3Dの値はそのまま記録する。
Toolbox::FVector3 Vector_Internal(Toolbox::FVector3 Value)
{
	return Value;
}
// 平面角のローカルX軸。
Toolbox::FVector3 Axis_Internal(Toolbox::f32 Angle)
{
	return {static_cast<Toolbox::f32>(Toolbox::Cos(Angle)), static_cast<Toolbox::f32>(Toolbox::Sin(Angle)), 0};
}
// 空間姿勢のローカルX軸。
Toolbox::FVector3 Axis_Internal(Toolbox::FQuaternion Rotation)
{
	return Rotation.Rotate({1, 0, 0});
}
// 別Worldの番号だけを除き、世代を保持して記録する。
template <typename T> FTraceCollider Collider_Internal(const T& Id)
{
	return {Id.Index, Id.Generation, Id.Body.Index, Id.Body.Generation, true};
}
// 空の支持を保持して記録する。
template <typename T> FTraceCollider OptionalCollider_Internal(const Toolbox::TOptional<T>& Id)
{
	return Id ? Collider_Internal(*Id) : FTraceCollider{};
}

// 次元・画面数・フレーム番号を含む失敗を残す。
void RequireFrame_Internal(bool bOk, const char* Name, bool bSplit, Toolbox::int32 Frame, const char* Message)
{
	if (!bOk)
	{
		Toolbox::Err << "INTERACTION_FAILURE " << Name << " views=" << (bSplit ? 2 : 1) << " frame=" << Frame << " "
		             << Message << "\n";
		Check(false, Message);
	}
}

// 2Dのシーン・位置・投影。
struct FInteractionSmoke2D : FInteraction2D
{
	// 起動する正規サンプルの型。
	using FScene = DInteraction2DScene;
	// 起動時に選ぶ次元。
	static constexpr bool b3D = false;
	// 実行ログへ残す次元名。
	static constexpr const char* Name = "2D";
	// Applicationが所有する現在のシーンを解決する。
	static FScene& Scene(FSmokeApp& App)
	{
		return App.Interaction2D();
	}
	// 回転の中心から離れた位置へ、表面と重ならずに乗せる。
	static FVector TurnStart(const FPose& Pose)
	{
		// 既知の傾いた上面から、半径とSkinを含む距離0.52だけ離す。
		const Toolbox::f64 Angle = Pose.Rotation;
		return {Pose.Position.X + 0.5f,
		        Pose.Position.Y +
		            static_cast<Toolbox::f32>((0.25 + 0.52 + 0.5 * Toolbox::Sin(Angle)) / Toolbox::Cos(Angle))};
	}
	// 描画に使ったViewから指定座標の画素位置を求める。
	static Dxf::FVector2 Screen(FScene& Scene, FVector World, Toolbox::int32 Side)
	{
		// 描画側と同じ投影を参照し、試験側へカメラ式を複製しない。
		const auto Point = Scene.ToScreen(World, Side);
		return {Point.X, Point.Y};
	}
	// 球の投影から離れた床の照合点を選ぶ。
	static FVector FloorPixel(FVector Position)
	{
		return Position + FVector{-0.85f, 0};
	}
	// 既存のプレイヤー色の合格条件を維持する。
	static bool PlayerPixel(FColor Pixel)
	{
		return Pixel.R == PlayerColor.R && Pixel.G == PlayerColor.G && Pixel.B == PlayerColor.B;
	}
	// 実際の床の色であることを確認する。
	static bool PlatformPixel(FColor Pixel)
	{
		return Pixel.R == 80 && Pixel.G == 150 && Pixel.B == 200;
	}
};

// 3Dのシーン・位置・投影。
struct FInteractionSmoke3D : FInteraction3D
{
	// 起動する正規サンプルの型。
	using FScene = DInteraction3DScene;
	// 起動時に選ぶ次元。
	static constexpr bool b3D = true;
	// 実行ログへ残す次元名。
	static constexpr const char* Name = "3D";
	// Applicationが所有する現在のシーンを解決する。
	static FScene& Scene(FSmokeApp& App)
	{
		return App.Interaction3D();
	}
	// 回転の中心から離れた位置へ、表面と重ならずに乗せる。
	static FVector TurnStart(const FPose& Pose)
	{
		return Pose.Position + FVector{0.5f, 0.77f, 0};
	}
	// 描画に使ったViewから指定座標の画素位置を求める。
	static Dxf::FVector2 Screen(FScene& Scene, FVector World, Toolbox::int32 Side)
	{
		// 正規サンプルが描画に渡したViewでの投影結果。
		const auto Projected = ProjectWorldToScreen(Scene.GetView(Side), 1280, 720, World);
		Check(Projected && Projected.Value().bInsideView, "interaction 3D projection failed");
		return Projected.Value().Screen;
	}
	// 球の投影から離れた床の照合点を選ぶ。
	static FVector FloorPixel(FVector Position)
	{
		// 球に隠れない、床の手前左の上面。
		return Position + FVector{-0.85f, 0.25f, -1.2f};
	}
	// 既存のプレイヤー色の合格条件を維持する。
	static bool PlayerPixel(FColor Pixel)
	{
		// 既存の3Dプレイヤー照合と同じ黄色の閾値。
		return Pixel.R > 150 && Pixel.G > 100 && Pixel.B < 110 && Pixel.R > Pixel.B + 80;
	}
	// 実際の床の色であることを確認する。
	static bool PlatformPixel(FColor Pixel)
	{
		// 光源の陰影が入った青色の床。背景と黄色の球を合格にしない。
		return Pixel.B > 90 && Pixel.B > Pixel.R + 40 && Pixel.G > Pixel.R + 20;
	}
	// 実Sensorの上面。右側カメラでも横床の手前を通して見える、プレイヤーから左へ離れた点。
	static FVector HazardPoint(FVector Player)
	{
		return {Player.X - 4.0f, InteractionLayout::HazardY + InteractionLayout::HazardHalfY, 0};
	}
	// 陰影の入る赤い危険帯。黒い背景・灰色の地面・黄色の球を合格にしない。
	static bool HazardPixel(FColor Pixel)
	{
		return Pixel.R > 70 && Pixel.G < 80 && Pixel.B < 80 && Pixel.R > Pixel.G + 35 && Pixel.R > Pixel.B + 35;
	}
};

// 同じ一括読戻し画像から、1／2画面のプレイヤー・横床と3Dの危険帯を照合する。
template <typename T> void Pixels_Internal(FSmokeApp& App, typename T::FScene& Scene, const Toolbox::FPath& Output)
{
	// 既存のプレイヤー・床に、3Dだけ危険帯の1点を加える。
	constexpr Toolbox::int32 PointsPerView = T::b3D ? 3 : 2;
	Dxf::FVector2 Points[6];
	// 一括読戻し後の照合色。
	FColor Colors[6];
	// 今描いた補間位置を使う。
	const auto& Character = Scene.GetPlayer()->GetCharacter();
	// 横に動く床の公開ハンドル。
	const auto* Platform = Scene.GetCourse().Platforms[0].Get();
	Check(Platform != nullptr && Platform->GetMover() != nullptr, "interaction capture platform missing");
	// 床の補間姿勢上の照合点。
	const auto Floor = T::FloorPixel(Platform->GetMover()->GetRenderPosition());
	// 今フレームに描画した画面の数。
	const Toolbox::int32 Views = Scene.IsSplit() ? 2 : 1;
	for (Toolbox::int32 Side = 0; Side < Views; ++Side)
	{
		Points[Side * PointsPerView] = T::Screen(Scene, Character.GetRenderCenter(), Side);
		Points[Side * PointsPerView + 1] = T::Screen(Scene, Floor, Side);
		if constexpr (T::b3D)
		{
			// 描画実装の頂点を期待値に使わず、危険領域の配置から上面の点を選ぶ。
			const auto Hazard = T::HazardPoint(Character.GetRenderCenter());
			Check(Toolbox::Abs(Hazard.X - InteractionLayout::HazardX) < InteractionLayout::HazardHalfX &&
			          Toolbox::Abs(Hazard.Z) < InteractionLayout::DepthHalf,
			      "interaction hazard capture point outside sensor");
			Points[Side * PointsPerView + 2] = T::Screen(Scene, Hazard, Side);
		}
	}
	// 次元と画面数を区別する実描画の保存名。
	const char* File = T::b3D ? (Scene.IsSplit() ? "interaction3d_split.png" : "interaction3d_single.png")
	                          : (Scene.IsSplit() ? "interaction2d_split.png" : "interaction2d_single.png");
	App.CapturePoints(Output / File, Points, Colors, static_cast<Toolbox::size_t>(Views * PointsPerView));
	for (Toolbox::int32 Side = 0; Side < Views; ++Side)
	{
		Toolbox::Out << "INTERACTION_PIXEL " << T::Name << " views=" << Views << " side=" << Side
		             << " player=" << static_cast<Toolbox::int32>(Colors[Side * PointsPerView].R) << ","
		             << static_cast<Toolbox::int32>(Colors[Side * PointsPerView].G) << ","
		             << static_cast<Toolbox::int32>(Colors[Side * PointsPerView].B)
		             << " floor=" << static_cast<Toolbox::int32>(Colors[Side * PointsPerView + 1].R) << ","
		             << static_cast<Toolbox::int32>(Colors[Side * PointsPerView + 1].G) << ","
		             << static_cast<Toolbox::int32>(Colors[Side * PointsPerView + 1].B) << "\n";
		RequireFrame_Internal(T::PlayerPixel(Colors[Side * PointsPerView]), T::Name, Scene.IsSplit(), 140,
		                      "interaction player pixel mismatch");
		RequireFrame_Internal(T::PlatformPixel(Colors[Side * PointsPerView + 1]), T::Name, Scene.IsSplit(), 140,
		                      "interaction platform pixel mismatch");
		if constexpr (T::b3D)
		{
			// 同じCPU画像の上面色を照合し、余分なGPU読戻しを行わない。
			const FColor Hazard = Colors[Side * PointsPerView + 2];
			Toolbox::Out << "INTERACTION_HAZARD_PIXEL " << T::Name << " views=" << Views << " side=" << Side
			             << " hazard=" << static_cast<Toolbox::int32>(Hazard.R) << ","
			             << static_cast<Toolbox::int32>(Hazard.G) << "," << static_cast<Toolbox::int32>(Hazard.B)
			             << "\n";
			RequireFrame_Internal(T::HazardPixel(Hazard), T::Name, Scene.IsSplit(), 140,
			                      "interaction hazard pixel mismatch");
		}
	}
}

// 正規サンプルの状態を読み取り、ゲーム処理を試験側へ複製しない。
template <typename T> void ReadFrame_Internal(typename T::FScene& Scene, FInteractionFrame& Out)
{
	// 正規サンプルが所有する移動Component。
	const auto& Character = Scene.GetPlayer()->GetCharacter();
	// 最後に確定した固定更新の結果。
	const auto& Last = Character.GetLastStep();
	// 通知を受けたゲーム側の状態。
	const auto& Rules = Scene.GetGameRules();
	// このフレームの記録先。
	auto& State = Out.State;
	State.Player = Vector_Internal(Character.GetCenter());
	State.Velocity = Vector_Internal(Character.GetVelocity());
	State.RenderPlayer = Vector_Internal(Character.GetRenderCenter());
	State.Steps = Character.GetStepCount();
	State.Ground = OptionalCollider_Internal(Character.GetGround().Collider);
	State.Carrier = OptionalCollider_Internal(Last.Carrier);
	State.CarryRequested = Vector_Internal(Last.CarryRequested);
	State.CarryApplied = Vector_Internal(Last.Carry.Applied);
	State.Horizontal = Last.Horizontal.Stop;
	State.Vertical = Last.Vertical.Stop;
	State.bJumped = Last.bJumped;
	State.bLanded = Last.bLanded;
	State.bCarried = Last.bCarried;
	State.bCarryBlocked = Last.bCarryBlocked;
	State.bInherited = Last.bInheritedGroundVelocity;
	for (Toolbox::size_t Index = 0; Index < 3; ++Index)
	{
		// 削除済みの床は解決できない。
		const auto* Platform = Scene.GetCourse().Platforms[Index].Get();
		if (Platform != nullptr && Platform->GetMover() != nullptr)
		{
			// 位置・向き・時刻を決めた正規のComponent。
			const auto* Mover = Platform->GetMover();
			State.Platforms[Index] = Vector_Internal(Mover->GetPose().Position);
			State.PlatformAxes[Index] = Axis_Internal(Mover->GetPose().Rotation);
			State.PlatformSeconds[Index] = Mover->GetElapsedSeconds();
		}
	}
	State.Door = Vector_Internal(Scene.GetCourse().Door.Get()->GetMover()->GetPose().Position);
	// 箱はSolverで動くため、キャラクターとは別に物理値を比較する。
	const auto CrateBody = Scene.GetCourse().Crate.Get()->GetRigid()->GetBodyId();
	State.Crate = Vector_Internal(Scene.GetPhysicsWorld().GetPosition(CrateBody));
	State.CrateVelocity = Vector_Internal(Scene.GetPhysicsWorld().GetVelocity(CrateBody));
	State.Collected = Rules.GetCollected();
	State.Occupants = Rules.GetPlateOccupants();
	State.CrateBegins = Rules.GetCrateBegins();
	State.CrateEnds = Rules.GetCrateEnds();
	State.Checkpoint = Rules.GetCheckpoint();
	State.Respawns = Rules.GetRespawns();
	State.bDoorOpen = Rules.IsDoorOpen();
	// 読んでも消費しない、直前の成功Stepの通知列。
	const auto& Batch = Scene.GetPhysicsWorld().GetEventBatch();
	Check(Batch.bPublished && !Batch.bOverflowed, "interaction event batch was not published");
	State.EventStep = Batch.StepIndex;
	State.Batch = Batch.BatchId;
	State.Pairs = Batch.PairCount;
	for (const auto& Event : Batch.Events)
	{
		// Worldの番号だけを除いたイベントの値。
		FTraceEvent Copy;
		Copy.Kind = Event.Kind;
		Copy.Phase = Event.Phase;
		Copy.EndReason = Event.EndReason;
		Copy.A = Collider_Internal(Event.ColliderA);
		Copy.B = Collider_Internal(Event.ColliderB);
		Copy.bNormal = Event.Normal.HasValue();
		if (Event.Normal)
		{
			Copy.Normal = Vector_Internal(*Event.Normal);
		}
		Out.Events.PushBack(Copy);
	}
}

// 位置設定は各仕掛けの入口を限定するために使い、反応は実入力と正規の固定更新で発生させる。
template <typename T> void Input_Internal(FSmokeApp& App, typename T::FScene& Scene, Toolbox::int32 Frame, bool bSplit)
{
	// 配置変更も正規ComponentのTeleport契約を使う。
	auto& Character = Scene.GetPlayer()->GetCharacter();
	App.Hold(EKey::V, Frame == 0 && bSplit);
	App.Hold(EKey::D, (Frame >= 3 && Frame < 11) || (Frame >= 12 && Frame < 60));
	App.Hold(EKey::P, Frame == 145 || Frame == 155);
	App.Hold(EKey::F1, Frame == 245 || Frame == 255);
	App.Hold(EKey::Space, Frame == 150 || Frame == 160 || (Frame >= 250 && Frame <= 258) || Frame == 265);
	App.Hold(EKey::R, Frame == 225);
	if (Frame == 3)
	{
		// 接触余裕を含む箱の左側。Beginの後にStayを通し、離れたらEndを受ける。
		Character.Teleport(T::At(InteractionLayout::CrateX - 0.92f, 0.52f));
	}
	else if (Frame == 12)
	{
		// ここから取得物はD入力だけで横断する。
		Character.Teleport(T::At(4.8f, 0.52f));
	}
	else if (Frame == 60)
	{
		Character.Teleport(T::At(InteractionLayout::PlateX, 0.52f));
	}
	else if (Frame == 121)
	{
		Character.Teleport(T::At(0, 0.52f));
	}
	else if (Frame == 130 || Frame == 170)
	{
		// 横床と昇降床へ着地し、入力なしで支持の運動に運ばせる。
		const Toolbox::size_t Index = Frame == 130 ? 0 : 1;
		// この固定更新の開始時の床の中心。
		const auto Position = Scene.GetCourse().Platforms[Index].Get()->GetMover()->GetPose().Position;
		// 昇降床の中央には箱が載るため、箱と重ならない左側の上面へ配置する。
		Character.Teleport(Position + T::At(Frame == 170 ? -0.85f : 0, 0.77f));
	}
	else if (Frame == 185)
	{
		Character.Teleport(T::TurnStart(Scene.GetCourse().Platforms[2].Get()->GetMover()->GetPose()));
	}
	else if (Frame == 205)
	{
		// 公開の寿命管理を通して床を消し、支持解除を確かめる。
		Scene.GetCourse().Platforms[2].Get()->Destroy();
	}
	else if (Frame == 210)
	{
		Character.Teleport(T::At(InteractionLayout::CheckpointX[1], InteractionLayout::CheckpointY[1]));
	}
	else if (Frame == 215)
	{
		Character.Teleport(T::At(InteractionLayout::HazardX, InteractionLayout::HazardY));
	}
	else if (Frame == 224)
	{
		Character.Teleport(T::At(23, 0.52f));
	}
}

// 固定入力で実際の仕掛けが動いたことを検査する。空の一致を合格にしない。
template <typename T> void Accept_Internal(const Toolbox::TVector<FInteractionFrame>& Trace, bool bSplit)
{
	// 失敗したフレームを次元とともに出力する窓口。
	auto Require = [&](bool bOk, Toolbox::int32 Frame, const char* Message)
	{
		RequireFrame_Internal(bOk, T::Name, bSplit, Frame, Message);
	};
	Require(Trace[11].State.CrateBegins == 1 && Trace[14].State.CrateEnds == 1, 14,
	        "crate Begin/End did not reach the sample rules exactly once");
	Require(Trace[60].State.Collected == InteractionLayout::PickupCount, 60,
	        "fixed walking input did not collect all pickups");
	Require(Trace[119].State.Occupants == 1 && Trace[119].State.bDoorOpen, 119,
	        "pressure plate must count only the player");
	Require(Toolbox::Abs(Trace[119].State.Door.Y - InteractionLayout::DoorOpenY) < 1e-4f, 119,
	        "pressure plate did not open the actual door");
	Require(Trace[125].State.Occupants == 0 && Trace[125].State.bDoorOpen, 125,
	        "empty plate must keep the door open during its hold time");
	Require(Trace[140].State.bCarried && Trace[140].State.Ground.bPresent &&
	            !(Trace[140].State.Player == Trace[132].State.Player),
	        140, "sliding platform did not carry the player");
	for (Toolbox::int32 Frame = 146; Frame < 155; ++Frame)
	{
		Require(Trace[Frame].State == Trace[145].State, Frame, "pause changed physics, platform time or event batch");
	}
	Require(Trace[156].State.Steps > Trace[145].State.Steps && !Trace[156].State.bJumped, 156,
	        "resume consumed a jump pressed while paused");
	// 有効なSpace入力の後に一度だけジャンプする。
	Toolbox::int32 Jumps = 0;
	for (Toolbox::int32 Frame = 160; Frame < 165; ++Frame)
	{
		Jumps += Trace[Frame].State.bJumped ? 1 : 0;
	}
	Require(Jumps == 1, 164, "platform jump was missing or repeated");
	if (!Trace[180].State.bCarried)
	{
		char Detail[256];
		snprintf(Detail, sizeof(Detail), "LIFT_DIAGNOSTIC %s views=%d y172=%.6f y180=%.6f ground=%llu carrier=%llu carried=%d", T::Name, bSplit ? 2 : 1, Trace[172].State.Player.Y, Trace[180].State.Player.Y, static_cast<Toolbox::uint64>(Trace[180].State.Ground.Body), static_cast<Toolbox::uint64>(Trace[180].State.Carrier.Body), Trace[180].State.bCarried);
		Toolbox::Err << static_cast<const char*>(Detail) << "\n";
	}
	Require(Trace[180].State.bCarried && Toolbox::Abs(Trace[180].State.Player.Y - Trace[172].State.Player.Y) > 0.01f,
	        180, "lift did not carry the player vertically");
	Require(Trace[200].State.bCarried && !(Trace[200].State.PlatformAxes[2] == Trace[190].State.PlatformAxes[2]) &&
	            !(Trace[200].State.Player == Trace[190].State.Player),
	        200, "rotating platform did not rotate and carry the player");
	Require(!Trace[207].State.Carrier.bPresent && !Trace[207].State.Ground.bPresent, 207,
	        "destroyed platform retained its support");
	Require(Trace[214].State.Checkpoint == 1, 214, "checkpoint Trigger did not record the position");
	Require(Trace[218].State.Respawns == 1 &&
	            Toolbox::Abs(Trace[218].State.Player.X - InteractionLayout::CheckpointX[1]) < 1e-4f &&
	            Toolbox::Abs(Trace[218].State.Player.Y - InteractionLayout::CheckpointY[1]) < 1e-4f,
	        218, "hazard Trigger did not restore the checkpoint");
	Require(Trace[227].State.Respawns == 2 &&
	            Toolbox::Abs(Trace[227].State.Player.X - InteractionLayout::CheckpointX[1]) < 1e-4f,
	        227, "R input did not restore the checkpoint");
	Require(Trace[239].State.Collected == InteractionLayout::PickupCount, 239, "pickup was collected more than once");
	// 設定Modalを開いている間は、停止後の物理とイベントを保持する。
	for (Toolbox::int32 Frame = 246; Frame < 255; ++Frame)
	{
		Require(Trace[Frame].State == Trace[245].State, Frame,
		        "settings modal changed physics, platform time or event batch");
	}
	Require(Trace[256].State.Steps > Trace[245].State.Steps, 256, "settings modal did not resume fixed updates");
	// Modal中から保持していたSpaceは、閉じた後に新たな押下として流さない。
	for (Toolbox::int32 Frame = 255; Frame <= 260; ++Frame)
	{
		Require(!Trace[Frame].State.bJumped, Frame, "held Space generated a jump after closing settings");
	}
	// 一度離して押し直した入力は、通常のジャンプとして一回だけ扱う。
	Toolbox::int32 SettingsJumps = 0;
	for (Toolbox::int32 Frame = 265; Frame < 270; ++Frame)
	{
		SettingsJumps += Trace[Frame].State.bJumped ? 1 : 0;
	}
	Require(SettingsJumps == 1, 269, "fresh Space after settings was missing or repeated");
	Require(!Trace[319].State.bDoorOpen &&
	            Toolbox::Abs(Trace[319].State.Door.Y - InteractionLayout::DoorClosedY) < 1e-4f,
	        319, "door did not close after the empty plate hold expired");
	// Contact／Triggerの全遷移が一度は実バッチに現れたか。
	bool Seen[2][3]{};
	for (Toolbox::size_t Frame = 0; Frame < Trace.Size(); ++Frame)
	{
		for (const auto& Event : Trace[Frame].Events)
		{
			Seen[static_cast<Toolbox::size_t>(Event.Kind)][static_cast<Toolbox::size_t>(Event.Phase)] = true;
		}
	}
	for (Toolbox::size_t Kind = 0; Kind < 2; ++Kind)
	{
		for (Toolbox::size_t Phase = 0; Phase < 3; ++Phase)
		{
			Require(Seen[Kind][Phase], 239, "contact or Trigger transition coverage missing");
		}
	}
}

// 一つのApplicationで記録し、切替・再入場で状態が新しくなることも確かめる。
template <typename T>
void Record_Internal(FDxLibBackends& Backends, const char* ProjectRoot, const Toolbox::FPath& Output, bool bSplit,
                     Toolbox::TVector<FInteractionFrame>& Trace)
{
	// 1回ごとに新しいApplicationを起動して、再起動も通す。
	FSmokeApp App(Backends, ProjectRoot);
	App.StartInteraction(T::b3D);
	App.Step();
	App.Step();
	// 起動直後の正規シーン。
	auto& Scene = T::Scene(App);
	Check(Scene.GetPlayer() != nullptr && Scene.GetPlayer()->IsInitialized(), "interaction player missing");
	Check(Scene.GetPlayer()->GetCharacter().GetCenter() == T::At(0, 0.52f), "interaction initial checkpoint position");
	Check(Scene.GetGameRules().GetCollected() == 0 && Scene.GetGameRules().GetRespawns() == 0 && !Scene.IsSplit(),
	      "interaction restart retained game state");
	Trace.Reserve(InteractionFrames);
	for (Toolbox::int32 Frame = 0; Frame < InteractionFrames; ++Frame)
	{
		Input_Internal<T>(App, Scene, Frame, bSplit);
		App.Step();
		// 描画を終えた直後に読む、当該フレームの記録。
		FInteractionFrame Entry;
		ReadFrame_Internal<T>(Scene, Entry);
		// 画面数によるJointの二重更新を、実Native Applicationでも数値で検出する。
		auto& JointValues = Entry.JointValues;
		const auto& World = Scene.GetPhysicsWorld();
		auto AppendBody = [&](typename T::FBodyId Id)
		{
			const auto Position = World.GetPosition(Id);
			const auto Velocity = World.GetVelocity(Id);
			JointValues.PushBack(static_cast<Toolbox::f64>(Id.Index));
			JointValues.PushBack(static_cast<Toolbox::f64>(Id.Generation));
			JointValues.PushBack(Position.X);
			JointValues.PushBack(Position.Y);
			JointValues.PushBack(Velocity.X);
			JointValues.PushBack(Velocity.Y);
			JointValues.PushBack(World.IsSleeping(Id) ? 1 : 0);
			if constexpr (T::b3D)
			{
				const auto Rotation = World.GetOrientation(Id);
				const auto Angular = World.GetAngularVelocity(Id);
				JointValues.PushBack(Position.Z);
				JointValues.PushBack(Velocity.Z);
				JointValues.PushBack(Rotation.X);
				JointValues.PushBack(Rotation.Y);
				JointValues.PushBack(Rotation.Z);
				JointValues.PushBack(Rotation.W);
				JointValues.PushBack(Angular.X);
				JointValues.PushBack(Angular.Y);
				JointValues.PushBack(Angular.Z);
			}
			else
			{
				JointValues.PushBack(World.GetAngle(Id));
				JointValues.PushBack(World.GetAngularVelocity(Id));
			}
		};
		for (const auto& Handle : Scene.GetJointCourse().GetBodies())
		{
			const auto* Body = Handle.Get();
			// 運搬支点のslotにはRigidBodyを重ねず、Moverを下で一度だけ記録する。
			if (Body == nullptr)
			{
				JointValues.PushBack(-1);
				continue;
			}
			Check(Body->HasBody(), "native joint trace body missing");
			AppendBody(Body->GetBodyId());
		}
		const auto Carrier = Scene.GetJointCourse().GetCarrier()->GetBodyId();
		Check(static_cast<bool>(Carrier), "native joint trace mover missing");
		AppendBody(*Carrier);
		for (const auto& Handle : Scene.GetJointCourse().GetJoints())
		{
			const auto* Joint = Handle.Get();
			const auto Id = Joint->GetJointId();
			const auto Observation = Joint->GetObservation();
			Check(Id && Observation, "native joint trace observation missing");
			JointValues.PushBack(static_cast<Toolbox::f64>(Id->Index));
			JointValues.PushBack(static_cast<Toolbox::f64>(Id->Generation));
			JointValues.PushBack(static_cast<Toolbox::f64>(Joint->GetConnectionState()));
			JointValues.PushBack(static_cast<Toolbox::f64>(Observation->SuccessfulStep));
			JointValues.PushBack(Observation->CurrentLength);
			JointValues.PushBack(Observation->Error);
		}

		if (Frame == 170 || Frame == 172 || Frame == 180)
		{
			// 失敗前の支持と、配置時の箱／床の実Bodyを同じフレームから残す。
			char Detail[512];
			const auto Lift = Scene.GetCourse().Platforms[1].Get()->GetMover()->GetBodyId();
			const auto* Crate = Scene.GetCourse().LiftCrate.Get()->GetRigid();
			snprintf(Detail, sizeof(Detail), "LIFT_SETUP %s frame=%d y=%.6f ground=%llu present=%d lift=%llu crate=%llu crateY=%.6f steps=%lld paused=%d liftY=%.6f", T::Name, Frame, Entry.State.Player.Y, static_cast<Toolbox::uint64>(Entry.State.Ground.Body), Entry.State.Ground.bPresent, static_cast<Toolbox::uint64>(Lift->Index), static_cast<Toolbox::uint64>(Crate->GetBodyId().Index), Scene.GetPhysicsWorld().GetPosition(Crate->GetBodyId()).Y, Entry.State.Steps, Scene.GetClock().IsPaused(), Entry.State.Platforms[1].Y);
			Toolbox::Err << static_cast<const char*>(Detail) << "\n";
			const auto& Recovery = Scene.GetPlayer()->GetCharacter().GetLastStep().Recovery;
			Toolbox::Err << "RECOVERY status=" << static_cast<Toolbox::int32>(Recovery.Status) << " body=" << (Recovery.Collider ? Recovery.Collider->Body.Index : 99999) << "\n";
			const auto& Move = Scene.GetPlayer()->GetCharacter().GetLastStep().Vertical;
			Toolbox::Err << "VERTICAL stop=" << static_cast<Toolbox::int32>(Move.Stop) << " contacts=" << Move.ContactCount << " vy=" << Entry.State.Velocity.Y << "\n";
			for (Toolbox::uint32 Contact = 0; Contact < Move.ContactCount; ++Contact)
			{
				Toolbox::Err << "CONTACT body=" << Move.Contacts[Contact].Collider.Body.Index << " ny=" << Move.Contacts[Contact].Normal.Y << "\n";
			}
		}
		RequireFrame_Internal(Entry.State.Player.IsValid() && Entry.State.Velocity.IsValid(), T::Name, bSplit, Frame,
		                      "interaction produced a nonfinite state");
		Trace.PushBack(Toolbox::Move(Entry));
		if (Frame == 140)
		{
			Pixels_Internal<T>(App, Scene, Output);
		}
		if (Frame == 248 && !bSplit)
		{
			// 設定Modalの実画像。画素の合否は横床上の既存検査と分けて保存する。
			const char* SettingsFile = T::b3D ? "interaction3d_settings.png" : "interaction2d_settings.png";
			App.CapturePoints(Output / SettingsFile, nullptr, nullptr, 0);
		}
	}
	Check(Scene.IsSplit() == bSplit, "interaction view toggle failed");
	for (const auto& Pickup : Scene.GetCourse().Pickups)
	{
		Check(Pickup.Get() == nullptr, "collected pickup was not destroyed");
	}
	Accept_Internal<T>(Trace, bSplit);
	Toolbox::Out << "INTERACTION_SEQUENCE " << T::Name << " views=" << (bSplit ? 2 : 1)
	             << " frames=" << InteractionFrames << " collected=" << Scene.GetGameRules().GetCollected()
	             << " respawns=" << Scene.GetGameRules().GetRespawns() << "\n";
	// Scene参照は切替後に使わない。往復後の新しいSceneだけを解決する。
	App.SwitchDimension();
	App.SwitchDimension();
	// 往復のScene切替で作り直されたシーン。
	const auto& Fresh = T::Scene(App);
	Check(Fresh.GetGameRules().GetCollected() == 0 && Fresh.GetGameRules().GetCheckpoint() == 0 &&
	          Fresh.GetGameRules().GetRespawns() == 0 && !Fresh.IsSplit(),
	      "interaction scene re-entry retained rules");
	Check(Fresh.GetPlayer()->GetCharacter().GetCenter() == T::At(0, 0.52f), "interaction scene re-entry position");
	App.Quit();
}

// 1／2画面の全数値とイベント順を一致させる。Worldの識別子以外は除外しない。
template <typename T>
void RunDimension_Internal(FDxLibBackends& Backends, const char* ProjectRoot, const Toolbox::FPath& Output)
{
	// 1画面で実行した全フレームの記録。
	Toolbox::TVector<FInteractionFrame> Single;
	// 同じ時刻と入力を2画面で実行した記録。
	Toolbox::TVector<FInteractionFrame> Split;
	Record_Internal<T>(Backends, ProjectRoot, Output, false, Single);
	Record_Internal<T>(Backends, ProjectRoot, Output, true, Split);
	for (Toolbox::int32 Frame = 0; Frame < InteractionFrames; ++Frame)
	{
		// 同じ番号に対応する1画面側の記録。
		const auto& A = Single[static_cast<Toolbox::size_t>(Frame)];
		// 同じ番号に対応する2画面側の記録。
		const auto& B = Split[static_cast<Toolbox::size_t>(Frame)];
		RequireFrame_Internal(A.State == B.State, T::Name, true, Frame, "view count changed numerical or game state");
		RequireFrame_Internal(A.JointValues.Size() == B.JointValues.Size(), T::Name, true, Frame, "view count changed joint trace size");
		for (Toolbox::size_t Index = 0; Index < A.JointValues.Size(); ++Index)
		{
			RequireFrame_Internal(A.JointValues[Index] == B.JointValues[Index], T::Name, true, Frame, "view count changed joint body, generation or observation");
		}

		RequireFrame_Internal(A.Events.Size() == B.Events.Size(), T::Name, true, Frame,
		                      "view count changed event count");
		for (Toolbox::size_t Index = 0; Index < A.Events.Size(); ++Index)
		{
			RequireFrame_Internal(A.Events[Index] == B.Events[Index], T::Name, true, Frame,
			                      "view count changed event order or content");
		}
	}
	Toolbox::Out << "INTERACTION_VIEW_INVARIANCE " << T::Name << " frames=" << InteractionFrames
	             << " state=exact events=exact joints=exact\n";
}
} // namespace

void RunInteractionSmoke(FDxLibBackends& Backends, const char* ProjectRoot, const Toolbox::FPath& Output)
{
	RunDimension_Internal<FInteractionSmoke2D>(Backends, ProjectRoot, Output);
	RunDimension_Internal<FInteractionSmoke3D>(Backends, ProjectRoot, Output);
}
} // namespace Dxf::GameplaySmoke
