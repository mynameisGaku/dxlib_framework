// SPDX-License-Identifier: NOASSERTION
#include "ModelViewerScene.h"
#include "PickingExample.h"
#include "Dxf/AssetService.h"
#include "Dxf/RenderContext.h"
#include "Dxf/SceneNavigator.h"
#include "Toolbox/Log.h"
#include "Toolbox/UniquePtr.h"
#include <stdio.h>
namespace Dxf::ModelViewer
{
namespace
{
// 失敗を例外へ変え、Applicationの失敗として報告させる。
void Require_Internal(TResult<void> Result)
{
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
}

template <typename T> T Take_Internal(TResult<T> Result)
{
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
	return Toolbox::Move(Result).Value();
}
} // namespace

// モデルを読み込み、インスタンスを配置する。
TResult<void> AModelViewerScene::Load_Internal()
{
	try
	{
		// 同じモデルデータを2体で共有し、変換と再生状態は別々に持つ。
		m_Column = Take_Internal(m_pAssets->LoadModel("Assets/Models/SkinnedColumn.fbx"));
		m_Box = Take_Internal(m_pAssets->LoadModel("Assets/Models/StaticBox.fbx"));
		m_Left = Take_Internal(m_pAssets->CreateModelInstance(m_Column));
		m_Right = Take_Internal(m_pAssets->CreateModelInstance(m_Column));
		m_BoxInstance = Take_Internal(m_pAssets->CreateModelInstance(m_Box));
		Require_Internal(m_Left.SetTransform(Toolbox::FMatrix4::Translation({-150, 0, 0})));
		Require_Internal(m_Right.SetTransform(Toolbox::FMatrix4::Translation({150, 0, 0})));
		Require_Internal(m_Left.Play("Bend"));
		Require_Internal(m_Right.Play("Bend"));
	}
	catch (const Toolbox::FException& Error)
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, Error.What());
	}
	return {};
}

TResult<void> AModelViewerScene::OnInitialize(const FInitContext& Context)
{
	m_pAssets = &Context.Assets;
	m_View.Eye = {0, 180, -520};
	m_View.Target = {0, 100, 0};
	m_View.NearPlane = 1.0f;
	m_View.FarPlane = 3000.0f;
	auto Font = Context.Assets.LoadFont();
	if (!Font)
	{
		return TResult<void>::Failure(Font.Error());
	}
	m_Font = Font.Value();
	DXF_LOG_INFO("ModelViewer", "Scene %u initialized", m_Generation);
	PrepareViews_Internal();
	return Load_Internal();
}

void AModelViewerScene::OnTick(const FTickContext& Context)
{
	const FInputSnapshot& Input = Context.Input;
	if (Input.WasPressed(EKey::Escape) && Context.Scenes != nullptr)
	{
		Context.Scenes->RequestQuit();
		return;
	}
	if (Input.WasPressed(EKey::Enter) && Context.Scenes != nullptr)
	{
		// 新しいSceneは自分でモデルを読み込む。古いSceneのインスタンスは退役時に解放される。
		Require_Internal(Context.Scenes->RequestChange(Toolbox::MakeUnique<AModelViewerScene>(m_Generation + 1)));
		return;
	}
	if (Input.WasPressed(EKey::R))
	{
		// すべての参照を外してから読み直す。キャッシュは参照がなくなった時点で失効している。
		m_Left = FModelInstance();
		m_Right = FModelInstance();
		m_BoxInstance = FModelInstance();
		m_Column = FModel();
		m_Box = FModel();
		m_pAssets->CollectUnused();
		Require_Internal(Load_Internal());
		m_Selected = -1;
		++m_Reloads;
		DXF_LOG_INFO("ModelViewer", "Reloaded models (%u)", m_Reloads);
	}
	if (Input.WasPressed(EKey::Num1))
	{
		if (m_Left.IsPlaying())
		{
			m_Left.Pause();
		}
		else
		{
			m_Left.Resume();
		}
	}
	if (Input.WasPressed(EKey::Num2))
	{
		Require_Internal(m_Right.SetSpeed(m_Right.GetSpeed() == 1.0 ? 2.5 : 1.0));
	}
	if (Input.WasPressed(EKey::Num3))
	{
		Require_Internal(m_Right.Play(m_Right.GetClip() == 0 ? "Twist" : "Bend"));
	}
	if (Input.WasPressed(EKey::V))
	{
		m_bSplit = !m_bSplit;
	}
	if (Input.WasPressed(EKey::P))
	{
		m_bPicking = !m_bPicking;
		m_Selected = -1;
	}
	if (Input.WasPressed(EKey::O))
	{
		m_View.bOrthographic = !m_View.bOrthographic;
		m_View.OrthographicHeight = 400;
	}
	const Toolbox::f64 Delta = Context.Time.DeltaSeconds;
	Require_Internal(m_Left.Advance(Delta));
	Require_Internal(m_Right.Advance(Delta));
	m_BoxAngle += Delta * 0.8;
	Require_Internal(m_BoxInstance.SetTransform(Toolbox::FMatrix4::Translation({0, 20, 220}) *
	                                            Toolbox::FMatrix4::Rotation({0, 1, 0}, static_cast<Toolbox::f32>(m_BoxAngle))));
	// 更新後の形状とカメラを、選択と描画で共用する。入力取得は既存Snapshotの一回だけ。
	PrepareViews_Internal();
	if (m_bPicking && Input.WasMousePressed(EMouseButton::Left))
	{
		m_Selected = -1;
		const FVector2 Mouse{static_cast<Toolbox::f32>(Input.GetRaw().MouseX), static_cast<Toolbox::f32>(Input.GetRaw().MouseY)};
		for (Toolbox::int32 Side = 0; Side < (m_bSplit ? 2 : 1); ++Side)
		{
			const Toolbox::int32 Picked = Take_Internal(PickExampleShapes(m_DrawViews[Side], 1280, 720, Mouse, m_PickSphere, m_PickBox));
			if (Picked >= 0)
			{
				m_Selected = Picked;
				break;
			}
		}
	}
}

