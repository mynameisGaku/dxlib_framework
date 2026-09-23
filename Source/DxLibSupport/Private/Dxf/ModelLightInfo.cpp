// SPDX-License-Identifier: NOASSERTION
#include "Dxf/ModelLightInfo.h"
namespace Dxf
{
// CPU基本形状の照明には触れず、モデル用光源を差し替える。
TResult<void> FModelLightInfo::ApplyTo(FRenderView3D& View, Toolbox::f32 Range, Toolbox::FVector3 Attenuation) const
{
	// 無効な入力で元のビューを壊さない。
	FRenderView3D Candidate = View;
	Candidate.bModelLightOverride = true;
	Candidate.ModelLightType = Type;
	Candidate.ModelLightPosition = Position;
	Candidate.ModelLightDirection = Direction;
	Candidate.ModelLightRadiance = bEnabled ? Radiance : Toolbox::FVector3{};
	Candidate.ModelLightRange = Range;
	Candidate.ModelLightAttenuation = Attenuation;
	Candidate.ModelLightInnerAngle = InnerAngle;
	Candidate.ModelLightOuterAngle = OuterAngle;
	if (!IsValidRenderView3D(Candidate))
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid imported light or attenuation");
	}
	View = Candidate;
	return {};
}
} // namespace Dxf
