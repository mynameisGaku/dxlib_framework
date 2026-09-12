#pragma once
#include "Dxf/Clock.h"
#include "Dxf/Input.h"
namespace Dxf
{
class FAssetService;
class FAudioPlayer;
class FSceneNavigator;
class DGameInstance;
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
};
} // namespace Dxf
