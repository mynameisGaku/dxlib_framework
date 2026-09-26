// SPDX-License-Identifier: NOASSERTION
#include "UiHostState.h"
#include "Dxf/UiRenderer.h"
#include "Dxf/AssetService.h"
#include "Dxf/GuardValue.h"
namespace Dxf::Detail
{
namespace
{
// 中間画像を使う表示先の条件。
struct FPanelTexture
{
	Toolbox::int32 Width = 0;
	Toolbox::int32 Height = 0;
	bool bTransparent = false;
	FColor Background;
	bool b3D = false;
};

FPanelTexture DescribeTexture_Internal(EUiDisplayKind Kind, const FUiWorldPanel2D& Panel2D,
                                       const FUiWorldPanel3D& Panel3D)
{
	FPanelTexture Texture;
	if (Kind == EUiDisplayKind::WorldPanel3D)
	{
		Texture.Width = Panel3D.TextureWidth;
		Texture.Height = Panel3D.TextureHeight;
		Texture.bTransparent = Panel3D.Composition == EUiPanelComposition::Transparent;
		Texture.Background = Panel3D.Background;
		Texture.b3D = true;
		return Texture;
	}
	const FUiPixelRect Rect = Panel2D.GetPixelRect();
	Texture.Width = Rect.Width();
	Texture.Height = Rect.Height();
	Texture.bTransparent = Panel2D.Composition == EUiPanelComposition::Transparent;
	Texture.Background = Panel2D.Background;
	return Texture;
}

// 透明な合成に必要な能力を確かめ、不足する能力と表示先を失敗に書く（描画を始める前）。
// 2Dのパネルは中間画像を2Dで貼るため、3Dの四角形の能力は要求しない。
TResult<void> CheckTransparentComposition_Internal(FUiDisplayId Id, const FRenderCapabilities& Capabilities, bool b3D)
{
	Toolbox::FString Missing;
	if (!Capabilities.bPremultipliedBlend2D)
	{
		Missing += " premultiplied-2d-blend";
	}
	if (!Capabilities.bAlphaTargetClear)
	{
		Missing += " alpha-target-clear";
	}
	if (b3D && !Capabilities.bTexturedQuads3D)
	{
		Missing += " textured-quads-3d";
	}
	if (b3D && !Capabilities.bPremultipliedQuads3D)
	{
		Missing += " premultiplied-quads-3d";
	}
	if (Missing.IsEmpty())
	{
		return {};
	}
	return TResult<void>::Failure(EErrorCode::BackendFailure,
	                              Toolbox::FString("UI display ") + Toolbox::ToString(Id) +
	                                  " requests transparent composition; backend lacks:" + Missing);
}
} // namespace

// 表示先の中間画像を描く。借用Rootはコールバック後に必ず再解決する。
TResult<bool> FUiHostState::RenderDisplayTexture_Internal(FRenderContext& Render, FDisplay& Display)
{
	const FPanelTexture Texture = DescribeTexture_Internal(Display.Kind, Display.Panel2D, Display.Panel3D);
	if (Display.Kind == EUiDisplayKind::WorldPanel3D && (Texture.Width <= 0 || Texture.Height <= 0))
	{
		return TResult<bool>::Failure(EErrorCode::InvalidArgument, "Invalid UI panel texture size");
	}
	if (Texture.Width <= 0 || Texture.Height <= 0)
	{
		// 画面の範囲が空の2Dのパネルは描かない（0x0の中間画像を作らない）。
		return TResult<bool>::Success(false);
	}
	if (!Texture.bTransparent && Texture.Background.A != 255)
	{
		return TResult<bool>::Failure(EErrorCode::InvalidArgument, "UI world panel background must be opaque");
	}
	if (Texture.bTransparent)
	{
		if (auto Capable = CheckTransparentComposition_Internal(Display.Id, Render.GetCapabilities(), Texture.b3D);
		    !Capable)
		{
			return TResult<bool>::Failure(Capable.Error());
		}
	}
	const FUiSurface Surface = MakeSurface_Internal(Display);
	Display.Root.Get()->SetSurface(Surface);
	auto Laid = Display.Root.Get()->Layout();
	if (!Laid)
	{
		return TResult<bool>::Failure(Laid.Error());
	}
	if (m_bStopped || Display.Root.Get() == nullptr || Find_Internal(Display.Id) == nullptr)
	{
		return TResult<bool>::Success(false);
	}
	if (!Display.Texture.IsValid() || Display.Texture.GetWidth() != Texture.Width ||
	    Display.Texture.GetHeight() != Texture.Height || Display.bTextureAlpha != Texture.bTransparent)
	{
		// 透明な合成ではアルファ付きの中間画像を使う。寸法・合成が変わったときだけ作り直す。
		auto Created = Display.pAssets->CreateRenderTarget(Texture.Width, Texture.Height, Texture.bTransparent);
		if (!Created)
		{
			return TResult<bool>::Failure(Created.Error());
		}
		Display.Texture = Created.Value();
		Display.bTextureAlpha = Texture.bTransparent;
		if (FDisplay* Live = Find_Internal(Display.Id))
		{
			Live->Texture = Display.Texture;
			Live->bTextureAlpha = Texture.bTransparent;
		}
	}
	m_DrawList.Clear();
	auto Built = Display.Root.Get()->BuildDrawList(m_DrawList);
	if (!Built)
	{
		return TResult<bool>::Failure(Built.Error());
	}
	if (m_bStopped || Display.Root.Get() == nullptr || Find_Internal(Display.Id) == nullptr)
	{
		return TResult<bool>::Success(false);
	}
	auto Previous = Render.GetRenderTarget();
	if (!Previous)
	{
		return TResult<bool>::Failure(Previous.Error());
	}
	auto Switched = Render.SetRenderTarget(Display.Texture);
	if (!Switched)
	{
		return TResult<bool>::Failure(Switched.Error());
	}
	TResult<void> Result;
	try
	{
		// 透明な合成は(0,0,0,0)で消去し、乗算済みアルファの内容を蓄積する。
		Result = Render.ClearTarget(Texture.bTransparent ? FColor{0, 0, 0, 0} : Texture.Background);
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
	// 最初の失敗を保ったまま、呼出し前の描画先へ戻す。
	const auto Back = Render.RestoreRenderTarget(Previous.Value());
	if (!Result)
	{
		return TResult<bool>::Failure(Result.Error());
	}
	if (!Back)
	{
		return TResult<bool>::Failure(Back.Error());
	}
	return TResult<bool>::Success(true);
}
// 3Dのパネルの中間画像を描く。
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
		auto Drawn = RenderDisplayTexture_Internal(Render, Display);
		if (!Drawn)
		{
			return TResult<void>::Failure(Drawn.Error());
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
		if (Display.Kind == EUiDisplayKind::WorldPanel2D && Display.Panel2D.bOffscreen)
		{
			// 中間画像へ描いてから、画面の範囲へ等倍で貼る（前後は他の2DのUIと同じLayer・受付順）。
			if (Display.pAssets == nullptr)
			{
				return TResult<void>::Failure(EErrorCode::InvalidState, "Offscreen UI panel requires an asset service");
			}
			FDisplay Copy = Display;
			auto Drawn = RenderDisplayTexture_Internal(Render, Copy);
			if (!Drawn)
			{
				return TResult<void>::Failure(Drawn.Error());
			}
			if (!Drawn.Value())
			{
				continue;
			}
			const FUiPixelRect Rect = Copy.Panel2D.GetPixelRect();
			FUiPixelRect Clip{0, 0, Render.GetTargetWidth(), Render.GetTargetHeight()};
			Clip = Clip.Intersect(Rect);
			if (!Copy.Panel2D.ScreenClip.IsEmpty())
			{
				Clip = Clip.Intersect(Copy.Panel2D.ScreenClip);
			}
			if (Clip.IsEmpty())
			{
				continue;
			}
			FSpriteDrawOptions Sprite;
			Sprite.Layer = Copy.Options.Layer;
			Sprite.Order = NextOrder++;
			Sprite.bClip = true;
			Sprite.ClipRect = {Clip.Left, Clip.Top, Clip.Right, Clip.Bottom};
			Sprite.Blend = Copy.bTextureAlpha ? EBlendMode2D::PremultipliedAlpha : EBlendMode2D::Alpha;
			auto Blitted = Render.Get2D().DrawSprite(
			    Copy.Texture.AsTexture(), {static_cast<Toolbox::f32>(Rect.Left), static_cast<Toolbox::f32>(Rect.Top)},
			    Sprite);
			if (!Blitted)
			{
				return Blitted;
			}
			continue;
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
		const FUiRect DisplayClip = MakeDisplayClip_Internal(Display, Surface);
		if (!DisplayClip.IsEmpty())
		{
			for (auto& Item : m_DrawList.EditItems())
			{
				Item.Clip = Item.Clip.Intersect(DisplayClip);
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
