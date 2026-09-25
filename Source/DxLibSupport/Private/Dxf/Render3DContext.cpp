// SPDX-License-Identifier: NOASSERTION
#include "Dxf/Render3DContext.h"
#include "RenderPass3D.h"
namespace Dxf
{
TResult<void> FRender3DContext::StateError_Internal()
{
	return TResult<void>::Failure(EErrorCode::InvalidState, "3D context unavailable or reentrant");
}
TResult<void> FRender3DContext::LimitError_Internal()
{
	return TResult<void>::Failure(EErrorCode::InvalidArgument, "3D frame command budget exceeded");
}
TResult<void> FRender3DContext::InvalidGeometry_Internal()
{
	return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid 3D geometry");
}
bool FRender3DContext::FitsTarget_Internal(const FRenderView3D& View) const noexcept
{
	return !View.bViewport || m_pAccess->m_TargetWidth == 0 ||
	       (View.Viewport.Right <= m_pAccess->m_TargetWidth && View.Viewport.Bottom <= m_pAccess->m_TargetHeight);
}
TResult<void> FRender3DContext::SetView(const FRenderView3D& View)
{
	if (!m_pAccess->IsAllowed())
	{
		return StateError_Internal();
	}
	if (!IsValidRenderView3D(View) || !FitsTarget_Internal(View) ||
	    m_ViewSerial == Toolbox::TNumericLimits<Toolbox::uint64>::Max())
	{
		return InvalidGeometry_Internal();
	}
	m_View = View;
	++m_ViewSerial;
	return {};
}
TResult<void> FRender3DContext::Submit(FGeometryCommand3D Command)
{
	if (!m_pAccess->IsAccepting())
	{
		return StateError_Internal();
	}
	if (!FitsTarget_Internal(m_View))
	{
		return InvalidGeometry_Internal();
	}
	if (!IsValidGeometry3D(Command))
	{
		return InvalidGeometry_Internal();
	}
	const Toolbox::size_t Primitives = Command.Geometry.Lines.Size() + Command.Geometry.Triangles.Size();
	if (m_Commands.Size() >= 65536 || Primitives > MaxFramePrimitives - m_PrimitiveCount)
	{
		return LimitError_Internal();
	}
	m_Commands.PushBack({Toolbox::Move(Command), m_View, m_ViewSerial});
	m_PrimitiveCount += Primitives;
	return {};
}
TResult<void> FRender3DContext::DrawLine(Toolbox::FVector3 Start, Toolbox::FVector3 End, const FDrawStyle3D& Options)
{
	if (!m_pAccess->IsAccepting())
	{
		return StateError_Internal();
	}
	FGeometryCommand3D Command;
	Command.Geometry.Lines.PushBack({Start, End});
	Command.Options = Options;
	return Submit(Toolbox::Move(Command));
}
TResult<void> FRender3DContext::DrawTriangle(Toolbox::FVector3 A, Toolbox::FVector3 B, Toolbox::FVector3 C,
                                             const FDrawStyle3D& Options)
{
	if (!m_pAccess->IsAccepting())
	{
		return StateError_Internal();
	}
	FGeometryCommand3D Command;
	Command.Geometry.Triangles.PushBack({A, B, C});
	Command.Options = Options;
	return Submit(Toolbox::Move(Command));
}
TResult<void> FRender3DContext::DrawBox(const Toolbox::FOBB& Box, const FDrawStyle3D& Options)
{
	if (!m_pAccess->IsAccepting())
	{
		return StateError_Internal();
	}
	auto Geometry = BuildBoxGeometry3D(Box);
	if (!Geometry)
	{
		return TResult<void>::Failure(Geometry.Error());
	}
	return Submit({Toolbox::Move(Geometry).Value(), Options});
}
TResult<void> FRender3DContext::DrawSphere(const Toolbox::FSphere& Sphere, const FDrawStyle3D& Options,
                                           Toolbox::uint32 Segments)
{
	if (!m_pAccess->IsAccepting())
	{
		return StateError_Internal();
	}
	auto Geometry = BuildSphereGeometry3D(Sphere, Segments);
	if (!Geometry)
	{
		return TResult<void>::Failure(Geometry.Error());
	}
	return Submit({Toolbox::Move(Geometry).Value(), Options});
}
TResult<void> FRender3DContext::DrawMesh(const FGeometry3D& Geometry, const FDrawStyle3D& Options)
{
	if (!m_pAccess->IsAccepting())
	{
		return StateError_Internal();
	}
	return Submit({Geometry, Options});
}
// 読み込んだモデルを1体描画する。変換と再生状態は受付時点の値を複写する。
// @param Instance 描画するインスタンス。
TResult<void> FRender3DContext::DrawModel(const FModelInstance& Instance)
{
	if (!m_pAccess->IsAccepting())
	{
		return StateError_Internal();
	}
	if (!FitsTarget_Internal(m_View))
	{
		return InvalidGeometry_Internal();
	}
	if (!Instance.IsValid())
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Model instance is not valid");
	}
	if (m_Models.Size() >= MaxFrameModels)
	{
		return LimitError_Internal();
	}
	FRecordedModel Record;
	Record.Instance = Instance.GetResource_Internal();
	Record.Draw.pInstance = Record.Instance.Get();
	Record.Draw.World = Instance.GetTransform();
	Record.Draw.Material = Instance.GetMaterial();
	for (Toolbox::size_t Index = 0; Index < Instance.GetModel().GetMorphCount(); ++Index)
	{
		Record.Draw.MorphWeights.PushBack(Instance.GetMorphWeight(Index));
	}
	Record.Draw.Clip = Instance.GetClip();
	Record.Draw.NativeTime = Instance.GetNativeTime_Internal();
	Record.View = m_View;
	Record.Serial = m_ViewSerial;
	m_Models.PushBack(Toolbox::Move(Record));
	return {};
}
// テクスチャを貼った四角形を記録する。
// @param Quad 四角形。
TResult<void> FRender3DContext::DrawTexturedQuad(const FTexturedQuad3D& Quad)
{
	if (!m_pAccess->IsAccepting())
	{
		return StateError_Internal();
	}
	if (!FitsTarget_Internal(m_View))
	{
		return InvalidGeometry_Internal();
	}
	if (!Quad.Texture.IsValid())
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Quad texture is not valid");
	}
	for (const auto& Corner : Quad.Corners)
	{
		if (!Toolbox::IsFinite(Corner.X) || !Toolbox::IsFinite(Corner.Y) || !Toolbox::IsFinite(Corner.Z))
		{
			return InvalidGeometry_Internal();
		}
	}
	if (m_Quads.Size() >= MaxFrameModels)
	{
		return LimitError_Internal();
	}
	m_Quads.PushBack({Quad, m_View, m_ViewSerial});
	return {};
}
void FRender3DContext::Clear_Internal() noexcept
{
	if (m_pAccess->IsAllowed())
	{
		m_Commands.Clear();
		m_Models.Clear();
		m_Quads.Clear();
		m_PrimitiveCount = 0;
	}
}
TResult<void> FRender3DContext::Execute_Internal(IRenderBackend& Backend)
{
	if (!m_pAccess->IsAllowed())
	{
		return StateError_Internal();
	}
	TGuardValue Busy(m_pAccess->m_bBusy, true);
	auto Commands = Toolbox::Move(m_Commands);
	// 実行または破棄まで、記録したインスタンスを生存させる。
	auto Models = Toolbox::Move(m_Models);
	// 実行または破棄まで、記録したテクスチャを生存させる。
	auto Quads = Toolbox::Move(m_Quads);
	m_PrimitiveCount = 0;
	if (Commands.IsEmpty() && Models.IsEmpty() && Quads.IsEmpty())
	{
		return {};
	}
	if (!Quads.IsEmpty() && !Backend.SupportsTexturedQuads3D())
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "Backend has no textured quad capability");
	}
	if (!Commands.IsEmpty() && !Backend.SupportsGeometry3D())
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "Backend has no 3D geometry capability");
	}
	if (!Models.IsEmpty() && !Backend.SupportsModels3D())
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "Backend has no 3D model capability");
	}
	// すべての変換・検証・確保を、Backendの状態を変更する前に終える。
	Toolbox::TVector<FPreparedGeometry3D> Prepared;
	Prepared.Reserve(Commands.Size());
	for (const auto& Record : Commands)
	{
		auto Result = PrepareGeometry3D(Record.Command, Record.View);
		if (!Result)
		{
			return TResult<void>::Failure(Result.Error());
		}
		Prepared.PushBack(Toolbox::Move(Result).Value());
	}
	// SetViewの呼出し区間を跨がずに計画する。同じIdを再使用しても別区間。
	// モデルは不透明として、同じ区間の形状（不透明・透明・Overlay）より先に描く。
	struct FViewPass
	{
		FRenderView3D View;
		Toolbox::TVector<FPreparedGeometry3D> Packets;
		Toolbox::size_t ModelBegin = 0;
		Toolbox::size_t ModelEnd = 0;
		Toolbox::size_t QuadBegin = 0;
		Toolbox::size_t QuadEnd = 0;
	};
	Toolbox::TVector<FViewPass> Passes;
	// 形状とモデルはどちらも区間番号の昇順に並ぶため、先頭どうしを比べて区間ごとにまとめる。
	for (Toolbox::size_t Begin = 0, ModelBegin = 0, QuadBegin = 0;
	     Begin < Commands.Size() || ModelBegin < Models.Size() || QuadBegin < Quads.Size();)
	{
		// 三つの列の先頭のうち、最も小さい区間番号の区間をまとめる。
		Toolbox::uint64 Serial = Toolbox::TNumericLimits<Toolbox::uint64>::Max();
		const FRenderView3D* pView = nullptr;
		if (Begin < Commands.Size() && Commands[Begin].Serial <= Serial)
		{
			Serial = Commands[Begin].Serial;
			pView = &Commands[Begin].View;
		}
		if (ModelBegin < Models.Size() && Models[ModelBegin].Serial < Serial)
		{
			Serial = Models[ModelBegin].Serial;
			pView = &Models[ModelBegin].View;
		}
		if (QuadBegin < Quads.Size() && Quads[QuadBegin].Serial < Serial)
		{
			Serial = Quads[QuadBegin].Serial;
			pView = &Quads[QuadBegin].View;
		}
		const FRenderView3D& View = *pView;
		Toolbox::size_t End = Begin;
		while (End < Commands.Size() && Commands[End].Serial == Serial)
		{
			++End;
		}
		Toolbox::size_t ModelEnd = ModelBegin;
		while (ModelEnd < Models.Size() && Models[ModelEnd].Serial == Serial)
		{
			++ModelEnd;
		}
		Toolbox::size_t QuadEnd = QuadBegin;
		while (QuadEnd < Quads.Size() && Quads[QuadEnd].Serial == Serial)
		{
			++QuadEnd;
		}
		FViewPass Pass{View, {}, ModelBegin, ModelEnd, QuadBegin, QuadEnd};
		if (End > Begin)
		{
			auto Plan = Detail::BuildRenderPasses3D_Internal(Prepared, Begin, End, View);
			if (!Plan)
			{
				return TResult<void>::Failure(Plan.Error());
			}
			Pass.Packets = Toolbox::Move(Plan).Value();
		}
		if (!Pass.Packets.IsEmpty() || ModelEnd > ModelBegin || QuadEnd > QuadBegin)
		{
			Passes.PushBack(Toolbox::Move(Pass));
		}
		Begin = End;
		ModelBegin = ModelEnd;
		QuadBegin = QuadEnd;
	}
	for (const auto& Pass : Passes)
	{
		if (Pass.View.bViewport && !Backend.SupportsViewports3D())
		{
			return TResult<void>::Failure(EErrorCode::BackendFailure, "Backend has no viewport capability");
		}
	}
	bool Active = false;
	TResult<void> Result;
	try
	{
		for (const auto& Pass : Passes)
		{
			if (Active)
			{
				Active = false;
				Result = Backend.EndView3D();
				if (!Result)
				{
					return Result;
				}
			}
			Active = true;
			Result = Backend.BeginView3D(Pass.View);
			if (!Result)
			{
				break;
			}
			for (Toolbox::size_t Index = Pass.ModelBegin; Index < Pass.ModelEnd; ++Index)
			{
				Result = Backend.DrawModel3D(Models[Index].Draw);
				if (!Result)
				{
					break;
				}
			}
			if (!Result)
			{
				break;
			}
			for (const auto& Packet : Pass.Packets)
			{
				Result = Backend.DrawGeometry3D(Packet);
				if (!Result)
				{
					break;
				}
			}
			if (!Result)
			{
				break;
			}
			// テクスチャを貼った四角形は同じ区間の形状の後に描く（半透明のパネルを想定）。
			for (Toolbox::size_t Index = Pass.QuadBegin; Index < Pass.QuadEnd; ++Index)
			{
				Result = Backend.DrawTexturedQuad3D(Quads[Index].Quad);
				if (!Result)
				{
					break;
				}
			}
			if (!Result)
			{
				break;
			}
		}
	}
	catch (...)
	{
		if (Active)
		{
			try
			{
				(void)Backend.EndView3D();
			}
			catch (...)
			{
			}
		}
		throw;
	}
	if (Active)
	{
		try
		{
			auto Cleanup = Backend.EndView3D();
			if (Result && !Cleanup)
			{
				return Cleanup;
			}
		}
		catch (const Toolbox::FException& Error)
		{
			if (Result)
			{
				return TResult<void>::Failure(EErrorCode::BackendFailure, Error.What());
			}
		}
		catch (...)
		{
			if (Result)
			{
				return TResult<void>::Failure(EErrorCode::BackendFailure, "3D cleanup exception");
			}
		}
	}
	return Result;
}
} // namespace Dxf
