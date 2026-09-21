// SPDX-License-Identifier: NOASSERTION
#include "Dxf/Render3DContext.h"
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
TResult<void> FRender3DContext::SetView(const FRenderView3D& View)
{
	if (!m_pAccess->IsAllowed())
	{
		return StateError_Internal();
	}
	if (!IsValidRenderView3D(View) || m_ViewSerial == Toolbox::TNumericLimits<Toolbox::uint64>::Max())
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
TResult<void> FRender3DContext::DrawTriangle(Toolbox::FVector3 A, Toolbox::FVector3 B, Toolbox::FVector3 C, const FDrawStyle3D& Options)
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
TResult<void> FRender3DContext::DrawSphere(const Toolbox::FSphere& Sphere, const FDrawStyle3D& Options, Toolbox::uint32 Segments)
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
void FRender3DContext::Clear_Internal() noexcept
{
	if (m_pAccess->IsAllowed())
	{
		m_Commands.Clear();
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
	m_PrimitiveCount = 0;
	if (Commands.IsEmpty())
	{
		return {};
	}
	if (!Backend.SupportsGeometry3D())
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "Backend has no 3D geometry capability");
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
	bool Active = false;
	Toolbox::uint64 Serial = 0;
	TResult<void> Result;
	try
	{
		for (Toolbox::size_t Index = 0; Index < Commands.Size(); ++Index)
		{
			if (!Active || Serial != Commands[Index].Serial)
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
				Serial = Commands[Index].Serial;
				Active = true;
				Result = Backend.BeginView3D(Commands[Index].View);
				if (!Result)
				{
					break;
				}
			}
			Result = Backend.DrawGeometry3D(Prepared[Index]);
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
}
