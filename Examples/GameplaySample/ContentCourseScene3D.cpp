// SPDX-License-Identifier: NOASSERTION
#include "ContentCourseScene3D.h"
#include "ContentCourseController.h"
#include "ContentCoursePanel.h"
#include "SampleHud.h"
#include "Dxf/SceneContentSource.h"
#include "Toolbox/String.h"
namespace Dxf::GameplaySample
{
DContentCourse3DScene::DContentCourse3DScene(FPreparedScene3D Prepared)
    : DContentScene3D(Prepared), m_Content(Toolbox::Move(Prepared))
{
}
FContentCourseControls& DContentCourse3DScene::GetControls() noexcept
{
	return m_Controls;
}
DContentCourse3DScene::FPrefabHandle DContentCourse3DScene::FindCoursePrefab(Toolbox::FStringView Id) const noexcept
{
	try
	{
		return GetPrefab(Id);
	}
	catch (...)
	{
		return {};
	}
}
TResult<void> DContentCourse3DScene::OnInitialize(const FInitContext& Context)
{
	m_pAssets = &Context.Assets;
	// Controllerを先に置き、前回成功した観察をPrefabが消去する前に読む。
	const auto Controller =
	    Spawn<TContentCourseController<DContentCourse3DScene, DPrismaticJoint3DComponent>>(*this, m_Controls);
	if (!Controller)
	{
		return TResult<void>::Failure(Controller.Error());
	}
	const auto Built = DContentScene3D::OnInitialize(Context);
	if (!Built)
	{
		return Built;
	}
	const auto Portal = Spawn<DContentCoursePortal>(true);
	if (!Portal)
	{
		return TResult<void>::Failure(Portal.Error());
	}
	m_Portal = Portal.Value();
	m_Ui.Initialize(
	    *this, Context.Assets,
	    [this]()
	    {
		    m_Controls.bSplit = !m_Controls.bSplit;
	    },
	    [this]()
	    {
		    m_Controls.bSpawn = true;
	    },
	    [this]()
	    {
		    m_Controls.bDestroy = true;
	    },
	    [this](DUiPanel& Panel, FUiScope& Scope)
	    {
		    BuildContentCoursePanel(Panel, Scope, m_Controls);
		    m_Portal.Get()->BuildPanel(Panel, Scope, m_Portal);
	    },
	    true);
	return {};
}
void DContentCourse3DScene::SpawnExtra()
{
	if (m_Content.Prefabs.IsEmpty())
	{
		return;
	}
	// 生存個体の物理状態を保存せず、初期定義から新しい世代を作る。
	auto Placement = m_Content.Definition->Placements[0];
	Placement.Position.X += 3 + static_cast<Toolbox::f32>(m_SpawnCount);
	++m_SpawnCount;
	const auto Id = Toolbox::FString("extra") + Toolbox::ToString(m_SpawnCount);
	// 明示した生成一回だけ同期準備する。モデル取込は所有側でブロックし得る。
	FSceneContentSource Source(m_pAssets->GetProjectRoot());
	auto Definition =
	    Source.LoadPrefab3D(m_Content.Prefabs[0].Definition->Path, MakeContentCourseOverrides(m_Controls));
	auto Prepared = PreparePrefab(Toolbox::Move(Definition), *m_pAssets);
	const auto Spawned = SpawnPrefab(Id, Toolbox::Move(Prepared), Placement);
	if (!Spawned)
	{
		throw Toolbox::FException(Spawned.Error().Message);
	}
}
void DContentCourse3DScene::ConfigureViews(bool Split)
{
	const auto Width = m_Ui.GetWidth();
	const auto Height = m_Ui.GetHeight();
	if (Width == m_Width && Height == m_Height && Split == m_bSplit)
	{
		return;
	}
	auto First = m_Content.Definition->Views[0];
	First.bViewport = Split;
	First.Viewport = {0, 0, Split ? Width / 2 : Width, Height};
	auto Second = m_Content.Definition->ViewCount == 2 ? m_Content.Definition->Views[1] : First;
	Second.bViewport = true;
	Second.Viewport = {Width / 2, 0, Width, Height};
	SetViews(First, Split ? Toolbox::TOptional<FRenderView3D>{Second} : Toolbox::TOptional<FRenderView3D>{});
	m_Width = Width;
	m_Height = Height;
	m_bSplit = Split;
}
void DContentCourse3DScene::OnDraw(FRenderContext& Render) const
{
	DContentScene3D::OnDraw(Render);
	m_Ui.Draw(Render, "File course: Space target / G spawn / X destroy / F2 reload / V views / P pause");
}
} // namespace Dxf::GameplaySample
