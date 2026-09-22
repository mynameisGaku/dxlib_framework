#pragma once
#include "Dxf/Clock.h"
#include "Dxf/Input.h"
#include "Dxf/InputSnapshot.h"
#include "Toolbox/Vector.h"
#include "Dxf/TaskDispatcher.h"
namespace Dxf
{
class FAssetService;
class FAudioPlayer;
class FSceneNavigator;
class DGameInstance;
class FPhysicsWorld2D;
class FPhysicsWorld3D;
/**
 * 準備中は有効なシーンやゲームの状態を変更せず、音声再生も開始しない。
 */
struct FInitContext
{
	/**
	 * アセットを読み込むサービス。
	 */
	FAssetService& Assets;
};
/**
 * フレーム更新時の入力・時間・サービス情報を管理する型。
 */
struct FTickContext
{
	/**
	 * フレームの入力情報。
	 */
	const FInputSnapshot& Input;
	/**
	 * フレームの時間情報。
	 */
	FFrameTime Time;
	/**
	 * シーン遷移の管理窓口。
	 */
	FSceneNavigator* Scenes = nullptr;
	/**
	 * シーン間で共有するゲーム状態。
	 */
	DGameInstance* Game = nullptr;
	/**
	 * 音声再生のサービス。
	 */
	FAudioPlayer* Audio = nullptr;
	/**
	 * シーンに紐づく音声の識別番号。
	 */
	Toolbox::uint64 AudioScope = 0;
	/**
	 * Applicationが所有するTaskの窓口。低レベルの単独利用ではnullptr。
	 */
	FTaskDispatcher* Tasks = nullptr;
	/**
	 * このSceneの有効化から終了までを保護する所属。OnEnter以降に使用する。
	 * GameInstanceの更新ではRoot。Scopeなしの単独利用では無効。
	 */
	FTaskScope TaskScope{};
};
/**
 * 固定時間更新時の入力・時間・物理情報を管理する型。
 * 保持入力は毎更新で有効、押下・解放のエッジは最初の更新と未配達分だけで有効。
 */
struct FFixedTickContext
{
	/**
	 * 現在フレームの入力情報。保持の判定に使う。
	 */
	const FInputSnapshot& Input;
	/**
	 * 固定更新が0回だったフレームの入力複写。エッジの紛失を防ぐ。
	 */
	const Toolbox::TVector<FInputSnapshot>& PendingInputs;
	/**
	 * 一回の固定更新に渡す秒数。
	 */
	Toolbox::f64 DeltaSeconds = 1.0 / 60.0;
	/**
	 * フレーム内の固定更新番号。
	 */
	Toolbox::uint32 StepIndex = 0;
	/**
	 * フレーム内で最初の固定更新か。
	 */
	bool bIsFirstStep = true;
	/**
	 * 一時停止しているか。
	 */
	bool bPaused = false;
	/**
	 * 描画補間用の残余割合。
	 */
	Toolbox::f64 InterpolationAlpha = 0;
	/**
	 * 2D物理ワールド。使わない次元はnullptr。
	 */
	FPhysicsWorld2D* Physics2D = nullptr;
	/**
	 * 3D物理ワールド。使わない次元はnullptr。
	 */
	FPhysicsWorld3D* Physics3D = nullptr;
	/**
	 * シーン遷移の管理窓口。
	 */
	FSceneNavigator* Scenes = nullptr;
	/**
	 * シーン間で共有するゲーム状態。
	 */
	DGameInstance* Game = nullptr;
	/**
	 * 今回の固定更新で押されたかを調べる。未配達分も一度だけ有効。
	 * @param Key 検索または入力のキー。
	 */
	bool WasPressed(EKey Key) const noexcept
	{
		if (bIsFirstStep && Input.WasPressed(Key))
		{
			return true;
		}
		for (Toolbox::size_t Index = 0; Index < PendingInputs.Size(); ++Index)
		{
			if (PendingInputs[Index].WasPressed(Key))
			{
				return true;
			}
		}
		return false;
	}
	/**
	 * 今回の固定更新で離されたかを調べる。未配達分も一度だけ有効。
	 * @param Key 検索または入力のキー。
	 */
	bool WasReleased(EKey Key) const noexcept
	{
		if (bIsFirstStep && Input.WasReleased(Key))
		{
			return true;
		}
		for (Toolbox::size_t Index = 0; Index < PendingInputs.Size(); ++Index)
		{
			if (PendingInputs[Index].WasReleased(Key))
			{
				return true;
			}
		}
		return false;
	}
	/**
	 * 今回の固定更新でマウスボタンが押されたかを調べる。
	 * @param Button 入力ボタンの番号。
	 */
	bool WasMousePressed(EMouseButton Button) const noexcept
	{
		if (bIsFirstStep && Input.WasMousePressed(Button))
		{
			return true;
		}
		for (Toolbox::size_t Index = 0; Index < PendingInputs.Size(); ++Index)
		{
			if (PendingInputs[Index].WasMousePressed(Button))
			{
				return true;
			}
		}
		return false;
	}
	/**
	 * 今回の固定更新でマウスボタンが離されたかを調べる。
	 * @param Button 入力ボタンの番号。
	 */
	bool WasMouseReleased(EMouseButton Button) const noexcept
	{
		if (bIsFirstStep && Input.WasMouseReleased(Button))
		{
			return true;
		}
		for (Toolbox::size_t Index = 0; Index < PendingInputs.Size(); ++Index)
		{
			if (PendingInputs[Index].WasMouseReleased(Button))
			{
				return true;
			}
		}
		return false;
	}
	/**
	 * 今回の固定更新でパッドボタンが押されたかを調べる。
	 * @param Pad ゲームパッドの番号。
	 * @param Button 入力ボタンの番号。
	 */
	bool WasPadPressed(Toolbox::size_t Pad, Toolbox::size_t Button) const noexcept
	{
		if (bIsFirstStep && Input.WasPadPressed(Pad, Button))
		{
			return true;
		}
		for (Toolbox::size_t Index = 0; Index < PendingInputs.Size(); ++Index)
		{
			if (PendingInputs[Index].WasPadPressed(Pad, Button))
			{
				return true;
			}
		}
		return false;
	}
	/**
	 * 今回の固定更新でパッドボタンが離されたかを調べる。
	 * @param Pad ゲームパッドの番号。
	 * @param Button 入力ボタンの番号。
	 */
	bool WasPadReleased(Toolbox::size_t Pad, Toolbox::size_t Button) const noexcept
	{
		if (bIsFirstStep && Input.WasPadReleased(Pad, Button))
		{
			return true;
		}
		for (Toolbox::size_t Index = 0; Index < PendingInputs.Size(); ++Index)
		{
			if (PendingInputs[Index].WasPadReleased(Pad, Button))
			{
				return true;
			}
		}
		return false;
	}
};
/**
 * シーン開始時のサービス情報を管理する型。
 */
struct FSceneActivationContext
{
	/**
	 * 音声再生のサービス。
	 */
	FAudioPlayer& Audio;
	/**
	 * シーン遷移の管理窓口。
	 */
	FSceneNavigator& Scenes;
	/**
	 * シーン間で共有するゲーム状態。
	 */
	DGameInstance* Game = nullptr;
	/**
	 * シーンに紐づく音声の識別番号。
	 */
	Toolbox::uint64 AudioScope = 0;
	/**
	 * Applicationが所有するTaskの窓口。低レベルの単独利用ではnullptr。
	 */
	FTaskDispatcher* Tasks = nullptr;
	/**
	 * このSceneの有効化から終了までを保護する所属。OnEnter以降に使用する。
	 * GameInstanceの更新ではRoot。Scopeなしの単独利用では無効。
	 */
	FTaskScope TaskScope{};
};
} // namespace Dxf
