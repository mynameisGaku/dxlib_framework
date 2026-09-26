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
		const bool bTransparent = Display.Panel3D.Composition == EUiPanelComposition::Transparent;
		if (!bTransparent && Display.Panel3D.Background.A != 255)
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
		    Display.Texture.GetHeight() != Display.Panel3D.TextureHeight || Display.bTextureAlpha != bTransparent)
		{
			// 透明な合成ではアルファ付きの中間画像を使う。
			auto Created = Display.pAssets->CreateRenderTarget(Display.Panel3D.TextureWidth,
			                                                   Display.Panel3D.TextureHeight, bTransparent);
			if (!Created)
			{
				return TResult<void>::Failure(Created.Error());
			}
			Display.Texture = Created.Value();
			Display.bTextureAlpha = bTransparent;
			if (FDisplay* Live = Find_Internal(Display.Id))
			{
				Live->Texture = Display.Texture;
				Live->bTextureAlpha = bTransparent;
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
			// 透明な合成は(0,0,0,0)で消去し、乗算済みアルファの内容を蓄積する。
			Result = Render.ClearTarget(bTransparent ? FColor{0, 0, 0, 0} : Display.Panel3D.Background);
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
	// パネルは利用者のViewの区間へ加える（同じ区間の深度で手前の物体に隠れ、その区間の形状の後に描く）。
	// ここでSetViewを呼ぶと別の区間になり、深度が消去されて手前の物体に隠れなくなる。
	const FRenderView3D& Current = Render.Get3D().GetView();
	if (Current.Id != View.Id || Current.Eye.X != View.Eye.X || Current.Eye.Y != View.Eye.Y ||
	    Current.Eye.Z != View.Eye.Z || Current.Target.X != View.Target.X || Current.Target.Y != View.Target.Y ||
	    Current.Target.Z != View.Target.Z)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument,
		                              "Set the same view with Get3D().SetView before drawing UI world panels");
	}
	// 不透明なパネルを登録順に描き、透明なパネルは視点から遠い順に描く（交差しない平面の前後を正しく合成する）。
	Toolbox::TVector<const FDisplay*> Order;
	for (const auto& Display : m_Displays)
	{
		if (Display.Kind == EUiDisplayKind::WorldPanel3D && Display.Root.Get() != nullptr && Display.Texture.IsValid())
		{
			Order.PushBack(&Display);
		}
	}
	auto Distance = [&View](const FDisplay* Display) -> Toolbox::f64
	{
		const FUiWorldPanel3D& Panel = Display->Panel3D;
		const Toolbox::FVector3 Center = (Panel.TopRight + Panel.BottomLeft) * 0.5f;
		const Toolbox::FVector3 Offset = Center - View.Eye;
		return Toolbox::Dot(Offset, Offset);
	};
	Toolbox::StableSort(Order.Begin(), Order.End(),
	                    [&](const FDisplay* A, const FDisplay* B)
	                    {
		                    const bool TransparentA = A->Panel3D.Composition == EUiPanelComposition::Transparent;
		                    const bool TransparentB = B->Panel3D.Composition == EUiPanelComposition::Transparent;
		                    if (TransparentA != TransparentB)
		                    {
			                    return !TransparentA;
		                    }
		                    return TransparentA && Distance(A) > Distance(B);
	                    });
	for (const FDisplay* Entry : Order)
	{
		const FDisplay& Display = *Entry;
		FTexturedQuad3D Quad;
		Quad.Texture = Display.Texture.AsTexture();
		Quad.Corners = {Display.Panel3D.TopLeft, Display.Panel3D.TopRight, Display.Panel3D.BottomRight(),
		                Display.Panel3D.BottomLeft};
		Quad.Depth = Display.Panel3D.Depth;
		Quad.bDoubleSided = Display.Panel3D.bDoubleSided;
		Quad.bPremultipliedAlpha = Display.bTextureAlpha;
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
