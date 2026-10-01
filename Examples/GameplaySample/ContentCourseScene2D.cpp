// SPDX-License-Identifier: NOASSERTION
#include "ContentCourseScene2D.h"
#include "ContentCourseController.h"
#include "ContentCoursePanel.h"
#include "SampleHud.h"
#include "Dxf/SceneContentSource.h"
#include "Toolbox/String.h"
namespace Dxf::GameplaySample
{
DContentCourse2DScene::DContentCourse2DScene(FPreparedScene2D Prepared)
    : DContentScene2D(Prepared), m_Content(Toolbox::Move(Prepared))
{
}
FContentCourseControls& DContentCourse2DScene::GetControls() noexcept
{
	return m_Controls;
}
DContentCourse2DScene::FPrefabHandle DContentCourse2DScene::FindCoursePrefab(Toolbox::FStringView Id) const noexcept
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
TResult<void> DContentCourse2DScene::OnInitialize(const FInitContext& Context)
{
	m_pAssets = &Context.Assets;
	// Controllerを先に置き、前回成功した観察をPrefabが消去する前に読む。
	const auto Controller =
	    Spawn<TContentCourseController<DContentCourse2DScene, DPrismaticJoint2DComponent>>(*this, m_Controls);
	if (!Controller)
	{
		return TResult<void>::Failure(Controller.Error());
	}
	const auto Built = DContentScene2D::OnInitialize(Context);
	if (!Built)
	{
		return Built;
	}
	const auto Portal = Spawn<DContentCoursePortal>(false);
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
void DContentCourse2DScene::SpawnExtra()
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
	    Source.LoadPrefab2D(m_Content.Prefabs[0].Definition->Path, MakeContentCourseOverrides(m_Controls));
	auto Prepared = PreparePrefab(Toolbox::Move(Definition), *m_pAssets);
	const auto Spawned = SpawnPrefab(Id, Toolbox::Move(Prepared), Placement);
	if (!Spawned)
	{
		throw Toolbox::FException(Spawned.Error().Message);
	}
}
void DContentCourse2DScene::ConfigureViews(bool Split)
{
	const auto Width = m_Ui.GetWidth();
	const auto Height = m_Ui.GetHeight();
	if (Width == m_Width && Height == m_Height && Split == m_bSplit)
	{
		return;
	}
	FContentView2D First = m_Content.Definition->Views[0];
	First.Origin = {static_cast<Toolbox::f32>(Width) / (Split ? 4 : 2), static_cast<Toolbox::f32>(Height) * 0.65f};
	First.bClip = Split;
	First.ClipRect = {0, 0, Split ? Width / 2 : Width, Height};
	auto Second = First;
	Second.Origin.X = static_cast<Toolbox::f32>(Width / 2) + static_cast<Toolbox::f32>(Width - Width / 2) / 2;
	Second.ClipRect = {Width / 2, 0, Width, Height};
	SetViews(First, Split ? Toolbox::TOptional<FContentView2D>{Second} : Toolbox::TOptional<FContentView2D>{});
	m_Width = Width;
	m_Height = Height;
	m_bSplit = Split;
}
void DContentCourse2DScene::OnDraw(FRenderContext& Render) const
{
	DContentScene2D::OnDraw(Render);
	m_Ui.Draw(Render, "File course: Space target / G spawn / X destroy / F2 reload / V views / P pause");
}
} // namespace Dxf::GameplaySample
