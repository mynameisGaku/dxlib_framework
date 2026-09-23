// SPDX-License-Identifier: NOASSERTION
#include "PickingExample.h"
#include "Toolbox/SegmentIntersection.h"
namespace Dxf::ModelViewer
{
TResult<Toolbox::int32> PickExampleShapes(const FRenderView3D& View, Toolbox::int32 Width, Toolbox::int32 Height, FVector2 Screen, const Toolbox::FSphere& Sphere, const Toolbox::FOBB& Box)
{
	const auto Line = MakeViewPickSegment(View, Width, Height, Screen);
	if (!Line)
	{
		return TResult<Toolbox::int32>::Failure(Line.Error());
	}
	if (!Line.Value())
	{
		return TResult<Toolbox::int32>::Success(-1);
	}
	try
	{
		const auto& Segment = *Line.Value();
		const auto BallHit = Toolbox::IntersectSegment(Segment.Start, Segment.End, Sphere);
		const auto BoxHit = Toolbox::IntersectSegment(Segment.Start, Segment.End, Box);
		// 同じ線分内の割合なのでそのまま手前を比較できる。同値なら球を維持する。
		return TResult<Toolbox::int32>::Success(BoxHit && (!BallHit || *BoxHit < *BallHit) ? 1 : (BallHit ? 0 : -1));
	}
	catch (const Toolbox::FException& Error)
	{
		return TResult<Toolbox::int32>::Failure(EErrorCode::InvalidArgument, Error.What());
	}
}
} // namespace Dxf::ModelViewer
