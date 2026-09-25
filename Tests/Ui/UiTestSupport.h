// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_TEST_UI_TEST_SUPPORT_H
#define DXF_TEST_UI_TEST_SUPPORT_H
#include "Dxf/UiRoot.h"
#include "Dxf/UiTextService.h"
#include "Support/Test.h"
namespace UiTest
{
using namespace Dxf;

/**
 * 非Nativeの試験用の文字の窓口（固定文字幅の代替）。ASCIIは画素の大きさの半分、それ以外は画素の大きさの幅。
 * 行の送りは画素の大きさ。フォントは無効な値（描画はしない試験だけで使う）。
 */
class FFixedWidthTextService final : public IUiTextService
{
public:
	/**
	 * 最後に解決した画素の大きさを記録する。
	 */
	TResult<FFont> ResolveFont(const FUiFontKey& Key) override
	{
		LastPixelSize = Key.PixelSize;
		++Resolves;
		return TResult<FFont>::Success(FFont{});
	}
	/**
	 * 文字数から幅を求める。
	 */
	TResult<Toolbox::int32> MeasureWidth(const FFont&, const Toolbox::FString& Text) override
	{
		++Measures;
		Toolbox::int32 Width = 0;
		for (Toolbox::size_t Index = 0; Index < Text.Size();)
		{
			const auto Lead = static_cast<unsigned char>(Text[Index]);
			if (Lead < 0x80)
			{
				Width += LastPixelSize / 2;
				Index += 1;
			}
			else
			{
				Width += LastPixelSize;
				Index += Lead < 0xE0 ? 2 : (Lead < 0xF0 ? 3 : 4);
			}
		}
		return TResult<Toolbox::int32>::Success(Width);
	}
	/**
	 * 行の送り。
	 */
	TResult<Toolbox::int32> GetLineHeight(const FFont&) override
	{
		return TResult<Toolbox::int32>::Success(LastPixelSize);
	}
	/**
	 * 最後に解決した画素の大きさ（計測に使う）。
	 */
	Toolbox::int32 LastPixelSize = 20;
	/**
	 * 計測の回数。
	 */
	Toolbox::int32 Measures = 0;
	/**
	 * 解決の回数。
	 */
	Toolbox::int32 Resolves = 0;
};

/**
 * 接続・切断・出来事を数える要素。
 */
class DProbe : public DUiElement
{
public:
	/**
	 * 初回の接続の回数。
	 */
	Toolbox::int32 FirstAttaches = 0;
	/**
	 * 接続の回数。
	 */
	Toolbox::int32 Attaches = 0;
	/**
	 * 切断の回数。
	 */
	Toolbox::int32 Detaches = 0;
	/**
	 * 届いたポインターの出来事の数（種類ごと）。
	 */
	Toolbox::int32 Pointer[7] = {};
	/**
	 * 届いた操作の数。
	 */
	Toolbox::int32 Navigation = 0;
	/**
	 * 接続時に投げるか。
	 */
	bool bThrowOnAttach = false;
	/**
	 * 押下を処理するか。
	 */
	bool bHandleDown = true;
	/**
	 * 押下で呼ぶ処理。
	 */
	Toolbox::TFunction<void(DProbe&)> OnDown;
	/**
	 * 接続時に呼ぶ処理。
	 */
	Toolbox::TFunction<void(DProbe&)> OnAttachHook;
	/**
	 * 固定の大きさで作る。
	 * @param Width 幅。
	 * @param Height 高さ。
	 */
	DProbe(Toolbox::f32 Width = 0, Toolbox::f32 Height = 0)
	{
		if (Width > 0)
		{
			SetWidth(FUiLength::Fixed(Width));
		}
		if (Height > 0)
		{
			SetHeight(FUiLength::Fixed(Height));
		}
		SetAlign(EUiAlign::Start, EUiAlign::Start);
		SetHitTest(EUiHitTest::Self);
	}
	using DUiElement::AddChild;
	using DUiElement::CapturePointer;
	using DUiElement::CreateChild;
	using DUiElement::ReleasePointer;

protected:
	void OnFirstAttach() override
	{
		++FirstAttaches;
	}
	void OnAttach() override
	{
		++Attaches;
		if (OnAttachHook)
		{
			OnAttachHook(*this);
		}
		if (bThrowOnAttach)
		{
			throw Toolbox::FException("probe attach failure");
		}
	}
	void OnDetach() noexcept override
	{
		++Detaches;
	}
	void OnPointerEvent(FUiPointerEvent& Event) override
	{
		++Pointer[static_cast<Toolbox::size_t>(Event.Type)];
		if (Event.Type == EUiPointerEventType::Down)
		{
			Event.bHandled = bHandleDown;
			if (OnDown)
			{
				OnDown(*this);
			}
		}
	}
	void OnNavigationEvent(FUiNavigationEvent& Event) override
	{
		++Navigation;
		(void)Event;
	}
};

/**
 * 1280x720の画素・高さ基準720（倍率1）の表示面。
 */
inline FUiSurface MakeSurface(Toolbox::int32 Width = 1280, Toolbox::int32 Height = 720, Toolbox::f32 Reference = 720)
{
	FUiScaleSettings Settings;
	Settings.ReferenceHeight = Reference;
	return FUiSurface({0, 0, Width, Height}, Settings);
}

/**
 * ポインターの一フレーム。
 * @param X 論理座標の横。
 * @param Y 論理座標の縦。
 * @param bDown 主ボタンを押しているか。
 * @param bPreviousDown 前のフレームで押していたか。
 */
inline FUiInputFrame PointerFrame(Toolbox::f32 X, Toolbox::f32 Y, bool bDown, bool bPreviousDown,
                                  Toolbox::f64 Delta = 1.0 / 60.0)
{
	FUiInputFrame Frame;
	Frame.DeltaSeconds = Delta;
	Frame.Pointer.bPresent = true;
	Frame.Pointer.Position = {X, Y};
	Frame.Pointer.Down[0] = bDown;
	Frame.Pointer.Pressed[0] = bDown && !bPreviousDown;
	Frame.Pointer.Released[0] = !bDown && bPreviousDown;
	return Frame;
}

/**
 * 操作の一フレーム。
 * @param Command 操作。
 * @param bDown 押しているか。
 * @param bPreviousDown 前のフレームで押していたか。
 */
inline FUiInputFrame NavigationFrame(EUiNavigationCommand Command, bool bDown, bool bPreviousDown,
                                     Toolbox::f64 Delta = 1.0 / 60.0)
{
	FUiInputFrame Frame;
	Frame.DeltaSeconds = Delta;
	Frame.Navigation.bActive = true;
	Frame.Navigation.Down[static_cast<Toolbox::size_t>(Command)] = bDown;
	Frame.Navigation.Pressed[static_cast<Toolbox::size_t>(Command)] = bDown && !bPreviousDown;
	return Frame;
}

/**
 * 表示面を設定してレイアウトまで行う。
 * @param Root ルート。
 */
inline void LayoutRoot(FUiRoot& Root, const FUiSurface& Surface = MakeSurface())
{
	Root.SetSurface(Surface);
	auto Result = Root.Layout();
	REQUIRE(static_cast<bool>(Result));
}

/**
 * クリックの一連（押下と解放）。
 * @param Root ルート。
 */
inline void Click(FUiRoot& Root, Toolbox::f32 X, Toolbox::f32 Y)
{
	REQUIRE(static_cast<bool>(Root.ProcessInput(PointerFrame(X, Y, false, false))));
	REQUIRE(static_cast<bool>(Root.ProcessInput(PointerFrame(X, Y, true, false))));
	REQUIRE(static_cast<bool>(Root.ProcessInput(PointerFrame(X, Y, false, true))));
}
} // namespace UiTest
#endif
