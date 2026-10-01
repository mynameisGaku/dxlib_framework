// SPDX-License-Identifier: NOASSERTION
#include "MechanismPanel.h"
#include "Dxf/UiButton.h"
#include "Dxf/UiLabel.h"
#include "Dxf/UiSlider.h"
#include "Dxf/UiToggle.h"
#include "Dxf/UiRoot.h"
namespace Dxf::GameplaySample
{
void BuildMechanismPanel(DUiPanel& Panel, FUiScope& Scope, FMechanismController& Controller)
{
	// ボタンは対象を順送りにする。物理要求は再開後の固定更新から送る。
	auto Selection = Panel.CreateChild<DUiButton>("Device: electric door (next)");
	Selection.Get()->SetHeight(FUiLength::Fixed(24));
	// 角度は目標×0.65rad、並進は目標+1mへ変換する。
	Panel.CreateChild<DUiLabel>("Target -1..1 / speed / torque or force");
	auto Target = Panel.CreateChild<DUiSlider>();
	Target.Get()->SetRange(-1, 1, 0.05);
	Target.Get()->SetValue(Controller.GetTarget());
	Target.Get()->SetHeight(FUiLength::Fixed(24));
	Target.Get()->SetWidth(FUiLength::Fill());
	Scope.Add(Target.Get()->OnValueChanged().Subscribe([&Controller](Toolbox::f64 Value)
	                                                   {
		                                                   Controller.SetMotion(Value, Controller.GetSpeed(), Controller.GetEffort());
	                                                   }));
	auto Speed = Panel.CreateChild<DUiSlider>();
	Speed.Get()->SetRange(0, 3, 0.05);
	Speed.Get()->SetValue(Controller.GetSpeed());
	Speed.Get()->SetHeight(FUiLength::Fixed(24));
	Speed.Get()->SetWidth(FUiLength::Fill());
	Scope.Add(Speed.Get()->OnValueChanged().Subscribe([&Controller](Toolbox::f64 Value)
	                                                  {
		                                                  Controller.SetMotion(Controller.GetTarget(), Value, Controller.GetEffort());
	                                                  }));
	auto Effort = Panel.CreateChild<DUiSlider>();
	Effort.Get()->SetRange(0, 150, 1);
	Effort.Get()->SetValue(Controller.GetEffort());
	Effort.Get()->SetHeight(FUiLength::Fixed(24));
	Effort.Get()->SetWidth(FUiLength::Fill());
	Scope.Add(Effort.Get()->OnValueChanged().Subscribe([&Controller](Toolbox::f64 Value)
	                                                   {
		                                                   Controller.SetMotion(Controller.GetTarget(), Controller.GetSpeed(), Value);
	                                                   }));
	auto Running = Panel.CreateChild<DUiToggle>("Run / finite brake", Controller.IsRunning());
	Running.Get()->SetHeight(FUiLength::Fixed(24));
	Scope.Add(Running.Get()->OnValueChanged().Subscribe([&Controller](bool Value)
	                                                    {
		                                                    Controller.SetRunning(Value);
	                                                    }));
	auto Limited = Panel.CreateChild<DUiToggle>("Limits", Controller.IsLimited());
	Limited.Get()->SetHeight(FUiLength::Fixed(24));
	Scope.Add(Limited.Get()->OnValueChanged().Subscribe([&Controller](bool Value)
	                                                    {
		                                                    Controller.SetLimited(Value);
	                                                    }));
	auto Connected = Panel.CreateChild<DUiToggle>("Connection", Controller.IsConnected());
	Connected.Get()->SetHeight(FUiLength::Fixed(24));
	Scope.Add(Connected.Get()->OnValueChanged().Subscribe([&Controller](bool Value)
	                                                      {
		                                                      Controller.SetConnected(Value);
	                                                      }));
	Scope.Add(Selection.Get()->OnClicked().Subscribe([&Controller, Selection, Target, Speed, Effort, Running, Limited, Connected]()
	                                                 {
		                                                 Controller.Select((Controller.GetSelection() + 1) % 5);
		                                                 const char* Names[5] = {"Device: manual door", "Device: electric door", "Device: sliding gate", "Device: moving lift", "Device: fixed assembly"};
		                                                 Selection.Get()->SetText(Names[Controller.GetSelection()]);
		                                                 Target.Get()->SetValue(Controller.GetTarget());
		                                                 Speed.Get()->SetValue(Controller.GetSpeed());
		                                                 Effort.Get()->SetValue(Controller.GetEffort());
		                                                 Running.Get()->SetValue(Controller.IsRunning());
		                                                 Limited.Get()->SetValue(Controller.IsLimited());
		                                                 Connected.Get()->SetValue(Controller.IsConnected());
	                                                 }));
	auto Kick = Panel.CreateChild<DUiButton>("Push manual door / fixed cargo");
	Kick.Get()->SetHeight(FUiLength::Fixed(24));
	Scope.Add(Kick.Get()->OnClicked().Subscribe([&Controller]()
	                                            {
		                                            Controller.RequestKick();
	                                            }));
}
} // namespace Dxf::GameplaySample
