// SPDX-License-Identifier: NOASSERTION
// 再配置したパッケージのdxf::native＋dxf::ui_runtimeだけを使う、UI入りの外部のApplication。
// 実DxLibで画面のUI（ボタン・日本語の文字）を固定入力で押して3Dのシーンへ切り替え、3Dの平面のUIの画素とクリックを確かめて終了する。
// 使い方: NativeUiApp <ProjectRoot> [<Style.dxfui> <Broken.dxfui>]（スタイルはProjectRootからの相対パス）。
// スタイルを渡さない場合は組込みスタイルだけで起動する。成功で"NATIVE_UI_CONSUMER_PASSED"と終了コード0。
#include "Dxf/Application.h"
#include "Dxf/NativeBackends.h"
#include "Dxf/SceneNavigator.h"
#include "Dxf/UiAssetTextService.h"
#include "Dxf/UiButton.h"
#include "Dxf/UiLabel.h"
#include "Dxf/UiSceneHost.h"
#include "Dxf/UiStyleResource.h"
#include "Dxf/ViewCoordinates.h"
#include "Toolbox/Platform.h"
#include "DxLib.h"
namespace
{
using namespace Dxf;
// 画面の消去色。
constexpr FColor ClearColor = {30, 40, 50, 255};
// 3Dの平面のUIの色。
constexpr FColor PanelColor = {40, 200, 90, 255};

// 失敗を例外にする。
void Check(bool bOk, const char* Message)
{
	if (!bOk)
	{
		throw Toolbox::FException(Message);
	}
}

void Require(TResult<void> Result)
{
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
}

// OSの入力の代わりに固定の入力を返す。
class FFixedInput final : public IInputSource
{
public:
	FRawInput Raw;
	TResult<FRawInput> Poll() override
	{
		return TResult<FRawInput>::Success(Raw);
	}
};

// 表示中の画面の画素（Present後の表の画面から読む）。
FColor ReadPixel(Toolbox::int32 X, Toolbox::int32 Y)
{
	Check(DxLib::SetDrawScreen(DX_SCREEN_FRONT) == 0, "front buffer");
	int R = 0;
	int G = 0;
	int B = 0;
	DxLib::GetColor2(DxLib::GetPixel(X, Y), &R, &G, &B);
	Check(DxLib::SetDrawScreen(DX_SCREEN_BACK) == 0, "back buffer");
	return {static_cast<Toolbox::uint8>(R), static_cast<Toolbox::uint8>(G), static_cast<Toolbox::uint8>(B), 255};
}

bool Near(FColor A, FColor B, Toolbox::int32 Tolerance)
{
	return Toolbox::Abs(A.R - B.R) <= Tolerance && Toolbox::Abs(A.G - B.G) <= Tolerance &&
	       Toolbox::Abs(A.B - B.B) <= Tolerance;
}

// 3Dの平面のUIとView。
class DUiScene3D final : public DScene
{
public:
	static FRenderView3D MakeView() noexcept
	{
		FRenderView3D View;
		View.Id = 3;
		View.Eye = {0, 0, -8};
		View.Target = {0, 0, 0};
		return View;
	}
	Toolbox::int32 GetClicks() const noexcept
	{
		return m_Clicks;
	}

protected:
	TResult<void> OnInitialize(const FInitContext& Context) override
	{
		m_pText = Toolbox::MakeUnique<FUiAssetTextService>(Context.Assets);
		FUiRootSettings Settings;
		Settings.Text = m_pText.Get();
		m_pRoot = Toolbox::MakeUnique<FUiRoot>(Settings);
		auto Button = m_pRoot->Create<DUiButton>("平面のUI");
		Button.Get()->SetWidth(FUiLength::Fill());
		Button.Get()->SetHeight(FUiLength::Fill());
		FUiStylePatch Style;
		Style.Background = PanelColor;
		Style.BorderWidth = 0.0f;
		Button.Get()->SetStyleOverride(Style);
		if (auto Added = m_pRoot->AddToLayer(EUiLayer::Normal, Button.Cast<DUiElement>()); !Added)
		{
			return Added;
		}
		m_Subscription = Button.Get()->OnClicked().Subscribe(
		    [this]()
		    {
			    ++m_Clicks;
		    });
		FUiWorldPanel3D Panel;
		Panel.TopLeft = {-2, 1.5f, 0};
		Panel.TopRight = {2, 1.5f, 0};
		Panel.BottomLeft = {-2, -1.5f, 0};
		Panel.TextureWidth = 256;
		Panel.TextureHeight = 192;
		FUiDisplayOptions Options;
		Options.Scale.Mode = EUiScaleMode::FixedPixel;
		m_Host.AddWorldPanel3D(*m_pRoot, Panel, Context.Assets, Options);
		Toolbox::TVector<FRenderView3D> Views;
		Views.PushBack(MakeView());
		m_Host.SetWorldViews3D(Views);
		m_Host.AttachTo(*this);
		return {};
	}
	void OnDraw(FRenderContext& Render) const override
	{
		Require(m_Host.RenderWorldPanelTextures(Render));
		const FRenderView3D View = MakeView();
		Require(Render.Get3D().SetView(View));
		Require(m_Host.DrawWorldPanels3D(Render, View));
		Require(m_Host.Draw(Render));
	}

private:
	Toolbox::TUniquePtr<FUiAssetTextService> m_pText;
	Toolbox::TUniquePtr<FUiRoot> m_pRoot;
	mutable FUiSceneHost m_Host;
	FUiSubscription m_Subscription;
	Toolbox::int32 m_Clicks = 0;
};

// 画面のUI（ボタンと日本語の文字）。ボタンを押すと3Dのシーンへ切り替える。
class DUiScene2D final : public DScene
{
public:
	explicit DUiScene2D(Toolbox::FPath StylePath) : m_StylePath(Toolbox::Move(StylePath))
	{
	}
	FUiRoot& GetRoot() const noexcept
	{
		return *m_pRoot;
	}
	DUiButton& GetButton() const noexcept
	{
		return *m_Button.Get();
	}
	FUiStyleResource& GetStyles() noexcept
	{
		return m_Styles;
	}

protected:
	TResult<void> OnInitialize(const FInitContext& Context) override
	{
		m_pText = Toolbox::MakeUnique<FUiAssetTextService>(Context.Assets);
		FUiRootSettings Settings;
		Settings.Text = m_pText.Get();
		m_pRoot = Toolbox::MakeUnique<FUiRoot>(Settings);
		const bool bStyled = !m_StylePath.ToUtf8().IsEmpty();
		if (bStyled)
		{
			// 外部のスタイルはプロジェクトの根からの相対パスで読む（作業ディレクトリに依らない）。
			m_Styles.Attach(*m_pRoot);
			if (auto Loaded = m_Styles.ReloadFile(Context.Assets.GetProjectRoot() / m_StylePath); !Loaded)
			{
				return Loaded;
			}
		}
		m_Button = m_pRoot->Create<DUiButton>("開始");
		m_Button.Get()->SetStyleId(bStyled ? "ConsumerButton" : "Button");
		m_Button.Get()->SetAbsolutePosition({100, 100});
		m_Button.Get()->SetWidth(FUiLength::Fixed(240));
		m_Button.Get()->SetHeight(FUiLength::Fixed(80));
		if (auto Added = m_pRoot->AddToLayer(EUiLayer::Panel, m_Button.Cast<DUiElement>()); !Added)
		{
			return Added;
		}
		auto Label = m_pRoot->Create<DUiLabel>("日本語のUI");
		Label.Get()->SetAbsolutePosition({100, 220});
		Label.Get()->SetWidth(FUiLength::Fixed(300));
		Label.Get()->SetHeight(FUiLength::Fixed(40));
		FUiStylePatch Text;
		Text.Foreground = FColor{255, 255, 255, 255};
		Text.FontSize = 28;
		Label.Get()->SetStyleOverride(Text);
		if (auto Added = m_pRoot->AddToLayer(EUiLayer::Panel, Label.Cast<DUiElement>()); !Added)
		{
			return Added;
		}
		m_Subscription = m_Button.Get()->OnClicked().Subscribe(
		    [this]()
		    {
			    m_bStart = true;
		    });
		FUiDisplayOptions Options;
		Options.Scale.Mode = EUiScaleMode::FixedPixel;
		m_Host.AddScreen(*m_pRoot, Options);
		m_Host.AttachTo(*this);
		return {};
	}
	void OnTick(const FTickContext& Context) override
	{
		if (m_bStart && Context.Scenes != nullptr)
		{
			m_bStart = false;
			Require(Context.Scenes->RequestChange<DUiScene3D>());
		}
	}
	void OnDraw(FRenderContext& Render) const override
	{
		Require(m_Host.Draw(Render));
	}

private:
	Toolbox::FPath m_StylePath;
	Toolbox::TUniquePtr<FUiAssetTextService> m_pText;
	FUiStyleResource m_Styles;
	Toolbox::TUniquePtr<FUiRoot> m_pRoot;
	mutable FUiSceneHost m_Host;
	TUiRef<DUiButton> m_Button;
	FUiSubscription m_Subscription;
	bool m_bStart = false;
};
} // namespace

