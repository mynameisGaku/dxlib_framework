// SPDX-License-Identifier: NOASSERTION
// 表示先ごとの検査。描画と同じ表示面・命令・追加のクリップで、描画せずに検査する。
#include "UiHostState.h"
#include "Dxf/UiDrawList.h"
#include "Dxf/UiInspection.h"
namespace Dxf::Detail
{
FUiRect FUiHostState::MakeDisplayClip_Internal(const FDisplay& Display, const FUiSurface& Surface) const noexcept
{
	if (Display.Kind != EUiDisplayKind::WorldPanel2D || Display.Panel2D.ScreenClip.IsEmpty())
	{
		return {};
	}
	// 2Dワールドのパネルは画面の切り抜きの範囲だけに描く。
	const FUiPixelRect Area = Surface.GetPixelRect().Intersect(Display.Panel2D.ScreenClip);
	const FVector2 TopLeft =
	    Surface.ToLogical({static_cast<Toolbox::f32>(Area.Left), static_cast<Toolbox::f32>(Area.Top)});
	const FVector2 BottomRight =
	    Surface.ToLogical({static_cast<Toolbox::f32>(Area.Right), static_cast<Toolbox::f32>(Area.Bottom)});
	return {TopLeft.X, TopLeft.Y, BottomRight.X - TopLeft.X, BottomRight.Y - TopLeft.Y};
}

TResult<FUiInspectionResult> FUiHostState::InspectDisplay(FUiDisplayId Id, Toolbox::size_t MaxIssues)
{
	const FDisplay* Display = Find_Internal(Id);
	if (Display == nullptr || Display->Root.Get() == nullptr)
	{
		return TResult<FUiInspectionResult>::Failure(EErrorCode::NotFound, "UI display not found");
	}
	FUiRoot& Root = *Display->Root.Get();
	const FUiSurface Surface = MakeSurface_Internal(*Display);
	Root.SetSurface(Surface);
	auto Laid = Root.Layout();
	if (!Laid)
	{
		return TResult<FUiInspectionResult>::Failure(Laid.Error());
	}
	// 描画と同じ命令を作り、表示先の追加のクリップを同じように掛ける。
	FUiDrawList List;
	auto Built = Root.BuildDrawList(List);
	if (!Built)
	{
		return TResult<FUiInspectionResult>::Failure(Built.Error());
	}
	// 通知でHostの表示先が変わった場合は、見つけ直して最新の値で続ける。
	Display = Find_Internal(Id);
	if (Display == nullptr)
	{
		return TResult<FUiInspectionResult>::Failure(EErrorCode::NotFound, "UI display removed during inspection");
	}
	FUiInspectionTarget Target;
	Target.DisplayId = Id;
	Target.DisplayClip = MakeDisplayClip_Internal(*Display, Surface);
	if (!Target.DisplayClip.IsEmpty())
	{
		for (auto& Item : List.EditItems())
		{
			Item.Clip = Item.Clip.Intersect(Target.DisplayClip);
		}
	}
	try
	{
		return TResult<FUiInspectionResult>::Success(InspectUiLayout(Root, &List, MaxIssues, Target));
	}
	catch (const Toolbox::FException& Error)
	{
		return TResult<FUiInspectionResult>::Failure(EErrorCode::InvalidState, Error.What());
	}
}
} // namespace Dxf::Detail
