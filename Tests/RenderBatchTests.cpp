// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/AssetService.h"
#include "Dxf/RenderQueue2D.h"
#include "Toolbox/Atomic.h"
#include "Toolbox/Thread.h"
#include "Toolbox/Thread.h"
using namespace Dxf;
using namespace Dxf::Testing;
namespace
{
// 検証用の画像資源を読み込む。
Toolbox::TVector<FTexture> LoadTestTextures_Internal(FAssetService& Assets)
{
	Toolbox::TVector<FTexture> Textures;
	Toolbox::TVector<Toolbox::FString> Names;
	Names.PushBack("a.bmp");
	Names.PushBack("b.bmp");
	Names.PushBack("c.bmp");
	for (Toolbox::size_t Index = 0; Index < Names.Size(); ++Index)
	{
		Textures.PushBack(Assets.LoadTexture(Names[Index]).Value());
	}
	return Textures;
}
// 並列生成の入力から描画命令を作る。
TResult<void> MakeSprite_Internal(const Toolbox::TVector<FTexture>& Textures, Toolbox::size_t Index,
                                  FRenderCommand& Out)
{
	FSpriteDrawOptions Options;
	Options.Layer = static_cast<Toolbox::int32>(Index % 3) - 1;
	Options.Order = static_cast<Toolbox::int32>(Index % 5);
	FSpriteCommand Command;
	Command.Texture = Textures[Index % Textures.Size()];
	Command.Position = {static_cast<Toolbox::f32>(Index), 0.0f};
	Command.Options = Options;
	Out = Toolbox::Move(Command);
	return {};
}
} // namespace
TEST("parallel batch matches serial submission order")
{
	// 検証用のバックエンド。
	FFakeBackend SingleBackend;
	// 検証に使用する資源管理。
	FAssetService SingleAssets(SingleBackend, SingleBackend, SingleBackend);
	// 検証用の画像資源。
	Toolbox::TVector<FTexture> SingleTextures = LoadTestTextures_Internal(SingleAssets);
	// 検証用の描画キュー。
	FRenderQueue2D SingleQueue;
	SingleQueue.SetAccepting_Internal(true);
	Toolbox::FJobSystem SingleJobs(1);
	REQUIRE(SingleQueue.SubmitGenerated(SingleJobs, 200,
	                                    [&](Toolbox::size_t Index, FRenderCommand& Out)
	                                    {
		                                    return MakeSprite_Internal(SingleTextures, Index, Out);
	                                    }));
	REQUIRE(SingleQueue.Execute_Internal(SingleBackend));
	// 検証用のバックエンド。
	FFakeBackend ParallelBackend;
	// 検証に使用する資源管理。
	FAssetService ParallelAssets(ParallelBackend, ParallelBackend, ParallelBackend);
	// 検証用の画像資源。
	Toolbox::TVector<FTexture> ParallelTextures = LoadTestTextures_Internal(ParallelAssets);
	// 検証用の描画キュー。
	FRenderQueue2D ParallelQueue;
	ParallelQueue.SetAccepting_Internal(true);
	Toolbox::FJobSystem ParallelJobs(4);
	REQUIRE(ParallelQueue.SubmitGenerated(ParallelJobs, 200,
	                                      [&](Toolbox::size_t Index, FRenderCommand& Out)
	                                      {
		                                      return MakeSprite_Internal(ParallelTextures, Index, Out);
	                                      }));
	REQUIRE(ParallelQueue.Execute_Internal(ParallelBackend));
	REQUIRE(ParallelBackend.GetTrace().DrawHandles == SingleBackend.GetTrace().DrawHandles);
	REQUIRE(ParallelBackend.GetTrace().Opacities == SingleBackend.GetTrace().Opacities);
}
TEST("parallel batch preserves input order for equal keys")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 検証用の画像資源。
	Toolbox::TVector<FTexture> Textures = LoadTestTextures_Internal(Assets);
	// 検証用の描画キュー。
	FRenderQueue2D Queue;
	Queue.SetAccepting_Internal(true);
	Toolbox::FJobSystem Jobs(4);
	REQUIRE(Queue.SubmitGenerated(Jobs, 12,
	                              [&](Toolbox::size_t Index, FRenderCommand& Out)
	                              {
		                              FSpriteCommand Command;
		                              Command.Texture = Textures[Index % Textures.Size()];
		                              Command.Position = {static_cast<Toolbox::f32>(Index), 0.0f};
		                              Out = Toolbox::Move(Command);
		                              return TResult<void>{};
	                              }));
	REQUIRE(Queue.Execute_Internal(Backend));
	REQUIRE(Backend.GetTrace().DrawHandles.Size() == 12);
	for (Toolbox::size_t Position = 0; Position < 12; ++Position)
	{
		// 同順位の描画は入力順に並ぶ。
		const Toolbox::int32 Expected =
		    Textures[Position % Textures.Size()].GetNativeHandle_Internal();
		REQUIRE(Backend.GetTrace().DrawHandles[Position] == Expected);
	}
}
TEST("parallel batch rejects invalid tail without changing the queue")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 検証用の画像資源。
	Toolbox::TVector<FTexture> Textures = LoadTestTextures_Internal(Assets);
	// 検証用の描画キュー。
	FRenderQueue2D Queue;
	Queue.SetAccepting_Internal(true);
	FSpriteCommand First;
	First.Texture = Textures[0];
	REQUIRE(Queue.Submit(FRenderCommand(Toolbox::Move(First))));
	Toolbox::FJobSystem Jobs(4);
	auto Result = Queue.SubmitGenerated(Jobs, 10,
	                                    [&](Toolbox::size_t Index, FRenderCommand& Out) -> TResult<void>
	                                    {
		                                    if (Index == 9)
		                                    {
			                                    return TResult<void>::Failure(EErrorCode::InvalidArgument,
			                                                                  "invalid tail");
		                                    }
		                                    return MakeSprite_Internal(Textures, Index, Out);
	                                    });
	REQUIRE(!Result);
	REQUIRE(Queue.Execute_Internal(Backend));
	REQUIRE(Backend.GetTrace().DrawHandles.Size() == 1);
}
TEST("parallel batch rejects generation exceptions without changing the queue")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 検証用の画像資源。
	Toolbox::TVector<FTexture> Textures = LoadTestTextures_Internal(Assets);
	// 検証用の描画キュー。
	FRenderQueue2D Queue;
	Queue.SetAccepting_Internal(true);
	Toolbox::FJobSystem Jobs(4);
	auto Result = Queue.SubmitGenerated(Jobs, 10,
	                                    [&](Toolbox::size_t Index, FRenderCommand& Out) -> TResult<void>
	                                    {
		                                    if (Index == 7)
		                                    {
			                                    throw Toolbox::FException("generation failure");
		                                    }
		                                    return MakeSprite_Internal(Textures, Index, Out);
	                                    });
	REQUIRE(!Result);
	REQUIRE(Queue.Execute_Internal(Backend));
	REQUIRE(Backend.GetTrace().DrawHandles.IsEmpty());
}
TEST("parallel batch handles empty range and stopped acceptance")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証用の描画キュー。
	FRenderQueue2D Queue;
	Queue.SetAccepting_Internal(true);
	Toolbox::FJobSystem Jobs(4);
	// 生成が呼ばれた回数。
	Toolbox::FAtomicCounter Generated;
	REQUIRE(Queue.SubmitGenerated(Jobs, 0,
	                              [&](Toolbox::size_t, FRenderCommand&) -> TResult<void>
	                              {
		                              Generated.FetchAdd(1);
		                              return {};
	                              }));
	REQUIRE(Generated.Load() == 0);
	Queue.SetAccepting_Internal(false);
	auto Stopped = Queue.SubmitGenerated(Jobs, 4,
	                                     [&](Toolbox::size_t, FRenderCommand&) -> TResult<void>
	                                     {
		                                     Generated.FetchAdd(1);
		                                     return {};
	                                     });
	REQUIRE(!Stopped);
	REQUIRE(Generated.Load() == 0);
}
TEST("parallel batch rejects invalidated resources")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 検証用の描画キュー。
	FRenderQueue2D Queue;
	Queue.SetAccepting_Internal(true);
	Toolbox::FJobSystem Jobs(4);
	auto Result = Queue.SubmitGenerated(Jobs, 4,
	                                    [&](Toolbox::size_t, FRenderCommand& Out) -> TResult<void>
	                                    {
		                                    FSpriteCommand Command;
		                                    Out = Toolbox::Move(Command);
		                                    return {};
	                                    });
	REQUIRE(!Result);
	REQUIRE(Queue.Execute_Internal(Backend));
	REQUIRE(Backend.GetTrace().DrawHandles.IsEmpty());
}
TEST("parallel batch runs generation on several threads")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 検証用の画像資源。
	Toolbox::TVector<FTexture> Textures = LoadTestTextures_Internal(Assets);
	// 到着した異なるスレッドの記録。
	Toolbox::TVector<Toolbox::uint64> Seen;
	// 記録を守るMutex。
	Toolbox::FMutex SeenMutex;
	// 全員集合の合図。
	Toolbox::FAtomicCounter Released;
	// 待ち時間を使い切ったか。
	Toolbox::FAtomicCounter TimedOut;
	// 検証用の描画キュー。
	FRenderQueue2D Queue;
	Queue.SetAccepting_Internal(true);
	Toolbox::FJobSystem Jobs(4);
	REQUIRE(Queue.SubmitGenerated(Jobs, 200,
	                              [&](Toolbox::size_t Index, FRenderCommand& Out)
	                              {
		                              // 生成を実行したスレッドを記録する。
		                              const Toolbox::uint64 Id = Toolbox::FThread::CurrentThreadId();
		                              {
			                              Toolbox::FScopedLock Lock(SeenMutex);
			                              bool bKnown = false;
			                              for (Toolbox::size_t Slot = 0; Slot < Seen.Size(); ++Slot)
			                              {
				                              if (Seen[Slot] == Id)
				                              {
					                              bKnown = true;
					                              break;
				                              }
			                              }
			                              if (!bKnown)
			                              {
				                              Seen.PushBack(Id);
			                              }
			                              if (Seen.Size() >= 3)
			                              {
				                              Released.Store(1);
			                              }
		                              }
		                              for (Toolbox::uint64 Spin = 0;
		                                   Spin < 100000000 && Released.Load() == 0; ++Spin)
		                              {
			                              Toolbox::FThread::Yield();
		                              }
		                              if (Released.Load() == 0)
		                              {
			                              TimedOut.FetchAdd(1);
			                              return TResult<void>::Failure(EErrorCode::BackendFailure,
			                                                            "worker rendezvous timed out");
		                              }
		                              return MakeSprite_Internal(Textures, Index, Out);
	                              }));
	REQUIRE(TimedOut.Load() == 0);
	REQUIRE(Seen.Size() >= 3);
	REQUIRE(Queue.Execute_Internal(Backend));
	REQUIRE(Backend.GetTrace().DrawHandles.Size() == 200);
}
TEST("parallel batch rejects drawing the render target into itself")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 検証用の画像資源。
	Toolbox::TVector<FTexture> Textures = LoadTestTextures_Internal(Assets);
	// 検証用の描画キュー。
	FRenderQueue2D Queue;
	Queue.SetAccepting_Internal(true);
	Queue.SetTarget_Internal(Textures[0].GetNativeHandle_Internal());
	Toolbox::FJobSystem Jobs(4);
	auto Result = Queue.SubmitGenerated(Jobs, 4,
	                                    [&](Toolbox::size_t Index, FRenderCommand& Out)
	                                    {
		                                    return MakeSprite_Internal(Textures, Index, Out);
	                                    });
	REQUIRE(!Result);
}
