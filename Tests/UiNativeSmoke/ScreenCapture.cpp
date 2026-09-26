// SPDX-License-Identifier: NOASSERTION
#include "ScreenCapture.h"
#include "DxLib.h"
namespace Dxf::UiSmoke
{
namespace
{
// 読み戻しの失敗は試験の失敗にする。
void Require_Internal(bool Value, const char* Error)
{
	if (!Value)
	{
		throw Toolbox::FException(Error);
	}
}

// CPU側の画像を解放する。
void ReleaseImage_Internal(void*, Toolbox::int32 Handle) noexcept
{
	(void)DxLib::DeleteSoftImage(Handle);
}
} // namespace

void FScreenCapture::Capture()
{
	m_Image = FNativeHandle(DxLib::MakeARGB8ColorSoftImage(1280, 720), nullptr, &ReleaseImage_Internal);
	Require_Internal(m_Image.Get() >= 0, "display capture image allocation");
	Require_Internal(DxLib::GetDrawScreenSoftImage(0, 0, 1280, 720, m_Image.Get()) == 0, "display capture readback");
}

FColor FScreenCapture::Pixel(Toolbox::int32 X, Toolbox::int32 Y) const
{
	Require_Internal(IsCaptured() && X >= 0 && Y >= 0 && X < 1280 && Y < 720, "display capture pixel range");
	int R = 0;
	int G = 0;
	int B = 0;
	int A = 0;
	Require_Internal(DxLib::GetPixelSoftImage(m_Image.Get(), X, Y, &R, &G, &B, &A) == 0, "display capture pixel read");
	return FColor{static_cast<Toolbox::uint8>(R), static_cast<Toolbox::uint8>(G), static_cast<Toolbox::uint8>(B), 255};
}

void FScreenCapture::Save(const Toolbox::FPath& Path) const
{
	Require_Internal(IsCaptured(), "display capture missing");
	Require_Internal(DxLib::SaveSoftImageToPng(Path.ToUtf8().CStr(), m_Image.Get(), 1) == 0, "display capture save");
}
} // namespace Dxf::UiSmoke