// 日本語・空白を含むパスをUTF-8で受け取るため、ワイド文字の引数から変換する。
int wmain(int Count, wchar_t** Args)
{
	if (Count != 2 && Count != 4)
	{
		Toolbox::Err << "Usage: NativeUiApp <ProjectRoot> [<Style.dxfui> <Broken.dxfui>]\n";
		return 2;
	}
	try
	{
		const Toolbox::FString Root = Toolbox::FromWide(Args[1]);
		const bool bStyled = Count == 4;
		const Toolbox::FPath Style(bStyled ? Toolbox::FromWide(Args[2]) : Toolbox::FString());
		const Toolbox::FPath Broken(bStyled ? Toolbox::FromWide(Args[3]) : Toolbox::FString());
		FDxLibBackends Backends;
		FFixedInput Input;
		const auto Services = Backends.GetServices();
		FApplicationSettings Settings;
		Settings.ProjectRoot = Root;
		Settings.Window.Width = 1280;
		Settings.Window.Height = 720;
		Settings.Window.bVSync = false;
		Settings.ExecutionThreadCount = 1;
		Settings.ClearColor = ClearColor;
		FApplication App(
		    {Services.Platform, Input, Services.Textures, Services.Sounds, Services.Fonts, Services.Renderer},
		    Settings);
		Check(static_cast<bool>(App.Start(Toolbox::MakeUnique<DUiScene2D>(Style))), "start failed");
		Toolbox::f64 Time = 0;
		auto Step = [&]
		{
			const auto Result = App.Step(Time);
			Time += 1.0 / 60.0;
			Check(Result && Result.Value(), "step failed");
		};
		for (Toolbox::int32 Frame = 0; Frame < 3; ++Frame)
		{
			Step();
		}
		auto* Scene2D = App.GetScenes().GetCurrent()->TryCast<DUiScene2D>();
		Check(Scene2D != nullptr, "2D UI scene missing");
		// ボタンの背景は、解決したスタイルの色で描かれる（外部スタイルなら指定の色）。
		const FColor Expected = Scene2D->GetButton().GetStyle().Background;
		if (bStyled)
		{
			Check(Expected.R == 0x20 && Expected.G == 0xa0 && Expected.B == 0xe0, "external style applied");
		}
		Check(Near(ReadPixel(110, 110), Expected, 0) && !Near(Expected, ClearColor, 0), "UI button pixel");
		// 日本語の文字の画素（白に近い画素が複数ある）。
		Toolbox::int32 TextPixels = 0;
		for (Toolbox::int32 Y = 222; Y < 258; Y += 2)
		{
			for (Toolbox::int32 X = 102; X < 300; X += 2)
			{
				const FColor Pixel = ReadPixel(X, Y);
				TextPixels += Pixel.R > 180 && Pixel.G > 180 && Pixel.B > 180 ? 1 : 0;
			}
		}
		Check(TextPixels > 10, "UI Japanese text pixels");
		if (bStyled)
		{
			// 読込に失敗した再読込は旧版を保つ。
			const Toolbox::uint64 Revision = Scene2D->GetStyles().GetRevision();
			Check(!Scene2D->GetStyles().ReloadFile(Toolbox::FPath(Root) / Broken), "broken style must fail");
			Check(Scene2D->GetStyles().GetRevision() == Revision, "broken style keeps revision");
			Step();
			Check(Near(ReadPixel(110, 110), Expected, 0), "broken style keeps old pixels");
		}
		// 固定入力でボタンを押して離すと、3Dのシーンへ切り替わる。
		Input.Raw.MouseX = 220;
		Input.Raw.MouseY = 140;
		Step();
		Input.Raw.MouseButtons[0] = true;
		Step();
		Input.Raw.MouseButtons[0] = false;
		for (Toolbox::int32 Frame = 0; Frame < 4; ++Frame)
		{
			Step();
		}
		auto* Scene3D = App.GetScenes().GetCurrent()->TryCast<DUiScene3D>();
		Check(Scene3D != nullptr, "3D UI scene missing");
		// 平面の中央の画素は平面のUIの色。
		auto Center = ProjectWorldToScreen(DUiScene3D::MakeView(), 1280, 720, {0.5f, -0.5f, 0});
		Check(Center && Center.Value().bInsideView, "panel projection");
		const auto X = static_cast<Toolbox::int32>(Center.Value().Screen.X);
		const auto Y = static_cast<Toolbox::int32>(Center.Value().Screen.Y);
		Check(Near(ReadPixel(X, Y), PanelColor, 2), "3D panel UI pixel");
		// 平面の上のクリックはそのUIへ届く。
		Input.Raw.MouseX = X;
		Input.Raw.MouseY = Y;
		Step();
		Input.Raw.MouseButtons[0] = true;
		Step();
		Input.Raw.MouseButtons[0] = false;
		Step();
		Check(Scene3D->GetClicks() == 1, "3D panel UI click");
		App.GetScenes().RequestQuit();
		const auto Quit = App.Step(Time);
		Check(Quit && !Quit.Value(), "quit failed");
		Toolbox::Out << "NATIVE_UI_CONSUMER_PASSED\n";
		return 0;
	}
	catch (const Toolbox::FException& Error)
	{
		Toolbox::Err << Error.What() << "\n";
		return 1;
	}
}
