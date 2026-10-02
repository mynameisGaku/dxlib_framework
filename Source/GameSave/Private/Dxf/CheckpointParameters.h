// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHECKPOINT_PARAMETERS_H
#define DXF_CHECKPOINT_PARAMETERS_H
#include "Dxf/ContentParameterValue.h"
#include "Toolbox/Vector.h"
namespace Dxf::SavePrivate
{
using namespace Toolbox;
// 宣言された型の値だけを比較し、辞書の順序を意味に含めない。
inline bool Same(const FContentParameterValue& A, const FContentParameterValue& B)
{
	if (A.Id != B.Id || A.Kind != B.Kind)
	{
		return false;
	}
	switch (A.Kind)
	{
	case EContentParameterKind::Boolean:
		return A.Boolean == B.Boolean;
	case EContentParameterKind::Number:
		return A.Number == B.Number;
	case EContentParameterKind::Vector2:
		return A.Vector2.X == B.Vector2.X && A.Vector2.Y == B.Vector2.Y;
	case EContentParameterKind::Vector3:
		return A.Vector3.X == B.Vector3.X && A.Vector3.Y == B.Vector3.Y && A.Vector3.Z == B.Vector3.Z;
	case EContentParameterKind::Color:
		return A.Color.R == B.Color.R && A.Color.G == B.Color.G && A.Color.B == B.Color.B && A.Color.A == B.Color.A;
	case EContentParameterKind::Asset:
		return A.Asset == B.Asset;
	}
	return false;
}
inline bool SameParameters(const TVector<FContentParameterValue>& A, const TVector<FContentParameterValue>& B)
{
	if (A.Size() != B.Size())
	{
		return false;
	}
	for (const auto& Value : A)
	{
		bool Found = false;
		for (const auto& Candidate : B)
		{
			Found = Found || Same(Value, Candidate);
		}
		if (!Found)
		{
			return false;
		}
	}
	return true;
}
} // namespace Dxf::SavePrivate
#endif
