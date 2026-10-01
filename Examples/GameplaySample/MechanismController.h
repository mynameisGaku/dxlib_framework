// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_MECHANISM_CONTROLLER_H
#define DXF_SAMPLE_MECHANISM_CONTROLLER_H
#include "Dxf/JointTargetMotion.h"
#include "MechanismOperationValues.h"
#include "Toolbox/Array.h"
namespace Dxf::GameplaySample
{
/**
 * UIからの操作意図。BodyやJointを所有せず、次の固定更新だけが要求を反映する。
 */
class FMechanismController
{
public:
	/**
	 * 操作する装置を選ぶ。0手動扉、1電動扉、2スライド、3昇降、4固定連結。
	 */
	void Select(Toolbox::int32 Index);
	/**
	 * 現在の対象番号を読む。読み取りで物理は進まない。
	 */
	Toolbox::int32 GetSelection() const noexcept
	{
		return m_Selection;
	}
	/**
	 * 有限の目標、速度、駆動力を予約する。目標は-1〜1、速度0以上、駆動力0以上。
	 */
	void SetMotion(Toolbox::f64 Target, Toolbox::f64 Speed, Toolbox::f64 Effort);
	/**
	 * 現在の操作意図を読む。
	 */
	Toolbox::f64 GetTarget() const noexcept
	{
		return m_Values[m_Selection].Target;
	}
	/**
	 * 最大速度を読む。
	 */
	Toolbox::f64 GetSpeed() const noexcept
	{
		return m_Values[m_Selection].Speed;
	}
	/**
	 * 最大TorqueまたはForceを読む。
	 */
	Toolbox::f64 GetEffort() const noexcept
	{
		return m_Values[m_Selection].Effort;
	}
	/**
	 * 目標操作を停止または再開する。停止は有限の0速度ブレーキ。
	 */
	void SetRunning(bool Value) noexcept
	{
		m_Values[m_Selection].bRunning = Value;
	}
	/**
	 * 目標操作を行うか。
	 */
	bool IsRunning() const noexcept
	{
		return m_Values[m_Selection].bRunning;
	}
	/**
	 * 対象のLimit使用を予約する。
	 */
	void SetLimited(bool Value) noexcept
	{
		m_Values[m_Selection].bLimited = Value;
	}
	/**
	 * Limitを使うか。
	 */
	bool IsLimited() const noexcept
	{
		return m_Values[m_Selection].bLimited;
	}
	/**
	 * 対象の接続または解除を予約する。
	 */
	void SetConnected(bool Value) noexcept
	{
		m_Values[m_Selection].bConnected = Value;
	}
	/**
	 * 接続の意図を読む。
	 */
	bool IsConnected() const noexcept
	{
		return m_Values[m_Selection].bConnected;
	}
	/**
	 * 手動扉または固定連結物への一度の力積を予約する。
	 */
	void RequestKick() noexcept
	{
		m_bKick = true;
	}
	/**
	 * 固定更新が力積予約を一度だけ受け取る。
	 */
	bool TakeKick() noexcept
	{
		const bool Value = m_bKick;
		m_bKick = false;
		return Value;
	}
	/**
	 * 固定更新が対象Indexの操作意図を読む。0〜4以外は拒否する。
	 */
	const FMechanismOperationValues& GetValues(Toolbox::int32 Index) const;

private:
	/**
	 * 対象の装置。
	 */
	Toolbox::int32 m_Selection = 1;
	/**
	 * 五つの装置の独立した操作意図。
	 */
	Toolbox::TArray<FMechanismOperationValues, 5> m_Values;
	/**
	 * 未配達の力積要求。
	 */
	bool m_bKick = false;
};
} // namespace Dxf::GameplaySample
#endif
