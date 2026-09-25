// SPDX-License-Identifier: NOASSERTION
// 再配置したパッケージを使う外部の利用者（dxf::framework・dxf::debug_tools）。Scene・物理・描画の窓口と、
// DPhysicsScene2D／3D上のキャラクター移動Componentの生成・固定更新・破棄を確かめる。終了コード0が成功。
#include "Dxf/GameScene.h"
#include "Dxf/AssetService.h"
#include "Dxf/CharacterMovementComponent2D.h"
#include "Dxf/CharacterMovementComponent3D.h"
#include "Dxf/InputStateTracker.h"
#include "Dxf/PhysicsDebugPicking3D.h"
#include "Dxf/PhysicsScene2D.h"
#include "Dxf/PhysicsScene3D.h"
#include "Dxf/RenderQueue2D.h"
#include "Dxf/RigidBodyComponent2D.h"
#include "Dxf/RigidBodyComponent3D.h"
// 資源を読まない利用者のための空の実装（呼ばれたら失敗を返す）。
class FNoAssets final : public Dxf::ITextureBackend, public Dxf::ISoundBackend, public Dxf::IFontBackend
{
public:
	Dxf::TResult<Dxf::FTextureAllocation> LoadTexture(const Toolbox::FString&, const Dxf::FTextureLoadOptions&) override
	{
		return Dxf::TResult<Dxf::FTextureAllocation>::Failure(Dxf::EErrorCode::NotFound, "none");
	}
	Dxf::TResult<Dxf::FTextureAllocation> LoadTextureMemory(const void*, Toolbox::size_t,
	                                                        const Dxf::FTextureLoadOptions&) override
	{
		return Dxf::TResult<Dxf::FTextureAllocation>::Failure(Dxf::EErrorCode::NotFound, "none");
	}
	Dxf::TResult<Dxf::FTextureAllocation> CreateRenderTarget(Toolbox::int32, Toolbox::int32, bool) override
	{
		return Dxf::TResult<Dxf::FTextureAllocation>::Failure(Dxf::EErrorCode::NotFound, "none");
	}
	void DeleteTexture(Toolbox::int32) noexcept override
	{
	}
	Dxf::TResult<Toolbox::int32> CreateFont(const Dxf::FFontOptions&) override
	{
		return Dxf::TResult<Toolbox::int32>::Failure(Dxf::EErrorCode::NotFound, "none");
	}
	void DeleteFont(Toolbox::int32) noexcept override
	{
	}
	Dxf::TResult<Toolbox::int32> LoadSound(const Toolbox::FString&, const Dxf::FSoundLoadOptions&) override
	{
		return Dxf::TResult<Toolbox::int32>::Failure(Dxf::EErrorCode::NotFound, "none");
	}
	Dxf::TResult<Toolbox::int32> LoadSoundMemory(const void*, Toolbox::size_t, const Dxf::FSoundLoadOptions&) override
	{
		return Dxf::TResult<Toolbox::int32>::Failure(Dxf::EErrorCode::NotFound, "none");
	}
	Dxf::TResult<Toolbox::int32> DuplicateSound(Toolbox::int32) override
	{
		return Dxf::TResult<Toolbox::int32>::Failure(Dxf::EErrorCode::NotFound, "none");
	}
	Dxf::TResult<void> StartSound(Toolbox::int32, bool) override
	{
		return Dxf::TResult<void>::Failure(Dxf::EErrorCode::NotFound, "none");
	}
	void StopSound(Toolbox::int32) noexcept override
	{
	}
	void DeleteSound(Toolbox::int32) noexcept override
	{
	}
	Dxf::TResult<void> SetSoundVolume(Toolbox::int32, Toolbox::f32) override
	{
		return Dxf::TResult<void>::Failure(Dxf::EErrorCode::NotFound, "none");
	}
	Dxf::TResult<bool> IsSoundPlaying(Toolbox::int32) override
	{
		return Dxf::TResult<bool>::Failure(Dxf::EErrorCode::NotFound, "none");
	}
};
// 上面y=0の床。
template <typename TRigid, typename TCollider, typename TBody, typename TDescription>
class DFloor final : public Dxf::DGameObject
{
public:
	explicit DFloor(TDescription Shape) : Shape(Shape)
	{
	}
	TDescription Shape;

protected:
	Dxf::TResult<void> OnInitialize(const Dxf::FInitContext&) override
	{
		TBody Body;
		Body.Type = Dxf::EBodyType::Static;
		if (!AddComponent<TRigid>(Body) || !AddComponent<TCollider>(Shape))
		{
			return Dxf::TResult<void>::Failure(Dxf::EErrorCode::InvalidState, "floor");
		}
		return {};
	}
};
// 移動Componentだけを持つキャラクター。
template <typename TComponent, typename TDescription> class DWalker final : public Dxf::DGameObject
{
public:
	using FDescriptionType = TDescription;
	explicit DWalker(TDescription Description) : Description(Description)
	{
	}
	TDescription Description;

protected:
	Dxf::TResult<void> OnInitialize(const Dxf::FInitContext&) override
	{
		if (!AddComponent<TComponent>(Description))
		{
			return Dxf::TResult<void>::Failure(Dxf::EErrorCode::InvalidState, "walker");
		}
		return {};
	}
};
// 生成・60回の固定更新で右へ歩いて接地を保つ・破棄。
template <typename TScene, typename TComponent, typename TFloor, typename TWalker, typename TCenter,
          typename TFloorShape>
