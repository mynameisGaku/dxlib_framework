// SPDX-License-Identifier: NOASSERTION
#include "UiHostState.h"
#include "Dxf/UiRenderer.h"
#include "Dxf/AssetService.h"
#include "Dxf/GuardValue.h"
namespace Dxf::Detail
{
// それぞれの表示面へ描画する。借用Rootはコールバック後に必ず再解決する。
TResult<void> FUiHostState::RenderWorldPanelTextures(FRenderContext& Render)
{
	if (m_bStopped || m_bDrawing)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "UI drawing unavailable or reentrant");
	}

	TGuardValue Busy(m_bDrawing, true);
	auto Displays = m_Displays;
	for (auto& Display : Displays)
	{
		if (Display.Kind != EUiDisplayKind::WorldPanel3D || Display.Root.Get() == nullptr || Display.pAssets == nullptr)
		{
			continue;
		}
		if (Display.Panel3D.TextureWidth <= 0 || Display.Panel3D.TextureHeight <= 0)
		{
			return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid UI panel texture size");
		}
		if (Display.Kind == EUiDisplayKind::WorldPanel3D && Display.Panel3D.Background.A != 255)
		{
			return TResult<void>::Failure(EErrorCode::InvalidArgument, "UI world panel background must be opaque");
		}
		const FUiSurface Surface = MakeSurface_Internal(Display);
		Display.Root.Get()->SetSurface(Surface);
		auto Laid = Display.Root.Get()->Layout();
		if (!Laid)
		{
			return Laid;
		}
		if (m_bStopped || Display.Root.Get() == nullptr || Find_Internal(Display.Id) == nullptr)
		{
			continue;
		}
		if (!Display.Texture.IsValid() || Display.Texture.GetWidth() != Display.Panel3D.TextureWidth ||
		    Display.Texture.GetHeight() != Display.Panel3D.TextureHeight)
		{
			auto Created =
			    Display.pAssets->CreateRenderTarget(Display.Panel3D.TextureWidth, Display.Panel3D.TextureHeight, false);
			if (!Created)
			{
				return TResult<void>::Failure(Created.Error());
			}
			Display.Texture = Created.Value();
			if (FDisplay* Live = Find_Internal(Display.Id))
			{
				Live->Texture = Display.Texture;
			}
		}
		m_DrawList.Clear();
		auto Built = Display.Root.Get()->BuildDrawList(m_DrawList);
		if (!Built)
		{
			return Built;
		}
		if (m_bStopped || Display.Root.Get() == nullptr || Find_Internal(Display.Id) == nullptr)
		{
			continue;
		}
		auto Previous = Render.GetRenderTarget();
		if (!Previous)
		{
			return TResult<void>::Failure(Previous.Error());
		}
		auto Switched = Render.SetRenderTarget(Display.Texture);
		if (!Switched)
		{
			return Switched;
		}
		TResult<void> Result;
		try
		{
			Result = Render.ClearTarget(Display.Panel3D.Background);
			if (Result)
			{
				auto Submitted = SubmitUiDrawList(Render.Get2D(), m_DrawList, Surface, {0, 0});
				if (!Submitted)
				{
					Result = TResult<void>::Failure(Submitted.Error());
				}
			}
		}
		catch (const Toolbox::FException& Error)
		{
			Result = TResult<void>::Failure(EErrorCode::UserException, Error.What());
		}
		catch (...)
		{
			Result = TResult<void>::Failure(EErrorCode::UserException, "UI texture drawing threw");
		}
		const auto Back = Render.RestoreRenderTarget(Previous.Value());
		if (!Result)
		{
			return Result;
		}
		if (!Back)
		{
			return Back;
		}
	}
	return {};
}
// 指定されたViewを明示的に使う。描画から入力View一覧を推測しない。
TResult<void> FUiHostState::DrawWorldPanels3D(FRenderContext& Render, const FRenderView3D& View)
{
	if (m_bStopped || m_bDrawing)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "UI drawing unavailable or reentrant");
	}

	TGuardValue Busy(m_bDrawing, true);
	auto Applied = Render.Get3D().SetView(View);
	if (!Applied)
	{
		return Applied;
	}
	for (const auto& Display : m_Displays)
	{
		if (Display.Kind != EUiDisplayKind::WorldPanel3D || Display.Root.Get() == nullptr || !Display.Texture.IsValid())
		{
			continue;
		}
		FTexturedQuad3D Quad;
		Quad.Texture = Display.Texture.AsTexture();
		Quad.Corners = {Display.Panel3D.TopLeft, Display.Panel3D.TopRight, Display.Panel3D.BottomRight(),
		                Display.Panel3D.BottomLeft};
		Quad.Depth = Display.Panel3D.Depth;
		Quad.bDoubleSided = Display.Panel3D.bDoubleSided;
		auto Drawn = Render.Get3D().DrawTexturedQuad(Quad);
		if (!Drawn)
		{
			return Drawn;
		}
	}
	return {};
}
// 同じRootも表示先ごとにレイアウト・描画する。イベントと時間は増えない。
TResult<void> FUiHostState::Draw(FRenderContext& Render)
{
	if (m_bStopped || m_bDrawing)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "UI drawing unavailable or reentrant");
	}

	TGuardValue Busy(m_bDrawing, true);
	if (Render.GetTargetWidth() > 0 && Render.GetTargetHeight() > 0)
	{
		m_ScreenWidth = Render.GetTargetWidth();
		m_ScreenHeight = Render.GetTargetHeight();
	}
	const auto Displays = m_Displays;
	Toolbox::int32 NextOrder = 0;
	for (const auto& Display : Displays)
	{
		if (m_bStopped || Display.Root.Get() == nullptr || Display.Kind == EUiDisplayKind::WorldPanel3D ||
		    Find_Internal(Display.Id) == nullptr)
		{
			continue;
		}
		if (Display.Kind == EUiDisplayKind::WorldPanel3D && Display.Panel3D.Background.A != 255)
		{
			return TResult<void>::Failure(EErrorCode::InvalidArgument, "UI world panel background must be opaque");
		}
		const FUiSurface Surface = MakeSurface_Internal(Display);
		Display.Root.Get()->SetSurface(Surface);
		auto Laid = Display.Root.Get()->Layout();
		if (!Laid)
		{
			return Laid;
		}
		if (m_bStopped || Display.Root.Get() == nullptr || Find_Internal(Display.Id) == nullptr)
		{
			continue;
		}
		m_DrawList.Clear();
		auto Built = Display.Root.Get()->BuildDrawList(m_DrawList);
		if (!Built)
		{
			return Built;
		}
		if (m_bStopped || Display.Root.Get() == nullptr || Find_Internal(Display.Id) == nullptr)
		{
			continue;
		}
		if (Display.Kind == EUiDisplayKind::WorldPanel2D && !Display.Panel2D.ScreenClip.IsEmpty())
		{
			const FUiPixelRect Area = Surface.GetPixelRect().Intersect(Display.Panel2D.ScreenClip);
			const FVector2 TopLeft =
			    Surface.ToLogical({static_cast<Toolbox::f32>(Area.Left), static_cast<Toolbox::f32>(Area.Top)});
			const FVector2 BottomRight =
			    Surface.ToLogical({static_cast<Toolbox::f32>(Area.Right), static_cast<Toolbox::f32>(Area.Bottom)});
			for (auto& Item : m_DrawList.EditItems())
			{
				Item.Clip =
				    Item.Clip.Intersect({TopLeft.X, TopLeft.Y, BottomRight.X - TopLeft.X, BottomRight.Y - TopLeft.Y});
			}
		}
		auto Submitted = SubmitUiDrawList(Render.Get2D(), m_DrawList, Surface, {Display.Options.Layer, NextOrder});
		if (!Submitted)
		{
			return TResult<void>::Failure(Submitted.Error());
		}
		NextOrder = Submitted.Value().NextOrder;
	}
	return {};
}
} // namespace Dxf::Detail
// namespace Dxf::Detail
