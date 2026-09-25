// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_SMOKE_SUPPORT_H
#define DXF_GAMEPLAY_SAMPLE_SMOKE_SUPPORT_H
#include "CharacterSample.h"
#include "Dxf/Application.h"
#include "Dxf/NativeBackends.h"
#include "Toolbox/Platform.h"
namespace Dxf::GameplaySmoke
{
/**
 * OS入力の揺れを排除する入力。更新と押下判定は本物のInputSystemへ任せる。
 */
class FScriptedInput final : public IInputSource
{
public:
	/**
	 * 次のフレームの入力。
	 */
	FRawInput State;
	/**
	 * 現在の入力を返す。
	 */
	TResult<FRawInput> Poll() override;
};

/**
 * 条件が偽なら、失敗の内容を例外で実行結果へ伝える。
 * @param bOk 条件。
 * @param Message 失敗の内容。
 */
void Check(bool bOk, const char* Message);

/**
 * 実Application・実DxLibを、固定した入力と固定したフレーム時刻で進める。
 * 最初のStepの時刻が基準になり、その後は固定更新の境界から4分の1ずらした時刻で、1フレームに1回の固定更新にする。
 */
class FSmokeApp
{
public:
	/**
	 * 実行時の窓口と設定からApplicationを作る（窓口はこのオブジェクトより長く生存すること）。
	 * @param Backends ネイティブの窓口。
	 * @param ProjectRoot アセットの起点。
	 */
	FSmokeApp(FDxLibBackends& Backends, const char* ProjectRoot);
	/**
	 * 2Dのサンプルから始め、基準のStepまで進める。
	 */
	void Start();
	/**
	 * 1フレーム進める（終了していたら失敗）。
	 */
	void Step();
	/**
	 * キーを1フレームだけ押す。
	 * @param Key キー。
	 */
	void Press(EKey Key);
	/**
	 * キーを押したまま／離した状態にする（次のStepから反映）。
	 * @param Key キー。
	 * @param bDown 押すか。
	 */
	void Hold(EKey Key, bool bDown);
	/**
	 * [Tab]で2D／3Dを切り替え、新しいシーンで1フレーム進める。
	 * 新しいシーンの固定更新の蓄積は0から始まり、1フレームの時刻差がちょうど固定幅になって境界に重なる。
	 * 時刻の絶対値が大きいと時刻差の丸めで固定更新が1回ずれるため、このフレームだけ4分の1長く進めて、
	 * 開始時と同じく境界から4分の1ずらす。
	 */
	void SwitchDimension();
	/**
	 * [Esc]で終了させ、Applicationが終了を返すことを確かめる。
	 */
	void Quit();
	/**
	 * 現在のシーンを2Dのサンプルとして返す（違えば失敗）。
	 */
	GameplaySample::DCharacterSample2DScene& Scene2D();
	/**
	 * 現在のシーンを3Dのサンプルとして返す（違えば失敗）。
	 */
	GameplaySample::DCharacterSample3DScene& Scene3D();
	/**
	 * 表示した画面を保存し、指定した画面座標の色を返す。
	 * @param Path 保存先。
	 * @param Point 画面座標。
	 */
	FColor Capture(const Toolbox::FPath& Path, FVector2 Point);

private:
	/**
	 * 固定した入力。
	 */
	FScriptedInput m_Input;
	/**
	 * 実Application。
	 */
	FApplication m_App;
	/**
	 * フレームの時刻（秒）。
	 */
	Toolbox::f64 m_Time = 0;
};
} // namespace Dxf::GameplaySmoke
#endif