int Walk(FNoAssets& Backend, TCenter Start, TCenter Move, TFloorShape Floor, int Code)
{
	Dxf::FAssetService Assets(Backend, Backend, Backend);
	Dxf::FInputStateTracker Tracker;
	TScene Scene;
	typename TWalker::FDescriptionType Description;
	Description.Center = Start;
	auto Walker = Scene.template Spawn<TWalker>(Description);
	if (!Walker || !Scene.template Spawn<TFloor>(Floor) || !Scene.Initialize_Internal({Assets}))
	{
		return Code;
	}
	auto Component = Walker.Value().Get()->template FindComponent<TComponent>();
	if (!Component.Get())
	{
		return Code + 1;
	}
	Component.Get()->SetMoveInput(Move);
	Dxf::FFrameTime Time;
	Time.DeltaSeconds = 1.0 / 60.0;
	Time.UnscaledDeltaSeconds = 1.0 / 60.0;
	for (int Frame = 0; Frame < 60; ++Frame)
	{
		if (!Scene.Tick_Internal({Tracker.GetSnapshot(), Time}))
		{
			return Code + 2;
		}
	}
	if (Component.Get()->GetStepCount() != 60 || !Component.Get()->IsGrounded() ||
	    !(Component.Get()->GetCenter().X > 4.7f) || !Component.Get()->GetBodyId())
	{
		return Code + 3;
	}
	// 破棄を要求すると、ハンドルからは参照できなくなり、以降の更新も失敗しない。
	Walker.Value().Get()->Destroy();
	if (Component || !Scene.Tick_Internal({Tracker.GetSnapshot(), Time}))
	{
		return Code + 4;
	}
	Scene.Shutdown_Internal();
	return 0;
}
int main()
{
	FNoAssets Backend;
	const int Walk2D = Walk<Dxf::DPhysicsScene2D, Dxf::DCharacterMovement2DComponent,
	                        DFloor<Dxf::DRigidBody2DComponent, Dxf::DCollider2DComponent, Dxf::FBodyDescription2D,
	                               Dxf::FColliderDescription2D>,
	                        DWalker<Dxf::DCharacterMovement2DComponent, Dxf::FCharacterMovementDescription2D>>(
	    Backend, Toolbox::FVector2{0, 0.52f}, Toolbox::FVector2{1, 0},
	    Dxf::FColliderDescription2D{Toolbox::FOrientedBox2D{{0, -1}, {50, 1}, 0}}, 201);
	if (Walk2D != 0)
	{
		return Walk2D;
	}
	const int Walk3D = Walk<Dxf::DPhysicsScene3D, Dxf::DCharacterMovement3DComponent,
	                        DFloor<Dxf::DRigidBody3DComponent, Dxf::DCollider3DComponent, Dxf::FBodyDescription3D,
	                               Dxf::FColliderDescription3D>,
	                        DWalker<Dxf::DCharacterMovement3DComponent, Dxf::FCharacterMovementDescription3D>>(
	    Backend, Toolbox::FVector3{0, 0.52f, 0}, Toolbox::FVector3{1, 0, 0},
	    Dxf::FColliderDescription3D{Toolbox::FOBB{{0, -1, 0}, {50, 1, 50}}}, 211);
	if (Walk3D != 0)
	{
		return Walk3D;
	}
	Dxf::FPhysicsWorld3D World;
	const auto Body = World.CreateBody({});
	Dxf::FColliderDescription3D Collider;
	Collider.Shape = Toolbox::FSphere{{0, 0, 0}, 1};
	const auto Id = World.AttachCollider(Body, Collider);
	const auto Snapshot = Dxf::CapturePhysicsDebugSnapshot3D(World, 0);
	if (!Snapshot)
	{
		return 6;
	}
	const auto Pick = Dxf::PickPhysicsDebugSnapshot3D(Snapshot.Value(), {{0, 0, -5}, {0, 0, 5}});
	if (!Pick || !Pick.Value() || Pick.Value()->Collider != Id)
	{
		return 7;
	}
	Dxf::DGameScene Scene;
	Scene.Shutdown_Internal();
	Dxf::FRenderQueue2D Queue;
	return Queue.Submit(Dxf::FRectangleCommand{}) ? 1 : 0;
}
