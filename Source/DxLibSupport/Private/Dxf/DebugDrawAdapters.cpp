// SPDX-License-Identifier: NOASSERTION
#include "Dxf/DebugDrawAdapters.h"
namespace Dxf
{
TResult<void> SubmitDebugDraw(const FDebugDrawStore2D& Store,
const FDebugDrawFilter& Filter, FRender2DContext& Render)
{
	auto Snapshot = Store.Snapshot(Filter);
	if (!Snapshot)
	{
		return TResult<void>::Failure(Snapshot.Error());
	}
	const auto& Values = Snapshot.Value();
	return Render.SubmitGenerated(Values.Size(), [&](Toolbox::size_t Index, FRenderCommand& Output)
	{
		Output = Values[Index];
		return TResult<void>{};
	}
	);
}
TResult<void> SubmitDebugDraw(const FDebugDrawStore3D& Store,
const FDebugDrawFilter& Filter, FRender3DContext& Render)
{
	auto Snapshot = Store.Snapshot(Filter);
	if (!Snapshot)
	{
		return TResult<void>::Failure(Snapshot.Error());
	}
	const auto& Values = Snapshot.Value();
	return Render.SubmitGenerated(Values.Size(), [&](Toolbox::size_t Index, FGeometryCommand3D& Output)
	{
		Output = Values[Index];
		return TResult<void>{};
	}
	);
}
}
