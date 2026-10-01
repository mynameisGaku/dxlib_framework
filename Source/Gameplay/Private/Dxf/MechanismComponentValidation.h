// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_MECHANISM_COMPONENT_VALIDATION_H
#define DXF_MECHANISM_COMPONENT_VALIDATION_H
#include "Dxf/JointFrame2D.h"
#include "Dxf/JointFrame3D.h"
#include "Dxf/AngularJointLimits.h"
#include "Dxf/LinearJointLimits.h"
#include "Dxf/AngularJointDrive.h"
#include "Dxf/LinearJointDrive.h"
namespace Dxf::GameplayPrivate
{
/**
 * 取付Frameの数値と方向を検証する。非有限・ゼロ方向は拒否する。
 * @param Value Body重心基準の位置と角度／Quaternion。
 */
void ValidateJointFrame(const FJointFrame2D& Value);
/**
 * 同じ取付Frameかを読み取る。Quaternionの符号反転は等価。
 * @param A 現在設定。
 * @param B 新設定。
 */
bool SameJointFrame(const FJointFrame2D& A, const FJointFrame2D& B) noexcept;
/**
 * 取付Frameの数値と方向を検証する。非有限・ゼロ方向は拒否する。
 * @param Value Body重心基準の位置と角度／Quaternion。
 */
void ValidateJointFrame(const FJointFrame3D& Value);
/**
 * 同じ取付Frameかを読み取る。Quaternionの符号反転は等価。
 * @param A 現在設定。
 * @param B 新設定。
 */
bool SameJointFrame(const FJointFrame3D& A, const FJointFrame3D& B) noexcept;
/**
 * 設定集合を部分反映する前に有限性と有効範囲を検証する。
 * @param Value 角度／並進に対応する要求値。
 */
void ValidateJointLimits(const FAngularJointLimits& Value);
/**
 * 設定集合を部分反映する前に有限性と有効範囲を検証する。
 * @param Value 角度／並進に対応する要求値。
 */
void ValidateJointDrive(const FAngularJointDrive& Value);
/**
 * 設定集合を部分反映する前に有限性と有効範囲を検証する。
 * @param Value 角度／並進に対応する要求値。
 */
void ValidateJointLimits(const FLinearJointLimits& Value);
/**
 * 設定集合を部分反映する前に有限性と有効範囲を検証する。
 * @param Value 角度／並進に対応する要求値。
 */
void ValidateJointDrive(const FLinearJointDrive& Value);
/**
 * 重心からのAnchorを2Dの物理姿勢で回す。
 * @param Angle 現在のrad角度。
 * @param Local Body重心基準の位置。
 */
Toolbox::FVector2 RotateJointAnchor(Toolbox::f32 Angle, Toolbox::FVector2 Local);
} // namespace Dxf::GameplayPrivate
#endif
