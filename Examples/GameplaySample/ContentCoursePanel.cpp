// SPDX-License-Identifier: NOASSERTION
#include "ContentCoursePanel.h"
#include "Dxf/UiButton.h"
#include "Dxf/UiRoot.h"
#include "Dxf/UiSlider.h"
#include "Dxf/UiToggle.h"
#include "Dxf/UiLabel.h"
namespace Dxf::GameplaySample
{
Toolbox::TVector<FContentParameterValue> MakeContentCourseOverrides(const FContentCourseControls& Controls)
{
	Toolbox::TVector<FContentParameterValue> Values;
	FContentParameterValue Speed;
	Speed.Id = "speed";
	Speed.Number = Controls.NextSpeed;
	Values.PushBack(Speed);
	FContentParameterValue Travel;
	Travel.Id = "travel";
	Travel.Number = Controls.NextTravel;
	Values.PushBack(Travel);
	FContentParameterValue Effort;
	Effort.Id = "effort";
	Effort.Number = Controls.NextEffort;
	Values.PushBack(Effort);
	FContentParameterValue Tint;
	Tint.Id = "tint";
	Tint.Kind = EContentParameterKind::Color;
	Tint.Color = Controls.bNextWarmColor ? FColor{240, 180, 80, 255} : FColor{80, 180, 240, 255};
	Values.PushBack(Tint);
	return Values;
}
void BuildContentCoursePanel(DUiPanel& Panel, FUiScope& Scope, FContentCourseControls& Controls)
{
	// 選択と生成パラメーターではなく、公開driveへのゲーム要求。
	auto Select = Panel.CreateChild<DUiButton>("Selected: doorA (next)");
	Select.Get()->SetHeight(FUiLength::Fixed(20));
	Scope.Add(Select.Get()->OnClicked().Subscribe(
	    [&Controls, Select]()
	    {
		    Controls.Selection = 1 - Controls.Selection;
		    Select.Get()->SetText(Controls.Selection == 0 ? "Selected: doorA (next)" : "Selected: doorB (next)");
	    }));
	auto Target = Panel.CreateChild<DUiSlider>();
	Target.Get()->SetRange(0, 2, 0.05);
	Target.Get()->SetValue(Controls.Target);
	Target.Get()->SetHeight(FUiLength::Fixed(20));
	Scope.Add(Target.Get()->OnValueChanged().Subscribe(
	    [&Controls](Toolbox::f64 Value)
	    {
		    Controls.Target = Value;
		    Controls.bOperate = true;
	    }));
	auto Speed = Panel.CreateChild<DUiSlider>();
	Speed.Get()->SetRange(0, 3, 0.05);
	Speed.Get()->SetValue(Controls.Speed);
	Speed.Get()->SetHeight(FUiLength::Fixed(20));
	Scope.Add(Speed.Get()->OnValueChanged().Subscribe(
	    [&Controls](Toolbox::f64 Value)
	    {
		    Controls.Speed = Value;
		    Controls.bOperate = true;
	    }));
	auto Effort = Panel.CreateChild<DUiSlider>();
	Effort.Get()->SetRange(0, 80, 1);
	Effort.Get()->SetValue(Controls.Effort);
	Effort.Get()->SetHeight(FUiLength::Fixed(20));
	Scope.Add(Effort.Get()->OnValueChanged().Subscribe(
	    [&Controls](Toolbox::f64 Value)
	    {
		    Controls.Effort = Value;
		    Controls.bOperate = true;
	    }));
	auto Run = Panel.CreateChild<DUiToggle>("Run / finite brake", Controls.bRunning);
	Run.Get()->SetHeight(FUiLength::Fixed(20));
	Scope.Add(Run.Get()->OnValueChanged().Subscribe(
	    [&Controls](bool Value)
	    {
		    Controls.bRunning = Value;
		    Controls.bOperate = true;
	    }));
	auto Connected = Panel.CreateChild<DUiToggle>("Connection", Controls.bConnected);
	Connected.Get()->SetHeight(FUiLength::Fixed(20));
	Scope.Add(Connected.Get()->OnValueChanged().Subscribe(
	    [&Controls](bool Value)
	    {
		    Controls.bConnected = Value;
		    Controls.bOperate = true;
	    }));
	// 以下は新生成だけの設定。上の値は選択中の個体への操作。
	Panel.CreateChild<DUiLabel>("Next instance: speed / travel / effort / color");
	auto NextSpeed = Panel.CreateChild<DUiSlider>();
	NextSpeed.Get()->SetRange(-3, 3, 0.05);
	NextSpeed.Get()->SetValue(Controls.NextSpeed);
	NextSpeed.Get()->SetHeight(FUiLength::Fixed(20));
	Scope.Add(NextSpeed.Get()->OnValueChanged().Subscribe(
	    [&Controls](Toolbox::f64 Value)
	    {
		    Controls.NextSpeed = Value;
	    }));
	auto NextTravel = Panel.CreateChild<DUiSlider>();
	NextTravel.Get()->SetRange(.1, 5, .1);
	NextTravel.Get()->SetValue(Controls.NextTravel);
	NextTravel.Get()->SetHeight(FUiLength::Fixed(20));
	Scope.Add(NextTravel.Get()->OnValueChanged().Subscribe(
	    [&Controls](Toolbox::f64 Value)
	    {
		    Controls.NextTravel = Value;
	    }));
	auto NextEffort = Panel.CreateChild<DUiSlider>();
	NextEffort.Get()->SetRange(0, 80, 1);
	NextEffort.Get()->SetValue(Controls.NextEffort);
	NextEffort.Get()->SetHeight(FUiLength::Fixed(20));
	Scope.Add(NextEffort.Get()->OnValueChanged().Subscribe(
	    [&Controls](Toolbox::f64 Value)
	    {
		    Controls.NextEffort = Value;
	    }));
	auto NextColor = Panel.CreateChild<DUiToggle>("Next instance warm color", Controls.bNextWarmColor);
	NextColor.Get()->SetHeight(FUiLength::Fixed(20));
	Scope.Add(NextColor.Get()->OnValueChanged().Subscribe(
	    [&Controls](bool Value)
	    {
		    Controls.bNextWarmColor = Value;
	    }));
	auto Spawn = Panel.CreateChild<DUiButton>("Spawn extra / G");
	Spawn.Get()->SetHeight(FUiLength::Fixed(20));
	Scope.Add(Spawn.Get()->OnClicked().Subscribe(
	    [&Controls]()
	    {
		    Controls.bSpawn = true;
	    }));
	auto Destroy = Panel.CreateChild<DUiButton>("Destroy selected / X");
	Destroy.Get()->SetHeight(FUiLength::Fixed(20));
	Scope.Add(Destroy.Get()->OnClicked().Subscribe(
	    [&Controls]()
	    {
		    Controls.bDestroy = true;
	    }));
}
} // namespace Dxf::GameplaySample
