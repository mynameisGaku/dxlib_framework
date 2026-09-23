// SPDX-License-Identifier: NOASSERTION
#include "ModelViewerScene.h"
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
	const Toolbox::f64 Delta = Context.Time.DeltaSeconds;
	Require_Internal(m_Left.Advance(Delta));
	Require_Internal(m_Right.Advance(Delta));
	m_BoxAngle += Delta * 0.8;
	Require_Internal(m_BoxInstance.SetTransform(Toolbox::FMatrix4::Translation({0, 20, 220}) *
	                                            Toolbox::FMatrix4::Rotation({0, 1, 0}, static_cast<Toolbox::f32>(m_BoxAngle))));
}

void AModelViewerScene::OnDraw(FRenderContext& Render) const
{
	auto& Draw3D = Render.Get3D();
	Require_Internal(Draw3D.SetView(m_View));
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
	char Text[256];
	snprintf(Text, sizeof(Text), "Scene #%u  reloads=%u   [1] left pause  [2] right speed  [3] right clip  [R] reload  [Enter] next scene  [Esc] quit",
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
	m_Left = FModelInstance();
	m_Right = FModelInstance();
	m_BoxInstance = FModelInstance();
	m_Column = FModel();
	m_Box = FModel();
	DXF_LOG_INFO("ModelViewer", "Scene %u deinitialized", m_Generation);
}
}
