// SPDX-License-Identifier: NOASSERTION
#include "ContentConsumer.h"
#include "Dxf/SceneContentSource.h"
#include "Dxf/ViewCoordinates.h"
#include "Dxf/NativeHandle.h"
#include "DxLib.h"
namespace
{
// 非0を最初の異常として扱う。
void Require(bool V)
{
	if (!V)
	{
		throw Toolbox::FException("External native Content failed");
	}
}
// 一括読戻しの一時画像だけを所有する。
void Release(void*, Toolbox::int32 Id) noexcept
{
	(void)DxLib::DeleteSoftImage(Id);
}
Dxf::FVector2 Project(const Dxf::FSceneDefinition2D& D, Toolbox::FVector2 P)
{
	return {D.Views[0].Origin.X + P.X * D.Views[0].PixelsPerMeter,
	        D.Views[0].Origin.Y - P.Y * D.Views[0].PixelsPerMeter};
}
Dxf::FVector2 Project(const Dxf::FSceneDefinition3D& D, Toolbox::FVector3 P)
{
	const auto Q = Dxf::ProjectWorldToScreen(D.Views[0], 1280, 720, P);
	Require(Q && Q.Value().bInsideView);
	return Q.Value().Screen;
}
// 本体の公開ContentSceneを利用し、手作り描画Sceneへ置き換えない。
template <typename TScene, typename TPrepared>
void Draw(Dxf::FApplication& App, const TPrepared& Prepared, const Toolbox::TFunction<void()>& Step)
{
	auto Scene = Toolbox::MakeUnique<TScene>(Prepared);
	auto* Live = Scene.Get();
	Require(static_cast<bool>(App.GetScenes().RequestChange(Toolbox::Move(Scene))));
	for (Toolbox::int32 I = 0; I < 5; ++I)
	{
		Step();
	}
	const auto Instance = Live->GetPrefab("doorA");
	Require(Instance.Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	Require(Instance.Get()->GetDistanceJoint("hanger").Get()->GetJointId() && Instance.Get()->GetFixedJoint("assembly").Get()->GetJointId() && Instance.Get()->GetRevoluteJoint("armDrive").Get()->GetJointId() && Instance.Get()->GetPrismaticJoint("doorDrive").Get()->GetJointId());
	const auto P = Instance.Get()->GetRigidBody("doorBody").Get()->GetRenderPosition();
	const auto Point = Project(*Prepared.Definition, P);
	const auto Expected = Instance.Get()->GetDefinition().Parts[1].Visual.Color;
	Dxf::FNativeHandle Image(DxLib::MakeARGB8ColorSoftImage(1280, 720), nullptr, &Release);
	Require(Image.Get() >= 0);
	Require(DxLib::SetDrawScreen(DX_SCREEN_FRONT) == 0);
	const auto Read = DxLib::GetDrawScreenSoftImage(0, 0, 1280, 720, Image.Get());
	Require(DxLib::SetDrawScreen(DX_SCREEN_BACK) == 0 && Read == 0);
	Toolbox::int32 Matches = 0;
	for (Toolbox::int32 Y = -5; Y <= 5; ++Y)
	{
		for (Toolbox::int32 X = -5; X <= 5; ++X)
		{
			int R = 0;
			int G = 0;
			int B = 0;
			int A = 0;
			Require(DxLib::GetPixelSoftImage(Image.Get(), static_cast<int>(Point.X) + X, static_cast<int>(Point.Y) + Y, &R, &G, &B, &A) == 0);
			if (Toolbox::Abs(R - Expected.R) <= 35 && Toolbox::Abs(G - Expected.G) <= 35 && Toolbox::Abs(B - Expected.B) <= 35)
			{
				++Matches;
			}
		}
	}
	Require(Matches > 50);
	Toolbox::Out << "EXTERNAL_CONTENT_PIXEL dimension=" << (Toolbox::IsSame<TScene, Dxf::DContentScene2D> ? 2 : 3)
	             << " matches=" << Matches << "\n";
}
} // namespace
void RunContentNativeConsumer(Dxf::FApplication& App, const Toolbox::FPath& Root, const Toolbox::TFunction<void()>& Step)
{
	Dxf::FSceneContentSource Source(Root);
	Draw<Dxf::DContentScene2D>( App, Dxf::PrepareScene(Source.LoadScene2D("Assets/Content/course2d.dxfscene.json"), App.GetAssets()), Step);
	Draw<Dxf::DContentScene3D>( App, Dxf::PrepareScene(Source.LoadScene3D("Assets/Content/course3d.dxfscene.json"), App.GetAssets()), Step);
}
