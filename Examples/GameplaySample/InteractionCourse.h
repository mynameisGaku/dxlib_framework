// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_INTERACTION_COURSE_H
#define DXF_GAMEPLAY_SAMPLE_INTERACTION_COURSE_H
#include "InteractionCheckpoint.h"
#include "InteractionCrate.h"
#include "InteractionDoor.h"
#include "InteractionHazard.h"
#include "InteractionPickup.h"
#include "InteractionPlatform.h"
#include "InteractionPressurePlate.h"
#include "Toolbox/Array.h"
namespace Dxf::GameplaySample
{
/**
 * 静止した地形（配置の箱を一つのStaticのBodyへ付ける）。
 * @tparam T 次元の型。
 */
template <typename T> class TInteractionGround final : public DGameObject
{
protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		auto Rigid = AddComponent<typename T::FRigid>(T::StaticBody(T::At(0, 0)));
		if (!Rigid)
		{
			return TResult<void>::Failure(Rigid.Error());
		}
		const auto& Boxes = InteractionLayout::GetGround();
		for (Toolbox::size_t Index = 0; Index < Boxes.Size(); ++Index)
		{
			auto Collider = AddComponent<typename T::FCollider>(T::Ground(Boxes[Index]));
			if (!Collider)
			{
				return TResult<void>::Failure(Collider.Error());
			}
		}
		return {};
	}
};
/**
 * 接触・Triggerのサンプルに置いたオブジェクト（描画・試験から参照する）。
 * @tparam T 次元の型。
 */
template <typename T> struct TInteractionCourse
{
	/**
	 * プレイヤー。
	 */
	TObjectHandle<typename T::FPlayer> Player;
	/**
	 * 触れる箱。
	 */
	TObjectHandle<TInteractionCrate<T>> Crate;
	/**
	 * 取得物（取得後は解決できない）。
	 */
	Toolbox::TArray<TObjectHandle<TInteractionPickup<T>>, InteractionLayout::PickupCount> Pickups;
	/**
	 * 圧力板と扉。
	 */
	TObjectHandle<TInteractionPressurePlate<T>> Plate;
	TObjectHandle<TInteractionDoor<T>> Door;
	/**
	 * 横に動く床・昇降床・回転する床。
	 */
	Toolbox::TArray<TObjectHandle<TInteractionPlatform<T>>, 3> Platforms;
	/**
	 * チェックポイント。
	 */
	Toolbox::TArray<TObjectHandle<TInteractionCheckpoint<T>>, InteractionLayout::CheckpointCount> Checkpoints;
	/**
	 * 危険領域。
	 */
	TObjectHandle<TInteractionHazard<T>> Hazard;
};
/**
 * 生成の結果を検査して、ハンドルを返す。
 */
template <typename TObject>
TResult<void> Keep_Internal(TResult<TObjectHandle<TObject>> Spawned, TObjectHandle<TObject>& Out)
{
	if (!Spawned)
	{
		return TResult<void>::Failure(Spawned.Error());
	}
	Out = Spawned.Value();
	return {};
}
/**
 * 配置のすべてのオブジェクトを置く。シーンの初期化から呼ぶ（Worldのイベントは先に有効化しておく）。
 * @param Scene 置く先のシーン。
 * @param Host シーンの窓口。
 * @param Course 置いたオブジェクト。
 */
template <typename T, typename TScene>
TResult<void> SpawnInteractionCourse(TScene& Scene, IInteractionHost<T>& Host, TInteractionCourse<T>& Course)
{
	if (auto Ground = Scene.template Spawn<TInteractionGround<T>>(); !Ground)
	{
		return TResult<void>::Failure(Ground.Error());
	}
	if (auto Kept = Keep_Internal(Scene.template Spawn<TInteractionCrate<T>>(Host), Course.Crate); !Kept)
	{
		return Kept;
	}
	for (Toolbox::int32 Index = 0; Index < InteractionLayout::PickupCount; ++Index)
	{
		if (auto Kept = Keep_Internal(Scene.template Spawn<TInteractionPickup<T>>(Host, Index),
		                              Course.Pickups[static_cast<Toolbox::size_t>(Index)]);
		    !Kept)
		{
			return Kept;
		}
	}
	if (auto Kept = Keep_Internal(Scene.template Spawn<TInteractionPressurePlate<T>>(Host), Course.Plate); !Kept)
	{
		return Kept;
	}
	if (auto Kept = Keep_Internal(Scene.template Spawn<TInteractionDoor<T>>(Host), Course.Door); !Kept)
	{
		return Kept;
	}
	const EInteractionPlatform Kinds[3] = {EInteractionPlatform::Sliding, EInteractionPlatform::Lift,
	                                       EInteractionPlatform::Turning};
	for (Toolbox::size_t Index = 0; Index < 3; ++Index)
	{
		if (auto Kept =
		        Keep_Internal(Scene.template Spawn<TInteractionPlatform<T>>(Kinds[Index]), Course.Platforms[Index]);
		    !Kept)
		{
			return Kept;
		}
	}
	for (Toolbox::int32 Index = 0; Index < InteractionLayout::CheckpointCount; ++Index)
	{
		if (auto Kept = Keep_Internal(Scene.template Spawn<TInteractionCheckpoint<T>>(Host, Index),
		                              Course.Checkpoints[static_cast<Toolbox::size_t>(Index)]);
		    !Kept)
		{
			return Kept;
		}
	}
	if (auto Kept = Keep_Internal(Scene.template Spawn<TInteractionHazard<T>>(Host), Course.Hazard); !Kept)
	{
		return Kept;
	}
	return Keep_Internal(Scene.template Spawn<typename T::FPlayer>(), Course.Player);
}
} // namespace Dxf::GameplaySample
#endif
