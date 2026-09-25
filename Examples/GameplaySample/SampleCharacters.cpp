// SPDX-License-Identifier: NOASSERTION
#include "SampleCharacters.h"
#include "SampleLevel.h"
namespace Dxf::GameplaySample
{
namespace
{
// 左右（と前後）の入力。押されていない方向は0。
Toolbox::f32 Axis_Internal(const FInputSnapshot& Input, EKey Negative, EKey Positive) noexcept
{
	return (Input.IsDown(Positive) ? 1.0f : 0.0f) - (Input.IsDown(Negative) ? 1.0f : 0.0f);
}
// 初期化前の参照を例外にする。
template <typename T> T& Require_Internal(T* Component, const char* Message)
{
	if (Component == nullptr)
	{
		throw Toolbox::FException(Message);
	}
	return *Component;
}
// 範囲の端での向き（範囲外へ出ていれば内側へ、範囲内なら今の向き）。
Toolbox::f32 Direction_Internal(Toolbox::f32 X, Toolbox::f32 MinX, Toolbox::f32 MaxX, Toolbox::f32 Current) noexcept
{
	if (X > MaxX)
	{
		return -1;
	}
	if (X < MinX)
	{
		return 1;
	}
	return Current;
}
} // namespace

DCharacterMovement2DComponent& DPlayer2D::GetCharacter() const
{
	return Require_Internal(m_Character.Get(), "2D sample player is not initialized");
}
TResult<void> DPlayer2D::OnInitialize(const FInitContext&)
{
	FCharacterMovementDescription2D Description;
	Description.Center = {StartX, StartY};
	auto Added = AddComponent<DCharacterMovement2DComponent>(Description);
	if (!Added)
	{
		return TResult<void>::Failure(Added.Error());
	}
	m_Character = Added.Value();
	return {};
}
void DPlayer2D::OnTick(const FTickContext& Context)
{
	DCharacterMovement2DComponent& Character = GetCharacter();
	Character.SetMoveInput({Axis_Internal(Context.Input, EKey::A, EKey::D), 0});
	if (Context.Input.WasPressed(EKey::Space))
	{
		Character.RequestJump();
	}
}

DCharacterMovement3DComponent& DPlayer3D::GetCharacter() const
{
	return Require_Internal(m_Character.Get(), "3D sample player is not initialized");
}
TResult<void> DPlayer3D::OnInitialize(const FInitContext&)
{
	FCharacterMovementDescription3D Description;
	Description.Center = {StartX, StartY, 0};
	auto Added = AddComponent<DCharacterMovement3DComponent>(Description);
	if (!Added)
	{
		return TResult<void>::Failure(Added.Error());
	}
	m_Character = Added.Value();
	return {};
}
void DPlayer3D::OnTick(const FTickContext& Context)
{
	DCharacterMovement3DComponent& Character = GetCharacter();
	Character.SetMoveInput(
	    {Axis_Internal(Context.Input, EKey::A, EKey::D), 0, Axis_Internal(Context.Input, EKey::S, EKey::W)});
	if (Context.Input.WasPressed(EKey::Space))
	{
		Character.RequestJump();
	}
}

DWalker2D::DWalker2D(Toolbox::FVector2 Start, Toolbox::f32 MinX, Toolbox::f32 MaxX) noexcept
    : m_Start(Start), m_MinX(MinX), m_MaxX(MaxX)
{
}
DCharacterMovement2DComponent& DWalker2D::GetCharacter() const
{
	return Require_Internal(m_Character.Get(), "2D sample walker is not initialized");
}
TResult<void> DWalker2D::OnInitialize(const FInitContext&)
{
	FCharacterMovementDescription2D Description;
	Description.Center = m_Start;
	// 遅めに歩く（プレイヤーと区別できるように）。
	Description.Settings.MaxSpeed = 2;
	auto Added = AddComponent<DCharacterMovement2DComponent>(Description);
	if (!Added)
	{
		return TResult<void>::Failure(Added.Error());
	}
	m_Character = Added.Value();
	return {};
}
void DWalker2D::OnTick(const FTickContext&)
{
	DCharacterMovement2DComponent& Character = GetCharacter();
	m_Direction = Direction_Internal(Character.GetCenter().X, m_MinX, m_MaxX, m_Direction);
	Character.SetMoveInput({m_Direction, 0});
}

DWalker3D::DWalker3D(Toolbox::FVector3 Start, Toolbox::f32 MinX, Toolbox::f32 MaxX) noexcept
    : m_Start(Start), m_MinX(MinX), m_MaxX(MaxX)
{
}
DCharacterMovement3DComponent& DWalker3D::GetCharacter() const
{
	return Require_Internal(m_Character.Get(), "3D sample walker is not initialized");
}
TResult<void> DWalker3D::OnInitialize(const FInitContext&)
{
	FCharacterMovementDescription3D Description;
	Description.Center = m_Start;
	Description.Settings.MaxSpeed = 2;
	auto Added = AddComponent<DCharacterMovement3DComponent>(Description);
	if (!Added)
	{
		return TResult<void>::Failure(Added.Error());
	}
	m_Character = Added.Value();
	return {};
}
void DWalker3D::OnTick(const FTickContext&)
{
	DCharacterMovement3DComponent& Character = GetCharacter();
	m_Direction = Direction_Internal(Character.GetCenter().X, m_MinX, m_MaxX, m_Direction);
	Character.SetMoveInput({m_Direction, 0, 0});
}
} // namespace Dxf::GameplaySample
