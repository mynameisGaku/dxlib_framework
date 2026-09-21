// SPDX-License-Identifier: NOASSERTION
// 故障注入の実装はToolbox境界に隔離し、この実行ファイルだけでリンクする。
#include "Dxf/RenderSystem.h"
#include "../../Source/Toolbox/Private/Toolbox/Testing/AllocationFault.h"
#include <stdio.h>
namespace
{
// Nativeは副作用の件数だけを保存する。検査器自体では確保しない。
class FFaultBackend final : public Dxf::IRenderBackend
{
public:
	Toolbox::uint32 m_Begins = 0;
	Toolbox::uint32 m_Draws = 0;
	Toolbox::uint32 m_Presents = 0;
	Dxf::TResult<void> SetTarget(Toolbox::int32,Toolbox::int32,Toolbox::int32) override { return {}; }
	Dxf::TResult<void> Clear(Dxf::FColor) override { return {}; }
	Dxf::TResult<void> ResetState(Toolbox::int32,Toolbox::int32) override { return {}; }
	Dxf::TResult<void> DrawSprite(const Dxf::FSpriteCommand&) override { return {}; }
	Dxf::TResult<void> DrawText(const Dxf::FTextCommand&) override { return {}; }
	Dxf::TResult<void> DrawRectangle(const Dxf::FRectangleCommand&) override { return {}; }
	bool SupportsGeometry3D() const noexcept override
	{
		return true;
	}
	Dxf::TResult<void> BeginView3D(const Dxf::FRenderView3D&) override { ++m_Begins; return {}; }
	Dxf::TResult<void> DrawGeometry3D(const Dxf::FPreparedGeometry3D&) override { ++m_Draws; return {}; }
	Dxf::TResult<void> EndView3D() override { return {}; }
	Dxf::TResult<void> Present() override { ++m_Presents; return {}; }
};
}
int main()
{
	Toolbox::uint32 HitCount = 0;
	Toolbox::uint32 CompleteRuns = 0;
	for (Toolbox::int64 Position = 0; Position < 1024; ++Position)
	{
		const Toolbox::uint64 Before = Toolbox::Testing::GetOutstandingTestAllocations();
		bool bOkay = true;
		bool bFailed = false;
		bool bInjected = false;
		
		{
			FFaultBackend Backend;
			Dxf::FRenderSystem Renderer(Backend);
			if (!Renderer.BeginFrame(640,480))
			{
				return 2;
			}
			for (Toolbox::uint32 Index = 0; Index < 18; ++Index)
			{
				Dxf::FGeometryCommand3D Command;
				const Toolbox::f32 Z = static_cast<Toolbox::f32>((Index * 7) % 19);
				if (Index % 2 == 0)
				{
					Command.Geometry.Triangles.PushBack({{0,0,Z},{1,0,Z},{0,1,Z}});
				}
				else
				{
					Command.Geometry.Lines.PushBack({{0,0,Z},{1,0,Z}});
				}
				Command.Options.Color.A = Index % 5 == 0 ? 255 : 128;
				if (Index % 6 == 0)
				{
					Command.Options.Layer = Dxf::ERenderLayer3D::Overlay;
				}
				if (!Renderer.GetContext().Get3D().Submit(Toolbox::Move(Command)))
				{
					return 3;
				}
			}
			Toolbox::Testing::SetAllocationFailureCountdown(Position);
			try
			{
				bFailed = !Renderer.EndFrame();
			}
			catch (...)
			{
				bFailed = true;
			}
			bInjected = Toolbox::Testing::WasAllocationFailureInjected();
			Toolbox::Testing::SetAllocationFailureCountdown(-1);
			if (bInjected)
			{
				++HitCount;
				bOkay = bFailed && Backend.m_Begins == 0 && Backend.m_Draws == 0 && Backend.m_Presents == 0;
			}
			else
			{
				++CompleteRuns;
				bOkay = !bFailed && Backend.m_Begins == 1 && Backend.m_Draws > 0 && Backend.m_Presents == 1;
			}
		}
		bOkay = bOkay && Toolbox::Testing::GetOutstandingTestAllocations() == Before;
		if (!bOkay)
		{
			printf("FAIL fault position=%lld injected=%d failed=%d before=%llu after=%llu\n",
				static_cast<long long>(Position), bInjected, bFailed,
				static_cast<unsigned long long>(Before), static_cast<unsigned long long>(Toolbox::Testing::GetOutstandingTestAllocations()));
			return 1;
		}
		if (CompleteRuns == 2)
		{
			if (HitCount == 0)
			{
				printf("FAIL no allocation failure was actually injected\n");
				return 1;
			}
			printf("PASS render planning allocation sweep: %u injected positions, %u complete runs; no Native begin/draw/present on failure\n",
				static_cast<unsigned>(HitCount), static_cast<unsigned>(CompleteRuns));
			return 0;
		}
	}
	printf("FAIL sweep exceeded budget before covering the successful path\n");
	return 1;
}
