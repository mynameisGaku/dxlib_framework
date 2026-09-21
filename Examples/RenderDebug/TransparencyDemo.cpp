// SPDX-License-Identifier: NOASSERTION
#include "TransparencyDemo.h"
namespace Dxf::RenderDebug
{
TResult<void> SubmitTransparencyDemo(FRender3DContext& Render)
{
	const FColor Colors[] = {{255, 80, 60, 112}, {60, 220, 100, 112}, {70, 120, 255, 112}};
	// ソートしない入力順とし、カメラ移動時もRenderer側の計画に任せる。
	for (Toolbox::size_t Index = 0; Index < 3; ++Index)
	{
		const Toolbox::f32 Z = static_cast<Toolbox::f32>(Index) - 1.0f;
		FGeometryCommand3D Command;
		Command.Options.Color = Colors[Index];
		Command.Geometry.Triangles.PushBack({{-2,1,Z}, {2,1,Z}, {2,4,Z}});
		Command.Geometry.Triangles.PushBack({{-2,1,Z}, {2,4,Z}, {-2,4,Z}});
		auto Result = Render.Submit(Toolbox::Move(Command));
		if (!Result)
		{
			return Result;
		}
	}
	return {};
}
}