void AModelViewerScene::PrepareViews_Internal()
{
	for (Toolbox::int32 Side = 0; Side < 2; ++Side)
	{
		m_DrawViews[Side] = m_View;
		m_DrawViews[Side].bViewport = m_bSplit;
		m_DrawViews[Side].Viewport = {Side * 640, 0, (Side + 1) * 640, 720};
		if (Side == 1)
		{
			m_DrawViews[Side].Eye = {400, 260, -450};
			m_DrawViews[Side].LightDirection = {1, -1, 0};
		}
	}
}

void AModelViewerScene::OnDraw(FRenderContext& Render) const
{
	auto& Draw3D = Render.Get3D();
	// 表示回数に関係なくアニメーション更新はOnTickだけで行う。
	for (Toolbox::int32 Side = 0; Side < (m_bSplit ? 2 : 1); ++Side)
	{
		const FRenderView3D& View = m_DrawViews[Side];
		Require_Internal(Draw3D.SetView(View));
		if (m_bPicking)
		{
			FDrawStyle3D SphereStyle;
			SphereStyle.Color = m_Selected == 0 ? FColor{255, 220, 30, 255} : FColor{80, 180, 240, 255};
			FDrawStyle3D BoxStyle;
			BoxStyle.Color = m_Selected == 1 ? FColor{255, 220, 30, 255} : FColor{240, 100, 80, 255};
			Require_Internal(Draw3D.DrawSphere(m_PickSphere, SphereStyle, 32));
			Require_Internal(Draw3D.DrawBox(m_PickBox, BoxStyle));
			// 選択した形状の中心に合わせ、同じビューの投影値で全画面2Dへ印を置く。
			if (m_Selected >= 0)
			{
				const auto Point = Take_Internal(ProjectWorldToScreen(View, 1280, 720, m_Selected == 0 ? m_PickSphere.Center : m_PickBox.Center));
				if (Point.bInsideView)
				{
					Require_Internal(Render.Get2D().DrawCircle(Point.Screen, 6));
					Require_Internal(Render.Get2D().DrawText(m_Font, m_Selected == 0 ? "Sphere" : "Box", {Point.Screen.X + 10, Point.Screen.Y}));
				}
			}
			continue;
		}
		Require_Internal(Draw3D.DrawModel(m_Left));
		Require_Internal(Draw3D.DrawModel(m_Right));
		Require_Internal(Draw3D.DrawModel(m_BoxInstance));
		// 床の目安線。モデルは不透明として同じビューの形状より先に描かれる。
		FDrawStyle3D Grid;
		Grid.Color = {70, 70, 70, 255};
		for (Toolbox::int32 Line = -4; Line <= 4; ++Line)
		{
			const Toolbox::f32 Offset = static_cast<Toolbox::f32>(Line * 75);
			Require_Internal(Draw3D.DrawLine({Offset, 0, -300}, {Offset, 0, 300}, Grid));
			Require_Internal(Draw3D.DrawLine({-300, 0, Offset}, {300, 0, Offset}, Grid));
		}
	}
	if (m_bPicking)
	{
		Require_Internal(Render.Get2D().DrawText(m_Font, "Shape picking (sphere / box): left click   [P] models   [V] split   [O] orthographic", {12, 62}));
	}
	char Text[256];
	snprintf(Text, sizeof(Text),
	         "Scene #%u  reloads=%u   [1] left pause  [2] right speed  [3] right clip  [V] split  [P] picking  [R] reload  [Enter] "
	         "next scene  [Esc] quit",
	         m_Generation, m_Reloads);
	Require_Internal(Render.Get2D().DrawText(m_Font, Text, {12, 12}));
	const FModel Model = m_Right.GetModel();
	const FModelClipInfo* RightClip = m_Right.GetClip() >= 0 ? Model.GetClip(static_cast<Toolbox::size_t>(m_Right.GetClip())) : nullptr;
	snprintf(Text, sizeof(Text), "left: Bend t=%.2fs %s    right: %s t=%.2fs speed=%.1fx", m_Left.GetTime(),
	         m_Left.IsPlaying() ? "playing" : "paused", RightClip != nullptr ? RightClip->Name.CStr() : "-", m_Right.GetTime(),
	         m_Right.GetSpeed());
	Require_Internal(Render.Get2D().DrawText(m_Font, Text, {12, 36}));
}

void AModelViewerScene::OnDeinitialize() noexcept
{
	m_Selected = -1;
	m_Left = FModelInstance();
	m_Right = FModelInstance();
	m_BoxInstance = FModelInstance();
	m_Column = FModel();
	m_Box = FModel();
	DXF_LOG_INFO("ModelViewer", "Scene %u deinitialized", m_Generation);
}
}
