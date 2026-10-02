#pragma once
#include "Dxf/InputSnapshot.h"
#include "Dxf/InputActionBindings.h"
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
	 * @param Descriptions 完全な割当集合。空名・重複名・不正入力を全体拒否する。
	 * @param Current 適用する元Snapshot。UIで消費する前の押し続けを確認する。
	 * 確保失敗でも現Mapを保持する。Action名のゲーム許可・予約・競合は上位のプロファイルが検査する。
	 */
	bool ReplaceBindings(const Toolbox::TVector<FInputActionBindings>& Descriptions, const FInputSnapshot& Current)
	{
		// 現Mapを消さず、履歴を含まない候補だけを構築する。
		FInputMap Candidate;
		for (const auto& Description : Descriptions)
		{
			if (Description.Action.IsEmpty() || Candidate.m_Actions.Find(Description.Action) != Candidate.m_Actions.End())
			{
				return false;
			}
			auto& State = Candidate.m_Actions[Description.Action];
			for (const auto& Input : Description.Bindings)
			{
				FBinding Binding;
				switch (Input.Device)
				{
				case EInputBindingDevice::Keyboard:
					if (Input.Key >= EKey::Count)
					{
						return false;
					}
					Binding = {EDevice::Keyboard, static_cast<Toolbox::size_t>(Input.Key), 0};
					break;
				case EInputBindingDevice::Mouse:
					if (Input.MouseButton >= EMouseButton::Count)
					{
						return false;
					}
					Binding = {EDevice::Mouse, static_cast<Toolbox::size_t>(Input.MouseButton), 0};
					break;
				case EInputBindingDevice::Gamepad:
					if (Input.PadSlot >= 4 || Input.PadButton >= 16)
					{
						return false;
					}
					Binding = {EDevice::Gamepad, Input.PadButton, Input.PadSlot};
					break;
				default:
					return false;
				}
				if (Toolbox::Find(State.Bindings.Begin(), State.Bindings.End(), Binding) != State.Bindings.End())
				{
					return false;
				}
				State.Bindings.PushBack(Binding);
			}
		}
		Candidate.SuppressUntilRelease(Current);
		// 全容量の確保と検査の後だけ、例外を出さない移動で公開する。
		static_assert(noexcept(m_Actions = Toolbox::Move(Candidate.m_Actions)));
		m_Actions = Toolbox::Move(Candidate.m_Actions);
		m_bReleaseToArm = true;
		return true;
	}
	/**
	 * @param Current Modal終了や再割当完了Frameの元Snapshot。
	 * 押し続けから新しいActionを発生させず、解放後に再度押した入力だけを有効にする。確保しない。
	 */
	void SuppressUntilRelease(const FInputSnapshot& Current) noexcept
	{
		m_bReleaseToArm = true;
		for (auto& [Name, Action] : m_Actions)
		{
			(void)Name;
			Action.bCurrent = false;
			Action.bPrevious = false;
			for (auto& Binding : Action.Bindings)
			{
				Binding.bArmed = IsBindingAvailable_Internal(Current, Binding) && !IsBindingDown_Internal(Current, Binding);
			}
		}
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
			for (auto& Binding : Action.Bindings)
			{
				// 他の割当が保持中でも、すべての割当の解放を観察する。
				const bool Down = IsBindingDown_Internal(Input, Binding);
				if (m_bReleaseToArm)
				{
					if (!IsBindingAvailable_Internal(Input, Binding))
					{
						Binding.bArmed = false;
					}
					else if (!Down)
					{
						Binding.bArmed = true;
					}
				}
				Action.bCurrent = Action.bCurrent || (Down && (!m_bReleaseToArm || Binding.bArmed));
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
		bool operator==(const FBinding& Other) const noexcept
		{
			return Device == Other.Device && Button == Other.Button && Pad == Other.Pad;
		}
		/**
		 * プロファイル適用後に解放を観察したか。割当の識別とは別。
		 */
		bool bArmed = true;
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
			Binding.bArmed = !m_bReleaseToArm;
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
	 * @param Input 元Snapshot。
	 * @param Binding 判定する入力。
	 * Pad切断と失焦は解放としてarmせず、接続復帰後の解放を待つ。
	 */
	static bool IsBindingAvailable_Internal(const FInputSnapshot& Input, const FBinding& Binding) noexcept
	{
		return Input.GetRaw().bFocused && (Binding.Device != EDevice::Gamepad || (Binding.Pad < Input.GetRaw().Pads.Size() && Input.GetRaw().Pads[Binding.Pad].bConnected));
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
	/**
	 * 従来のBind利用は変更せず、完全プロファイルと明示抑止だけで有効にする。
	 */
	bool m_bReleaseToArm = false;
};
} // namespace Dxf
