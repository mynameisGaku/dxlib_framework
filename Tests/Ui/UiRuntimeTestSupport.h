// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_RUNTIME_TEST_SUPPORT_H
#define DXF_UI_RUNTIME_TEST_SUPPORT_H
#include "UiTestSupport.h"
#include "Dxf/UiSceneHost.h"
#include "Dxf/UiButton.h"
#include "Dxf/UiPopup.h"
#include "Dxf/RenderSystem.h"
#include "Dxf/AssetService.h"
#include "Support/FakeBackend.h"
namespace UiTest
{
/**
 * 描画境界だけの記録用代替。Nativeの画素検証ではない。
 */
class FRecordingRenderer final : public IRenderBackend, public IFontBackend
{
public:
	Testing::FFakeBackend Assets;
	Toolbox::TVector<FRectangleCommand> Rectangles;
	Toolbox::TVector<FRenderView3D> Views;
	Toolbox::TVector<FTexturedQuad3D> Quads;
	Toolbox::TVector<Toolbox::FString> Texts;
	bool Clip = false;
	bool ThrowSet = false;
	bool ThrowReset = false;
	bool ThrowDraw = false;
	bool FailDraw = false;
	Toolbox::int32 ClipResets = 0;
	Toolbox::int32 CurrentTarget = -1;
	Toolbox::int32 Presents = 0;
	Toolbox::TMap<Toolbox::int32, Toolbox::int32> FontSizes;
	TResult<Toolbox::int32> CreateFont(const FFontOptions& Options) override
	{
		auto Created = Assets.CreateFont(Options);
		if (Created)
		{
			FontSizes[Created.Value()] = Options.Size;
		}
		return Created;
	}

	void DeleteFont(Toolbox::int32 Handle) noexcept override
	{
		FontSizes.Erase(Handle);
		Assets.DeleteFont(Handle);
	}

	TResult<Toolbox::int32> GetFontLineHeight(Toolbox::int32 Handle) override
	{
		const auto It = FontSizes.Find(Handle);
		if (It == FontSizes.End())
		{
			return TResult<Toolbox::int32>::Failure(EErrorCode::NotFound, "fake font expired");
		}
		return TResult<Toolbox::int32>::Success(It->Second);
	}

	TResult<Toolbox::int32> MeasureTextWidth(Toolbox::int32 Handle, const Toolbox::FString& Text) override
	{
		auto Size = GetFontLineHeight(Handle);
		if (!Size)
		{
			return Size;
		}
		Toolbox::int32 Count = 0;
		for (Toolbox::size_t I = 0; I < Text.Size(); ++I)
		{
			if ((static_cast<unsigned char>(Text[I]) & 0xc0) != 0x80)
			{
				++Count;
			}
		}
		return TResult<Toolbox::int32>::Success(Count * Size.Value() / 2);
	}

	bool SupportsShapes2D() const noexcept override
	{
		return true;
	}

	bool SupportsGeometry3D() const noexcept override
	{
		return true;
	}

	TResult<void> DrawGeometry3D(const FPreparedGeometry3D&) override
	{
		return {};
	}

	TResult<void> DrawCircle2D(const FCircleCommand2D&) override
	{
		return {};
	}

	TResult<void> DrawTriangle2D(const FTriangleCommand2D&) override
	{
		return {};
	}

	TResult<void> DrawLine2D(const FLineCommand2D&) override
	{
		return {};
	}

	TResult<void> SetTarget(Toolbox::int32 Handle, Toolbox::int32, Toolbox::int32) override
	{
		CurrentTarget = Handle;
		return {};
	}

	TResult<void> Clear(FColor) override
	{
		return {};
	}

	TResult<void> ResetState(Toolbox::int32, Toolbox::int32) override
	{
		return {};
	}

	TResult<void> DrawSprite(const FSpriteCommand&) override
	{
		return {};
	}

	TResult<void> DrawText(const FTextCommand& Command) override
	{
		Texts.PushBack(Command.Text.Get());
		return {};
	}

	TResult<void> DrawRectangle(const FRectangleCommand& Value) override
	{
		if (ThrowDraw)
		{
			throw Toolbox::FException("primary draw exception");
		}
		if (FailDraw)
		{
			return TResult<void>::Failure(EErrorCode::BackendFailure, "primary draw failure");
		}
		Rectangles.PushBack(Value);
		return {};
	}

	bool SupportsClip2D() const noexcept override
	{
		return true;
	}

	TResult<void> SetClip2D(bool Active, FIntRect) override
	{
		Clip = Active;
		if (!Active)
		{
			++ClipResets;
		}
		if ((Active && ThrowSet) || (!Active && ThrowReset))
		{
			throw Toolbox::FException(Active ? "primary clip exception" : "secondary reset exception");
		}
		return {};
	}

	bool SupportsTexturedQuads3D() const noexcept override
	{
		return true;
	}

	bool SupportsViewports3D() const noexcept override
	{
		return true;
	}

	TResult<void> BeginView3D(const FRenderView3D& View) override
	{
		Views.PushBack(View);
		return {};
	}

	TResult<void> DrawTexturedQuad3D(const FTexturedQuad3D& Quad) override
	{
		Quads.PushBack(Quad);
		return {};
	}

	TResult<void> Present() override
	{
		++Presents;
		return {};
	}
};
/**
 * 生入力を一度ずつ進めるホスト試験用のフレーム生成器。
 */
class FHostInput
{
public:
	FInputStateTracker Tracker;
	FRawInput Raw;
	Toolbox::uint64 Frame = 0;
	FInputSnapshot Send(FUiSceneHost& Host)
	{
		Tracker.Advance(Raw);
		FFrameTime Time;
		Time.FrameIndex = ++Frame;
		Time.UnscaledDeltaSeconds = 1.0 / 60.0;
		Time.DeltaSeconds = 1.0 / 60.0;
		return Host.RouteInput({Tracker.GetSnapshot(), Time});
	}
};
inline FUiDisplayOptions PixelOptions()
{
	FUiDisplayOptions Result;
	Result.Scale.Mode = EUiScaleMode::FixedPixel;
	return Result;
}

inline TUiRef<DUiButton> FullButton(FUiRoot& Root)
{
	auto Button = Root.Create<DUiButton>("button");
	Button.Get()->SetWidth(FUiLength::Fill());
	Button.Get()->SetHeight(FUiLength::Fill());
	REQUIRE(Root.AddToLayer(EUiLayer::Normal, Button.Cast<DUiElement>()));
	return Button;
}
} // namespace UiTest
#endif
