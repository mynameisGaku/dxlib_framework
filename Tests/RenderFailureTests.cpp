#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/AssetService.h"
#include "Dxf/RenderSystem2D.h"
#include "Toolbox/Utility.h"

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

// Fails after forwarding, modelling a backend that mutated state before reporting failure.
class FFailingRenderer final : public IRenderBackend
{
public:
	void FailNext(ERenderOperation Operation, bool bThrow = false)
	{
		m_Operation = Operation;
		m_bThrow = bThrow;
	}
	// 画面表示の観測回数を返す。
	Toolbox::int32 GetPresentations() const
	{
		return m_Backend.GetTrace().Presentations;
	}
	// 指定された描画先を記録する。
	TResult<void> SetTarget(Toolbox::int32 Handle, Toolbox::int32 Width, Toolbox::int32 Height) override
	{
		// 検証対象の操作が返した成否と値。
		auto Result = m_Backend.SetTarget(Handle, Width, Height);
		return Result ? Complete_Internal(ERenderOperation::Target) : Result;
	}
	// 画面消去の呼び出しを記録する。
	TResult<void> Clear(FColor Color) override
	{
		// 検証対象の操作が返した成否と値。
		auto Result = m_Backend.Clear(Color);
		return Result ? Complete_Internal(ERenderOperation::Clear) : Result;
	}
	// 描画状態を既定値へ戻したことを記録する。
	TResult<void> ResetState(Toolbox::int32 Width, Toolbox::int32 Height) override
	{
		// 検証対象の操作が返した成否と値。
		auto Result = m_Backend.ResetState(Width, Height);
		return Result ? Complete_Internal(ERenderOperation::Reset) : Result;
	}
	// スプライト描画の引数を記録する。
	TResult<void> DrawSprite(const FSpriteCommand& Command) override
	{
		return m_Backend.DrawSprite(Command);
	}
	// 文字描画の呼び出しを記録する。
	TResult<void> DrawText(const FTextCommand& Command) override
	{
		return m_Backend.DrawText(Command);
	}
	// 矩形描画の呼び出しを記録する。
	TResult<void> DrawRectangle(const FRectangleCommand& Command) override
	{
		return m_Backend.DrawRectangle(Command);
	}
	// 画面表示の回数を記録する。
	TResult<void> Present() override
	{
		// 検証対象の操作が返した成否と値。
		auto Result = Complete_Internal(ERenderOperation::Present);
		return Result ? m_Backend.Present() : Result;
	}

private:
	// 保留されている検証処理を完了させる。
	TResult<void> Complete_Internal(ERenderOperation Operation)
	{
		if (m_Operation != Operation)
		{
			return {};
		}
		m_Operation = ERenderOperation::None;
		if (m_bThrow)
		{
			throw Toolbox::FException("injected renderer exception");
		}
		return TResult<void>::Failure(EErrorCode::BackendFailure, "injected renderer failure");
	}
	// 検証対象が参照するバックエンド。
	FFakeBackend m_Backend;
	// 指定したフックで実行する操作。
	ERenderOperation m_Operation = ERenderOperation::None;
	// フックで例外を発生させるか。
	bool m_bThrow = false;
};
} // namespace

TEST("native callback failure poisons the entire frame even when ignored")
{
	// 検証用のバックエンド。
	FFailingRenderer Backend;
	// フックへ渡す描画環境。
	FRenderSystem2D Render(Backend);
	REQUIRE(Render.BeginFrame(320, 240));
	auto Native = Render.Native(
	    []
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
	// 検証用のバックエンド。
	FFailingRenderer Backend;
	// フックへ渡す描画環境。
	FRenderSystem2D Render(Backend);
	REQUIRE(Render.BeginFrame(320, 240));
	auto Native = Render.Native(
	    []() -> TResult<void>
	    {
		    throw Toolbox::FException("user draw exception");
	    });
	REQUIRE(!Native && Native.Error().Code == EErrorCode::UserException);
	REQUIRE(!Render.GetContext().FillRectangle({0, 0, 10, 10}));
	REQUIRE(!Render.EndFrame());
	REQUIRE(Backend.GetPresentations() == 0);
}

TEST("failed clear cannot present a partially cleared frame")
{
	// 検証用のバックエンド。
	FFailingRenderer Backend;
	// フックへ渡す描画環境。
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
		// 検証用のバックエンド。
		FFailingRenderer Backend;
		// フックへ渡す描画環境。
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
	// 検証用のバックエンド。
	FFailingRenderer Backend;
	// フックへ渡す描画環境。
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
	// 検証用のバックエンド。
	FFailingRenderer Backend;
	// フックへ渡す描画環境。
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
	// 画像・音声資源を提供するバックエンド。
	FFakeBackend AssetsBackend;
	// 検証に使用する資源管理。
	FAssetService Assets(AssetsBackend, AssetsBackend, AssetsBackend);
	// 描画先または遷移先。
	auto Target = Assets.CreateRenderTarget(64, 64);
	REQUIRE(Target);
	for (auto Operation : {ERenderOperation::Target, ERenderOperation::Reset})
	{
		// 検証用のバックエンド。
		FFailingRenderer Backend;
		// フックへ渡す描画環境。
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
	// 検証用のバックエンド。
	FFailingRenderer Backend;
	// フックへ渡す描画環境。
	FRenderSystem2D Render(Backend);
	REQUIRE(Render.BeginFrame(320, 240));
	Backend.FailNext(ERenderOperation::Reset, true);
	auto Native = Render.Native(
	    []
	    {
		    return TResult<void>{};
	    });
	REQUIRE(!Native && Native.Error().Code == EErrorCode::BackendFailure);
	REQUIRE(!Render.EndFrame());
	REQUIRE(Backend.GetPresentations() == 0);
	Render.CancelFrame();
	REQUIRE(Render.BeginFrame(320, 240));
	REQUIRE(Render.EndFrame());
}
