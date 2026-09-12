#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/AssetService.h"
#include "Dxf/RenderSystem2D.h"
#include <stdexcept>

using namespace Dxf;
using namespace Dxf::Testing;

namespace
{
enum class ERenderOperation
{
	None,
	Target,
	Reset,
	Clear,
	Present
};

/** Fails after forwarding, modelling a backend that mutated state before reporting failure. */
class FFailingRenderer final : public IRenderBackend
{
public:
	void FailNext(ERenderOperation Operation, bool bThrow = false)
	{
		m_Operation = Operation;
		m_bThrow = bThrow;
	}
	int GetPresentations() const
	{
		return m_Backend.GetTrace().Presentations;
	}
	TResult<void> SetTarget(int Handle, int Width, int Height) override
	{
		auto Result = m_Backend.SetTarget(Handle, Width, Height);
		return Result ? Complete_Internal(ERenderOperation::Target) : Result;
	}
	TResult<void> Clear(FColor Color) override
	{
		auto Result = m_Backend.Clear(Color);
		return Result ? Complete_Internal(ERenderOperation::Clear) : Result;
	}
	TResult<void> ResetState(int Width, int Height) override
	{
		auto Result = m_Backend.ResetState(Width, Height);
		return Result ? Complete_Internal(ERenderOperation::Reset) : Result;
	}
	TResult<void> DrawSprite(const FSpriteCommand& Command) override
	{
		return m_Backend.DrawSprite(Command);
	}
	TResult<void> DrawText(const FTextCommand& Command) override
	{
		return m_Backend.DrawText(Command);
	}
	TResult<void> DrawRectangle(const FRectangleCommand& Command) override
	{
		return m_Backend.DrawRectangle(Command);
	}
	TResult<void> Present() override
	{
		auto Result = Complete_Internal(ERenderOperation::Present);
		return Result ? m_Backend.Present() : Result;
	}
private:
	TResult<void> Complete_Internal(ERenderOperation Operation)
	{
		if (m_Operation != Operation)
		{
			return {};
		}
		m_Operation = ERenderOperation::None;
		if (m_bThrow)
		{
			throw std::runtime_error("injected renderer exception");
		}
		return TResult<void>::Failure(EErrorCode::BackendFailure, "injected renderer failure");
	}
	FFakeBackend m_Backend;
	ERenderOperation m_Operation = ERenderOperation::None;
	bool m_bThrow = false;
};
}

TEST("native callback failure poisons the entire frame even when ignored")
{
	FFailingRenderer Backend;
	FRenderSystem2D Render(Backend);
	REQUIRE(Render.BeginFrame(320, 240));
	auto Native = Render.Native([]
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "partial native draw");
	});
	REQUIRE(!Native);
	auto End = Render.EndFrame();
	REQUIRE(!End);
	REQUIRE(End.Error().Message == "partial native draw");
	REQUIRE(Backend.GetPresentations() == 0);
	REQUIRE(Render.BeginFrame(320, 240));
	REQUIRE(Render.EndFrame());
	REQUIRE(Backend.GetPresentations() == 1);
}

TEST("native callback exception rejects further commands and cannot be presented")
{
	FFailingRenderer Backend;
	FRenderSystem2D Render(Backend);
	REQUIRE(Render.BeginFrame(320, 240));
	auto Native = Render.Native([]() -> TResult<void>
	{
		throw std::runtime_error("user draw exception");
	});
	REQUIRE(!Native && Native.Error().Code == EErrorCode::UserException);
	REQUIRE(!Render.GetContext().FillRectangle({0, 0, 10, 10}));
	REQUIRE(!Render.EndFrame());
	REQUIRE(Backend.GetPresentations() == 0);
}

TEST("failed clear cannot present a partially cleared frame")
{
	FFailingRenderer Backend;
	FRenderSystem2D Render(Backend);
	REQUIRE(Render.BeginFrame(320, 240));
	Backend.FailNext(ERenderOperation::Clear);
	REQUIRE(!Render.ClearTarget({0, 0, 0, 255}));
	REQUIRE(!Render.EndFrame());
	REQUIRE(Backend.GetPresentations() == 0);
}

TEST("begin frame contains backend exceptions at each startup stage and is reusable")
{
	for (auto Operation : {ERenderOperation::Target, ERenderOperation::Reset, ERenderOperation::Clear})
	{
		FFailingRenderer Backend;
		FRenderSystem2D Render(Backend);
		Backend.FailNext(Operation, true);
		auto Begin = Render.BeginFrame(320, 240);
		REQUIRE(!Begin && Begin.Error().Code == EErrorCode::BackendFailure);
		REQUIRE(Render.BeginFrame(320, 240));
		REQUIRE(Render.EndFrame());
	}
}

TEST("clear target contains backend exceptions and retains the first failure")
{
	FFailingRenderer Backend;
	FRenderSystem2D Render(Backend);
	REQUIRE(Render.BeginFrame(320, 240));
	Backend.FailNext(ERenderOperation::Clear, true);
	auto Clear = Render.ClearTarget({0, 0, 0, 255});
	REQUIRE(!Clear && Clear.Error().Code == EErrorCode::BackendFailure);
	REQUIRE(!Render.Flush());
	REQUIRE(!Render.EndFrame());
	REQUIRE(Backend.GetPresentations() == 0);
}

TEST("present exception closes the failed frame so the next frame can begin")
{
	FFailingRenderer Backend;
	FRenderSystem2D Render(Backend);
	REQUIRE(Render.BeginFrame(320, 240));
	Backend.FailNext(ERenderOperation::Present, true);
	auto End = Render.EndFrame();
	REQUIRE(!End && End.Error().Code == EErrorCode::BackendFailure);
	REQUIRE(Render.BeginFrame(320, 240));
	REQUIRE(Render.EndFrame());
	REQUIRE(Backend.GetPresentations() == 1);
}

TEST("render target backend exception is contained and rollback keeps the frame usable")
{
	FFakeBackend AssetsBackend;
	FAssetService Assets(AssetsBackend, AssetsBackend, AssetsBackend);
	auto Target = Assets.CreateRenderTarget(64, 64);
	REQUIRE(Target);
	for (auto Operation : {ERenderOperation::Target, ERenderOperation::Reset})
	{
		FFailingRenderer Backend;
		FRenderSystem2D Render(Backend);
		REQUIRE(Render.BeginFrame(320, 240));
		Backend.FailNext(Operation, true);
		auto Switch = Render.SetRenderTarget(Target.Value());
		REQUIRE(!Switch && Switch.Error().Code == EErrorCode::BackendFailure);
		REQUIRE(Render.EndFrame());
	}
}

TEST("native state restoration exception is contained and blocks presentation")
{
	FFailingRenderer Backend;
	FRenderSystem2D Render(Backend);
	REQUIRE(Render.BeginFrame(320, 240));
	Backend.FailNext(ERenderOperation::Reset, true);
	auto Native = Render.Native([] { return TResult<void>{}; });
	REQUIRE(!Native && Native.Error().Code == EErrorCode::BackendFailure);
	REQUIRE(!Render.EndFrame());
	REQUIRE(Backend.GetPresentations() == 0);
	Render.CancelFrame();
	REQUIRE(Render.BeginFrame(320, 240));
	REQUIRE(Render.EndFrame());
}
