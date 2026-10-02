// SPDX-License-Identifier: NOASSERTION
#include "SaveUiSmoke.h"
#include "Toolbox/JsonWriter.h"
#include "DxLib.h"
namespace Dxf::GameplaySmoke
{
void RunSaveUiSmoke(FSmokeApp& App, GameplaySample::ASaveCourseGameInstance& Game, UiSmoke::FForwardRenderer& Render, FDxLibBackends& Backends, const Toolbox::FPath& Output, Toolbox::TFunction<Toolbox::uint64()> CompletedSteps)
{
	// 文字と矩形の観察は所有側のこの試行内だけ。実描画を置換しない。
	Toolbox::TVector<FTextCommand> Texts;
	Toolbox::TVector<FRectangleCommand> Rectangles;
	Render.ObserveText = [&](const FTextCommand& Text)
	{
		Texts.PushBack(Text);
	};
	Render.ObserveRectangle = [&](const FRectangleCommand& Rectangle)
	{
		Rectangles.PushBack(Rectangle);
	};
	const auto Step = [&]()
	{
		Texts.Clear();
		Rectangles.Clear();
		App.Step();
	};
	const auto Press = [&](EKey Key)
	{
		Texts.Clear();
		Rectangles.Clear();
		App.Press(Key);
		Step();
		// 解放時に要求された倍率は、次の所有側更新で適用する。
		Step();
	};
	// F1を開いた後は通常のPause契約で物理を止め、倍率を変えても採取しない。
	Press(EKey::F1);
	Check(Game.GetSettings().Volume == .35f && Game.GetSettings().UiScale == 1.5f, "native menu opening altered persisted settings");
	const auto Before = CompletedSteps();
	for (Toolbox::uint32 Layout = 0; Layout < 2; ++Layout)
	{
		if (Layout == 1)
		{
			Check(DxLib::SetWindowSize(1001, 501) == 0, "save UI odd resize");
			Step();
			Step();
		}
		const auto Locate = [&](const char* Label, Toolbox::f32 Below)
		{
			for (Toolbox::uint32 Attempt = 0; Attempt < 60; ++Attempt)
			{
				Toolbox::TOptional<FTextCommand> Found;
				for (const auto& Text : Texts)
				{
					if (Text.Text == Label)
					{
						Found = Text;
						break;
					}
				}
				Check(static_cast<bool>(Found), "save UI label not drawn");
				const auto& Clip = Found->Options.ClipRect;
				if (Found->Position.Y >= Clip.Top && Found->Position.Y + Below < Clip.Bottom - 2)
				{
					return *Found;
				}
				App.SetPointer((Clip.Left + Clip.Right) / 2, (Clip.Top + Clip.Bottom) / 2, false, Found->Position.Y < Clip.Top ? 1 : -1);
				Step();
				App.SetPointer((Clip.Left + Clip.Right) / 2, (Clip.Top + Clip.Bottom) / 2, false, 0);
				Step();
			}
			throw Toolbox::FException("save UI label not reachable");
		};
		// 表示したButtonの内部矩形と色を一括GPU読戻しで独立照合する。
		const auto Original = Game.GetSettings();
		const auto Button = Locate("Toggle 1 / 2 views", 36);
		Toolbox::TOptional<FRectangleCommand> Plate;
		Toolbox::int64 Smallest = 0x7fffffffffffffff;
		for (const auto& Rectangle : Rectangles)
		{
			const auto R = Rectangle.Rectangle;
			const auto Area = static_cast<Toolbox::int64>(R.Right - R.Left) * (R.Bottom - R.Top);
			if (Rectangle.bFilled && Rectangle.Options.Opacity == 1 && R.Left <= Button.Position.X && R.Right > Button.Position.X + 20 && R.Top <= Button.Position.Y && R.Bottom > Button.Position.Y + 8 && Area < Smallest)
			{
				Plate = Rectangle;
				Smallest = Area;
			}
		}
		Check(static_cast<bool>(Plate), "save UI button background not drawn");
		const auto R = Plate->Rectangle;
		const Dxf::FVector2 Point{static_cast<Toolbox::f32>(R.Right - 10), static_cast<Toolbox::f32>(R.Bottom - 5)};
		Dxf::FVector2 Points[9];
		FColor Colors[9];
		Toolbox::size_t Index = 0;
		for (Toolbox::int32 Y = -1; Y <= 1; ++Y)
		{
			for (Toolbox::int32 X = -1; X <= 1; ++X)
			{
				Points[Index++] = {Point.X + X, Point.Y + Y};
			}
		}
		App.CapturePoints(Output / (Layout == 0 ? "ui-scale-single.png" : "ui-scale-odd.png"), Points, Colors, 9);
		for (const auto Color : Colors)
		{
			const auto Expected = Plate->Options.Color;
			Check(Toolbox::Abs(static_cast<Toolbox::int32>(Color.R) - Expected.R) <= 8 && Toolbox::Abs(static_cast<Toolbox::int32>(Color.G) - Expected.G) <= 8 && Toolbox::Abs(static_cast<Toolbox::int32>(Color.B) - Expected.B) <= 8, "save UI background pixel differs");
		}
		App.SetPointer(static_cast<Toolbox::int32>(Button.Position.X) + 3, static_cast<Toolbox::int32>(Button.Position.Y) + 3, true, 0);
		Step();
		App.SetPointer(static_cast<Toolbox::int32>(Button.Position.X) + 3, static_cast<Toolbox::int32>(Button.Position.Y) + 3, false, 0);
		Step();
		Check(Game.GetSettings().Volume == Original.Volume && Game.GetSettings().UiScale == Original.UiScale, "native view button altered independent settings");
		const auto FocusSlider = [&](const char* Label)
		{
			const auto Text = Locate(Label, 80);
			int Width = 0;
			int Height = 0;
			Check(DxLib::GetDrawScreenSize(&Width, &Height) == 0, "save UI display size");
			const auto Scale = Toolbox::Clamp(static_cast<Toolbox::f32>(Height) / 720 * Game.GetSettings().UiScale, .25f, 8.0f);
			const auto FontHeight = Backends.GetServices().Fonts.GetFontLineHeight(Text.Font.GetNativeHandle_Internal());
			Check(static_cast<bool>(FontHeight), "save UI font height");
			const auto X = static_cast<Toolbox::int32>(Text.Position.X + 32 * Scale);
			const auto Y = static_cast<Toolbox::int32>(Text.Position.Y + FontHeight.Value() + 18 * Scale);
			App.SetPointer(X, Y, true, 0);
			Step();
			App.SetPointer(X, Y, false, 0);
			Step();
			Step();
		};
		FocusSlider("音量（適用と保存は別）");
		const auto Volume = Game.GetSettings().Volume;
		Press(EKey::Right);
		Check(Toolbox::Abs(Game.GetSettings().Volume - Volume - .05f) < .001, "save UI volume click not routed at scale");
		FocusSlider("UI倍率（描画とクリック判定）");
		const auto Scale = Game.GetSettings().UiScale;
		Press(EKey::Right);
		Check(Toolbox::Abs(Game.GetSettings().UiScale - Scale - .25f) < .001, "save UI scale click not routed");
		// 試行用の未保存設定変更を戻す。元の設定面へ書かない。
		Game.RequestSettings(Original);
		Step();
		Step();
		Check(CompletedSteps() == Before, "save UI Pause added fixed updates");
		Toolbox::Out << "SAVE_UI_NATIVE_VERIFIED layout=" << (Layout == 0 ? "single" : "odd") << " scale=" << Toolbox::FJsonWriter::Number(Original.UiScale) << '\n';
	}
	Press(EKey::F1);
	Render.ObserveText = {};
	Render.ObserveRectangle = {};
	Texts.Clear();
	Rectangles.Clear();
	App.SetPointer(0, 0, false, 0);
	Check(DxLib::SetWindowSize(1280, 720) == 0, "save UI resize restore");
	App.Step();
}
} // namespace Dxf::GameplaySmoke
