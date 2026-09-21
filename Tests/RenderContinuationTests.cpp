// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/RenderSystem.h"
#include "Toolbox/Atomic.h"

namespace
{
// ネイティブ命令の呼び出し順のみを観測する。キューやJob本体は実装を直接使う。
class FRecordingBackend final : public Dxf::IRenderBackend
{
public:
	Toolbox::TVector<Toolbox::int32> m_Commands;
	Toolbox::uint64 m_OwnerThreadId = Toolbox::FThread::CurrentThreadId();
	Toolbox::size_t m_Presentations = 0;
	Toolbox::size_t m_ForeignCalls = 0;
	Dxf::TResult<void> SetTarget(Toolbox::int32, Toolbox::int32, Toolbox::int32) override
	{
		return CheckThread_Internal();
	}
	Dxf::TResult<void> Clear(Dxf::FColor) override
	{
		return CheckThread_Internal();
	}
	Dxf::TResult<void> ResetState(Toolbox::int32, Toolbox::int32) override
	{
		return CheckThread_Internal();
	}
	Dxf::TResult<void> DrawSprite(const Dxf::FSpriteCommand& Command) override
	{
		m_Commands.PushBack(Command.Texture.GetNativeHandle_Internal());
		return CheckThread_Internal();
	}
	Dxf::TResult<void> DrawText(const Dxf::FTextCommand& Command) override
	{
		m_Commands.PushBack(Command.Font.GetNativeHandle_Internal());
		return CheckThread_Internal();
	}
	Dxf::TResult<void> DrawRectangle(const Dxf::FRectangleCommand& Command) override
	{
		m_Commands.PushBack(Command.Rectangle.Left);
		return CheckThread_Internal();
	}
	Dxf::TResult<void> Present() override
	{
		++m_Presentations;
		return CheckThread_Internal();
	}
private:
	Dxf::TResult<void> CheckThread_Internal()
	{
		if (Toolbox::FThread::CurrentThreadId() != m_OwnerThreadId)
		{
			++m_ForeignCalls;
		}
		return {};
	}
};
Dxf::FRenderCommand Rectangle_Internal(Toolbox::int32 Key, Toolbox::int32 Layer = 0, Toolbox::int32 Order = 0)
{
	Dxf::FRectangleCommand Command;
	Command.Rectangle = {Key, 0, Key + 1, 1};
	Command.Options.Layer = Layer;
	Command.Options.Order = Order;
	return Command;
}
}
TEST("generated batch refuses synchronous Submit reentry")
{
	Toolbox::FJobSystem Jobs(1);
	Dxf::FRenderQueue2D Queue;
	Queue.SetAccepting_Internal(true);
	REQUIRE(Queue.Submit(Rectangle_Internal(7)));
	bool bRejected = false;
	auto Result = Queue.SubmitGenerated(Jobs, 1, [&](Toolbox::size_t, Dxf::FRenderCommand& Output)
	{
		bRejected = !Queue.Submit(Rectangle_Internal(99));
		Output = Rectangle_Internal(8);
		return Dxf::TResult<void>{};
	});
	REQUIRE(Result);
	REQUIRE(bRejected);
	FRecordingBackend Backend;
	REQUIRE(Queue.Execute_Internal(Backend));
	REQUIRE(Backend.m_Commands.Size() == 2);
	REQUIRE(Backend.m_Commands[0] == 7);
	REQUIRE(Backend.m_Commands[1] == 8);
}
TEST("generated batch refuses nested generation")
{
	Toolbox::FJobSystem Jobs(1);
	Dxf::FRenderQueue2D Queue;
	Queue.SetAccepting_Internal(true);
	bool bRejected = false;
	auto Result = Queue.SubmitGenerated(Jobs, 1, [&](Toolbox::size_t, Dxf::FRenderCommand& Output)
	{
		bRejected = !Queue.SubmitGenerated(Jobs, 1, [](Toolbox::size_t, Dxf::FRenderCommand& Nested)
		{
			Nested = Rectangle_Internal(99);
			return Dxf::TResult<void>{};
		});
		Output = Rectangle_Internal(8);
		return Dxf::TResult<void>{};
	});
	REQUIRE(Result);
	REQUIRE(bRejected);
}
TEST("generated batch cannot execute the existing queue")
{
	Toolbox::FJobSystem Jobs(1);
	Dxf::FRenderQueue2D Queue;
	FRecordingBackend Backend;
	Queue.SetAccepting_Internal(true);
	REQUIRE(Queue.Submit(Rectangle_Internal(7)));
	bool bRejected = false;
	REQUIRE(Queue.SubmitGenerated(Jobs, 1, [&](Toolbox::size_t, Dxf::FRenderCommand& Output)
	{
		bRejected = !Queue.Execute_Internal(Backend);
		Output = Rectangle_Internal(8);
		return Dxf::TResult<void>{};
	}));
	REQUIRE(bRejected);
	REQUIRE(Backend.m_Commands.IsEmpty());
	REQUIRE(Queue.Execute_Internal(Backend));
	REQUIRE(Backend.m_Commands.Size() == 2);
}
TEST("queue rejects foreign worker submissions before mutable access")
{
	Toolbox::FJobSystem Jobs(2);
	Dxf::FRenderQueue2D Queue;
	Queue.SetAccepting_Internal(true);
	bool bRejected = false;
	Toolbox::FJobFence Fence;
	REQUIRE(Jobs.TrySubmit([&]()
	{
		bRejected = !Queue.Submit(Rectangle_Internal(99));
	}, &Fence));
	REQUIRE(Jobs.Wait(Fence));
	REQUIRE(Fence.FailureCount() == 0);
	REQUIRE(bRejected);
}
TEST("queue rejects inline job submissions outside generation")
{
	Toolbox::FJobSystem Jobs(1);
	Dxf::FRenderQueue2D Queue;
	Queue.SetAccepting_Internal(true);
	bool bRejected = false;
	REQUIRE(Jobs.TrySubmit([&]()
	{
		bRejected = !Queue.Submit(Rectangle_Internal(99));
	}));
	REQUIRE(bRejected);
}
TEST("failed generation preserves queue and permits subsequent submission")
{
	Toolbox::FJobSystem Jobs(4);
	Dxf::FRenderQueue2D Queue;
	Queue.SetAccepting_Internal(true);
	REQUIRE(Queue.Submit(Rectangle_Internal(7)));
	REQUIRE(!Queue.SubmitGenerated(Jobs, 128, [](Toolbox::size_t Index, Dxf::FRenderCommand& Output)
	{
		Output = Rectangle_Internal(static_cast<Toolbox::int32>(Index));
		return Index == 17 ? Dxf::TResult<void>::Failure(Dxf::EErrorCode::InvalidArgument, "test failure")
		                   : Dxf::TResult<void>{};
	}, 1));
	REQUIRE(Queue.Submit(Rectangle_Internal(8)));
	FRecordingBackend Backend;
	REQUIRE(Queue.Execute_Internal(Backend));
	REQUIRE(Backend.m_Commands.Size() == 2);
	REQUIRE(Backend.m_Commands[0] == 7);
	REQUIRE(Backend.m_Commands[1] == 8);
}
TEST("generated commands keep input order on one and four lanes")
{
	for (Toolbox::uint32 Lanes : {1U, 4U})
	{
		Toolbox::FJobSystem Jobs(Lanes);
		Dxf::FRenderQueue2D Queue;
		Queue.SetAccepting_Internal(true);
		REQUIRE(Queue.Submit(Rectangle_Internal(-1)));
		REQUIRE(Queue.SubmitGenerated(Jobs, 256, [](Toolbox::size_t Index, Dxf::FRenderCommand& Output)
		{
			if (Index % 8 == 0)
			{
				Toolbox::FThread::Yield();
			}
			Output = Rectangle_Internal(static_cast<Toolbox::int32>(Index));
			return Dxf::TResult<void>{};
		}, 1));
		REQUIRE(Queue.Submit(Rectangle_Internal(256)));
		FRecordingBackend Backend;
		REQUIRE(Queue.Execute_Internal(Backend));
		REQUIRE(Backend.m_Commands.Size() == 258);
		for (Toolbox::size_t Index = 0; Index < Backend.m_Commands.Size(); ++Index)
		{
			REQUIRE(Backend.m_Commands[Index] == static_cast<Toolbox::int32>(Index) - 1);
		}
	}
}
TEST("inline jobs cannot invoke renderer Native or end a frame")
{
	Toolbox::FJobSystem Jobs(1);
	FRecordingBackend Backend;
	Dxf::FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(320, 240));
	bool bNativeRejected = false;
	bool bEndRejected = false;
	bool bInvoked = false;
	REQUIRE(Jobs.TrySubmit([&]()
	{
		bNativeRejected = !Renderer.Native([&]()
		{
			bInvoked = true;
			return Dxf::TResult<void>{};
		});
		bEndRejected = !Renderer.EndFrame();
	}));
	REQUIRE(bNativeRejected);
	REQUIRE(bEndRejected);
	REQUIRE(!bInvoked);
	REQUIRE(Backend.m_Presentations == 0);
	REQUIRE(Renderer.EndFrame());
}
TEST("fence construction does not promise no-throw allocation")
{
	REQUIRE(!noexcept(Toolbox::FJobFence()));
}
TEST("public context generates batches without crossing Native barriers")
{
	Toolbox::FJobSystem Jobs(4);
	FRecordingBackend Backend;
	Dxf::FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(320, 240));
	REQUIRE(Renderer.SetExecutionJobs(Jobs));
	auto& Context = Renderer.GetContext();
	REQUIRE(Context.Get2D().SubmitGenerated( 3, [](Toolbox::size_t Index, Dxf::FRenderCommand& Output)
	{
		Output = Rectangle_Internal(static_cast<Toolbox::int32>(Index));
		return Dxf::TResult<void>{};
	}, 1));
	REQUIRE(Context.Native([&]()
	{
		Backend.m_Commands.PushBack(90);
		return Dxf::TResult<void>{};
	}));
	REQUIRE(Context.Get2D().SubmitGenerated( 3, [](Toolbox::size_t Index, Dxf::FRenderCommand& Output)
	{
		Output = Rectangle_Internal(static_cast<Toolbox::int32>(Index) + 10, -100);
		return Dxf::TResult<void>{};
	}, 1));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.m_Commands.Size() == 7);
	REQUIRE(Backend.m_Commands[0] == 0);
	REQUIRE(Backend.m_Commands[2] == 2);
	REQUIRE(Backend.m_Commands[3] == 90);
	REQUIRE(Backend.m_Commands[4] == 10);
	REQUIRE(Backend.m_Commands[6] == 12);
	REQUIRE(Backend.m_ForeignCalls == 0);
}
TEST("nested inline jobs preserve the actual OS worker owner")
{
	Toolbox::FJobSystem Outer(2);
	Toolbox::FJobSystem Inline(1);
	Toolbox::FJobFence Fence;
	bool bOuterWorker = false;
	bool bInlineWorker = true;
	bool bActiveJob = false;
	REQUIRE(Outer.TrySubmit([&]()
	{
		Inline.TrySubmit([&]()
		{
			bOuterWorker = Outer.IsInWorkerThread();
			bInlineWorker = Inline.IsInWorkerThread();
			bActiveJob = Toolbox::FJobSystem::IsExecutingJob();
		});
	}, &Fence));
	REQUIRE(Outer.Wait(Fence));
	REQUIRE(bOuterWorker);
	REQUIRE(!bInlineWorker);
	REQUIRE(bActiveJob);
	REQUIRE(!Toolbox::FJobSystem::IsExecutingJob());
}
TEST("internal queue mutations are ignored during generation")
{
	Toolbox::FJobSystem Jobs(1);
	Dxf::FRenderQueue2D Queue;
	Queue.SetAccepting_Internal(true);
	REQUIRE(Queue.Submit(Rectangle_Internal(7)));
	REQUIRE(Queue.SubmitGenerated(Jobs, 1, [&](Toolbox::size_t, Dxf::FRenderCommand& Output)
	{
		Queue.Clear_Internal();
		Queue.SetAccepting_Internal(false);
		Queue.SetTarget_Internal(37);
		Output = Rectangle_Internal(8);
		return Dxf::TResult<void>{};
	}));
	REQUIRE(Queue.Submit(Rectangle_Internal(9)));
	FRecordingBackend Backend;
	REQUIRE(Queue.Execute_Internal(Backend));
	REQUIRE(Backend.m_Commands.Size() == 3);
	REQUIRE(Backend.m_Commands[0] == 7);
	REQUIRE(Backend.m_Commands[2] == 9);
}
TEST("invalid tail and throwing generators do not append partial results")
{
	for (Toolbox::uint32 Lanes : {1U, 4U})
	{
		Toolbox::FJobSystem Jobs(Lanes);
		Dxf::FRenderQueue2D Queue;
		Queue.SetAccepting_Internal(true);
		REQUIRE(Queue.Submit(Rectangle_Internal(7)));
		REQUIRE(!Queue.SubmitGenerated(Jobs, 64, [](Toolbox::size_t Index, Dxf::FRenderCommand& Output)
		{
			if (Index != 63)
			{
				Output = Rectangle_Internal(static_cast<Toolbox::int32>(Index));
			}
			return Dxf::TResult<void>{};
		}, 1));
		REQUIRE(!Queue.SubmitGenerated(Jobs, 64, [](Toolbox::size_t Index, Dxf::FRenderCommand& Output)
		{
			if (Index == 0)
			{
				throw Toolbox::FException("generator failed");
			}
			Output = Rectangle_Internal(static_cast<Toolbox::int32>(Index));
			return Dxf::TResult<void>{};
		}, 1));
		REQUIRE(Jobs.GetSubmittedJobCount() == Jobs.GetCompletedJobCount());
		REQUIRE(Queue.Submit(Rectangle_Internal(8)));
		FRecordingBackend Backend;
		REQUIRE(Queue.Execute_Internal(Backend));
		REQUIRE(Backend.m_Commands.Size() == 2);
		REQUIRE(Backend.m_Commands[0] == 7);
		REQUIRE(Backend.m_Commands[1] == 8);
	}
}
TEST("empty batch and stopped queue have explicit contracts")
{
	Toolbox::FJobSystem Jobs(1);
	Dxf::FRenderQueue2D Queue;
	Toolbox::size_t Calls = 0;
	auto Generate = [&](Toolbox::size_t, Dxf::FRenderCommand&)
	{
		++Calls;
		return Dxf::TResult<void>{};
	};
	REQUIRE(!Queue.SubmitGenerated(Jobs, 0, Generate));
	Queue.SetAccepting_Internal(true);
	REQUIRE(Queue.SubmitGenerated(Jobs, 0, Generate));
	REQUIRE(Calls == 0);
	Jobs.Shutdown();
	REQUIRE(!Queue.SubmitGenerated(Jobs, 1, Generate));
	REQUIRE(Calls == 0);
	REQUIRE(Queue.Submit(Rectangle_Internal(7)));
}
TEST("oversized generated count fails without changing the queue")
{
	Toolbox::FJobSystem Jobs(1);
	Dxf::FRenderQueue2D Queue;
	Queue.SetAccepting_Internal(true);
	REQUIRE(Queue.Submit(Rectangle_Internal(7)));
	auto Result = Queue.SubmitGenerated(Jobs, Toolbox::TNumericLimits<Toolbox::size_t>::Max(),
	    [](Toolbox::size_t, Dxf::FRenderCommand&)
	    {
		    return Dxf::TResult<void>{};
	    });
	REQUIRE(!Result);
	REQUIRE(Result.Error().Code == Dxf::EErrorCode::InvalidArgument);
	FRecordingBackend Backend;
	REQUIRE(Queue.Execute_Internal(Backend));
	REQUIRE(Backend.m_Commands.Size() == 1);
}
TEST("generated errors are selected in input order")
{
	Toolbox::FJobSystem Jobs(4);
	Dxf::FRenderQueue2D Queue;
	Queue.SetAccepting_Internal(true);
	auto Result = Queue.SubmitGenerated(Jobs, 100, [](Toolbox::size_t Index, Dxf::FRenderCommand& Output)
	{
		Output = Rectangle_Internal(static_cast<Toolbox::int32>(Index));
		if (Index == 2)
		{
			return Dxf::TResult<void>::Failure(Dxf::EErrorCode::InvalidArgument, "first input error");
		}
		if (Index == 60)
		{
			return Dxf::TResult<void>::Failure(Dxf::EErrorCode::InvalidArgument, "later input error");
		}
		return Dxf::TResult<void>{};
	}, 1);
	REQUIRE(!Result);
	REQUIRE(Result.Error().Message == "first input error");
}
TEST("worker cannot cancel a frame or execute Native calls")
{
	Toolbox::FJobSystem Jobs(2);
	FRecordingBackend Backend;
	Dxf::FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(320, 240));
	REQUIRE(Renderer.GetContext().Get2D().FillRectangle({7, 0, 8, 1}));
	bool bNativeRejected = false;
	bool bEndRejected = false;
	Toolbox::FJobFence Fence;
	REQUIRE(Jobs.TrySubmit([&]()
	{
		Renderer.CancelFrame();
		bNativeRejected = !Renderer.Native([]()
		{
			return Dxf::TResult<void>{};
		});
		bEndRejected = !Renderer.EndFrame();
	}, &Fence));
	REQUIRE(Jobs.Wait(Fence));
	REQUIRE(Fence.FailureCount() == 0);
	REQUIRE(bNativeRejected && bEndRejected);
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.m_Commands.Size() == 1);
	REQUIRE(Backend.m_ForeignCalls == 0);
}
TEST("layer and explicit order precede stable generated input order")
{
	Toolbox::FJobSystem Jobs(4);
	Dxf::FRenderQueue2D Queue;
	Queue.SetAccepting_Internal(true);
	REQUIRE(Queue.SubmitGenerated(Jobs, 100, [](Toolbox::size_t Index, Dxf::FRenderCommand& Output)
	{
		Output = Rectangle_Internal(static_cast<Toolbox::int32>(Index), static_cast<Toolbox::int32>(Index % 3),
		                            static_cast<Toolbox::int32>(Index % 5));
		return Dxf::TResult<void>{};
	}, 0));
	FRecordingBackend Backend;
	REQUIRE(Queue.Execute_Internal(Backend));
	Toolbox::size_t Position = 0;
	// ソート実装とは別の列挙順を正解として比較する。
	for (Toolbox::int32 Layer = 0; Layer < 3; ++Layer)
	{
		for (Toolbox::int32 Order = 0; Order < 5; ++Order)
		{
			for (Toolbox::int32 Input = 0; Input < 100; ++Input)
			{
				if (Input % 3 == Layer && Input % 5 == Order)
				{
					REQUIRE(Backend.m_Commands[Position++] == Input);
				}
			}
		}
	}
	REQUIRE(Position == Backend.m_Commands.Size());
}
namespace
{
struct FReleaseTrace
{
	Toolbox::size_t Count = 0;
	Toolbox::uint64 Thread = 0;
	bool WasJob = true;
};
void RecordRelease_Internal(void* Context, Toolbox::int32) noexcept
{
	auto* Trace = static_cast<FReleaseTrace*>(Context);
	++Trace->Count;
	Trace->Thread = Toolbox::FThread::CurrentThreadId();
	Trace->WasJob = Toolbox::FJobSystem::IsExecutingJob();
}
}
TEST("pinned sprite resources are finally released on the owner thread")
{
	Toolbox::FJobSystem Jobs(4);
	FReleaseTrace Trace;
	Dxf::FTexture Texture(Toolbox::MakeShared<Dxf::FTextureResource>(
	    Dxf::FNativeHandle(42, &Trace, &RecordRelease_Internal), Dxf::FTextureMetadata{32, 32, false}));
	Dxf::FRenderQueue2D Queue;
	Queue.SetAccepting_Internal(true);
	REQUIRE(Queue.SubmitGenerated(Jobs, 128, [&](Toolbox::size_t Index, Dxf::FRenderCommand& Output)
	{
		Output = Dxf::FSpriteCommand{Texture, {static_cast<Toolbox::f32>(Index), 0}, {}};
		return Dxf::TResult<void>{};
	}, 1));
	Texture = {};
	REQUIRE(Trace.Count == 0);
	FRecordingBackend Backend;
	REQUIRE(Queue.Execute_Internal(Backend));
	REQUIRE(Trace.Count == 1);
	REQUIRE(Trace.Thread == Toolbox::FThread::CurrentThreadId());
	REQUIRE(!Trace.WasJob);
}
TEST("feedback rejection preserves old commands and pinned resource")
{
	Toolbox::FJobSystem Jobs(4);
	FReleaseTrace Trace;
	Dxf::FTexture Texture(Toolbox::MakeShared<Dxf::FTextureResource>(
	    Dxf::FNativeHandle(42, &Trace, &RecordRelease_Internal), Dxf::FTextureMetadata{32, 32, false}));
	Dxf::FRenderQueue2D Queue;
	Queue.SetAccepting_Internal(true);
	REQUIRE(Queue.Submit(Rectangle_Internal(7)));
	Queue.SetTarget_Internal(42);
	REQUIRE(!Queue.SubmitGenerated(Jobs, 32, [&](Toolbox::size_t, Dxf::FRenderCommand& Output)
	{
		Output = Dxf::FSpriteCommand{Texture, {}, {}};
		return Dxf::TResult<void>{};
	}, 1));
	REQUIRE(Trace.Count == 0);
	FRecordingBackend Backend;
	REQUIRE(Queue.Execute_Internal(Backend));
	REQUIRE(Backend.m_Commands.Size() == 1);
	Texture = {};
	REQUIRE(Trace.Count == 1);
	REQUIRE(!Trace.WasJob);
}
