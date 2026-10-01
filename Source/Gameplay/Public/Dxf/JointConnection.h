// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_JOINTCONNECTION_H
#define DXF_GAMEPLAY_JOINTCONNECTION_H
#include "Dxf/DistanceJointConnection.h"
namespace Dxf
{
/**
 * 新旧Jointで共用する接続状態。既存の公開型と列挙値を維持する。
 */
using EJointConnection = EDistanceJointConnection;
} // namespace Dxf
#endif
