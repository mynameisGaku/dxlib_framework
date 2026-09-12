#pragma once
#include "Dxf/InputSnapshot.h"
#include "Toolbox/Algorithm.h"
#include "Toolbox/String.h"
#include "Toolbox/Map.h"
#include "Toolbox/Vector.h"

namespace Dxf
{
/**
 * 入力の割り当てをアクションへ集約する。新しい入力スナップショットごとに一度更新する。
 */
class FInputMap
{
public:
	/**
	 * キーを入力アクションへ割り当てる。
	 * @param Action 入力アクション。
	 * @param Key 検索または入力のキー。
	 */
	bool Bind(Toolbox::FString Action, EKey Key)
	{
		return Key < EKey::Count &&
		       Bind_Internal(Toolbox::Move(Action), {EDevice::Keyboard, static_cast<Toolbox::size_t>(Key), 0});
	}
	/**
	 * マウスボタンを入力アクションへ割り当てる。
	 * @param Action 入力アクション。
	 * @param Button 入力ボタンの番号。
	 */
	bool BindMouse(Toolbox::FString Action, EMouseButton Button)
	{
		return Button < EMouseButton::Count &&
		       Bind_Internal(Toolbox::Move(Action), {EDevice::Mouse, static_cast<Toolbox::size_t>(Button), 0});
	}
	/**
	 * パッドボタンを入力アクションへ割り当てる。
	 * @param Action 入力アクション。
	 * @param Pad ゲームパッドの番号。
	 * @param Button 入力ボタンの番号。
	 */
	bool BindPad(Toolbox::FString Action, Toolbox::size_t Pad, Toolbox::size_t Button)
	{
		return Pad < 4 && Button < 16 && Bind_Internal(Toolbox::Move(Action), {EDevice::Gamepad, Button, Pad});
	}
	/**
	 * 入力アクションの割り当てを取り除く。
	 * @param Action 入力アクション。
	 */
	bool Unbind(const Toolbox::FString& Action)
	{
		return m_Actions.Erase(Action) != 0;
	}
	/**
	 * 蓄積した内容を消去する。
	 */
	void Clear() noexcept
	{
		m_Actions.Clear();
	}
	/**
	 * 入力または管理対象の状態を更新する。
	 * @param Input フレームの入力情報。
	 */
	void Update(const FInputSnapshot& Input)
	{
		// アクション名と対応する状態を順に更新する。
		for (auto& [Name, Action] : m_Actions)
		{
			(void)Name;
			Action.bPrevious = Action.bCurrent;
			Action.bCurrent = false;
			// 入力の割り当て情報を順に処理する。
			for (const auto& Binding : Action.Bindings)
			{
				Action.bCurrent = Action.bCurrent || IsBindingDown_Internal(Input, Binding);
			}
		}
	}
	/**
	 * 現在押されているかを調べる。
	 * @param Action 入力アクション。
	 */
	bool IsDown(const Toolbox::FString& Action) const
	{
		// 現在の状態。
		const auto* State = Find_Internal(Action);
		return State && State->bCurrent;
	}
	/**
	 * 今回のフレームで押されたかを調べる。
	 * @param Action 入力アクション。
	 */
	bool WasPressed(const Toolbox::FString& Action) const
	{
		// 現在の状態。
		const auto* State = Find_Internal(Action);
		return State && State->bCurrent && !State->bPrevious;
	}
	/**
	 * 今回のフレームで離されたかを調べる。
	 * @param Action 入力アクション。
	 */
	bool WasReleased(const Toolbox::FString& Action) const
	{
		// 現在の状態。
		const auto* State = Find_Internal(Action);
		return State && !State->bCurrent && State->bPrevious;
	}
	/**
	 * 正負のアクションから軸入力を求める。
	 * @param NegativeAction 軸の負方向に対応するアクション名。
	 * @param PositiveAction 軸の正方向に対応するアクション名。
	 */
	Toolbox::f32 GetAxis(const Toolbox::FString& NegativeAction, const Toolbox::FString& PositiveAction) const
	{
		return static_cast<Toolbox::f32>(IsDown(PositiveAction)) - static_cast<Toolbox::f32>(IsDown(NegativeAction));
	}

private:
	/**
	 * 割り当てる入力デバイスを識別する。
	 */
	enum class EDevice
	{
		/**
		 * キーボードの入力。
		 */
		Keyboard,
		/**
		 * マウスの入力。
		 */
		Mouse,
		/**
		 * ゲームパッドの入力。
		 */
		Gamepad
	};
	/**
	 * 入力デバイスとボタンの割り当てを表す。
	 */
	struct FBinding
	{
		/**
		 * 入力デバイスの種類。
		 */
		EDevice Device;
		/**
		 * 入力ボタンの番号。
		 */
		Toolbox::size_t Button;
		/**
		 * ゲームパッドの番号。
		 */
		Toolbox::size_t Pad;
		/**
		 * 識別情報が等しいかを比較する。
		 */
		bool operator==(const FBinding&) const = default;
	};
	/**
	 * アクションの割り当てと前後フレームの状態を保持する。
	 */
	struct FActionState
	{
		/**
		 * アクションに割り当てた入力一覧。
		 */
		Toolbox::TVector<FBinding> Bindings;
		/**
		 * 現在のフレームで押されているか。
		 */
		bool bCurrent = false;
		/**
		 * 前回のフレームで押されていたか。
		 */
		bool bPrevious = false;
	};
	/**
	 * キーを入力アクションへ割り当てる。
	 * @param Action 入力アクション。
	 * @param Binding 入力の割り当て情報。
	 */
	bool Bind_Internal(Toolbox::FString Action, FBinding Binding)
	{
		if (Action.IsEmpty())
		{
			return false;
		}
		// アクションに割り当てた入力一覧。
		auto& Bindings = m_Actions[Toolbox::Move(Action)].Bindings;
		if (Toolbox::Find(Bindings.Begin(), Bindings.End(), Binding) == Bindings.End())
		{
			Bindings.PushBack(Binding);
		}
		return true;
	}
	/**
	 * 割り当て先の入力が押されているかを調べる。
	 * @param Input フレームの入力情報。
	 * @param Binding 入力の割り当て情報。
	 */
	static bool IsBindingDown_Internal(const FInputSnapshot& Input, const FBinding& Binding) noexcept
	{
		switch (Binding.Device)
		{
		case EDevice::Keyboard:
			return Input.IsDown(static_cast<EKey>(Binding.Button));
		case EDevice::Mouse:
			return Input.IsMouseDown(static_cast<EMouseButton>(Binding.Button));
		case EDevice::Gamepad:
			return Input.IsPadDown(Binding.Pad, Binding.Button);
		}
		return false;
	}
	/**
	 * 条件に一致する登録情報を探す。
	 * @param Name 登録名。
	 */
	const FActionState* Find_Internal(const Toolbox::FString& Name) const
	{
		// 検索結果のイテレーター。
		const auto It = m_Actions.Find(Name);
		return It == m_Actions.End() ? nullptr : &It->Second;
	}
	/**
	 * アクション名と入力状態の対応表。
	 */
	Toolbox::TMap<Toolbox::FString, FActionState> m_Actions;
};
} // namespace Dxf
