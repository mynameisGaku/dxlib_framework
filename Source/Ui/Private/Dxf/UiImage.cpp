// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiImage.h"
#include "Dxf/UiDrawContext.h"
namespace Dxf
{
DUiImage::DUiImage(FTexture Texture) : m_Texture(Toolbox::Move(Texture))
{
	SetStyleId("Image");
	SetHitTest(EUiHitTest::None);
}

void DUiImage::SetTexture(FTexture Texture)
{
	m_Texture = Toolbox::Move(Texture);
	InvalidateMeasure();
}

void DUiImage::SetFit(EUiImageFit Fit)
{
	if (Fit > EUiImageFit::Stretch)
	{
		throw Toolbox::FException("Invalid UI image fit");
	}
	m_Fit = Fit;
	InvalidateArrange();
}

FUiSize DUiImage::OnMeasure(FUiLayoutContext&, FUiSize)
{
	return {static_cast<Toolbox::f32>(m_Texture.GetWidth()), static_cast<Toolbox::f32>(m_Texture.GetHeight())};
}

FUiRect DUiImage::GetImageRect() const noexcept
{
	const FUiRect Area = GetContentRect();
	if (m_Fit == EUiImageFit::Stretch || m_Texture.GetWidth() <= 0 || m_Texture.GetHeight() <= 0)
	{
		return Area;
	}
	const Toolbox::f32 X = Area.Width / static_cast<Toolbox::f32>(m_Texture.GetWidth());
	const Toolbox::f32 Y = Area.Height / static_cast<Toolbox::f32>(m_Texture.GetHeight());
	const Toolbox::f32 Scale = m_Fit == EUiImageFit::Contain ? Toolbox::Min(X, Y) : Toolbox::Max(X, Y);
	const Toolbox::f32 Width = static_cast<Toolbox::f32>(m_Texture.GetWidth()) * Scale;
	const Toolbox::f32 Height = static_cast<Toolbox::f32>(m_Texture.GetHeight()) * Scale;
	return {Area.X + (Area.Width - Width) * 0.5f, Area.Y + (Area.Height - Height) * 0.5f, Width, Height};
}

void DUiImage::OnDraw(FUiDrawContext& Context) const
{
	DUiElement::OnDraw(Context);
	if (m_Texture.GetWidth() <= 0 || m_Texture.GetHeight() <= 0)
	{
		return;
	}
	const auto Before = Context.PushClip_Internal(GetContentRect());
	Context.DrawImage(m_Texture, GetImageRect(), GetStyle().Foreground);
	Context.PopClip_Internal(Before);
}

void DUiImage::SetInspectionPeakScale(Toolbox::f32 Scale)
{
	if (!Toolbox::IsFinite(Scale) || Scale < 1)
	{
		throw Toolbox::FException("Invalid UI image peak scale");
	}
	m_PeakScale = Scale;
}
} // namespace Dxf
// namespace Dxf
