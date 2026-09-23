// SPDX-License-Identifier: NOASSERTION
#include "Dxf/ModelCameraInfo.h"
namespace Dxf
{
// 成功したときだけカメラ設定を反映する。
TResult<void> FModelCameraInfo::ApplyTo(FRenderView3D& View) const
{
	// ライトなど他の設定を保持した検証用の候補。
	FRenderView3D Candidate = View;
	Candidate.Eye = Eye;
	Candidate.Target = Eye + Forward;
	Candidate.Up = Up;
	Candidate.VerticalFov = VerticalFov;
	Candidate.NearPlane = NearPlane;
	Candidate.FarPlane = FarPlane;
	Candidate.bOrthographic = bOrthographic;
	Candidate.OrthographicHeight = OrthographicHeight;
	if (!IsValidRenderView3D(Candidate))
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid imported camera");
	}
	View = Candidate;
	return {};
}
} // namespace Dxf
